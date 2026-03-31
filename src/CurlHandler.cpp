//
// Created by mctrivia on 01/02/24.
//

#include "CurlHandler.h"
#include "static_block.hpp"
#include <map>
#include <stdexcept>

using namespace std;


// Static block to register our callback function with IPFS Controller
static_block {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

namespace CurlHandler {
    namespace {
        // Thread-local CURL handle - reused across calls to preserve
        // WinHTTP session and connection handles (avoids TCP handshake per request)
        thread_local CURL* tl_curl = nullptr;

        CURL* acquireHandle() {
            if (!tl_curl) {
                tl_curl = curl_easy_init();
            } else {
                curl_easy_reset(tl_curl);
            }
            return tl_curl;
        }

        /**
         * Private callback function for curl to build a string with returned data
         */
        size_t writeCallback(void* contents, size_t size, size_t nmemb, string* s) {
            size_t newLength = size * nmemb;
            try {
                s->append((char*) contents, newLength);
                return newLength;
            } catch (bad_alloc& e) {
                // handle memory problem
                return 0;
            }
        }

        /**
         * Private callback function for curl to build a file with returned data
         */
        size_t writeData(void* ptr, size_t size, size_t nmemb, FILE* stream) {
            size_t written = fwrite(ptr, size, nmemb, stream);
            return written;
        }
    } // namespace

    string get(const string& url, unsigned int timeout) {
        CURL* curl = acquireHandle();
        if (!curl) {
            throw runtime_error("Failed to initialize CURL");
        }

        string readBuffer;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        if (timeout > 0) curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OPERATION_TIMEDOUT) throw exceptionTimeout();
        if (res != CURLE_OK) {
            throw runtime_error(curl_easy_strerror(res));
        }
        return readBuffer;
    }

    string post(const string& url, const map<string, string>& data, unsigned int timeout) {
        //preprocess post data
        string postData;
        for (const auto& entry: data) {
            if (!postData.empty()) {
                postData += "&";
            }
            postData += entry.first + "=" + entry.second;
        }

        CURL* curl = acquireHandle();
        if (!curl) {
            throw runtime_error("Failed to initialize CURL");
        }

        string readBuffer;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        if (timeout > 0) curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OPERATION_TIMEDOUT) throw exceptionTimeout();
        if (res != CURLE_OK) {
            throw runtime_error(curl_easy_strerror(res));
        }
        return readBuffer;
    }

    void getDownload(const string& url, const string& fileName, unsigned int timeout) {
        CURL* curl = acquireHandle();
        if (!curl) {
            throw runtime_error("Failed to initialize CURL");
        }

        FILE* fp;
#ifdef _WIN32
        errno_t err = fopen_s(&fp, fileName.c_str(), "wb");
        if (err != 0 || fp == NULL) {
            throw runtime_error("Failed to open file for writing: " + fileName);
        }
#else
        fp = fopen(fileName.c_str(), "wb");
        if (fp == NULL) {
            throw runtime_error("Failed to open file for writing: " + fileName);
        }
#endif
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        if (timeout > 0) curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);
        CURLcode res = curl_easy_perform(curl);
        fclose(fp);
        if (res == CURLE_OPERATION_TIMEDOUT) throw exceptionTimeout();
        if (res != CURLE_OK) {
            throw runtime_error(curl_easy_strerror(res));
        }
    }

    void postDownload(const string& url, const string& fileName, const map<string, string>& data, unsigned int timeout) {
        //preprocess post data
        string postData;
        for (const auto& entry: data) {
            if (!postData.empty()) {
                postData += "&";
            }
            postData += entry.first + "=" + entry.second;
        }

        CURL* curl = acquireHandle();
        if (!curl) {
            throw runtime_error("Failed to initialize CURL");
        }

        FILE* fp;
#ifdef _WIN32
        errno_t err = fopen_s(&fp, fileName.c_str(), "wb");
        if (err != 0 || fp == NULL) {
            throw runtime_error("Failed to open file for writing: " + fileName);
        }
#else
        fp = fopen(fileName.c_str(), "wb");
        if (fp == NULL) {
            throw runtime_error("Failed to open file for writing: " + fileName);
        }
#endif
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        if (timeout > 0) curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);
        CURLcode res = curl_easy_perform(curl);
        fclose(fp);
        if (res == CURLE_OPERATION_TIMEDOUT) throw exceptionTimeout();
        if (res != CURLE_OK) {
            throw runtime_error(curl_easy_strerror(res));
        }
    }


} // namespace CurlHandler
