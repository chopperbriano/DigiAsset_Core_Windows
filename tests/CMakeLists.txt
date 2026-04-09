# 'Google_test' is the subproject name
project(Google_tests)

# 'lib' is the folder with Google Test sources
add_subdirectory(lib/googletest)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
if(MSVC)
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 11)
endif()


set(TESTS
        BitIO.cpp
        DatabaseChain.cpp
        DigiByteCore.cpp
        IPFS.cpp
        Base58Tests.cpp
        DigiAssetTest.cpp
        DigiAssetRulesTest.cpp
        DigiAssetTransactionTest.cpp
        TestHelpers.cpp TestHelpers.h RPCMethods.cpp RPCMethods.h)

#load RPC tests
file(GLOB RPC_METHOD_TEST_SOURCES RPC_Methods/*.cpp)
foreach(SOURCE_FILE ${RPC_METHOD_TEST_SOURCES})
    list(APPEND TESTS ${SOURCE_FILE})
endforeach()

# 'Google_Tests_run' is the target name
# 'test1.cpp tests2.cpp' are source files with tests
include_directories("${CMAKE_SOURCE_DIR}/jsoncpp/install/include")
include_directories("${CMAKE_SOURCE_DIR}/libjson-rpc-cpp/install/include")
include_directories("${CMAKE_SOURCE_DIR}/src")

add_executable(Google_Tests_run ${TESTS})   #digibyteapi
target_link_libraries(Google_Tests_run digiasset_core_lib)
target_link_libraries(Google_Tests_run gtest gtest_main)

if(WIN32 AND MSVC)
  # Disable C++20 char8_t breaking change (u8"" literals stay as const char[])
  target_compile_options(Google_Tests_run PRIVATE /Zc:char8_t-)
  target_link_libraries(Google_Tests_run "${CMAKE_SOURCE_DIR}/jsoncpp/install/lib/jsoncpp_static.lib")
  target_link_libraries(Google_Tests_run "${CMAKE_SOURCE_DIR}/libjson-rpc-cpp/install/lib/jsonrpccpp-common.lib")
  target_link_libraries(Google_Tests_run "${CMAKE_SOURCE_DIR}/libjson-rpc-cpp/install/lib/jsonrpccpp-client.lib")
  target_link_libraries(Google_Tests_run winhttp)
else()
  target_link_libraries(Google_Tests_run sqlite3 curl)
  target_link_libraries(Google_Tests_run ssl crypto pthread)
  target_link_libraries(Google_Tests_run /lib/x86_64-linux-gnu/libjsoncpp.so jsonrpccpp-common jsonrpccpp-client)
endif()

# ---- Unit Tests (no external services required) ----
set(UNIT_TESTS
    BlobUnitTests.cpp
    BlobEdgeCaseTests.cpp
    ConfigUnitTests.cpp
    ConfigEdgeCaseTests.cpp
    DatabaseUnitTests.cpp
    DatabaseFunctionalTests.cpp
    VersionTests.cpp
    Base58Tests.cpp)

add_executable(Unit_Tests_run ${UNIT_TESTS})
target_link_libraries(Unit_Tests_run digiasset_core_lib)
target_link_libraries(Unit_Tests_run gtest gtest_main)

if(WIN32 AND MSVC)
  target_compile_options(Unit_Tests_run PRIVATE /Zc:char8_t-)
  target_link_libraries(Unit_Tests_run "${CMAKE_SOURCE_DIR}/jsoncpp/install/lib/jsoncpp_static.lib")
  target_link_libraries(Unit_Tests_run "${CMAKE_SOURCE_DIR}/libjson-rpc-cpp/install/lib/jsonrpccpp-common.lib")
  target_link_libraries(Unit_Tests_run "${CMAKE_SOURCE_DIR}/libjson-rpc-cpp/install/lib/jsonrpccpp-client.lib")
  target_link_libraries(Unit_Tests_run winhttp)
else()
  target_link_libraries(Unit_Tests_run sqlite3 curl)
  target_link_libraries(Unit_Tests_run ssl crypto pthread)
  target_link_libraries(Unit_Tests_run /lib/x86_64-linux-gnu/libjsoncpp.so jsonrpccpp-common jsonrpccpp-client)
endif()
