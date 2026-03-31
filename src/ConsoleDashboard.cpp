//
// Console Dashboard - in-place TUI for DigiAsset Core
//

#include "ConsoleDashboard.h"
#include "AppMain.h"
#include "Version.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

// ---- VT100 escape helpers --------------------------------------------------
// These work on Windows 10 1511+ once ENABLE_VIRTUAL_TERMINAL_PROCESSING is on.

#define ESC "\033["
#define CURSOR_HOME    ESC "H"
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
#define FG_WHITE       ESC "37m"
#define FG_BRIGHT_WHITE ESC "97m"

static inline std::string moveTo(int row, int col) {
    return std::string(ESC) + std::to_string(row) + ";" + std::to_string(col) + "H";
}

// ---- ConsoleDashboard implementation ----------------------------------------

ConsoleDashboard::ConsoleDashboard() {
    _lastTime = std::chrono::steady_clock::now();
}

ConsoleDashboard::~ConsoleDashboard() {
    stop();
}

bool ConsoleDashboard::enableVT100() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return false;
    mode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
    return SetConsoleMode(hOut, mode) != 0;
#else
    return true; // Unix terminals support VT100 natively
#endif
}

void ConsoleDashboard::start() {
    if (_running) return;
    _running = true;
#ifdef _WIN32
    SetConsoleTitleA("DigiAsset Core");
#endif
    // Clear the screen and render immediately so the user sees the dashboard
    std::cout << ESC "2J" << CURSOR_HOME << std::flush;
    render();
    _thread = std::thread(&ConsoleDashboard::refreshLoop, this);
}

void ConsoleDashboard::stop() {
    _running = false;
    if (_thread.joinable()) {
        _thread.join();
    }
    // Restore cursor visibility
    std::cout << SHOW_CURSOR << std::flush;
}

void ConsoleDashboard::addMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(_msgMutex);
    _messages.push_back(message);
    while (_messages.size() > MAX_MESSAGES) {
        _messages.pop_front();
    }
}

void ConsoleDashboard::refreshLoop() {
    while (_running) {
        render();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void ConsoleDashboard::updateConsoleSize() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        _width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        _height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
#else
    _width = 80;
    _height = 25;
#endif
    if (_width < 40) _width = 40;
    if (_height < 15) _height = 15;
}

void ConsoleDashboard::render() {
    updateConsoleSize();

    AppMain* app = AppMain::GetInstance();

    // ---- Gather data --------------------------------------------------------
    // Sync state
    int syncState = 0;
    unsigned int syncHeight = 0;
    unsigned int chainTip = 0;
    std::string syncStatusText;
    std::string syncColor = FG_WHITE;

    ChainAnalyzer* analyzer = app->getChainAnalyzerIfSet();
    if (analyzer) {
        syncState = analyzer->getSync();
        syncHeight = analyzer->getSyncHeight();
    }

    DigiByteCore* dgb = app->getDigiByteCoreIfSet();

    if (dgb) {
        try {
            chainTip = dgb->getBlockCount();
        } catch (...) {
            chainTip = syncHeight; // fallback
        }
    }

    // Determine sync status text
    if (syncState == ChainAnalyzer::SYNCED) {
        syncStatusText = "Fully Synced";
        syncColor = FG_GREEN;
    } else if (syncState == ChainAnalyzer::STOPPED) {
        syncStatusText = "Stopped";
        syncColor = FG_RED;
    } else if (syncState == ChainAnalyzer::INITIALIZING) {
        syncStatusText = "Initializing...";
        syncColor = FG_YELLOW;
    } else if (syncState == ChainAnalyzer::REWINDING) {
        syncStatusText = "Rewinding (fork detected)";
        syncColor = FG_YELLOW;
    } else if (syncState == ChainAnalyzer::BUSY) {
        syncStatusText = "Optimizing Indexes...";
        syncColor = FG_YELLOW;
    } else {
        // Negative = blocks behind
        int blocksBehind = -syncState;
        syncStatusText = "Syncing (" + formatNumber(blocksBehind) + " blocks behind)";
        syncColor = FG_CYAN;
    }

    // Compute blocks/sec from height delta
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - _lastTime).count();
    if (elapsed >= 1.0 && syncHeight > _lastHeight) {
        _blocksPerSec = (syncHeight - _lastHeight) / elapsed;
        _lastHeight = syncHeight;
        _lastTime = now;
    } else if (syncHeight <= _lastHeight && elapsed >= 3.0) {
        // Not making progress
        _blocksPerSec = 0.0;
        _lastHeight = syncHeight;
        _lastTime = now;
    }

    // ETA
    std::string etaText = "--";
    double progress = 0.0;
    if (chainTip > 0 && syncHeight > 0) {
        progress = (double)syncHeight / (double)chainTip;
        if (progress > 1.0) progress = 1.0;
    }
    if (syncState < 0 && _blocksPerSec > 0.01) {
        int blocksBehind = -syncState;
        double etaSec = blocksBehind / _blocksPerSec;
        etaText = formatDuration(etaSec);
    } else if (syncState == ChainAnalyzer::SYNCED) {
        etaText = "Complete";
    }

    // Connection status
    bool dgbOnline = (dgb != nullptr);
    bool dbOnline = (app->getDatabaseIfSet() != nullptr);
    bool ipfsOnline = (app->getIPFSIfSet() != nullptr);
    RPC::Server* rpcServer = app->getRpcServerIfSet();
    bool rpcOnline = (rpcServer != nullptr);
    unsigned int rpcPort = 0;
    if (rpcOnline) {
        rpcPort = rpcServer->getPort();
    }

    // ---- Build output -------------------------------------------------------
    std::ostringstream out;
    int w = _width;

    // Hide cursor + home
    out << HIDE_CURSOR << CURSOR_HOME;

    // Row 1-2: Header
    std::string title = "DigiAsset Core v" + getVersionString();
    out << BOLD << FG_BRIGHT_WHITE << ERASE_LINE << centerText(title, w) << RESET << "\n";
    out << ERASE_LINE << std::string(w, '-') << "\n";

    // Row 3-4: Connection Status
    out << BOLD << ERASE_LINE << " Services:" << RESET << "\n";

    out << ERASE_LINE;
    out << "  DigiByte Core: " << (dgbOnline ? (std::string(FG_GREEN) + "Online") : (std::string(FG_RED) + "Offline")) << RESET;
    out << "    Database: " << (dbOnline ? (std::string(FG_GREEN) + "Ready") : (std::string(FG_RED) + "N/A")) << RESET;
    out << "    IPFS: " << (ipfsOnline ? (std::string(FG_GREEN) + "Connected") : (std::string(FG_YELLOW) + "N/A")) << RESET;
    out << "    RPC: " << (rpcOnline ? (std::string(FG_GREEN) + "Port " + std::to_string(rpcPort)) : (std::string(FG_RED) + "Off")) << RESET;
    // Pad to full width
    out << "\n";

    // Row 5: separator
    out << ERASE_LINE << std::string(w, '-') << "\n";

    // Row 6-10: Sync Progress
    out << BOLD << ERASE_LINE << " Sync Status: " << syncColor << syncStatusText << RESET << "\n";

    // Height line
    out << ERASE_LINE << "  Block Height:  " << FG_BRIGHT_WHITE << formatNumber(syncHeight) << RESET;
    if (chainTip > 0) {
        out << " / " << formatNumber(chainTip);
    }
    out << "\n";

    // Speed & ETA
    out << ERASE_LINE << "  Speed:         ";
    if (_blocksPerSec >= 0.01) {
        out << FG_BRIGHT_WHITE;
        if (_blocksPerSec >= 1.0) {
            out << std::fixed << std::setprecision(1) << _blocksPerSec << " blocks/sec";
        } else {
            double secPerBlock = 1.0 / _blocksPerSec;
            out << std::fixed << std::setprecision(0) << secPerBlock << " sec/block";
        }
        out << RESET;
    } else {
        out << DIM << "-- " << RESET;
    }
    out << "    ETA: " << FG_BRIGHT_WHITE << etaText << RESET << "\n";

    // Progress bar
    int barWidth = w - 20;
    if (barWidth < 10) barWidth = 10;
    out << ERASE_LINE << "  Progress:  " << progressBar(progress, barWidth);
    out << " " << std::fixed << std::setprecision(1) << (progress * 100.0) << "%\n";

    // Row 11: separator
    out << ERASE_LINE << std::string(w, '-') << "\n";

    // Row 12+: Recent Log Messages
    out << BOLD << ERASE_LINE << " Recent Messages:" << RESET << "\n";

    // How many log rows can we show?
    int headerRows = 12; // rows used above
    int logRows = _height - headerRows - 1; // -1 for bottom padding
    if (logRows < 3) logRows = 3;

    {
        std::lock_guard<std::mutex> lock(_msgMutex);
        int startIdx = 0;
        if ((int)_messages.size() > logRows) {
            startIdx = (int)_messages.size() - logRows;
        }
        int printed = 0;
        for (int i = startIdx; i < (int)_messages.size() && printed < logRows; ++i, ++printed) {
            out << ERASE_LINE << "  ";
            const std::string& msg = _messages[i];
            // Colorize by level prefix
            if (msg.substr(0, 8) == "CRITICAL") {
                out << FG_RED;
            } else if (msg.substr(0, 7) == "WARNING") {
                out << FG_YELLOW;
            } else if (msg.substr(0, 5) == "ERROR") {
                out << FG_RED;
            } else if (msg.substr(0, 5) == "DEBUG") {
                out << DIM;
            } else {
                out << FG_WHITE;
            }
            // Truncate to fit console width
            int maxLen = w - 3;
            if ((int)msg.size() > maxLen && maxLen > 3) {
                out << msg.substr(0, maxLen - 3) << "...";
            } else {
                out << msg;
            }
            out << RESET << "\n";
        }
        // Fill remaining rows with blank lines to prevent leftover text
        for (int i = printed; i < logRows; ++i) {
            out << ERASE_LINE << "\n";
        }
    }

    // Write everything in one shot to minimize flicker
    std::cout << out.str() << std::flush;
}

// ---- Static helpers ---------------------------------------------------------

std::string ConsoleDashboard::centerText(const std::string& text, int width) {
    if ((int)text.size() >= width) return text;
    int pad = (width - (int)text.size()) / 2;
    return std::string(pad, ' ') + text;
}

std::string ConsoleDashboard::padRight(const std::string& text, int width) {
    if ((int)text.size() >= width) return text;
    return text + std::string(width - (int)text.size(), ' ');
}

std::string ConsoleDashboard::formatDuration(double seconds) {
    if (seconds < 0) return "--";
    if (seconds < 60) {
        return std::to_string((int)seconds) + " sec";
    } else if (seconds < 3600) {
        int mins = (int)(seconds / 60);
        return std::to_string(mins) + " min";
    } else if (seconds < 86400) {
        double hrs = seconds / 3600.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << hrs << " hours";
        return oss.str();
    } else {
        double days = seconds / 86400.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << days << " days";
        return oss.str();
    }
}

std::string ConsoleDashboard::formatNumber(uint64_t n) {
    std::string s = std::to_string(n);
    // Insert commas
    int pos = (int)s.size() - 3;
    while (pos > 0) {
        s.insert(pos, ",");
        pos -= 3;
    }
    return s;
}

std::string ConsoleDashboard::progressBar(double fraction, int width) {
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;

    int filled = (int)(fraction * width);
    int empty = width - filled;

    std::string bar = FG_GREEN;
    bar += std::string("[");
    bar += std::string(filled, '#');
    bar += std::string(empty, ' ');
    bar += std::string("]");
    bar += RESET;
    return bar;
}
