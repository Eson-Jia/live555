cmake_minimum_required(VERSION 3.19)
project(live)

set(CMAKE_CXX_STANDARD 11)

include_directories(BasicUsageEnvironment/include)
include_directories(groupsock/include)
include_directories(liveMedia)
include_directories(liveMedia/include)
include_directories(mediaServer)
include_directories(testProgs)
include_directories(UsageEnvironment/include)

find_library(GROUP_SOCK NAMES groupsock HINTS groupsock)
find_library(LIVE_MEDIA NAMES liveMedia  HINTS liveMedia)
find_library(BASIC_USG_ENV NAMES BasicUsageEnvironment HINTS BasicUsageEnvironment )
find_library(USG_ENV NAMES UsageEnvironment HINTS UsageEnvironment )

add_executable(live555MediaServer
        mediaServer/live555MediaServer.cpp
        mediaServer/DynamicRTSPServer.cpp
        )

target_link_libraries(
        live555MediaServer
        ${LIVE_MEDIA}
        ${BASIC_USG_ENV}
        ${USG_ENV}
        ${GROUP_SOCK}
        ssl
        crypto)

add_executable(proxyServer
        proxyServer/live555ProxyServer.cpp
        )

target_link_libraries(
        proxyServer
        ${LIVE_MEDIA}
        ${BASIC_USG_ENV}
        ${USG_ENV}
        ${GROUP_SOCK}
        ssl
        crypto
)

add_executable(
        hlsProxy
        hlsProxy/live555HLSProxy.cpp
        )

target_link_libraries(
        hlsProxy
        ${LIVE_MEDIA}
        ${BASIC_USG_ENV}
        ${USG_ENV}
        ${GROUP_SOCK}
        ssl
        crypto
)