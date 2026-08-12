cmake_minimum_required(VERSION 3.14)
project(spm-cli CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

include(FetchContent)

message(STATUS "nlohmann_json FetchContent...")
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

message(STATUS "ixwebsocket FetchContent...")
FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG v11.4.4
)
set(USE_TLS ON CACHE BOOL "Enable SSL/TLS support" FORCE)
FetchContent_MakeAvailable(ixwebsocket)

add_executable(spm-cli src/main.cpp)

target_link_libraries(spm-cli
    PRIVATE
    nlohmann_json::nlohmann_json
    ixwebsocket
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)