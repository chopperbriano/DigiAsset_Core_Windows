#include "PoolDashboard.h"
#include "PoolDatabase.h"
#include "PoolServer.h"
#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif

// --- VT100 escape helpers --- (same subset ConsoleDashboard uses in main exe)
#define ESC "\033["
#define CURSOR_HOME    ESC "H"
#define ERASE_SCREEN   ESC "2J"
#define ERASE_LINE     ESC "2K"
#define HIDE_CURSOR    ESC "?25l"
#define SHOW_CURSOR    ESC "?25h"
#define BOLD           ESC "1m"
#define DIM            ESC "2m"
#define RESET          ESC "0m"
#define FG_GREEN       ESC "32m"
#define FG_YELLOW      ESC "33m"
#define FG_RED         ESC "31m"
#define FG_CYAN        ESC "36m"
#define FG_BRIGHT_WHITE ESC "97m"

namespace {
    std::string formatDuration(int64_t seconds) {
        int d = (int) (seconds / 86400);
        seconds %= 86400;
        int h = (int) (seconds / 3600);
        seconds %= 3600;
        int m = (int) (seconds / 60);
        int s = (int) (seconds % 60);
        std::ostringstream out;
        if (d > 0) out << d << "d ";
        if (d > 0 || h > 0) {
            out << std::setfill('0') << std::setw(2) << h << ":"
                << std::setfill('0') << std::setw(2) << m << ":"
                << std::setfill('0') << std::setw(2) << s;
        } else {
            out << m << " min " << s << " sec";
        }
        return out.str();
    }

    std::string formatNumber(uint64_t n) {
        std::string s = std::to_string(n);
        std::string out;
        int count = 0;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            if (count > 0 && count % 3 == 0) out += ',';
            out += *it;
            count++;
        }
        std::reverse(out.begin(), out.end());
        return out;
    }
}

PoolDashboard::PoolDashboard(PoolDatabase& db, PoolServer& server)
    : _db(db), _server(server), _startTime(std::chrono::system_clock::now()) {
}

PoolDashboard::~PoolDashboard() {
    stop();
}

bool PoolDashboard::enableVT100() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return false;
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, mode) != 0;
#else
    return true;
#endif
}

void PoolDashboard::start() {
    if (_running.exchange(true)) return;
    std::cout << ESC "2J" << CURSOR_HOME << HIDE_CURSOR << std::flush;
    render();
    _thread = std::thread([this]() { this->refreshLoop(); });
}

void PoolDashboard::stop() {
    if (!_running.exchange(false)) return;
    if (_thread.joinable()) _thread.join();
    std::cout << SHOW_CURSOR << std::flush;
}

void PoolDashboard::addLog(const std::string& line) {
    std::lock_guard<std::mutex> lk(_logMutex);
    _logLines.push_back(line);
    while (_logLines.size() > MAX_LOG_LINES) _logLines.pop_front();
}

void PoolDashboard::refreshLoop() {
    while (_running.load()) {
        processInput();
        render();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void PoolDashboard::processInput() {
#ifdef _WIN32
    while (_kbhit()) {
        int ch = _getch();
        if (ch == 'q' || ch == 'Q' || ch == 3 /* Ctrl+C */) {
            _quit.store(true);
        } else if (ch == 'p' || ch == 'P') {
            addLog("Pending payouts: 0.0000 DGB (Phase 3 not yet implemented)");
        } else if (ch == 'e' || ch == 'E') {
            addLog("Execute payout: Phase 3 not yet implemented");
        } else if (ch == 'n' || ch == 'N') {
            addLog("Registered nodes: " + std::to_string(_db.countTotalNodes()));
        } else if (ch == 'a' || ch == 'A') {
            addLog("Permanent assets: " + std::to_string(_db.countPermanentAssets()) +
                   " across " + std::to_string(_db.countPermanentPages()) + " pages");
        } else if (ch == 'h' || ch == 'H' || ch == '?') {
            addLog("Pool Server - keys: Q=Quit  N=Node count  A=Asset count  P=Pending payouts  E=Execute payout");
        }
    }
#endif
}

void PoolDashboard::render() {
    std::ostringstream out;
    out << HIDE_CURSOR << CURSOR_HOME;

    const int w = 90;
    auto separator = [&]() { out << ERASE_LINE << std::string(w, '-') << "\n"; };

    // Header
    std::string title = "DigiAsset Pool Server (experimental) - Phase 1";
    int pad = (w - (int) title.size()) / 2;
    out << BOLD << FG_BRIGHT_WHITE << ERASE_LINE
        << std::string(pad > 0 ? pad : 0, ' ') << title << RESET << "\n";
    separator();

    // Status rows
    auto now = std::chrono::system_clock::now();
    int64_t uptime = std::chrono::duration_cast<std::chrono::seconds>(now - _startTime).count();
    int64_t oneHourAgo = std::chrono::duration_cast<std::chrono::seconds>(
                                 now.time_since_epoch()).count() - 3600;

    unsigned int totalNodes = _db.countTotalNodes();
    unsigned int activeNodes = _db.countNodesSeenSince(oneHourAgo);
    unsigned int permAssets = _db.countPermanentAssets();
    unsigned int permPages = _db.countPermanentPages();
    uint64_t requests = _server.getRequestCount();

    out << ERASE_LINE << "  " << "Listening:     " << FG_GREEN << "Port " << _server.getPort() << RESET
        << "            " << "Requests:      " << FG_BRIGHT_WHITE << formatNumber(requests) << RESET << "\n";

    out << ERASE_LINE << "  " << "Registered:    " << FG_BRIGHT_WHITE << totalNodes << " nodes" << RESET
        << "           " << "Active (1h):   " << FG_BRIGHT_WHITE << activeNodes << " nodes" << RESET << "\n";

    out << ERASE_LINE << "  " << "Permanent:     " << FG_BRIGHT_WHITE << formatNumber(permAssets)
        << " assets" << RESET << " / " << FG_BRIGHT_WHITE << permPages << " pages" << RESET << "\n";

    // Payout row. Shows the poolpayouts config state prominently so the
    // operator always knows what clients are seeing. If poolpayouts is
    // still disabled (Phase 1 default), say so in yellow. When Phase 3
    // flips to enabled, switch to green.
    {
        bool payoutsEnabled = _server.getPayoutsEnabled();
        out << ERASE_LINE << "  " << "Payouts:       ";
        if (payoutsEnabled) {
            out << FG_GREEN << "ENABLED" << RESET << DIM
                << "  (Pending: 0.0000 DGB / Paid: 0.0000 DGB)" << RESET;
        } else {
            out << FG_YELLOW << "disabled" << RESET << DIM
                << "  (clients see 'registered (no payouts yet)')" << RESET;
        }
        out << "\n";
    }

    // Time row
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &t);
#else
    tmBuf = *std::localtime(&t);
#endif
    char timeStr[16];
    std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tmBuf);

    out << ERASE_LINE << "  " << "Time:          " << FG_BRIGHT_WHITE << timeStr << RESET
        << "            " << "Uptime:        " << FG_BRIGHT_WHITE << formatDuration(uptime) << RESET << "\n";

    separator();

    // Log area
    out << BOLD << ERASE_LINE << " Log:" << RESET << "\n";
    {
        std::lock_guard<std::mutex> lk(_logMutex);
        for (const auto& line: _logLines) {
            out << ERASE_LINE << "  " << line << "\n";
        }
        // Pad to MAX_LOG_LINES so the key-hint row sits in a stable position.
        for (size_t i = _logLines.size(); i < MAX_LOG_LINES; i++) {
            out << ERASE_LINE << "\n";
        }
    }

    // Key hints row
    out << ERASE_LINE << DIM
        << " [Q] Quit  [N] Nodes  [A] Assets  [P] Pending Payouts  [E] Execute Payout  [H] Help"
        << RESET;

    // Clear to end of screen to scrub any stale output from longer renders.
    out << ESC "J";

    std::cout << out.str() << std::flush;
}
