cmake_minimum_required(VERSION 3.20)
project(kvstore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(Boost REQUIRED COMPONENTS system)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)
find_package(spdlog REQUIRED)

# === Core library ===
add_library(kvstore_lib
    src/store.cpp
    src/sharded_map.cpp
    src/ttl_manager.cpp
    src/wal.cpp
    src/snapshot.cpp
    src/server.cpp
    src/protocol.cpp
)
target_include_directories(kvstore_lib PUBLIC include)
target_link_libraries(kvstore_lib PUBLIC 
    Boost::system
    spdlog::spdlog
    pthread
)
target_compile_options(kvstore_lib PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -Wshadow -Wnon-virtual-dtor -Wconversion
)

# === Executable ===
add_executable(kvstore src/main.cpp)
target_link_libraries(kvstore PRIVATE kvstore_lib)

# === Tests ===
enable_testing()
add_executable(kvstore_tests
    tests/test_store.cpp
    tests/test_sharded_map.cpp
    tests/test_protocol.cpp
    tests/test_wal.cpp
    tests/test_integration.cpp
)
target_link_libraries(kvstore_tests PRIVATE kvstore_lib GTest::gtest_main)
gtest_discover_tests(kvstore_tests)

# === Benchmarks ===
add_executable(kvstore_bench bench/bench_store.cpp)
target_link_libraries(kvstore_bench PRIVATE kvstore_lib benchmark::benchmark)

# === Sanitizer build ===
option(SANITIZER "Enable sanitizer (address, thread, undefined)" "")
if(SANITIZER)
    target_compile_options(kvstore_lib PUBLIC -fsanitize=${SANITIZER} -fno-omit-frame-pointer)
    target_link_options(kvstore_lib PUBLIC -fsanitize=${SANITIZER})
endif()
