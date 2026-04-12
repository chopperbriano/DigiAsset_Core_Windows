//
// PoolDatabase - sqlite wrapper for the pool server's persistent state.
//
// Stores the set of registered nodes (peerId -> payout address), the
// canonical permanent-assets list (one row per assetId/txhash/cid with a
// page number), the payouts ledger, and pool config. Uses the sqlite3
// amalgamation already in src/sqlite3.c.
//

#ifndef DIGIASSET_POOL_DATABASE_H
#define DIGIASSET_POOL_DATABASE_H

#include "sqlite3.h"
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class PoolDatabase {
public:
    explicit PoolDatabase(const std::string& dbPath);
    ~PoolDatabase();

    PoolDatabase(const PoolDatabase&) = delete;
    PoolDatabase& operator=(const PoolDatabase&) = delete;

    // Node registration. Called on keepalive and /list requests.
    // First call inserts; subsequent calls update lastSeen and payoutAddress.
    void upsertNode(const std::string& peerId,
                    const std::string& payoutAddress);

    // Permanent assets (one row per (assetId, txHash, cid) tuple).
    // Used by the first-run snapshot and, later, operator-added entries.
    void insertPermanentAsset(const std::string& assetId,
                              const std::string& txHash,
                              const std::string& cid,
                              unsigned int page);

    // Mark a permanent page as done=true. The snapshot sets this based on
    // whether mctrivia's source page had done=true at fetch time.
    void setPermanentPageDone(unsigned int page, bool done, const std::string& daily);

    // Has the first-run snapshot already populated the db?
    bool hasPermanentData();

    // Build the JSON body for /permanent/<page>.json: the {changes, daily, done}
    // shape mctrivia's server uses, serialized as a string the HTTP handler
    // can send straight to the client.
    std::string buildPermanentPageJson(unsigned int page);

    // Build the JSON body for /nodes.json - array of {id: peerId} objects
    // for every node seen within the last 7 days.
    std::string buildNodesJson();

    // Build the JSON body for /map.json - for now, one empty-geo entry per
    // known node so the existing dashboard's node count isn't wrong.
    std::string buildMapJson();

    // Dashboard counters.
    unsigned int countNodesSeenSince(int64_t unixSeconds);
    unsigned int countTotalNodes();
    unsigned int countPermanentAssets();
    unsigned int countPermanentPages();

    // Pool-local config key/value store (separate from the operator's
    // editable pool.cfg; this is runtime state like "last snapshot time").
    void setConfig(const std::string& key, const std::string& value);
    std::string getConfig(const std::string& key, const std::string& defaultValue = "");

private:
    sqlite3* _db = nullptr;
    std::mutex _mutex;
    void exec(const char* sql);
    void buildSchema();
};

#endif // DIGIASSET_POOL_DATABASE_H
