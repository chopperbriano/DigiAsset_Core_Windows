//
// Created by mctrivia on 11/09/23.
//

#include "RPC/Server.h"
#include "AppMain.h"
#include "Config.h"
#include "DigiByteCore.h"
#include "Log.h"
#include "RPC/MethodList.h"
#include "Version.h"
#include "utils.h"
#include <algorithm>
#include <boost/asio.hpp>
#include <iostream>
#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <memory>
#include <thread>
#include <vector>



using namespace std;

namespace RPC {
    namespace {
        ///List of RPC commands whose values will not change until a new block is added
        vector<string> cacheableRpcCommands = {
                "getbestblockhash",
                "getblock",
                "getblockchaininfo",
                "getblockcount",
                "getblockfilter",
                "getblockhash",
                "getblockheader",
                "getblockstats",
                "getchaintips",
                "getchaintxstats",
                "getdifficulty",
                "getmempoolancestors",
                "getmempooldescendants",
                "getmempoolentry",
                "getmempoolinfo",
                "getrawmempool",
                "gettxout",
                "gettxoutproof",
                "gettxoutsetinfo",
                "preciousblock",
                "pruneblockchain",
                "savemempool",
                "scantxoutset",
                "verifychain",
                "verifytxoutproof"};

        // Simple Base64 decoder for Basic Auth when OpenSSL is unavailable
        static std::string base64Decode(const std::string& input) {
            static const std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string decoded;
            int val = 0;
            int valb = -8;
            for (unsigned char c : input) {
                if (isspace(c) || c == '=') {
                    if (c == '=') break;
                    continue;
                }
                size_t pos = charset.find(c);
                if (pos == std::string::npos) break;
                val = (val << 6) + static_cast<int>(pos);
                valb += 6;
                if (valb >= 0) {
                    decoded.push_back(char((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            return decoded;
        }
    } // namespace

    /**
     * Small class that allows easy forcing close socket when it goes out of scope
     */
    class SocketRAII {
    public:
        explicit SocketRAII(tcp::socket& s) : socket(s) {}
        ~SocketRAII() { socket.close(); }

    private:
        tcp::socket& socket;
    };

    /*
    ███████╗███████╗████████╗██╗   ██╗██████╗
    ██╔════╝██╔════╝╚══██╔══╝██║   ██║██╔══██╗
    ███████╗█████╗     ██║   ██║   ██║██████╔╝
    ╚════██║██╔══╝     ██║   ██║   ██║██╔═══╝
    ███████║███████╗   ██║   ╚██████╔╝██║
    ╚══════╝╚══════╝   ╚═╝    ╚═════╝ ╚═╝
     */
    Server::Server(const string& fileName) : _io() {
        Log* log = Log::GetInstance();

        // Create work to keep io_ running
        auto work = boost::asio::make_work_guard(_io);

        // Create a pool of threads to run all of the io_services.
        size_t poolSize = 8; // default
        for (std::size_t i = 0; i < poolSize; ++i) {
            _thread_pool.emplace_back([this] { run_thread(); });
        }

        _acceptor = tcp::acceptor(_io);

        try {
            Config config = Config(fileName);
            _username = config.getString("rpcuser");
            _password = config.getString("rpcpassword");
            _port = config.getInteger("rpcassetport", 14024);

            tcp::endpoint endpoint(tcp::v4(), _port);
            _acceptor.open(endpoint.protocol());
            _acceptor.set_option(tcp::acceptor::reuse_address(true));
            _acceptor.bind(endpoint);
            _acceptor.listen();

            _allowedRPC = config.getBoolMap("rpcallow");
            _showParamsOnError = config.getBool("rpcdebugshowparamsonerror", false);
            log->addMessage("RPC Server listening on port " + std::to_string(_port));
        } catch (const std::exception& e) {
            log->addMessage(std::string("RPC::Server ctor exception: ") + e.what(), Log::CRITICAL);
            throw;
        } catch (...) {
            log->addMessage("RPC::Server ctor unknown exception", Log::CRITICAL);
            throw;
        }
    }

    Server::~Server() {
        // Wait for all threads in the pool to exit.
        for (auto& thread: _thread_pool) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void Server::run_thread() {
        _io.run();
    }

    void Server::start() {
        Log* log = Log::GetInstance();
        try {
            accept();
        } catch (const std::exception& e) {
            log->addMessage(std::string("RPC server stopped: ") + e.what(), Log::CRITICAL);
        } catch (...) {
            log->addMessage("RPC server stopped with unknown error", Log::CRITICAL);
        }
    }

    /*
     ██████╗ ███████╗███╗   ██╗███████╗██████╗ ██╗ ██████╗
    ██╔════╝ ██╔════╝████╗  ██║██╔════╝██╔══██╗██║██╔════╝
    ██║  ███╗█████╗  ██╔██╗ ██║█████╗  ██████╔╝██║██║
    ██║   ██║██╔══╝  ██║╚██╗██║██╔══╝  ██╔══██╗██║██║
    ╚██████╔╝███████╗██║ ╚████║███████╗██║  ██║██║╚██████╗
     ╚═════╝ ╚══════╝╚═╝  ╚═══╝╚══════╝╚═╝  ╚═╝╚═╝ ╚═════╝
     */

    void Server::accept() {
        Log* log = Log::GetInstance();
        while (true) {
            uint64_t callNumber = _callCounter++;
            try {
                auto socket = std::make_shared<tcp::socket>(_io);
                _acceptor.accept(*socket);
                log->addMessage("RPC call #" + std::to_string(callNumber) + " received", Log::DEBUG);
                boost::asio::post(_io, [this, socket, callNumber]() { this->handleConnection(socket, callNumber); });
            } catch (const std::exception& e) {
                log->addMessage("RPC accept error: " + std::string(e.what()), Log::DEBUG);
            } catch (...) {
                log->addMessage("RPC accept unknown error", Log::DEBUG);
            }
        }
    }

    void Server::handleConnection(std::shared_ptr<tcp::socket> socket, uint64_t callNumber) {
        Log* log = Log::GetInstance();

        std::chrono::steady_clock::time_point startTime;
        string method = "NA";
        try {
            //get the socket
            SocketRAII socketGuard(*socket); //make sure socket always gets closed

            //start timer and add debug log
            log->addMessage("RPC call #" + std::to_string(callNumber) + " started", Log::DEBUG);
            startTime = std::chrono::steady_clock::now();

            // Handle the request and send the response
            Value response;
            Value request;
            bool error = false;
            try {
                request = parseRequest(*socket);
                method = request["method"].asString();
                log->addMessage("RPC call #" + std::to_string(callNumber) + " method: " + method, Log::DEBUG);
                response = handleRpcRequest(request);
            } catch (const DigiByteException& e) {
                log->addMessage("Expected exception in RPC call #" + std::to_string(callNumber) + ": " + e.getMessage(), Log::DEBUG);
                response = createErrorResponse(e.getCode(), e.getMessage(), request);
                error = true;
            } catch (const std::exception& e) {
                log->addMessage("Unexpected exception in RPC call #" + std::to_string(callNumber) + ": " + e.what(), Log::DEBUG);
                response = createErrorResponse(RPC_MISC_ERROR, "Unexpected Error", request);
                error = true;
            } catch (...) {
                log->addMessage("Unknown exception in RPC call #" + std::to_string(callNumber), Log::DEBUG);
                response = createErrorResponse(RPC_MISC_ERROR, "Unexpected Error", request);
                error = true;
            }

            sendResponse(*socket, response);
            if (error && _showParamsOnError && request.isMember("params")) {
                log->addMessage("RPC call #" + std::to_string(callNumber) + " params: " + request["params"].toStyledString(), Log::DEBUG);
            }
        } catch (const std::exception& e) {
            log->addMessage("Unexpected exception trying to reply to RPC call #" + std::to_string(callNumber) + ": " + e.what(), Log::DEBUG);
        } catch (...) {
            log->addMessage("Unknown exception trying to reply to RPC call #" + std::to_string(callNumber), Log::DEBUG);
        }


        //calculate time taken
        auto duration = std::chrono::steady_clock::now() - startTime;
        log->addMessage("RPC call #" + std::to_string(callNumber) + " finished in " + to_string(std::chrono::duration_cast<std::chrono::microseconds>(duration).count()) + " µs", Log::DEBUG);
    }

    Value Server::parseRequest(tcp::socket& socket) {
        // Read the HTTP request headers
        char data[1024];
        size_t length = socket.read_some(boost::asio::buffer(data, sizeof(data)));

        // Parse the HTTP request headers to determine content length
        string requestStr(data, length);
        size_t bodyStart = requestStr.find("\r\n\r\n");
        if (bodyStart == string::npos) {
            throw DigiByteException(HTTP_BAD_REQUEST, "Invalid HTTP request: No request body found.");
        }
        string headers = requestStr.substr(0, bodyStart);

        //verify the authentication
        if (!basicAuth(headers)) {
            throw DigiByteException(HTTP_UNAUTHORIZED, "Invalid HTTP request: No valid authentication provided.");
        }

        // Find the Content-Length header
        int contentLength = stoi(getHeader(headers, "Content-Length"));

        // Prepare to read the body
        std::string jsonContent = requestStr.substr(bodyStart + 4);
        int remainingContent = contentLength - jsonContent.size();

        // Read the rest of the body if not all content was received
        while (remainingContent > 0) {
            char buffer[1024];
            std::size_t bytesRead = socket.read_some(boost::asio::buffer(buffer, std::min(remainingContent, static_cast<int>(sizeof(buffer)))));
            jsonContent.append(buffer, bytesRead);
            remainingContent -= bytesRead;
        }

        // Parse the JSON content
        Json::CharReaderBuilder readerBuilder;
        Json::Value doc;
        istringstream jsonContentStream(jsonContent);
        string errs;

        if (!Json::parseFromStream(readerBuilder, jsonContentStream, &doc, &errs)) {
            throw DigiByteException(RPC_PARSE_ERROR, "JSON parsing error: " + errs);
        }

        return doc;
    }

    string Server::getHeader(const string& headers, const string& wantedHeader) {
        //find the auth header
        string lowerHeaders = headers;
        string lowerWantedHeader = wantedHeader;
        transform(lowerHeaders.begin(), lowerHeaders.end(), lowerHeaders.begin(), ::tolower);
        transform(lowerWantedHeader.begin(), lowerWantedHeader.end(), lowerWantedHeader.begin(), ::tolower);

        size_t start = lowerHeaders.find(lowerWantedHeader + ": ");
        if (start == string::npos) throw DigiByteException(HTTP_BAD_REQUEST, wantedHeader + " header not found");
        size_t end = lowerHeaders.find("\r\n", start);
        size_t headerLength = lowerWantedHeader.length() + 2; //+2 for ": "
        return headers.substr(start + headerLength, end - start - headerLength);
    }

    // Basic authentication function
    bool Server::basicAuth(const string& headers) {
        string authHeader = getHeader(headers, "Authorization");

        // Extract and decode the base64-encoded credentials
        string base64Credentials = authHeader.substr(6); // Remove "Basic "
        string decoded = base64Decode(base64Credentials);

        return (decoded == _username + ":" + _password);
    }

    Value Server::executeCall(const std::string& methodName, const Json::Value& params, const Json::Value& id) {
        AppMain* app = AppMain::GetInstance();

        //see if method on approved list
        if (!isRPCAllowed(methodName)) throw DigiByteException(RPC_FORBIDDEN_BY_SAFE_MODE, methodName + " is forbidden");

        //see if cached
        Response response;
        if (app->getRpcCache()->isCached(methodName, params, response)) return response.toJSON(id);

        //see if custom method handler
        if (methods.find(methodName) != methods.end()) {
            // Method exists in the map, so call it and set the result
            response = methods[methodName](params);
        } else {
            // Method does not exist in the map, fallback to sending to DigiByte core
            Json::Value walletResponse = app->getDigiByteCore()->sendcommand(methodName, params);
            response.setResult(walletResponse);

            //if the method is not it cacheableRpcCommands then disable caching
            if (std::find(cacheableRpcCommands.begin(), cacheableRpcCommands.end(), methodName) == cacheableRpcCommands.end()) {
                response.setBlocksGoodFor(-1);
            }
        }

        //cache response
        app->getRpcCache()->addResponse(methodName, params, response);

        //return as json
        return response.toJSON(id);
    }

    Value Server::handleRpcRequest(const Value& request) {
        Json::Value id;

        //lets get the id(user defined value they can use as a reference)
        if (request.isMember("id")) {
            id = request["id"];
        } else {
            id = Value(Json::nullValue);
        }

        //lets get the command the user wants
        if (!request.isMember("method") || !request["method"].isString()) {
            throw DigiByteException(RPC_METHOD_NOT_FOUND, "Method not found");
        }
        string methodName = request["method"].asString();

        //lets check params is present
        if (request.isMember("params") && !request["params"].isArray()) {
            throw DigiByteException(RPC_INVALID_PARAMS, "Invalid params");
        }
        const Json::Value& params = request.isMember("params") ? request["params"] : Value(Json::nullValue);

        return executeCall(methodName, params, id);
    }


    Value Server::createErrorResponse(int code, const string& message, const Value& request) {
        // Create a JSON-RPC error response
        Json::Value response;
        response["result"] = Json::nullValue;
        Json::Value error;
        error["code"] = code;
        error["message"] = message;
        response["error"] = error;
        if (request.isMember("id")) response["id"] = request["id"];
        return response;
    }

    void Server::sendResponse(tcp::socket& socket, const Value& response) {
        Json::StreamWriterBuilder writer;
        writer.settings_["indentation"] = "";

        // Serialize JSON object to string.
        string jsonResponse = writeString(writer, response);

        // Create the HTTP response string.
        string httpResponse = "HTTP/1.1 200 OK\r\n";
        httpResponse += "Content-Length: " + to_string(jsonResponse.length()) + "\r\n";
        httpResponse += "Content-Type: application/json\r\n\r\n";
        httpResponse += jsonResponse;

        // Write the HTTP response to the socket.
        boost::asio::write(socket, boost::asio::buffer(httpResponse.c_str(), httpResponse.length()));
    }


    /*
     ██████╗ ███████╗████████╗████████╗███████╗██████╗ ███████╗
    ██╔════╝ ██╔════╝╚══██╔══╝╚══██╔══╝██╔════╝██╔══██╗██╔════╝
    ██║  ███╗█████╗     ██║      ██║   █████╗  ██████╔╝███████╗
    ██║   ██║██╔══╝     ██║      ██║   ██╔══╝  ██╔══██╗╚════██║
    ╚██████╔╝███████╗   ██║      ██║   ███████╗██║  ██║███████║
     ╚═════╝ ╚══════╝   ╚═╝      ╚═╝   ╚══════╝╚═╝  ╚═╝╚══════╝
     */
    unsigned int Server::getPort() {
        return _port;
    }

    bool Server::isRPCAllowed(const string& method) {
        auto it = _allowedRPC.find(method);
        if (it == _allowedRPC.end()) {
            if (_allowRPCDefault == -1) {
                _allowRPCDefault = 0; //default.  must set to prevent possible infinite loop
                _allowRPCDefault = isRPCAllowed("*") ? 1 : 0;
            }
            return _allowRPCDefault;
        }
        return it->second;
    }

} // namespace RPC