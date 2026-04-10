//
// Console Dashboard - in-place TUI for DigiAsset Core
//

#include "ConsoleDashboard.h"
#include "AppMain.h"
#include "Config.h"
#include "CurlHandler.h"
#include "Log.h"
#include "Version.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
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
    SetConsoleTitleA(PRODUCT_NAME);
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
        processInput();
        render();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void ConsoleDashboard::processInput() {
#ifdef _WIN32
    while (_kbhit()) {
        int ch = _getch();
        switch (ch) {
            case 'q':
            case 'Q':
                _quitRequested = true;
                if (_quitCallback) _quitCallback();
                break;
            case 'l':
            case 'L':
                // Toggle log level between INFO and DEBUG
                _showDebug = !_showDebug;
                {
                    Log* log = Log::GetInstance();
                    log->setMinLevelToScreen(_showDebug ? Log::DEBUG : Log::INFO);
                    log->addMessage(_showDebug ? "Log level: DEBUG" : "Log level: INFO");
                }
                break;
            case 'p':
            case 'P':
                // Check published ports
                {
                    Log* log = Log::GetInstance();
                    log->addMessage("Checking published ports...");
                    std::thread([this]() { checkPorts(); }).detach();
                }
                break;
            case 'a':
            case 'A':
                // List recent assets
                {
                    Log* log = Log::GetInstance();
                    Database* adb = AppMain::GetInstance()->getDatabaseIfSet();
                    if (!adb) {
                        log->addMessage("Database not ready");
                        break;
                    }
                    uint64_t total = adb->getAssetCountOnChain();
                    log->addMessage("--- Assets: " + formatNumber(total) + " unique IDs ---");
                    try {
                        auto assets = adb->getLastAssetsIssued(10);
                        if (assets.empty()) {
                            log->addMessage("  No assets found yet");
                        } else {
                            log->addMessage("  Last 10 (index may be higher due to sub-types):");
                            for (const auto& a : assets) {
                                log->addMessage("  #" + std::to_string(a.assetIndex) +
                                    " | " + a.assetId.substr(0, 20) + "..." +
                                    " | block " + formatNumber(a.height));
                            }
                        }
                    } catch (...) {
                        log->addMessage("  Error reading assets");
                    }
                }
                break;
            case 'f':
            case 'F':
                // Fix IPFS Addresses.Announce when we've detected the fixable condition
                {
                    Log* log = Log::GetInstance();
                    log->addMessage("Attempting to fix IPFS announce list...");
                    std::thread([this]() { applyIpfsAnnounceFix(); }).detach();
                }
                break;
            case 'h':
            case 'H':
            case '?':
                // Show system info in log
                {
                    Log* log = Log::GetInstance();
                    AppMain* app = AppMain::GetInstance();
                    log->addMessage("--- System Info ---");
                    log->addMessage("Version: " + getProductVersionString() + " (upstream " + getUpstreamVersionString() + ")");
                    Database* hdb = app->getDatabaseIfSet();
                    if (hdb) {
                        log->addMessage("Assets on chain: " + formatNumber(hdb->getAssetCountOnChain()));
                    }
                    ChainAnalyzer* ha = app->getChainAnalyzerIfSet();
                    if (ha) {
                        log->addMessage("Sync height: " + formatNumber(ha->getSyncHeight()));
                    }
                    WebServer* hw = app->getWebServerIfSet();
                    if (hw) {
                        log->addMessage("Web UI: http://localhost:" + std::to_string(hw->getPort()) + "/");
                        std::string ip = hw->getExternalIP();
                        if (!ip.empty() && ip != "unknown") {
                            log->addMessage("External IP: " + ip);
                        }
                    }
                    log->addMessage("Keys: Q=Quit  A=Assets  P=Port check  L=Log level  F=Fix IPFS announce  H=This info");
                }
                break;
            default:
                break;
        }
    }
#endif
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

    // Payout balance
    loadPayoutInfo();
    // Run registration check in background to avoid blocking render
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - _lastPspCheck).count();
        if (elapsed >= 600.0 || _pspStatus.empty()) {
            std::thread([this]() { checkPspRegistration(); }).detach();
        }
    }
    // Run IPFS announce diagnosis in background.
    // Poll fast (every 15s) while we're waiting for a user-applied fix to
    // take effect after IPFS Desktop restart; otherwise poll every 10 min.
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - _lastIpfsAnnounceCheck).count();
        double interval = 600.0;
        {
            std::lock_guard<std::mutex> lock(_ipfsAnnounceMutex);
            if (_ipfsAnnounceFixApplied && !_ipfsAnnouncedDirectly) {
                interval = 15.0; // aggressive polling until Kubo picks up the change
            }
        }
        if (elapsed >= interval || !_ipfsAnnounceChecked) {
            // Only run once WebServer has had time to resolve external IP
            WebServer* ws = app->getWebServerIfSet();
            if (ws && !ws->getExternalIP().empty() && ws->getExternalIP() != "unknown") {
                std::thread([this]() { checkIpfsAnnounce(); }).detach();
            }
        }
    }

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

    // Asset count (query at most once every 5 seconds to avoid DB load)
    Database* db = app->getDatabaseIfSet();
    if (db && elapsed >= 1.0) {
        auto timeSinceAssetQuery = std::chrono::duration<double>(now - _lastAssetCountTime).count();
        if (timeSinceAssetQuery >= 5.0 || _assetCount == 0) {
            try {
                _assetCount = db->getAssetCountOnChain();
                _lastAssetCountTime = now;
            } catch (...) {}
        }
    }

    // Connection status
    bool dgbOnline = (dgb != nullptr);
    bool dbOnline = (db != nullptr);
    bool ipfsOnline = (app->getIPFSIfSet() != nullptr);
    RPC::Server* rpcServer = app->getRpcServerIfSet();
    bool rpcOnline = (rpcServer != nullptr);
    unsigned int rpcPort = 0;
    if (rpcOnline) {
        rpcPort = rpcServer->getPort();
    }
    WebServer* webServer = app->getWebServerIfSet();
    bool webOnline = (webServer != nullptr && webServer->isRunning());
    unsigned short webPort = 0;
    std::string externalIP;
    if (webServer) {
        webPort = webServer->getPort();
        externalIP = webServer->getExternalIP();
    }

    // ---- Build output -------------------------------------------------------
    std::ostringstream out;
    int w = _width;
    int totalRows = 0; // track exactly how many rows we emit

    // Hide cursor + home
    out << HIDE_CURSOR << CURSOR_HOME;

    // Row 1-2: Header
    std::string title = getProductVersionString() + "  (upstream " + getUpstreamVersionString() + ")";
    out << BOLD << FG_BRIGHT_WHITE << ERASE_LINE << centerText(title, w) << RESET << "\n"; totalRows++;
    out << ERASE_LINE << std::string(w, '-') << "\n"; totalRows++;

    // Row 3-5: Services
    out << ERASE_LINE
        << "  DigiByte Core: " << (dgbOnline ? (std::string(FG_GREEN) + "Online") : (std::string(FG_RED) + "Offline")) << RESET
        << "    Database: " << (dbOnline ? (std::string(FG_GREEN) + "Ready") : (std::string(FG_RED) + "N/A")) << RESET
        << "    IPFS: " << (ipfsOnline ? (std::string(FG_GREEN) + "Connected") : (std::string(FG_YELLOW) + "N/A")) << RESET
        << "\n"; totalRows++;
    out << ERASE_LINE
        << "  RPC Server:    " << (rpcOnline ? (std::string(FG_GREEN) + "Port " + std::to_string(rpcPort)) : (std::string(FG_RED) + "Off")) << RESET
        << "    Web Server: " << (webOnline ? (std::string(FG_GREEN) + "Port " + std::to_string(webPort)) : (std::string(FG_RED) + "Off")) << RESET
        << "\n"; totalRows++;
    // Web UI link + External IP
    if (webOnline) {
        out << ERASE_LINE
            << "  Web UI: " << FG_CYAN << "http://localhost:" << std::to_string(webPort) << "/" << RESET;
        if (!externalIP.empty() && externalIP != "unknown") {
            out << "    External IP: " << FG_BRIGHT_WHITE << externalIP << RESET;
        }
        out << "\n"; totalRows++;
    }
    // Payout address + balance
    if (!_payoutAddress.empty()) {
        out << ERASE_LINE
            << "  Payout: " << FG_BRIGHT_WHITE << _payoutAddress << RESET;
        if (!_payoutBalance.empty()) {
            out << "    Balance: " << FG_GREEN << _payoutBalance << RESET
                << DIM << " (updates every 4h)" << RESET;
        }
        out << "\n"; totalRows++;
    }
    // PSP pool status + node count
    if (!_pspStatus.empty()) {
        out << ERASE_LINE << "  PSP Pool: ";
        if (_pspStatus.find("reachable") != std::string::npos &&
            _pspStatus.find("unreachable") == std::string::npos) {
            out << FG_GREEN << _pspStatus << RESET;
        } else {
            out << FG_YELLOW << _pspStatus << RESET;
        }
        if (_pspNodeCount > 0) {
            out << "    Network: " << FG_BRIGHT_WHITE << _pspNodeCount
                << RESET << DIM << " nodes online" << RESET;
        }
        out << "\n"; totalRows++;
    }

    // IPFS announce hint — shown only when there's actionable advice
    {
        std::lock_guard<std::mutex> lock(_ipfsAnnounceMutex);
        if (!_ipfsAnnounceHint.empty()) {
            out << ERASE_LINE << "  IPFS: " << FG_YELLOW << _ipfsAnnounceHint << RESET << "\n";
            totalRows++;
        }
    }

    // Row 5: separator
    out << ERASE_LINE << std::string(w, '-') << "\n"; totalRows++;

    // Row 6: Sync Status
    out << BOLD << ERASE_LINE << " Sync: " << syncColor << syncStatusText << RESET << "\n"; totalRows++;

    // Row 7: Height
    out << ERASE_LINE << "  Block:   " << FG_BRIGHT_WHITE << formatNumber(syncHeight) << RESET;
    if (chainTip > 0) {
        out << " / " << formatNumber(chainTip);
    }
    out << "\n"; totalRows++;

    // Row 8: Speed & ETA
    out << ERASE_LINE << "  Speed:   ";
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
        out << DIM << "--" << RESET;
    }
    out << "    ETA: " << FG_BRIGHT_WHITE << etaText << RESET << "\n"; totalRows++;

    // Row 9: Assets
    out << ERASE_LINE << "  Assets:  " << FG_BRIGHT_WHITE;
    if (_assetCount > 0) {
        out << formatNumber(_assetCount);
    } else {
        out << DIM << "--";
    }
    out << RESET << "\n"; totalRows++;

    // Row 10: Progress bar
    int barWidth = w - 18;
    if (barWidth < 10) barWidth = 10;
    out << ERASE_LINE << "  " << progressBar(progress, barWidth);
    out << " " << std::fixed << std::setprecision(1) << (progress * 100.0) << "%\n"; totalRows++;

    // Row 11: separator
    out << ERASE_LINE << std::string(w, '-') << "\n"; totalRows++;

    // Row 12: Messages header
    out << BOLD << ERASE_LINE << " Log:" << RESET << "\n"; totalRows++;

    // Remaining rows: log messages — reserve 1 row for help bar at bottom
    int logRows = _height - totalRows - 2; // -1 help bar, -1 to avoid scrolling
    if (logRows < 1) logRows = 1;

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
        // Fill remaining rows with blank lines
        for (int i = printed; i < logRows; ++i) {
            out << ERASE_LINE << "\n";
        }
    }

    // Help bar (cursor is already on the right line after the \n above)
    // Include [F] only when the fix is applicable, to avoid cluttering the UI.
    bool showFixKey = false;
    {
        std::lock_guard<std::mutex> lock(_ipfsAnnounceMutex);
        showFixKey = !_ipfsAnnouncedDirectly && _ipfsPort4001Open && _ipfsAnnounceChecked;
    }
    out << ERASE_LINE << DIM << " [Q] Quit  [A] Assets  [P] Ports  [L] Log Level";
    if (showFixKey) {
        out << "  " << RESET << FG_YELLOW << "[F] Fix IPFS" << RESET << DIM;
    }
    out << "  [H] Info" << RESET;

    // Write everything in one shot to minimize flicker
    std::cout << out.str() << std::flush;
}

// ---- PSP registration status ------------------------------------------------

void ConsoleDashboard::checkPspRegistration() {
    auto now = std::chrono::steady_clock::now();

    // Check pool reachability via map.json (public, no auth needed).
    // map.json is the authoritative list of nodes the server has recently
    // seen alive, so its contents are the only honest signal for "am I
    // registered". The /keepalive endpoint ALWAYS returns
    // {"error":"unsubscribe failed will time out anyways"} regardless of
    // success — don't use it as a registration signal.
    try {
        std::string mapResponse = CurlHandler::get("https://ipfs.digiassetx.com/map.json", 5000);
        int nodeCount = 0;
        size_t pos = 0;
        while ((pos = mapResponse.find("\"version\"", pos)) != std::string::npos) {
            nodeCount++;
            pos++;
        }
        _pspNodeCount = nodeCount;

        // Check if our own payout address appears anywhere in map.json —
        // that's a strong signal the server sees us as an active contributor.
        // (Peer ID matching would be better but map.json keys by payout.)
        bool selfListed = false;
        try {
            Config config("config.cfg");
            std::string payout = config.getString("psp0payout", "");
            if (!payout.empty() && mapResponse.find(payout) != std::string::npos) {
                selfListed = true;
            }
        } catch (...) {}

        if (selfListed) {
            _pspStatus = "Pool reachable - this node is listed";
        } else {
            _pspStatus = "Pool reachable - this node not yet listed";
        }
        _lastPspCheck = now; // cache for 10 min
    } catch (...) {
        _pspStatus = "Pool unreachable";
        // don't cache failure — retry next refresh
    }
}

// ---- Payout balance ---------------------------------------------------------

void ConsoleDashboard::loadPayoutInfo() {
    if (!_payoutLoaded) {
        try {
            Config config("config.cfg");
            _payoutAddress = config.getString("psp0payout", "");
        } catch (...) {}
        _payoutLoaded = true;
        _lastBalanceTime = std::chrono::steady_clock::now() - std::chrono::seconds(120); // force immediate fetch
    }

    if (_payoutAddress.empty()) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - _lastBalanceTime).count();
    if (elapsed < 14400.0 && !_payoutBalance.empty()) return; // refresh every 4 hours

    try {
        std::string response = CurlHandler::get(
            "http://chainz.cryptoid.info/dgb/api.dws?q=getbalance&a=" + _payoutAddress, 5000);
        // Response is a balance string like "21.2103" (varying decimal precision)
        // Trim whitespace
        while (!response.empty() && (response.back() == '\n' || response.back() == '\r' || response.back() == ' ')) {
            response.pop_back();
        }
        // Format to exactly 4 decimal places
        try {
            double balance = std::stod(response);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(4) << balance;
            _payoutBalance = oss.str() + " DGB";
        } catch (...) {
            _payoutBalance = response + " DGB"; // fallback to raw response
        }
        _lastBalanceTime = now;
    } catch (...) {
        if (_payoutBalance.empty()) _payoutBalance = "unavailable";
    }
}

// ---- IPFS announce detection & repair --------------------------------------

namespace {
    std::string urlEncodeComponent(const std::string& s) {
        std::string out;
        out.reserve(s.size() * 3);
        for (unsigned char c : s) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                out += (char)c;
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", c);
                out += buf;
            }
        }
        return out;
    }

    // Read ipfspath from config.cfg, default to Kubo's standard HTTP API.
    std::string getIpfsApiBase() {
        try {
            Config config("config.cfg");
            std::string p = config.getString("ipfspath", "http://localhost:5001/api/v0/");
            if (!p.empty() && p.back() != '/') p += '/';
            return p;
        } catch (...) {
            return "http://localhost:5001/api/v0/";
        }
    }
}

void ConsoleDashboard::checkIpfsAnnounce() {
    // Snapshot the WAN IP from the web server (already resolved via ipify at startup)
    AppMain* app = AppMain::GetInstance();
    WebServer* ws = app->getWebServerIfSet();
    std::string wanIp;
    if (ws) wanIp = ws->getExternalIP();
    if (wanIp.empty() || wanIp == "unknown") {
        std::lock_guard<std::mutex> lock(_ipfsAnnounceMutex);
        _ipfsAnnounceHint.clear();
        return;
    }

    std::string apiBase = getIpfsApiBase();

    // Step 1: does IPFS /id already list an address containing our WAN IP?
    bool announced = false;
    try {
        std::string idJson = CurlHandler::post(apiBase + "id", {}, 5000);
        // Cheap substring check — we don't need full JSON parsing for this.
        if (idJson.find(wanIp) != std::string::npos) {
            announced = true;
        }
    } catch (...) {
        // IPFS API unreachable — can't diagnose, bail out quietly
        std::lock_guard<std::mutex> lock(_ipfsAnnounceMutex);
        _ipfsAnnounceHint.clear();
        _ipfsAnnounceChecked = false;
        return;
    }

    // Step 2: if not announced, probe port 4001 externally to see whether
    // fixing is even possible. If the port isn't reachable, setting
    // Addresses.Announce would advertise a dead address.
    bool portOpen = false;
    if (!announced) {
        try {
            std::string resp = CurlHandler::get("http://ifconfig.co/port/4001", 10000);
            if (resp.find("\"reachable\": true") != std::string::npos ||
                resp.find("\"reachable\":true") != std::string::npos) {
                portOpen = true;
            }
        } catch (...) {
            // leave portOpen = false
        }
    }

    std::lock_guard<std::mutex> lock(_ipfsAnnounceMutex);
    _ipfsAnnouncedDirectly = announced;
    _ipfsPort4001Open = portOpen;
    _ipfsAnnounceChecked = true;
    _lastIpfsAnnounceCheck = std::chrono::steady_clock::now();

    if (announced) {
        // Convergence reached: the fix (manual or auto) has taken effect
        _ipfsAnnounceFixApplied = false;
        _ipfsAnnounceHint.clear();
    } else if (_ipfsAnnounceFixApplied) {
        // Fix was applied but Kubo hasn't picked it up yet — keep reminding
        _ipfsAnnounceHint = "Waiting for IPFS Desktop restart to pick up new announce list...";
    } else if (portOpen) {
        _ipfsAnnounceHint = "Port 4001 is open but IPFS isn't announcing it - press [F] to fix";
    } else {
        _ipfsAnnounceHint.clear(); // NAT'd with no port forward — relay path is the right answer
    }
}

bool ConsoleDashboard::applyIpfsAnnounceFix() {
    Log* log = Log::GetInstance();
    AppMain* app = AppMain::GetInstance();

    // Must have a known WAN IP to announce
    WebServer* ws = app->getWebServerIfSet();
    std::string wanIp;
    if (ws) wanIp = ws->getExternalIP();
    if (wanIp.empty() || wanIp == "unknown") {
        log->addMessage("Fix aborted: external IP not known yet", Log::WARNING);
        return false;
    }

    // Sanity check: only run if our diagnosis said the port is open. This
    // prevents setting Announce to a dead address on a NAT'd box.
    {
        std::lock_guard<std::mutex> lock(_ipfsAnnounceMutex);
        if (!_ipfsAnnounceChecked) {
            log->addMessage("Fix aborted: diagnosis hasn't run yet, please wait a moment", Log::WARNING);
            return false;
        }
        if (_ipfsAnnouncedDirectly) {
            log->addMessage("Nothing to fix: IPFS is already announcing a direct address");
            return false;
        }
        if (!_ipfsPort4001Open) {
            log->addMessage("Fix aborted: port 4001 is NOT reachable from the internet. "
                            "Forward TCP 4001 on your router first (press [P] to recheck).",
                            Log::WARNING);
            return false;
        }
    }

    // Build the JSON array argument: ["/ip4/<wan>/tcp/4001","/ip4/<wan>/udp/4001/quic-v1"]
    std::string jsonArray = "[\"/ip4/" + wanIp + "/tcp/4001\",\"/ip4/" + wanIp + "/udp/4001/quic-v1\"]";

    // Build the IPFS config URL: POST .../config?arg=Addresses.Announce&arg=<encoded>&json=true
    std::string apiBase = getIpfsApiBase();
    std::string url = apiBase + "config?arg=Addresses.Announce&arg="
                      + urlEncodeComponent(jsonArray) + "&json=true";

    log->addMessage("Setting Kubo Addresses.Announce = " + jsonArray);
    try {
        std::string response = CurlHandler::post(url, {}, 10000);
        // Kubo echoes the new config on success; look for our WAN IP in the response.
        if (response.find(wanIp) == std::string::npos) {
            log->addMessage("IPFS config set returned unexpected response: " + response.substr(0, 200),
                            Log::WARNING);
            return false;
        }
    } catch (const std::exception& e) {
        log->addMessage(std::string("IPFS config set failed: ") + e.what(), Log::WARNING);
        return false;
    }

    log->addMessage("Addresses.Announce updated successfully.");
    log->addMessage("IMPORTANT: restart IPFS Desktop (tray icon -> Quit, relaunch) "
                    "to activate the new announce list.", Log::WARNING);
    log->addMessage("After IPFS restart, DigiAssetCore will verify the change "
                    "automatically (checking every 15s).");

    // Mark the fix as applied so the render loop polls aggressively until
    // Kubo picks up the new announce list. Reset _lastIpfsAnnounceCheck
    // to zero so the next render triggers an immediate re-check.
    {
        std::lock_guard<std::mutex> lock(_ipfsAnnounceMutex);
        _ipfsAnnounceFixApplied = true;
        _ipfsAnnounceHint = "Addresses.Announce set - restart IPFS Desktop to activate";
        _lastIpfsAnnounceCheck = std::chrono::steady_clock::time_point{};
    }
    return true;
}

// ---- Port checking ----------------------------------------------------------

void ConsoleDashboard::checkPorts() {
    Log* log = Log::GetInstance();
    AppMain* app = AppMain::GetInstance();

    // Get external IP
    WebServer* ws = app->getWebServerIfSet();
    std::string externalIP;
    if (ws) {
        externalIP = ws->getExternalIP();
    }
    if (externalIP.empty() || externalIP == "unknown") {
        log->addMessage("Cannot check ports: external IP unknown", Log::WARNING);
        return;
    }

    // Collect ports to check
    struct PortInfo {
        int port;
        std::string name;
    };
    std::vector<PortInfo> ports;
    ports.push_back({4001, "IPFS Swarm"});
    if (ws) {
        ports.push_back({(int)ws->getPort(), "Web UI"});
    }
    ports.push_back({12024, "DigiByte P2P"});

    log->addMessage("--- Port Check (external IP: " + externalIP + ") ---");

    for (const auto& p : ports) {
        std::string status;
        try {
            // ifconfig.co/port/N does an external TCP connect to our IP on port N
            // Returns JSON: {"ip":"...","port":N,"reachable":true/false}
            std::string response = CurlHandler::get(
                "http://ifconfig.co/port/" + std::to_string(p.port), 10000);
            if (response.find("\"reachable\": true") != std::string::npos ||
                response.find("\"reachable\":true") != std::string::npos) {
                status = "Open";
            } else {
                status = "Closed";
            }
        } catch (...) {
            status = "Check failed";
        }

        if (status == "Open") {
            log->addMessage("  Port " + std::to_string(p.port) + " (" + p.name + "): " + status);
        } else {
            log->addMessage("  Port " + std::to_string(p.port) + " (" + p.name + "): " + status, Log::WARNING);
        }
    }
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
