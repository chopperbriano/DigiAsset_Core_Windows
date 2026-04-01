//
// Embedded web server for DigiAsset Core
// Based on web/main.cpp — serves the web UI via Boost Beast HTTP
//

#ifndef DIGIASSET_CORE_WEBSERVER_H
#define DIGIASSET_CORE_WEBSERVER_H

#include <atomic>
#include <string>
#include <thread>

class WebServer {
public:
    explicit WebServer(const std::string& configFile = "config.cfg");
    ~WebServer();

    void start();
    void stop();

    unsigned short getPort() const { return _port; }
    bool isRunning() const { return _running; }

    // Get the external IP address (cached, fetched once on first call)
    std::string getExternalIP();

private:
    void serverLoop();

    unsigned short _port = 8090;
    std::string _webRoot;
    std::string _srcRoot;
    std::thread _thread;
    std::atomic<bool> _running{false};
    std::atomic<bool> _stopRequested{false};

    // Cached external IP
    std::string _externalIP;
    bool _externalIPFetched = false;
};

#endif // DIGIASSET_CORE_WEBSERVER_H
