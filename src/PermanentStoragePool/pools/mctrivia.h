//
// Created by mctrivia on 04/11/23.
//

#ifndef DIGIASSET_CORE_MCTRIVIA_H
#define DIGIASSET_CORE_MCTRIVIA_H



#include "PermanentStoragePool/PermanentStoragePool.h"
#include "utils.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>


class mctrivia : public PermanentStoragePool {
public:
    // Reflects what we actually know about mctrivia's server today. The
    // historical C++ code assumed an on-chain fee-matching pool protocol that
    // was never deployed server-side — see memory/project_psp_payment_diagnosis.md
    // for the full story. These values are what the dashboard and logs report.
    enum class Health {
        Unknown = 0, // not probed yet this session
        Ok      = 1, // last probe returned 2xx
        Broken  = 2  // last probe returned 5xx or threw
    };

private:
    enum ServerCalls {
        KEEP_ALIVE,
        UNSUBSCRIBE,
        REPORT
    };

    // Keepalive thread (existing).
    std::thread _keepAliveThread;
    std::atomic<bool> _keepRunning;

    // Permanent-list fetcher thread. Walks /permanent/<page>.json, pins every
    // CID via IPFS, and opportunistically probes /list/<floor>.json to report
    // pool registration health in the dashboard.
    std::thread _permanentFetcherThread;
    std::atomic<bool> _fetcherRunning;

    // Persistent identity. Read from config key `psp1secret`; generated and
    // written back to config.cfg on first run. Previously regenerated every
    // startup, which made the node look like a new identity on every restart.
    std::string _secretCode;

    // Permanent-list walker state.
    unsigned int _permanentPage = 23; // default to current active page on fresh install
    std::mutex _healthMutex;
    Health _registrationHealth = Health::Unknown;   // /list/<floor>.json POST
    Health _permanentFetchHealth = Health::Unknown; // /permanent/<page>.json GET
    std::string _daily;                             // "daily" field from /permanent response
    std::chrono::steady_clock::time_point _lastRegistrationProbe{};

    void keepAliveTask();
    void permanentFetcherTask();
    void probeListEndpoint(); // POST /list/<floor>.json once, update _registrationHealth
    bool fetchAndPinPermanentPage(unsigned int page); // returns true if page was `done`
    void updateBadList();
    void _callServer(ServerCalls command, const std::string& extra = "");

    std::vector<std::string> _badAssets;
    std::vector<std::string> _badFiles;
    unsigned int _badTime = 0;
    bool _visible = true;

protected:
    void _setConfig(const Config& config) override;
    void _reportAssetBad(const std::string& assetId) override;
    void _reportFileBad(const std::string& cid) override;

public:
    mctrivia();

    //called by Node Operators that subscribe to PSP
    std::string serializeMetaProcessor(const DigiByteTransaction& tx) override;                                              //always returns "" — see .cpp for why
    std::unique_ptr<PermanentStoragePoolMetaProcessor> deserializeMetaProcessor(const std::string& serializedData) override; //stub processor; never actually invoked
    void start() override;
    void stop() override;

    //called by API
    bool isAssetBad(const std::string& assetId) override;

    //called by asset creator
    void enable(DigiByteTransaction& tx) override;            //makes changes to tx to enable psp on that transaction(must be called last before publishing)
    uint64_t getCost(const DigiByteTransaction& tx) override; //estimates the cost of using this psp and returns in DGB sats(may not be exact since exchange rates may change)
    std::string getName() override;                           //gets the name of the PSP
    std::string getDescription() override;                    //gets the description
    std::string getURL() override;                            //gets the PSP's website

    //called by dashboard / anyone curious about pool state
    Health getRegistrationHealth();
    Health getPermanentFetchHealth();
    std::string getDailyPayoutStr();
    // No getPermanentPage() — only the fetcher thread touches _permanentPage,
    // and exposing it to other threads would require synchronization for no
    // user-visible benefit today.
};

// Stub metadata processor. The old mctriviaMetaProcessor used serialized-byte
// budgets produced by the dead on-chain serializer path; it is never reached
// now that serializeMetaProcessor always returns "".
class mctriviaMetaProcessor : public PermanentStoragePoolMetaProcessor {
public:
    mctriviaMetaProcessor(const std::string& serializedData, unsigned int poolIndex);
    bool _shouldPinFile(const std::string& name, const std::string& mimeType, const std::string& cid) override;
};


#endif //DIGIASSET_CORE_MCTRIVIA_H
