#include "PoolVerifier.h"
#include "PoolDatabase.h"
#include "CurlHandler.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

namespace {
    // URL-encode a peerId or multiaddr so it can be passed as a query arg
    // to the IPFS HTTP API. Only reserved characters need encoding; a bare
    // /p2p/<id> contains '/' which must become %2F.
    std::string urlEscape(const std::string& s) {
        std::ostringstream out;
        out << std::hex << std::uppercase;
        for (unsigned char c: s) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                out << c;
            } else {
                out << '%';
                if ((int) c < 16) out << '0';
                out << (int) c;
            }
        }
        return out.str();
    }
}

PoolVerifier::PoolVerifier(PoolDatabase& db, const std::string& ipfsApiBase)
    : _db(db), _ipfsApiBase(ipfsApiBase) {
    // Ensure trailing slash so `_ipfsApiBase + "swarm/connect"` is well-formed.
    if (!_ipfsApiBase.empty() && _ipfsApiBase.back() != '/') _ipfsApiBase += '/';
}

PoolVerifier::~PoolVerifier() {
    stop();
}

void PoolVerifier::start() {
    if (_running.exchange(true)) return;
    _thread = std::thread([this]() { this->loop(); });
}

void PoolVerifier::stop() {
    if (!_running.exchange(false)) return;
    if (_thread.joinable()) _thread.join();
}

void PoolVerifier::loop() {
    // One-time small delay so we don't fire probes before the pool server
    // has even logged "accepting connections". Feels nicer for the log.
    std::this_thread::sleep_for(std::chrono::seconds(5));

    while (_running.load()) {
        // Pull a small batch of peers to verify. Limit 10 means each
        // iteration touches at most 10 peers; with a 60s sleep that's
        // 600 probes/hour against kubo, well within its comfort zone.
        auto peers = _db.getPeerIdsForVerification(10);

        // Detect the local IPFS node's own peerId so we can auto-verify
        // same-machine clients. When the pool server and client run on the
        // same box, swarm/connect to our own WAN IP fails due to NAT
        // hairpinning — but the node is definitionally serving content.
        std::string localPeerId;
        try {
            std::string idResp = CurlHandler::post(_ipfsApiBase + "id", {}, 5000);
            size_t idPos = idResp.find("\"ID\"");
            if (idPos == std::string::npos) idPos = idResp.find("\"id\"");
            if (idPos != std::string::npos) {
                size_t q1 = idResp.find('"', idResp.find(':', idPos) + 1);
                size_t q2 = (q1 != std::string::npos) ? idResp.find('"', q1 + 1) : std::string::npos;
                if (q1 != std::string::npos && q2 != std::string::npos)
                    localPeerId = idResp.substr(q1 + 1, q2 - q1 - 1);
            }
        } catch (...) {}

        for (const auto& peer: peers) {
            if (!_running.load()) break;
            _probesAttempted.fetch_add(1);

            // Auto-verify if the peer's multiaddr contains the local node's
            // own peerId. Same-machine = serving by definition.
            bool isSelf = false;
            if (!localPeerId.empty() && peer.find(localPeerId) != std::string::npos) {
                isSelf = true;
            }

            bool ok = isSelf ? true : verifyPeer(peer);
            if (ok) {
                _probesSucceeded.fetch_add(1);
                _db.recordVerifySuccess(peer);
            } else {
                _db.recordVerifyFailure(peer);
            }
        }

        // Sleep ~60s between iterations, checking the stop flag every second
        // so shutdown is responsive.
        for (int i = 0; i < 60 && _running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

bool PoolVerifier::verifyPeer(const std::string& peerIdOrMultiaddr) {
    // Normalize to a multiaddr. If the input already contains '/', assume
    // it's a multiaddr like /ip4/1.2.3.4/tcp/4001/p2p/<id>. If it's a bare
    // peerId, wrap it in /p2p/<id> so kubo's swarm/connect does a DHT lookup.
    std::string target = peerIdOrMultiaddr;
    if (target.find('/') == std::string::npos) {
        target = "/p2p/" + target;
    }

    const std::string url = _ipfsApiBase + "swarm/connect?arg=" + urlEscape(target);

    // swarm/connect has to do a DHT lookup for bare-peerId inputs, which
    // can be slow on cold-cached nodes. 30s is generous; anything longer
    // and we'd rather mark the node as unreachable.
    std::string body;
    try {
        body = CurlHandler::post(url, {}, 30000);
    } catch (...) {
        return false;
    }

    // Kubo returns JSON with a "Strings" array of human-readable results.
    // Success looks like:
    //   {"Strings":["connect 12D3Koo... success"]}
    // Failure looks like:
    //   {"Message":"failed to dial ...","Code":0,"Type":"error"}
    if (body.empty()) return false;
    if (body.find("\"Type\":\"error\"") != std::string::npos) return false;
    if (body.find("success") != std::string::npos) return true;

    // Default: assume failure. We'd rather have a false negative (node gets
    // retried next iteration) than a false positive (marking a dead node as
    // reachable).
    return false;
}
