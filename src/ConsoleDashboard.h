//
// Console Dashboard - in-place TUI for DigiAsset Core
//

#ifndef DIGIASSET_CORE_CONSOLEDASHBOARD_H
#define DIGIASSET_CORE_CONSOLEDASHBOARD_H

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>

class ConsoleDashboard {
public:
    ConsoleDashboard();
    ~ConsoleDashboard();

    // Start/stop the refresh thread
    void start();
    void stop();

    // Called by Log to add a message to the recent-messages area
    void addMessage(const std::string& message);

    // Enable VT100 escape sequences on the Windows console.
    // Call once at the very start of main(), before any output.
    static bool enableVT100();

private:
    // Refresh loop (runs in its own thread)
    void refreshLoop();

    // Render one frame to the console
    void render();

    // Helpers
    static std::string centerText(const std::string& text, int width);
    static std::string padRight(const std::string& text, int width);
    static std::string formatDuration(double seconds);
    static std::string formatNumber(uint64_t n);
    static std::string progressBar(double fraction, int width);

    // State
    std::atomic<bool>       _running{false};
    std::thread             _thread;
    std::mutex              _msgMutex;
    std::deque<std::string> _messages;
    static const size_t     MAX_MESSAGES = 100;

    // For computing blocks/sec from height deltas
    unsigned int _lastHeight = 0;
    std::chrono::steady_clock::time_point _lastTime;
    double _blocksPerSec = 0.0;

    // Cached asset count (refreshed every 5 seconds)
    uint64_t _assetCount = 0;
    std::chrono::steady_clock::time_point _lastAssetCountTime;

    // Console dimensions
    int _width = 80;
    int _height = 25;
    void updateConsoleSize();
};

#endif // DIGIASSET_CORE_CONSOLEDASHBOARD_H
