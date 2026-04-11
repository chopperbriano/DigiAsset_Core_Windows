//
// Created by mctrivia on 04/11/23.
//

#include "PermanentStoragePoolList.h"
#include "AppMain.h"
#include "Log.h"
#include "PermanentStoragePool/pools/local.h"
#include "PermanentStoragePool/pools/mctrivia.h"
#include "static_block.hpp"
#include "utils.h"
#include <vector>

using namespace std;

// Static block to register our callback function with IPFS Controller
static_block {
    IPFS::registerCallback(PSP_CALLBACK_NEWMETADATA_ID, PermanentStoragePoolList::_callbackNewMetadata);
}


/*
██████╗ ██████╗ ███╗   ██╗███████╗████████╗ █████╗ ███╗   ██╗████████╗███████╗
██╔════╝██╔═══██╗████╗  ██║██╔════╝╚══██╔══╝██╔══██╗████╗  ██║╚══██╔══╝██╔════╝
██║     ██║   ██║██╔██╗ ██║███████╗   ██║   ███████║██╔██╗ ██║   ██║   ███████╗
██║     ██║   ██║██║╚██╗██║╚════██║   ██║   ██╔══██║██║╚██╗██║   ██║   ╚════██║
╚██████╗╚██████╔╝██║ ╚████║███████║   ██║   ██║  ██║██║ ╚████║   ██║   ███████║
╚═════╝ ╚═════╝ ╚═╝  ╚═══╝╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝   ╚══════╝
 Add new PSP to end of list.
 */
PermanentStoragePoolList::PermanentStoragePoolList(const string& configFile) {
    //read the config file
    Config config(configFile);

    //make sure random number generator is seeded
    srand(static_cast<unsigned int>(time(nullptr)));

    //add known pools to list
    addPool(std::unique_ptr<PermanentStoragePool>(new local()), config);
    addPool(std::unique_ptr<PermanentStoragePool>(new mctrivia()), config);
}


/*
██╗████████╗███████╗██████╗  █████╗ ████████╗ ██████╗ ██████╗
██║╚══██╔══╝██╔════╝██╔══██╗██╔══██╗╚══██╔══╝██╔═══██╗██╔══██╗
██║   ██║   █████╗  ██████╔╝███████║   ██║   ██║   ██║██████╔╝
██║   ██║   ██╔══╝  ██╔══██╗██╔══██║   ██║   ██║   ██║██╔══██╗
██║   ██║   ███████╗██║  ██║██║  ██║   ██║   ╚██████╔╝██║  ██║
╚═╝   ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝    ╚═════╝ ╚═╝  ╚═╝
 */

PermanentStoragePoolList::iterator PermanentStoragePoolList::begin() {
    return _pools.begin();
}

PermanentStoragePoolList::iterator PermanentStoragePoolList::end() {
    return _pools.end();
}

PermanentStoragePoolList::const_iterator PermanentStoragePoolList::begin() const {
    return _pools.begin();
}

PermanentStoragePoolList::const_iterator PermanentStoragePoolList::end() const {
    return _pools.end();
}



/*
██████╗ ███████╗████████╗████████╗███████╗██████╗ ███████╗
██╔════╝ ██╔════╝╚══██╔══╝╚══██╔══╝██╔════╝██╔══██╗██╔════╝
██║  ███╗█████╗     ██║      ██║   █████╗  ██████╔╝███████╗
██║   ██║██╔══╝     ██║      ██║   ██╔══╝  ██╔══██╗╚════██║
╚██████╔╝███████╗   ██║      ██║   ███████╗██║  ██║███████║
╚═════╝ ╚══════╝   ╚═╝      ╚═╝   ╚══════╝╚═╝  ╚═╝╚══════╝
 */
/**
 * Returns a PSP at random(does not return local ever)
 * @return
 */
PermanentStoragePool* PermanentStoragePoolList::getRandomPool() {
    // Generate a random index
    unsigned int index = (std::rand() % (_pools.size() - 1)) + 1;

    // Return the pool at the random index
    return _pools[index].get();
}

/**
 * Returns a specific PSP based on its index
 * @param poolIndex
 * @return
 */
PermanentStoragePool* PermanentStoragePoolList::getPool(unsigned int poolIndex) {
    // Check if the index is within the bounds of the vector
    if (poolIndex >= _pools.size()) {
        throw std::out_of_range("Pool index out of range");
    }

    // Return the pool at the specified index
    return _pools[poolIndex].get();
}

/**
 * Returns how many pools there are
 * @return
 */
unsigned int PermanentStoragePoolList::getPoolCount() {
    return _pools.size();
}

/**
 * Adds a pool to the list(called by constructor only)
 * @param pool
 */
void PermanentStoragePoolList::addPool(std::unique_ptr<PermanentStoragePool> pool, const Config& config) {
    unsigned int index = static_cast<unsigned int>(_pools.size());
    pool->setPoolIndexAndInitialize(index, config);
    _pools.push_back(std::move(pool));
}

/*
██╗██████╗ ███████╗███████╗     ██████╗ █████╗ ██╗     ██╗         ██████╗  █████╗  ██████╗██╗  ██╗
██║██╔══██╗██╔════╝██╔════╝    ██╔════╝██╔══██╗██║     ██║         ██╔══██╗██╔══██╗██╔════╝██║ ██╔╝
██║██████╔╝█████╗  ███████╗    ██║     ███████║██║     ██║         ██████╔╝███████║██║     █████╔╝
██║██╔═══╝ ██╔══╝  ╚════██║    ██║     ██╔══██║██║     ██║         ██╔══██╗██╔══██║██║     ██╔═██╗
██║██║     ██║     ███████║    ╚██████╗██║  ██║███████╗███████╗    ██████╔╝██║  ██║╚██████╗██║  ██╗
╚═╝╚═╝     ╚═╝     ╚══════╝     ╚═════╝╚═╝  ╚═╝╚══════╝╚══════╝    ╚═════╝ ╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝
 */

void PermanentStoragePoolList::_callbackNewMetadata(const string& cid, const string& extra, const string& content, bool failed) {
  try {
    AppMain* main = AppMain::GetInstance();
    //failed will always be false since no maxSleep ever set

    //convert content string in to processed json
    Json::Value metadata;
    Json::CharReaderBuilder rbuilder;
    istringstream s(content);
    string errs;
    if (!Json::parseFromStream(rbuilder, s, &metadata, &errs)) return; //invalid json data so don't process

    //Check if there is a data.urls section
    if (!metadata.isMember("data") || !metadata["data"].isObject()) return; //Improperly formatted
    Json::Value data = metadata["data"];
    if (!data.isMember("urls") || !data["urls"].isArray()) return; //Improperly formatted
    Json::Value urls = data["urls"];

    //pin the main metadata if less than limit
    IPFS* ipfs = main->getIPFS();
    if (content.length() < PSP_PIN_METADATA_LIMIT) {
        try { ipfs->pin(cid); } catch (...) {}
    }

    //get the list of known PSPs
    PermanentStoragePoolList* pools = main->getPermanentStoragePoolList();

    //decode what PSP this file is part of and call its processor
    //extra is in the form "a:assetIndex,p#:?,p#:?..."   where # is the pool number, ? is the encoded processor instructions.
    //assetIndex will always be first
    std::vector<std::string> pairs = utils::split(extra, ',');
    unsigned int assetIndex;
    for (const auto& pair: pairs) {
        std::vector<std::string> parts = utils::split(pair, ':');
        switch (parts[0][0]) {
            case 'a': //asset index.  Must always be first part of string
                assetIndex = stoul(parts[1]);
                break;

            case 'p': //pool.  Can be multiple pools an asset is associated with but likely only 1
                //get the processor
                unsigned int poolNumber = std::stoul(parts[0].substr(1));
                PermanentStoragePool* pool = pools->getPool(poolNumber);
                std::unique_ptr<PermanentStoragePoolMetaProcessor> processor = pool->deserializeMetaProcessor(parts[1]);
                bool pinnedAll = true;

                //Go through URLs and pin those we care about
                for (const auto& obj: urls) {
                    //get name, url, and if present mime type
                    if (!obj.isMember("name") || !obj["name"].isString()) continue;
                    if (!obj.isMember("url") || !obj["url"].isString()) continue;
                    string name = obj["name"].asString();
                    string url = obj["url"].asString();
                    if (!IPFS::isIPFSurl(url)) {
                        pinnedAll = false;
                        continue; //we can't pin urls that are not for ipfs
                    }
                    string subCID = IPFS::getCID(url);
                    string mimeType =
                            (obj.isMember("mimeType") && obj["mimeType"].isString()) ? obj["mimeType"].asString() : "";

                    //check if we should pin for this PPS
                    bool shouldPin = processor->shouldPinFile(name, mimeType, subCID);

                    //keep track of if we pinned all files
                    if (!shouldPin) {
                        pinnedAll = false;
                        continue;
                    }

                    //pin the file if subscribed to that psp
                    if (pool->subscribed()) ipfs->pin(subCID);
                }

                //if all files where pinned then mark this as part of the PSP
                if (!pinnedAll) break;
                pool->markAssetAsPartOfPool(assetIndex);

                //pin the metadata if subscribed
                if (pool->subscribed()) {
                    try { ipfs->pin(cid); } catch (...) {}
                }
                break;
        }
    }
  } catch (const std::exception& e) {
    Log::GetInstance()->addMessage(std::string("PSP metadata callback exception: ") + e.what(), Log::WARNING);
  } catch (...) {
    Log::GetInstance()->addMessage("PSP metadata callback unknown exception", Log::WARNING);
  }
}
void PermanentStoragePoolList::processNewMetaData(const DigiByteTransaction& tx, unsigned int assetIndex, const string& cid) {
    //compute the decoding instructions
    string extra = "a:" + to_string(assetIndex);
    for (auto& pool: *this) {
        string serialized = pool->serializeMetaProcessor(tx);               //see if part of pool
        if (serialized.empty()) continue;                                   //skip if not part of pool
        extra += ",p" + to_string(pool->getPoolIndex()) + ":" + serialized; //add to instructions
    }

    // DEBUG: surface the detection result so we can tell if serializer found
    // a matching output. "a:N" alone = no pool matched; "a:N,p1:S_..." = matched.
    // Also dump the raw outputs at DEBUG so we can see addresses/digibyte
    // values the serializer saw.
    {
        Log* log = Log::GetInstance();
        log->addMessage("PSP processNewMetaData: assetIdx=" + to_string(assetIndex) +
                        " extra=" + extra, Log::DEBUG);
        size_t outCount = tx.getOutputCount();
        for (size_t i = 0; i < outCount; i++) {
            AssetUTXO out = tx.getOutput(i);
            log->addMessage("  out[" + to_string(i) + "] addr='" + out.address +
                            "' dgb=" + to_string(out.digibyte), Log::DEBUG);
        }
    }

    //download the metadata
    IPFS* ipfs = AppMain::GetInstance()->getIPFS();
    ipfs->callOnDownload(cid, "", extra, PSP_CALLBACK_NEWMETADATA_ID);
}
