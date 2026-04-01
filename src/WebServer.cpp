//
// Embedded web server for DigiAsset Core
// Based on web/main.cpp — serves the web UI via Boost Beast HTTP
//

#include "WebServer.h"
#include "Config.h"
#include "CurlHandler.h"
#include "Log.h"

// Use real Boost Beast headers (not the stub in src/boost/)
// The NuGet Boost include path must come before src/ in CMakeLists
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// ---- Helpers ----------------------------------------------------------------

static bool fileExistsLocal(const std::string& fileName) {
    struct stat buffer {};
    return (stat(fileName.c_str(), &buffer) == 0);
}

static std::string getMimeType(const std::string& path) {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") return "text/html";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") return "text/css";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") return "application/javascript";
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") return "application/json";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".png") return "image/png";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".svg") return "image/svg+xml";
    return "text/html";
}

// ---- WebServer implementation -----------------------------------------------

WebServer::WebServer(const std::string& configFile) {
    Config config(configFile);
    _port = static_cast<unsigned short>(config.getInteger("webport", 8090));

    // Determine paths relative to the executable
    // In a typical layout: exe is in build/src/Release/, web files are in web/
    _webRoot = "../../../web/";
    _srcRoot = "../../../src/";

    // If running from the repo root, adjust
    if (!fileExistsLocal(_webRoot + "index.html")) {
        _webRoot = "web/";
        _srcRoot = "src/";
    }
    if (!fileExistsLocal(_webRoot + "index.html")) {
        _webRoot = "../web/";
        _srcRoot = "../src/";
    }
}

WebServer::~WebServer() {
    stop();
}

void WebServer::start() {
    if (_running) return;
    _stopRequested = false;
    _thread = std::thread(&WebServer::serverLoop, this);
}

void WebServer::stop() {
    _stopRequested = true;
    if (_thread.joinable()) {
        _thread.join();
    }
    _running = false;
}

std::string WebServer::getExternalIP() {
    if (_externalIPFetched) return _externalIP;
    try {
        _externalIP = CurlHandler::get("http://api.ipify.org", 5000);
        // Trim whitespace/newlines
        while (!_externalIP.empty() && (_externalIP.back() == '\n' || _externalIP.back() == '\r' || _externalIP.back() == ' ')) {
            _externalIP.pop_back();
        }
    } catch (...) {
        _externalIP = "unknown";
    }
    _externalIPFetched = true;
    return _externalIP;
}

void WebServer::serverLoop() {
    Log* log = Log::GetInstance();
    _running = true;

    while (!_stopRequested) {
        try {
            net::io_context ioc{1};
            tcp::acceptor acceptor{ioc, {net::ip::make_address("0.0.0.0"), _port}};

            log->addMessage("Web Server listening on port " + std::to_string(_port));

            while (!_stopRequested) {
                tcp::socket socket{ioc};

                // Set a short timeout so we can check _stopRequested periodically
                acceptor.accept(socket);

                // Read request
                beast::flat_buffer buffer;
                http::request<http::string_body> req;
                beast::error_code ec;
                http::read(socket, buffer, req, ec);
                if (ec) continue;

                // Build response
                http::response<http::string_body> res;
                res.version(req.version());
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);

                if (req.method() != http::verb::get) {
                    res.result(http::status::bad_request);
                    res.set(http::field::content_type, "text/html");
                    res.body() = "Unknown HTTP-method";
                    res.prepare_payload();
                    http::write(socket, res, ec);
                    continue;
                }

                std::string target(req.target());
                if (target.empty() || target[0] != '/' || target.find("..") != std::string::npos) {
                    res.result(http::status::bad_request);
                    res.set(http::field::content_type, "text/html");
                    res.body() = "Illegal request-target";
                    res.prepare_payload();
                    http::write(socket, res, ec);
                    continue;
                }

                // Resolve file path
                std::string path;
                if (target.substr(0, 5) == "/src/") {
                    path = _srcRoot + target.substr(1); // strip leading /
                } else if (target.substr(0, 5) == "/rpc/") {
                    path = _srcRoot + "RPC/Methods/" + target.substr(5);
                    if (!fileExistsLocal(path)) {
                        path = _webRoot + target.substr(1);
                    }
                } else {
                    path = _webRoot + target.substr(1);
                }
                if (target.back() == '/') {
                    path += "index.html";
                }

                // Read file
                std::ifstream is(path, std::ifstream::binary);
                if (!is) {
                    res.result(http::status::not_found);
                    res.set(http::field::content_type, "text/html");
                    res.body() = "The resource '" + target + "' was not found.";
                    res.prepare_payload();
                    http::write(socket, res, ec);
                    continue;
                }

                std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
                res.result(http::status::ok);
                res.set(http::field::content_type, getMimeType(path));
                res.body() = content;
                res.keep_alive(req.keep_alive());
                res.prepare_payload();
                http::write(socket, res, ec);

                socket.shutdown(tcp::socket::shutdown_send, ec);
            }
        } catch (const std::exception& e) {
            if (!_stopRequested) {
                log->addMessage(std::string("Web Server error: ") + e.what(), Log::WARNING);
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
    }
    _running = false;
}
