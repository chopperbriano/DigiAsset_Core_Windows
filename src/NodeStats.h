//
// NodeStats - shared process-wide cache of slow-to-compute stats so that
// the dashboard and the RPC method getnodestats can both report the same
// numbers without either one duplicating the HTTP calls or holding up the
// RPC thread on slow network fetches.
//
// The dashboard is the writer: pollBitswapStats() and checkPermanentCoverage()
// in ConsoleDashboard.cpp populate these fields as they complete. The RPC
// method getnodestats.cpp is the reader. All accessors are mutex-guarded.
//

#ifndef DIGIASSET_CORE_NODESTATS_H
#define DIGIASSET_CORE_NODESTATS_H

#include <cstdint>
#include <mutex>
#include <string>

class NodeStats {
    std::mutex _m;
    bool _bitswapProbed = false;
    bool _bitswapAvailable = false;
    uint64_t _blocksSent = 0;
    uint64_t _dataSent = 0;
    double _blocksPerMin = 0.0;

    bool _coverageChecked = false;
    unsigned int _coverageTracked = 0;
    unsigned int _coverageHave = 0;

    NodeStats() = default;

public:
    NodeStats(const NodeStats&) = delete;
    NodeStats& operator=(const NodeStats&) = delete;
    static NodeStats& instance();

    void setBitswap(bool available, uint64_t blocksSent, uint64_t dataSent, double blocksPerMin);
    void setCoverage(unsigned int tracked, unsigned int have);

    struct Snapshot {
        bool bitswapProbed;
        bool bitswapAvailable;
        uint64_t blocksSent;
        uint64_t dataSent;
        double blocksPerMin;
        bool coverageChecked;
        unsigned int coverageTracked;
        unsigned int coverageHave;
    };
    Snapshot snapshot();
};

#endif // DIGIASSET_CORE_NODESTATS_H
