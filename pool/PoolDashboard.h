//
// PoolDashboard - minimal VT100 TUI for the pool server exe.
//
// Deliberately separate from the main DigiAssetCore ConsoleDashboard so the
// pool server is a standalone exe with no link-time dependency on the main
// lib. Reimplements the minimum set of helpers we need (VT100 init, cursor
// home, ERASE_LINE, FG colors, key polling, a log buffer).
//

#ifndef DIGIASSET_POOL_DASHBOARD_H
#define DIGIASSET_POOL_DASHBOARD_H

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

class PoolDatabase;
class PoolServer;

class PoolDashboard {
public:
    PoolDashboard(PoolDatabase& db, PoolServer& server);
    ~PoolDashboard();

    // Enable VT100 escape sequences on the Windows console. Returns true if
    // the terminal is capable — if false, the caller should fall back to
    // printing plain lines.
    static bool enableVT100();

    void start();
    void stop();

    void addLog(const std::string& line);

    bool quitRequested() const { return _quit.load(); }

private:
    PoolDatabase& _db;
    PoolServer& _server;
    std::atomic<bool> _running{false};
    std::atomic<bool> _quit{false};
    std::thread _thread;
    std::chrono::system_clock::time_point _startTime;

    std::mutex _logMutex;
    std::deque<std::string> _logLines;
    static constexpr size_t MAX_LOG_LINES = 20;

    void refreshLoop();
    void render();
    void processInput();
};

#endif // DIGIASSET_POOL_DASHBOARD_H
