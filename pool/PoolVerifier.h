//
// PoolVerifier - background dial-back checker for registered pool nodes.
//
// Every iteration (default ~60s), pull up to N peerIds from the pool
// database ordered by least-recently-verified, and for each one ask the
// local IPFS node to swarm-connect to it. Success = the peer is findable
// and reachable via IPFS. Failure = it's not, at least not from here.
//
// Phase 2 scope: just connectivity. We don't yet verify that the peer is
// actually serving specific permanent-list CIDs we hand them — that's a
// Phase 2.1 refinement. Passing connectivity is still a much stronger
// signal than "they sent us a keepalive recently", which is all Phase 1
// had.
//
// Phase 3 will gate payouts on (verifyFails == 0 && lastVerifyOk recent),
// so a node that silently falls off IPFS won't get paid even if it keeps
// pinging /keepalive.
//

#ifndef DIGIASSET_POOL_VERIFIER_H
#define DIGIASSET_POOL_VERIFIER_H

#include <atomic>
#include <string>
#include <thread>

class PoolDatabase;

class PoolVerifier {
public:
    PoolVerifier(PoolDatabase& db, const std::string& ipfsApiBase);
    ~PoolVerifier();

    PoolVerifier(const PoolVerifier&) = delete;
    PoolVerifier& operator=(const PoolVerifier&) = delete;

    void start();
    void stop();

    // Live counters for the dashboard.
    uint64_t getProbesAttempted() const { return _probesAttempted.load(); }
    uint64_t getProbesSucceeded() const { return _probesSucceeded.load(); }

private:
    PoolDatabase& _db;
    std::string _ipfsApiBase;
    std::atomic<bool> _running{false};
    std::thread _thread;
    std::atomic<uint64_t> _probesAttempted{0};
    std::atomic<uint64_t> _probesSucceeded{0};

    void loop();
    bool verifyPeer(const std::string& peerIdOrMultiaddr);
};

#endif // DIGIASSET_POOL_VERIFIER_H
