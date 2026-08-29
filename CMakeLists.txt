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

add_executable(spm src/main.cpp)

target_link_libraries(spm
    PRIVATE
    nlohmann_json::nlohmann_json
    ixwebsocket
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

add_executable(test_emitter src/veneer/test_emitter.cpp)
target_link_libraries(test_emitter PRIVATE nlohmann_json::nlohmann_json)

add_executable(test_lexer src/veneer/test_lexer.cpp)
target_link_libraries(test_lexer PRIVATE nlohmann_json::nlohmann_json)

add_executable(test_parser src/veneer/test_parser.cpp)
target_link_libraries(test_parser PRIVATE nlohmann_json::nlohmann_json)

add_executable(test_resolver src/veneer/test_resolver.cpp)
target_link_libraries(test_resolver PRIVATE nlohmann_json::nlohmann_json)

add_executable(test_registry src/veneer/test_registry.cpp)
target_link_libraries(test_registry PRIVATE nlohmann_json::nlohmann_json)

add_executable(test_extractor src/veneer/test_extractor.cpp)
target_link_libraries(test_extractor PRIVATE nlohmann_json::nlohmann_json)

add_executable(test_html_validator src/veneer/test_html_validator.cpp)
target_link_libraries(test_html_validator PRIVATE nlohmann_json::nlohmann_json)

enable_testing()
add_test(NAME test_emitter COMMAND test_emitter)
add_test(NAME test_lexer COMMAND test_lexer)
add_test(NAME test_parser COMMAND test_parser)
add_test(NAME test_resolver COMMAND test_resolver)
add_test(NAME test_registry COMMAND test_registry)
add_test(NAME test_extractor COMMAND test_extractor)
add_test(NAME test_html_validator COMMAND test_html_validator)