cmake_minimum_required(VERSION 3.21)
project(digiasset_core-web)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_CXX_STANDARD 17)

include_directories("${CMAKE_SOURCE_DIR}/src")

set(HEADER_FILES
        ../src/Config.h
        )

set(SOURCE_FILES
        ../src/Config.cpp
        )

add_executable(digiasset_core-web main.cpp ${SOURCE_FILES} ${HEADER_FILES})

if(WIN32 AND MSVC)
  # Use real Boost headers from NuGet (not the stub in src/boost/)
  # The real Boost include must come BEFORE src/ to avoid picking up the stub
  target_include_directories(digiasset_core-web BEFORE PRIVATE
    "${CMAKE_SOURCE_DIR}/packages/boost.1.82.0/lib/native/include")
  add_definitions(-D_WIN32_WINNT=0x0601)
endif()

install(TARGETS digiasset_core-web DESTINATION bin)