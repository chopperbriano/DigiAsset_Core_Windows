//
// Created by mctrivia on 11/09/23.
//

#ifndef DIGIASSET_CORE_RPC_SERVER_H
#define DIGIASSET_CORE_RPC_SERVER_H


#define HTTP_BAD_REQUEST 400
#define HTTP_UNAUTHORIZED 401
#define RPC_METHOD_NOT_FOUND (-32601)
#define RPC_INVALID_PARAMS (-32602)
#define RPC_PARSE_ERROR (-32700)
#define RPC_FORBIDDEN_BY_SAFE_MODE (-2)
#define RPC_MISC_ERROR (-1)

// Macro definition in a common header or the RPC server file
#define REGISTER_RPC_METHOD(methodName) registerMethod(#methodName, &std::methodName)



// Specific sub-headers, not <boost/asio.hpp>, because the latter is shadowed
// by the no-op stub at src/boost/asio.hpp on this fork's include path.
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <jsonrpccpp/server.h>
#include <string>
// #include <jsonrpccpp/server/connectors/httpserver.h>
#include "ChainAnalyzer.h"
#include "DigiByteCore.h"
#include "UniqueTaskQueue.h"
#include <sstream>

using namespace std;
using namespace jsonrpc;
using boost::asio::ip::tcp;

namespace RPC {

    class Server {
        std::atomic<uint64_t> _callCounter{0};

        boost::asio::io_context _io{};
        // Work guard keeps _io::run() from returning when the queue is empty.
        // MUST be a member so it outlives construction — previously it was a
        // local in the ctor, which meant the thread pool ran for a few
        // microseconds and then exited the moment the ctor returned, leaving
        // posted work with nothing to execute it.
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _workGuard;
        std::vector<std::thread> _thread_pool;

        tcp::acceptor _acceptor;
        std::string _username;
        std::string _password;
        unsigned int _port;
        std::map<std::string, bool> _allowedRPC;
        int8_t _allowRPCDefault = -1; //unknown
        bool _showParamsOnError = false;

        //functions to handle requests
        Value parseRequest(tcp::socket& socket);
        [[noreturn]] void accept();
        void handleConnection(std::shared_ptr<tcp::socket> socket, uint64_t callNumber);
        Value handleRpcRequest(const Value& request);
        static Value createErrorResponse(int code, const std::string& message, const Value& request);
        static void sendResponse(tcp::socket& socket, const Value& response);
        bool basicAuth(const std::string& header);
        static std::string getHeader(const std::string& headers, const std::string& wantedHeader);
        void run_thread();

    public:
        explicit Server(const std::string& fileName = "config.cfg");
        ~Server();

        void start();
        unsigned int getPort();
        bool isRPCAllowed(const string& method);
        Value executeCall(const std::string& methodName, const Json::Value& params, const Json::Value& id = 1);
    };

} // namespace RPC
#endif //DIGIASSET_CORE_RPC_SERVER_H
