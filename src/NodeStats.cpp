#include "NodeStats.h"

NodeStats& NodeStats::instance() {
    static NodeStats s;
    return s;
}

void NodeStats::setBitswap(bool available, uint64_t blocksSent, uint64_t dataSent, double blocksPerMin) {
    std::lock_guard<std::mutex> lk(_m);
    _bitswapProbed = true;
    _bitswapAvailable = available;
    if (available) {
        _blocksSent = blocksSent;
        _dataSent = dataSent;
        _blocksPerMin = blocksPerMin;
    }
}

void NodeStats::setCoverage(unsigned int tracked, unsigned int have) {
    std::lock_guard<std::mutex> lk(_m);
    _coverageChecked = true;
    _coverageTracked = tracked;
    _coverageHave = have;
}

NodeStats::Snapshot NodeStats::snapshot() {
    std::lock_guard<std::mutex> lk(_m);
    Snapshot s;
    s.bitswapProbed = _bitswapProbed;
    s.bitswapAvailable = _bitswapAvailable;
    s.blocksSent = _blocksSent;
    s.dataSent = _dataSent;
    s.blocksPerMin = _blocksPerMin;
    s.coverageChecked = _coverageChecked;
    s.coverageTracked = _coverageTracked;
    s.coverageHave = _coverageHave;
    return s;
}
