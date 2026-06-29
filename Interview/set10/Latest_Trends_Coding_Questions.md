# Set 10: Latest Trends, Best Practices & Live Coding
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: Latest C++ Trends & Technologies (2024-2026)

---

## Q1: What's new in C++23 and what's coming in C++26?

### Answer:

**C++23 Key Features:**
```cpp
// 1. std::expected | Better error handling than exceptions
#include <expected>
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) return std::unexpected("Division by zero");
    return a / b;
}
auto result = divide(10, 3);
if (result) std::cout << *result;  // 3
else std::cout << result.error();  // "Division by zero"

// 2. std::print / std::println | Type-safe formatting
#include <print>
std::println("Hello, {}! You are {} years old.", name, age);
// Replaces: printf (unsafe), cout (verbose), fmt::print (external)

// 3. std::generator | Coroutine-based generator
#include <generator>
std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a;
        a = b;
        b = temp + b;
    }
}
for (int n : fibonacci() | std::views::take(10))
    std::print("{} ", n);  // 0 1 1 2 3 5 8 13 21 34

// 4. std::mdspan | Multi-dimensional array view
#include <mdspan>
std::vector<double> data(rows * cols);
std::mdspan<double, std::dextents<size_t, 2>> matrix(data.data(), rows, cols);
matrix[2, 3] = 42.0;  // Access like 2D array, data is contiguous

// 5. std::flat_map / std::flat_set | Cache-friendly sorted containers
#include <flat_map>
std::flat_map<std::string, int> scores;  // Backed by sorted vectors
scores["Alice"] = 95;  // Cache-friendly iteration, slower insert

// 6. Deducing this (Explicit object parameter)
struct Widget {
    template<typename Self>
    auto& value(this Self&& self) {
        return std::forward<Self>(self).value_;
    }
    // Replaces 4 overloads: const&, &, const&&, &&
};

// 7. std::stacktrace | Programmatic stack traces
#include <stacktrace>
void crashHandler() {
    auto trace = std::stacktrace::current();
    std::println("{}", std::to_string(trace));
}

// 8. Ranges improvements: ranges::to, zip, chunk, slide
auto vec = std::views::iota(1, 10)
         | std::views::filter([](int x) { return x % 2 == 0; })
         | std::ranges::to<std::vector>();  // Materialize range to container

auto pairs = std::views::zip(names, ages);  // Iterate two ranges together
```

**C++26 (Expected/In Progress):**
```
- std::execution (Senders/Receivers) | Standard async framework
- Reflection (compile-time introspection)
- Pattern matching
- Contracts (preconditions, postconditions, assertions)
- std::inplace_vector (fixed-capacity, stack-allocated vector)
- Improved coroutine library support
- std::hive (formerly colony) | pool-like container
```

**C++26 Reflection Deep Dive (P2996 ? approved for C++26):**
```cpp
// Compile-time inspection of types, members, and functions
#include <meta>

struct Config {
    std::string host;
    int port;
    bool useTLS;
};

// Auto-generate JSON serialization via reflection (no macros, no code gen)
template<typename T>
std::string toJSON(const T& obj) {
    std::string result = "{";
    bool first = true;
    template for (constexpr auto member : std::meta::nonstatic_data_members_of(^T)) {
        if (!first) result += ",";
        result += "\"" + std::string(std::meta::identifier_of(member)) + "\":";
        result += serialize(obj.[:member:]);  // Splice operator
        first = false;
    }
    return result + "}";
}
// toJSON(Config{"localhost", 8080, true})
// | {"host":"localhost","port":8080,"useTLS":true}
// No boilerplate! Works for ANY struct!
```

**C++26 Contracts Deep Dive (P2900):**
```cpp
// Preconditions, postconditions, and assertions checked at runtime
int divide(int a, int b)
    pre(b != 0)                          // Precondition
    post(r: r * b == a || a % b != 0)    // Postcondition (r = return value)
{
    return a / b;
}

void processBuffer(std::span<int> buf, size_t idx)
    pre(idx < buf.size())               // Bounds check contract
    pre(!buf.empty())                    // Non-empty contract
{
    buf[idx] *= 2;
}

// Contract violation modes (configurable at build time):
// - enforce: Calls violation handler (default: abort)
// - observe: Log violation but continue (production monitoring)
// - ignore: Zero overhead (same as no contract | release builds)
// 
// Key benefit: Contracts are part of the function signature,
// visible in documentation and IDE tooling
```

### Explanation:
| Feature | Impact | Your Domain |
|---------|--------|-------------|
| `std::expected` | Replaces error codes AND exceptions | Finance (error handling on hot path) |
| `std::mdspan` | Zero-copy multi-dim views | CAD (matrices), Gaming (texture data) |
| `std::flat_map` | Cache-friendly maps | Gaming (ECS), HFT (lookup tables) |
| `std::generator` | Easy lazy sequences | All (data processing pipelines) |
| Senders/Receivers | Standard async model | All (replaces ad-hoc thread pools) |
| Reflection | Auto-serialization, ORM | Enterprise (IBM), reducing boilerplate |
| Contracts | Eliminate ad-hoc assertions | All (API boundaries, safety-critical) |

---

## Q2: What build systems and package managers are current best practice for C++?

### Answer:

**Build Systems (2025-2026):**
| Tool | Status | Best For |
|------|--------|----------|
| CMake 3.28+ | Industry standard | Most projects, modules support |
| Bazel | Growing in enterprise | Large monorepos (Google-style) |
| Meson | Simpler alternative | Small-medium projects |
| build2 | Niche but modern | New projects wanting integrated tooling |

**Package Managers:**
| Tool | Status | Best For |
|------|--------|----------|
| vcpkg | Most popular (Microsoft) | Cross-platform, CMake integration |
| Conan 2.x | Strong enterprise | Complex dependency graphs, binary caching |
| CPM.cmake | Lightweight | CMake-only projects |

**Modern CMake Best Practices:**
```cmake
cmake_minimum_required(VERSION 3.28)
project(MyCADApp VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C++20 Modules support
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# vcpkg integration
find_package(fmt CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS asio)

add_library(geometry_core)
target_sources(geometry_core
    PUBLIC FILE_SET CXX_MODULES FILES
        src/geometry.cppm
        src/topology.cppm
)
target_link_libraries(geometry_core PRIVATE fmt::fmt)

# Sanitizers for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(geometry_core PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(geometry_core PRIVATE
        -fsanitize=address,undefined
    )
endif()
```

---

## Q3: What are Senders/Receivers (std::execution) and why do they matter?

### Answer:
```cpp
// std::execution (P2300) | The future of C++ async
// Currently available via stdexec (reference implementation)
#include <stdexec/execution.hpp>

using namespace stdexec;

// Sender: describes async work (lazy, composable)
auto work = just(42)                          // Start with value 42
          | then([](int x) { return x * 2; }) // Transform
          | then([](int x) { return std::to_string(x); }); // Transform again

// Nothing has executed yet! Senders are lazy descriptions.

// Receiver: consumes the result
// Scheduler: decides WHERE to run (thread pool, GPU, etc.)
auto result = sync_wait(std::move(work));  // Execute and wait
// result = "84"

// Real-world: Async pipeline with error handling
auto pipeline = 
    schedule(threadPool.get_scheduler())   // Run on thread pool
  | then([] { return readFile("data.csv"); })
  | then([](auto data) { return parseCSV(data); })
  | then([](auto records) { return aggregate(records); })
  | upon_error([](auto err) { log("Pipeline failed: ", err); });

// Parallel fan-out
auto parallel = when_all(
    on(gpuScheduler, computePhysics()),
    on(cpuPool, computeAI()),
    on(ioScheduler, loadAssets())
);
```

### Explanation:
**Why this matters (replacing ad-hoc async):**
- **Composable**: Chain operations like ranges for data, but for async work
- **Scheduler-agnostic**: Same code runs on thread pool, GPU, or single-threaded
- **Structured concurrency**: Resources tied to scope, no leaked tasks
- **Zero allocation**: Small async operations don't need heap
- **Replaces**: `std::async` (broken), hand-rolled thread pools, Boost.Asio patterns

**Impact on your domains:**
- **Gaming**: GPU + CPU work orchestration without manual synchronization
- **Finance**: Market data processing pipelines
- **CAD**: Parallel geometry computation with structured cancellation

---

# Part B: Best Practices & Coding Standards

---

## Q4: What are the key principles of modern C++ coding standards?

### Answer:

**Core Guidelines (from ISO C++ Core Guidelines by Stroustrup & Sutter):**

```cpp
// 1. RAII everywhere | no resource leaks possible
{
    auto file = std::ifstream("data.txt");  // Closes automatically
    auto conn = ConnectionPool::acquire();  // Returns automatically
    auto lock = std::scoped_lock(mutex);    // Unlocks automatically
}  // Everything cleaned up, even with exceptions

// 2. Prefer value semantics over pointer semantics
struct Point { double x, y, z; };
Point translate(Point p, Vector v) {  // Pass by value, return by value
    return {p.x + v.dx, p.y + v.dy, p.z + v.dz};
}

// 3. Use const correctly
void process(const std::vector<int>& data);  // Won't modify
int calculateArea() const;                    // Member function won't modify object
const auto& ref = getExpensiveObject();       // Avoid copy

// 4. Prefer composition over inheritance
class Car {
    Engine engine_;          // HAS-A (composition)
    Transmission trans_;     // HAS-A
    // NOT: class Car : public Vehicle, public Engine
};

// 5. Don't use raw new/delete
auto p = std::make_unique<Widget>();     // Not: new Widget()
auto v = std::vector<int>(1000);         // Not: new int[1000]
auto s = std::string("hello");           // Not: strdup/malloc

// 6. Use string_view for non-owning string parameters
void log(std::string_view message);  // No copy, accepts string, string_view, char*

// 7. Use span for non-owning array parameters
void process(std::span<const int> data);  // Accepts vector, array, C-array

// 8. Structured error handling
std::expected<Result, Error> tryOperation();  // C++23
std::optional<int> findValue(Key k);          // C++17 | nullable return

// 9. Use enum class, not plain enum
enum class Color { RED, GREEN, BLUE };  // Scoped, type-safe
// NOT: enum Color { RED, GREEN, BLUE }; // Pollutes namespace

// 10. Mark functions noexcept when they don't throw
void swap(Widget& a, Widget& b) noexcept;
~Widget() noexcept;  // Destructors should ALWAYS be noexcept
```

---

## Q5: What testing strategies do you use for C++ projects?

### Answer:
```cpp
// Testing Pyramid for C++:
//
//        /\
//       /  \     E2E Tests (few, slow)
//      /    \    - Full system tests, integration with hardware/UI
//     / 
        $match = # Set 10: Latest Trends, Best Practices & Live Coding
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: Latest C++ Trends & Technologies (2024-2026)

---

## Q1: What's new in C++23 and what's coming in C++26|

### Answer:

**C++23 Key Features:**
```cpp
// 1. std::expected -> Better error handling than exceptions
#include <expected>
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) return std::unexpected("Division by zero");
    return a / b;
}
auto result = divide(10, 3);
if (result) std::cout << *result;  // 3
else std::cout << result.error();  // "Division by zero"

// 2. std::print / std::println -> Type-safe formatting
#include <print>
std::println("Hello, {}! You are {} years old.", name, age);
// Replaces: printf (unsafe), cout (verbose), fmt::print (external)

// 3. std::generator -> Coroutine-based generator
#include <generator>
std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a;
        a = b;
        b = temp + b;
    }
}
for (int n : fibonacci() | std::views::take(10))
    std::print("{} ", n);  // 0 1 1 2 3 5 8 13 21 34

// 4. std::mdspan -> Multi-dimensional array view
#include <mdspan>
std::vector<double> data(rows * cols);
std::mdspan<double, std::dextents<size_t, 2>> matrix(data.data(), rows, cols);
matrix[2, 3] = 42.0;  // Access like 2D array, data is contiguous

// 5. std::flat_map / std::flat_set -> Cache-friendly sorted containers
#include <flat_map>
std::flat_map<std::string, int> scores;  // Backed by sorted vectors
scores["Alice"] = 95;  // Cache-friendly iteration, slower insert

// 6. Deducing this (Explicit object parameter)
struct Widget {
    template<typename Self>
    auto& value(this Self&& self) {
        return std::forward<Self>(self).value_;
    }
    // Replaces 4 overloads: const&, &, const&&, &&
};

// 7. std::stacktrace -> Programmatic stack traces
#include <stacktrace>
void crashHandler() {
    auto trace = std::stacktrace::current();
    std::println("{}", std::to_string(trace));
}

// 8. Ranges improvements: ranges::to, zip, chunk, slide
auto vec = std::views::iota(1, 10)
         | std::views::filter([](int x) { return x % 2 == 0; })
         | std::ranges::to<std::vector>();  // Materialize range to container

auto pairs = std::views::zip(names, ages);  // Iterate two ranges together
```

**C++26 (Expected/In Progress):**
```
- std::execution (Senders/Receivers) -> Standard async framework
- Reflection (compile-time introspection)
- Pattern matching
- Contracts (preconditions, postconditions, assertions)
- std::inplace_vector (fixed-capacity, stack-allocated vector)
- Improved coroutine library support
- std::hive (formerly colony) -> pool-like container
```

**C++26 Reflection Deep Dive (P2996 ? approved for C++26):**
```cpp
// Compile-time inspection of types, members, and functions
#include <meta>

struct Config {
    std::string host;
    int port;
    bool useTLS;
};

// Auto-generate JSON serialization via reflection (no macros, no code gen)
template<typename T>
std::string toJSON(const T& obj) {
    std::string result = "{";
    bool first = true;
    template for (constexpr auto member : std::meta::nonstatic_data_members_of(^T)) {
        if (!first) result += ",";
        result += "\"" + std::string(std::meta::identifier_of(member)) + "\":";
        result += serialize(obj.[:member:]);  // Splice operator
        first = false;
    }
    return result + "}";
}
// toJSON(Config{"localhost", 8080, true})
// ? {"host":"localhost","port":8080,"useTLS":true}
// No boilerplate! Works for ANY struct!
```

**C++26 Contracts Deep Dive (P2900):**
```cpp
// Preconditions, postconditions, and assertions checked at runtime
int divide(int a, int b)
    pre(b != 0)                          // Precondition
    post(r: r * b == a || a % b != 0)    // Postcondition (r = return value)
{
    return a / b;
}

void processBuffer(std::span<int> buf, size_t idx)
    pre(idx < buf.size())               // Bounds check contract
    pre(!buf.empty())                    // Non-empty contract
{
    buf[idx] *= 2;
}

// Contract violation modes (configurable at build time):
// - enforce: Calls violation handler (default: abort)
// - observe: Log violation but continue (production monitoring)
// - ignore: Zero overhead (same as no contract -> release builds)
// 
// Key benefit: Contracts are part of the function signature,
// visible in documentation and IDE tooling
```

### Explanation:
| Feature | Impact | Your Domain |
|---------|--------|-------------|
| `std::expected` | Replaces error codes AND exceptions | Finance (error handling on hot path) |
| `std::mdspan` | Zero-copy multi-dim views | CAD (matrices), Gaming (texture data) |
| `std::flat_map` | Cache-friendly maps | Gaming (ECS), HFT (lookup tables) |
| `std::generator` | Easy lazy sequences | All (data processing pipelines) |
| Senders/Receivers | Standard async model | All (replaces ad-hoc thread pools) |
| Reflection | Auto-serialization, ORM | Enterprise (IBM), reducing boilerplate |
| Contracts | Eliminate ad-hoc assertions | All (API boundaries, safety-critical) |

---

## Q2: What build systems and package managers are current best practice for C++|

### Answer:

**Build Systems (2025-2026):**
| Tool | Status | Best For |
|------|--------|----------|
| CMake 3.28+ | Industry standard | Most projects, modules support |
| Bazel | Growing in enterprise | Large monorepos (Google-style) |
| Meson | Simpler alternative | Small-medium projects |
| build2 | Niche but modern | New projects wanting integrated tooling |

**Package Managers:**
| Tool | Status | Best For |
|------|--------|----------|
| vcpkg | Most popular (Microsoft) | Cross-platform, CMake integration |
| Conan 2.x | Strong enterprise | Complex dependency graphs, binary caching |
| CPM.cmake | Lightweight | CMake-only projects |

**Modern CMake Best Practices:**
```cmake
cmake_minimum_required(VERSION 3.28)
project(MyCADApp VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C++20 Modules support
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# vcpkg integration
find_package(fmt CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS asio)

add_library(geometry_core)
target_sources(geometry_core
    PUBLIC FILE_SET CXX_MODULES FILES
        src/geometry.cppm
        src/topology.cppm
)
target_link_libraries(geometry_core PRIVATE fmt::fmt)

# Sanitizers for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(geometry_core PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(geometry_core PRIVATE
        -fsanitize=address,undefined
    )
endif()
```

---

## Q3: What are Senders/Receivers (std::execution) and why do they matter|

### Answer:
```cpp
// std::execution (P2300) -> The future of C++ async
// Currently available via stdexec (reference implementation)
#include <stdexec/execution.hpp>

using namespace stdexec;

// Sender: describes async work (lazy, composable)
auto work = just(42)                          // Start with value 42
          | then([](int x) { return x * 2; }) // Transform
          | then([](int x) { return std::to_string(x); }); // Transform again

// Nothing has executed yet! Senders are lazy descriptions.

// Receiver: consumes the result
// Scheduler: decides WHERE to run (thread pool, GPU, etc.)
auto result = sync_wait(std::move(work));  // Execute and wait
// result = "84"

// Real-world: Async pipeline with error handling
auto pipeline = 
    schedule(threadPool.get_scheduler())   // Run on thread pool
  | then([] { return readFile("data.csv"); })
  | then([](auto data) { return parseCSV(data); })
  | then([](auto records) { return aggregate(records); })
  | upon_error([](auto err) { log("Pipeline failed: ", err); });

// Parallel fan-out
auto parallel = when_all(
    on(gpuScheduler, computePhysics()),
    on(cpuPool, computeAI()),
    on(ioScheduler, loadAssets())
);
```

### Explanation:
**Why this matters (replacing ad-hoc async):**
- **Composable**: Chain operations like ranges for data, but for async work
- **Scheduler-agnostic**: Same code runs on thread pool, GPU, or single-threaded
- **Structured concurrency**: Resources tied to scope, no leaked tasks
- **Zero allocation**: Small async operations don't need heap
- **Replaces**: `std::async` (broken), hand-rolled thread pools, Boost.Asio patterns

**Impact on your domains:**
- **Gaming**: GPU + CPU work orchestration without manual synchronization
- **Finance**: Market data processing pipelines
- **CAD**: Parallel geometry computation with structured cancellation

---

# Part B: Best Practices & Coding Standards

---

## Q4: What are the key principles of modern C++ coding standards|

### Answer:

**Core Guidelines (from ISO C++ Core Guidelines by Stroustrup & Sutter):**

```cpp
// 1. RAII everywhere -> no resource leaks possible
{
    auto file = std::ifstream("data.txt");  // Closes automatically
    auto conn = ConnectionPool::acquire();  // Returns automatically
    auto lock = std::scoped_lock(mutex);    // Unlocks automatically
}  // Everything cleaned up, even with exceptions

// 2. Prefer value semantics over pointer semantics
struct Point { double x, y, z; };
Point translate(Point p, Vector v) {  // Pass by value, return by value
    return {p.x + v.dx, p.y + v.dy, p.z + v.dz};
}

// 3. Use const correctly
void process(const std::vector<int>& data);  // Won't modify
int calculateArea() const;                    // Member function won't modify object
const auto& ref = getExpensiveObject();       // Avoid copy

// 4. Prefer composition over inheritance
class Car {
    Engine engine_;          // HAS-A (composition)
    Transmission trans_;     // HAS-A
    // NOT: class Car : public Vehicle, public Engine
};

// 5. Don't use raw new/delete
auto p = std::make_unique<Widget>();     // Not: new Widget()
auto v = std::vector<int>(1000);         // Not: new int[1000]
auto s = std::string("hello");           // Not: strdup/malloc

// 6. Use string_view for non-owning string parameters
void log(std::string_view message);  // No copy, accepts string, string_view, char*

// 7. Use span for non-owning array parameters
void process(std::span<const int> data);  // Accepts vector, array, C-array

// 8. Structured error handling
std::expected<Result, Error> tryOperation();  // C++23
std::optional<int> findValue(Key k);          // C++17 ? nullable return

// 9. Use enum class, not plain enum
enum class Color { RED, GREEN, BLUE };  // Scoped, type-safe
// NOT: enum Color { RED, GREEN, BLUE }; // Pollutes namespace

// 10. Mark functions noexcept when they don't throw
void swap(Widget& a, Widget& b) noexcept;
~Widget() noexcept;  // Destructors should ALWAYS be noexcept
```

---

## Q5: What testing strategies do you use for C++ projects|

### Answer:
```cpp
// Testing Pyramid for C++:
//
//        /\
//       /  \     E2E Tests (few, slow)
//      /    \    - Full system tests, integration with hardware/UI
//     /+----+\
//    /        \  Integration Tests (moderate)
//   /          \ - Component interaction, DB, network
//  /+----------+\
// /              \ Unit Tests (many, fast)
//  +------------+  - Pure logic, no I/O, fast (<1ms each)

// === Unit Testing with Google Test ===
#include <gtest/gtest.h>

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book;

    void SetUp() override {
        // Common setup
    }
};

TEST_F(OrderBookTest, MatchingBuyAndSellAtSamePrice) {
    book.addOrder({.side = Side::SELL, .price = 100.0, .quantity = 50});
    auto trades = book.addOrder({.side = Side::BUY, .price = 100.0, .quantity = 30});

    ASSERT_EQ(trades.size(), 1);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);
    EXPECT_EQ(trades[0].quantity, 30);
}

TEST_F(OrderBookTest, NoMatchWhenPricesDontCross) {
    book.addOrder({.side = Side::SELL, .price = 101.0, .quantity = 50});
    auto trades = book.addOrder({.side = Side::BUY, .price = 99.0, .quantity = 30});

    EXPECT_TRUE(trades.empty());
    EXPECT_DOUBLE_EQ(book.bestBid(), 99.0);
    EXPECT_DOUBLE_EQ(book.bestAsk(), 101.0);
}

// === Property-based testing (Fuzz testing) ===
// "For any sequence of valid orders, the book invariants hold"
TEST(OrderBookProperty, InvariantsMaintained) {
    OrderBook book;
    std::mt19937 rng(42);

    for (int i = 0; i < 10000; ++i) {
        Order order;
        order.side = (rng() % 2) ? Side::BUY : Side::SELL;
        order.price = 90.0 + (rng() % 2000) / 100.0;
        order.quantity = 1 + rng() % 1000;
        book.addOrder(order);

        // Invariant: bestBid < bestAsk (or one side is empty)
        if (book.bestBid() > 0 && book.bestAsk() > 0) {
            ASSERT_LT(book.bestBid(), book.bestAsk());
        }
    }
}

// === Benchmark tests ===
#include <benchmark/benchmark.h>

static void BM_OrderBookInsert(benchmark::State& state) {
    OrderBook book;
    for (auto _ : state) {
        Order order{.side = Side::BUY, .price = 100.0, .quantity = 100};
        benchmark::DoNotOptimize(book.addOrder(order));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBookInsert);
```

**CI/CD Pipeline:**
```yaml
# GitHub Actions / Jenkins pipeline
stages:
  - build:
      - cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZERS=ON
      - cmake --build build -j $(nproc)
  - unit_test:
      - ctest --test-dir build --output-on-failure
  - sanitizer_test:
      - ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build
  - fuzz_test:
      - ./build/fuzz_target -max_total_time=300
  - integration_test:
      - ./run_integration_tests.sh
  - benchmark:
      - ./build/benchmarks --benchmark_format=json > results.json
      - python compare_benchmarks.py results.json baseline.json
```

**Fuzz Testing (libFuzzer / AFL) ? finding bugs humans miss:**
```cpp
// Fuzz target: Find crashes in your parser
// Build: clang++ -fsanitize=fuzzer,address fuzz_target.cpp -o fuzz_target
// Run:   ./fuzz_target corpus/ -max_total_time=3600

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Feed random bytes to your parser
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        FIXMessage::parse(input);          // Test FIX parser
    } catch (...) {
        // Exceptions are OK -> crashes/UB are bugs
    }
    return 0;
}

// Structured fuzzing with protobuf (more targeted):
// Define valid input structure -> fuzzer generates valid-ish inputs
// Finds edge cases like:
// - Empty fields, max-length strings, integer overflow
// - Invalid UTF-8, null bytes in strings
// - Deeply nested structures, circular references
```

**Mutation Testing (assessing test quality):**
```
Concept: Modify (mutate) your source code, then run tests.
         If tests still pass -> tests are weak (mutation survived)
         If tests fail -> tests caught the bug (mutation killed)

Tools for C++:
  - Mull (https://github.com/mull-project/mull) -> LLVM-based
  - Dextool (D-lang based, works with C/C++)
  
Example mutations:
  Original:         if (x > 0) return true;
  Mutation 1:       if (x >= 0) return true;    // Boundary mutation
  Mutation 2:       if (x < 0) return true;     // Negate condition
  Mutation 3:       if (x > 0) return false;    // Return value mutation

Mutation Score = killed_mutants / total_mutants
  70-80% = OK, 80-90% = Good, 90%+ = Excellent
  
// Run: mull-runner --test-framework=gtest ./build/my_tests
```

**Testing Pyramid for C++ projects:**
```
           /  \           E2E / Integration Tests
          /    \          (slow, broad, fewer)
         /------\         
        / System  \       System Tests
       /   Tests   \      (medium speed)
      /-------------\     
     /  Integration  \    Component Integration
    /    Tests        \   (mock external deps)
   /-------------------\  
  /   Unit Tests        \ Unit Tests
 /     (fast, focused,   \(GTest/Catch2, mock with GMock)
/       many)              \
```

---

# Part C: Live Coding Questions

---

## Q6: Implement a thread-safe Singleton Logger with log rotation (Live Coding)

### Expected Solution:
```cpp
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <filesystem>
#include <format>

class Logger {
public:
    enum class Level { DEBUG, INFO, WARN, ERROR };

    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(Level level, std::string_view message,
             std::source_location loc = std::source_location::current()) {
        if (level < minLevel_) return;

        std::lock_guard lock(mutex_);

        auto now = std::chrono::system_clock::now();
        auto timeStr = std::format("{:%Y-%m-%d %H:%M:%S}", now);

        auto entry = std::format("[{}] [{}] {} ({}:{})\n",
            timeStr, levelToStr(level), message,
            loc.file_name(), loc.line());

        checkRotation();
        file_ << entry;
        file_.flush();
    }

    void setLevel(Level level) { minLevel_ = level; }
    void setMaxFileSize(size_t bytes) { maxFileSize_ = bytes; }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() : maxFileSize_(10 * 1024 * 1024) {
        openLogFile();
    }

    void openLogFile() {
        file_.open("app.log", std::ios::app);
        currentSize_ = std::filesystem::file_size("app.log");
    }

    void checkRotation() {
        if (currentSize_ >= maxFileSize_) {
            file_.close();
            auto timestamp = std::format("{:%Y%m%d_%H%M%S}",
                std::chrono::system_clock::now());
            std::filesystem::rename("app.log", "app." + timestamp + ".log");
            openLogFile();
            currentSize_ = 0;
        }
    }

    static constexpr const char* levelToStr(Level level) {
        switch (level) {
            case Level::DEBUG: return "DEBUG";
            case Level::INFO:  return "INFO ";
            case Level::WARN:  return "WARN ";
            case Level::ERROR: return "ERROR";
        }
        return "+---+";
    }

    std::ofstream file_;
    std::mutex mutex_;
    Level minLevel_ = Level::INFO;
    size_t maxFileSize_;
    size_t currentSize_ = 0;
};

// Usage
#define LOG_DEBUG(msg) Logger::instance().log(Logger::Level::DEBUG, msg)
#define LOG_INFO(msg)  Logger::instance().log(Logger::Level::INFO, msg)
#define LOG_ERROR(msg) Logger::instance().log(Logger::Level::ERROR, msg)
```

### Interviewer Evaluation Points:
- Thread safety (mutex on write path)
- Meyer's singleton (thread-safe in C++11+)
- Log rotation to prevent disk exhaustion
- `std::source_location` (C++20) for automatic file/line
- Proper RAII for file handles
- Level filtering before taking lock (optimization)

---

## Q7: Implement `std::shared_ptr` from scratch (Live Coding)

### Expected Solution:
```cpp
#include <atomic>
#include <utility>

template<typename T>
class SharedPtr {
    struct ControlBlock {
        std::atomic<int> refCount{1};
        std::atomic<int> weakCount{0};
    };

    T* ptr_ = nullptr;
    ControlBlock* ctrl_ = nullptr;

    void release() {
        if (ctrl_) {
            if (ctrl_->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete ptr_;
                if (ctrl_->weakCount.load(std::memory_order_acquire) == 0) {
                    delete ctrl_;
                }
            }
        }
        ptr_ = nullptr;
        ctrl_ = nullptr;
    }

public:
    SharedPtr() = default;

    explicit SharedPtr(T* ptr) : ptr_(ptr), ctrl_(new ControlBlock{}) {}

    // Copy -> increment ref count
    SharedPtr(const SharedPtr& other) : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        if (ctrl_) {
            ctrl_->refCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Move -> transfer ownership
    SharedPtr(SharedPtr&& other) noexcept
        : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        other.ptr_ = nullptr;
        other.ctrl_ = nullptr;
    }

    // Copy assignment
    SharedPtr& operator=(const SharedPtr& other) {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ctrl_ = other.ctrl_;
            if (ctrl_) {
                ctrl_->refCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }

    // Move assignment
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ctrl_ = other.ctrl_;
            other.ptr_ = nullptr;
            other.ctrl_ = nullptr;
        }
        return *this;
    }

    ~SharedPtr() { release(); }

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }

    int use_count() const {
        return ctrl_ ? ctrl_->refCount.load(std::memory_order_relaxed) : 0;
    }

    explicit operator bool() const { return ptr_ != nullptr; }

    void reset() { release(); }
    void reset(T* ptr) {
        release();
        ptr_ = ptr;
        ctrl_ = new ControlBlock{};
    }
};

// make_shared equivalent -> single allocation for object + control block
template<typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    // In production, allocate T and ControlBlock together
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}
```

### Interviewer Evaluation Points:
- Atomic reference counting (thread-safe)
- Proper copy/move semantics (Rule of 5)
- Self-assignment check
- `release()` handles deletion correctly
- Memory ordering awareness
- Knows `make_shared` optimization (single allocation)

---

## Q8: Design and implement a simple compile-time reflection system (Live Coding)

### Expected Solution:
```cpp
#include <string_view>
#include <tuple>
#include <iostream>

// Macro-based reflection (until C++26 static reflection)
#define REFLECT(Type, ...) \
    static constexpr auto reflectedFields() { \
        return std::make_tuple(__VA_ARGS__); \
    } \
    static constexpr std::string_view typeName() { return #Type; }

#define FIELD(name) \
    std::pair{std::string_view{#name}, &std::remove_pointer_t<decltype(this)>::name}

// Usage
struct Person {
    std::string name;
    int age;
    double salary;

    REFLECT(Person, FIELD(name), FIELD(age), FIELD(salary))
};

// Generic serializer using reflection
template<typename T>
void toJSON(const T& obj, std::ostream& out) {
    out << "{ ";
    bool first = true;

    std::apply([&](auto&&... fields) {
        ((
            out << (first -> "" : ", "),
            out << "\"" << fields.first << "\": ",
            printValue(out, obj.*(fields.second)),
            first = false
        ), ...);
    }, T::reflectedFields());

    out << " }";
}

template<typename T>
void printValue(std::ostream& out, const T& val) {
    if constexpr (std::is_same_v<T, std::string>) {
        out << "\"" << val << "\"";
    } else {
        out << val;
    }
}

// Usage
void example() {
    Person p{"Alice", 30, 95000.0};
    toJSON(p, std::cout);
    // Output: { "name": "Alice", "age": 30, "salary": 95000 }
}
```

### Explanation:
- Uses **fold expressions** and **`std::apply`** for iteration over tuple
- **`if constexpr`** for type-specific serialization
- **Pointer-to-member** (`&Person::name`) for field access
- **C++26 will have real reflection** | this pattern will be replaced by `std::meta::members_of(^Person)`

---

## Q9: Implement a simple coroutine-based task system (Live Coding)

### Expected Solution:
```cpp
#include <coroutine>
#include <optional>
#include <iostream>
#include <queue>
#include <functional>

// Simple Task coroutine
struct Task {
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() { if (handle) handle.destroy(); }
    Task(Task&& t) noexcept : handle(t.handle) { t.handle = nullptr; }
    Task& operator=(Task&&) = delete;
};

// Scheduler
class Scheduler {
    std::queue<std::coroutine_handle<>> ready_;

public:
    // Custom awaiter that yields to scheduler
    auto suspend() {
        struct Awaiter {
            Scheduler& sched;
            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<> h) {
                sched.ready_.push(h);  // Re-enqueue
            }
            void await_resume() {}
        };
        return Awaiter{*this};
    }

    void schedule(std::coroutine_handle<> handle) {
        ready_.push(handle);
    }

    void run() {
        while (!ready_.empty()) {
            auto handle = ready_.front();
            ready_.pop();
            if (!handle.done()) {
                handle.resume();
            }
        }
    }
};

// Usage
Scheduler scheduler;

Task taskA() {
    std::cout << "A: step 1\n";
    co_await scheduler.suspend();
    std::cout << "A: step 2\n";
    co_await scheduler.suspend();
    std::cout << "A: step 3\n";
}

Task taskB() {
    std::cout << "B: step 1\n";
    co_await scheduler.suspend();
    std::cout << "B: step 2\n";
}
```

---

# ENHANCED SECTION: Industry Trends 2025-2026

> *What senior architects and principal engineers are actually discussing and adopting in production.*

---

## Q6: How is Rust interop changing the C++ ecosystem| When should you consider Rust|

### Answer:
```cpp
// C++ calling Rust via C FFI (extern "C")
// Rust side:
// #[no_mangle]
// pub extern "C" fn rust_parse_json(input: *const c_char, len: usize) -> *mut JsonDoc

// C++ side:
extern "C" {
    void* rust_parse_json(const char* input, size_t len);
    void rust_free_json(void* doc);
}

// Modern approach: cxx bridge (type-safe, no unsafe FFI)
// Generates both C++ and Rust code from a shared definition
```

**When to use Rust vs C++:**
| Factor | Stay C++ | Consider Rust |
|--------|----------|---------------|
| Existing codebase | Large C++ codebase (iCluster: 500K+ lines) | Greenfield project |
| Team expertise | Team knows C++ deeply | Team willing to learn |
| Memory safety | Using sanitizers + static analysis | Need compile-time guarantees |
| Libraries needed | Mature C++ ecosystem (Boost, Qt) | cargo ecosystem sufficient |
| Platform | IBM i, embedded, legacy OS | Linux/macOS/Windows |
| Hiring | More C++ developers available | Growing Rust talent pool |

### Explanation:
**Senior perspective**: The question isn't "C++ or Rust" | it's "where does each fit in our stack?" Production systems increasingly use both: Rust for new security-sensitive components (parsers, network handling), C++ for existing engines (CAD kernels, game engines, trading systems). The iCluster communication layer (DMKAPI) handles encryption and protocol parsing | these would benefit from Rust's memory safety if starting fresh.

---

## Q7: Explain Observability (Metrics, Logs, Traces) for C++ systems.

### Answer:
```
The Three Pillars:

1. METRICS (quantitative -> Prometheus, Grafana)
   counter: requests_total, errors_total
   gauge: active_connections, queue_depth  
   histogram: request_latency_ms (p50, p95, p99)
   
   iCluster metrics: apply_latency, journal_position_lag,
                     staging_store_depth, link_check_failures

2. LOGS (qualitative -> structured JSON, ELK stack)
   {"timestamp":"...","level":"ERROR","component":"OMMIRROR",
    "group":"PAYROLL","message":"Send failed","retry":3}
   
   iCluster uses DMKLOG with severity levels and event forwarding

3. TRACES (request flow -> OpenTelemetry, Jaeger)
   Trace ID propagated: Source -> Network -> Target
   Span: "SAVOBJ" (200ms) -> "NetworkSend" (50ms) -> "RSTOBJ" (300ms)
   
   iCluster uses conversation tags (conv_slot) and packet counters
   as correlation IDs -> identical concept to trace IDs
```

### Explanation:
Modern observability answers three questions: "What's broken?" (metrics), "Why?" (logs), "Where in the flow?" (traces). iCluster already implements all three, just with 1990s terminology. Understanding the mapping between iCluster's monitoring and modern observability helps explain your experience to interviewers at cloud-native companies.

---

## Q8: What is Platform Engineering and how does it affect C++ system design|

### Answer:
```
Platform Engineering = Building internal developer platforms (IDPs)
that make application teams self-service

For C++ teams:
1. BUILD PLATFORM
   - Standardized CMake templates, vcpkg manifests
   - Remote build caching (Bazel remote cache, ccache)
   - CI/CD pipelines with sanitizers + static analysis
   
2. DEPLOY PLATFORM  
   - Containerized C++ services (minimal Alpine + static binary)
   - Feature flags for runtime configuration
   - Canary deployments with automatic rollback
   
3. OBSERVE PLATFORM
   - Standardized metrics library (OpenTelemetry C++ SDK)
   - Centralized logging with structured format
   - Distributed tracing for multi-service architectures

4. TEST PLATFORM
   - Fuzz testing infrastructure (OSS-Fuzz integration)
   - Performance regression detection (automated benchmarks)
   - Chaos engineering for resilience testing
```

### Explanation:
**Senior architect note**: Platform engineering is how you scale engineering practices across hundreds of developers. Instead of each team reinventing CI/CD, build systems, and observability, a platform team provides golden paths. For iCluster, the build system (BLDDMKPGM*.CLLE files) IS a platform | it standardizes how every module is compiled, linked, and deployed.

---
void example() {
    auto a = taskA();
    auto b = taskB();
    scheduler.schedule(a.handle);
    scheduler.schedule(b.handle);
    scheduler.run();
    // Output (interleaved):
    // A: step 1
    // B: step 1
    // A: step 2
    // B: step 2
    // A: step 3
}
```

### Explanation:
- **Cooperative multitasking** without threads
- **Custom awaiter** controls suspension/resumption behavior
- **Scheduler** maintains a ready queue of coroutine handles
- **Use cases**: Game loop (entity updates), async I/O event loops, state machines

---

## Q10: Quick-fire coding questions (expect these as warm-up or final round)

### Q10a: Implement `string_to_int` (atoi) handling edge cases
```cpp
#include <string_view>
#include <optional>
#include <limits>

std::optional<int> stringToInt(std::string_view str) {
    if (str.empty()) return std::nullopt;

    size_t i = 0;
    // Skip whitespace
    while (i < str.size() && str[i] == ' ') ++i;
    if (i == str.size()) return std::nullopt;

    // Handle sign
    bool negative = false;
    if (str[i] == '-' || str[i] == '+') {
        negative = (str[i] == '-');
        ++i;
    }

    if (i == str.size() || !std::isdigit(str[i]))
        return std::nullopt;

    long long result = 0;
    while (i < str.size() && std::isdigit(str[i])) {
        result = result * 10 + (str[i] - '0');
        // Overflow check
        if (!negative && result > std::numeric_limits<int>::max())
            return std::nullopt;
        if (negative && -result < std::numeric_limits<int>::min())
            return std::nullopt;
        ++i;
    }

    return static_cast<int>(negative ? -result : result);
}
```

### Q10b: Reverse words in a string in-place
```cpp
void reverseWords(std::string& s) {
    // Step 1: Reverse entire string
    std::reverse(s.begin(), s.end());

    // Step 2: Reverse each word
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == ' ') {
            std::reverse(s.begin() + start, s.begin() + i);
            start = i + 1;
        }
    }

    // Step 3: Clean up extra spaces (if needed)
    // "hello world" | "world hello"
}
```

### Q10c: Check if a binary tree is balanced
```cpp
struct TreeNode {
    int val;
    TreeNode* left;
    TreeNode* right;
};

bool isBalanced(TreeNode* root) {
    return height(root) != -1;
}

int height(TreeNode* node) {
    if (!node) return 0;
    int left = height(node->left);
    if (left == -1) return -1;
    int right = height(node->right);
    if (right == -1) return -1;
    if (std::abs(left - right) > 1) return -1;
    return 1 + std::max(left, right);
}
// O(n) time, O(h) space | single pass, not the naive O(n²) approach
```

### Q10d: Detect cycle in linked list and find the start
```cpp
ListNode* detectCycle(ListNode* head) {
    ListNode* slow = head;
    ListNode* fast = head;

    // Phase 1: Detect cycle
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) break;
    }

    if (!fast || !fast->next) return nullptr;  // No cycle

    // Phase 2: Find cycle start
    slow = head;
    while (slow != fast) {
        slow = slow->next;
        fast = fast->next;
    }
    return slow;  // Start of cycle
}
// Floyd's algorithm | O(n) time, O(1) space
```

### Q10e: Implement a moving average calculator
```cpp
class MovingAverage {
    std::queue<double> window_;
    int maxSize_;
    double sum_ = 0;

public:
    MovingAverage(int size) : maxSize_(size) {}

    double next(double val) {
        window_.push(val);
        sum_ += val;
        if (static_cast<int>(window_.size()) > maxSize_) {
            sum_ -= window_.front();
            window_.pop();
        }
        return sum_ / window_.size();
    }
};
// O(1) per update | used in finance for technical analysis (SMA)
```

---

# Summary: Interview Preparation Checklist

```
| Modern C++ (C++17/20/23) | concepts, ranges, coroutines, modules
| Data Structures & Algorithms | implement from scratch, complexity analysis
| Design Patterns | Strategy, Observer, Factory, Command, CRTP, Type Erasure
| LLD | Parking Lot, Chess, Elevator, File System, Rate Limiter
| HLD | CAD collaboration, Trading system, Game server, Build system
| Multithreading | Memory model, thread pool, lock-free, atomics
| Memory & Performance | Allocators, cache optimization, SIMD, profiling
| Domain Knowledge | CAD geometry, Game ECS/loop, Finance FIX/OrderBook
| Projects | 2-3 deep stories with STAR-T framework, quantified results
| Behavioral | Disagreements, mentoring, incidents, estimation
| Latest trends | C++23 features, build tools, Senders/Receivers
| Live coding | Practice timed implementation (45-60 min problems)
```

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    \
//    /        \  Integration Tests (moderate)
//   /          \ - Component interaction, DB, network
//  / 
        $match = # Set 10: Latest Trends, Best Practices & Live Coding
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: Latest C++ Trends & Technologies (2024-2026)

---

## Q1: What's new in C++23 and what's coming in C++26?

### Answer:

**C++23 Key Features:**
```cpp
// 1. std::expected | Better error handling than exceptions
#include <expected>
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) return std::unexpected("Division by zero");
    return a / b;
}
auto result = divide(10, 3);
if (result) std::cout << *result;  // 3
else std::cout << result.error();  // "Division by zero"

// 2. std::print / std::println | Type-safe formatting
#include <print>
std::println("Hello, {}! You are {} years old.", name, age);
// Replaces: printf (unsafe), cout (verbose), fmt::print (external)

// 3. std::generator | Coroutine-based generator
#include <generator>
std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a;
        a = b;
        b = temp + b;
    }
}
for (int n : fibonacci() | std::views::take(10))
    std::print("{} ", n);  // 0 1 1 2 3 5 8 13 21 34

// 4. std::mdspan | Multi-dimensional array view
#include <mdspan>
std::vector<double> data(rows * cols);
std::mdspan<double, std::dextents<size_t, 2>> matrix(data.data(), rows, cols);
matrix[2, 3] = 42.0;  // Access like 2D array, data is contiguous

// 5. std::flat_map / std::flat_set | Cache-friendly sorted containers
#include <flat_map>
std::flat_map<std::string, int> scores;  // Backed by sorted vectors
scores["Alice"] = 95;  // Cache-friendly iteration, slower insert

// 6. Deducing this (Explicit object parameter)
struct Widget {
    template<typename Self>
    auto& value(this Self&& self) {
        return std::forward<Self>(self).value_;
    }
    // Replaces 4 overloads: const&, &, const&&, &&
};

// 7. std::stacktrace | Programmatic stack traces
#include <stacktrace>
void crashHandler() {
    auto trace = std::stacktrace::current();
    std::println("{}", std::to_string(trace));
}

// 8. Ranges improvements: ranges::to, zip, chunk, slide
auto vec = std::views::iota(1, 10)
         | std::views::filter([](int x) { return x % 2 == 0; })
         | std::ranges::to<std::vector>();  // Materialize range to container

auto pairs = std::views::zip(names, ages);  // Iterate two ranges together
```

**C++26 (Expected/In Progress):**
```
- std::execution (Senders/Receivers) | Standard async framework
- Reflection (compile-time introspection)
- Pattern matching
- Contracts (preconditions, postconditions, assertions)
- std::inplace_vector (fixed-capacity, stack-allocated vector)
- Improved coroutine library support
- std::hive (formerly colony) | pool-like container
```

**C++26 Reflection Deep Dive (P2996 ? approved for C++26):**
```cpp
// Compile-time inspection of types, members, and functions
#include <meta>

struct Config {
    std::string host;
    int port;
    bool useTLS;
};

// Auto-generate JSON serialization via reflection (no macros, no code gen)
template<typename T>
std::string toJSON(const T& obj) {
    std::string result = "{";
    bool first = true;
    template for (constexpr auto member : std::meta::nonstatic_data_members_of(^T)) {
        if (!first) result += ",";
        result += "\"" + std::string(std::meta::identifier_of(member)) + "\":";
        result += serialize(obj.[:member:]);  // Splice operator
        first = false;
    }
    return result + "}";
}
// toJSON(Config{"localhost", 8080, true})
// | {"host":"localhost","port":8080,"useTLS":true}
// No boilerplate! Works for ANY struct!
```

**C++26 Contracts Deep Dive (P2900):**
```cpp
// Preconditions, postconditions, and assertions checked at runtime
int divide(int a, int b)
    pre(b != 0)                          // Precondition
    post(r: r * b == a || a % b != 0)    // Postcondition (r = return value)
{
    return a / b;
}

void processBuffer(std::span<int> buf, size_t idx)
    pre(idx < buf.size())               // Bounds check contract
    pre(!buf.empty())                    // Non-empty contract
{
    buf[idx] *= 2;
}

// Contract violation modes (configurable at build time):
// - enforce: Calls violation handler (default: abort)
// - observe: Log violation but continue (production monitoring)
// - ignore: Zero overhead (same as no contract | release builds)
// 
// Key benefit: Contracts are part of the function signature,
// visible in documentation and IDE tooling
```

### Explanation:
| Feature | Impact | Your Domain |
|---------|--------|-------------|
| `std::expected` | Replaces error codes AND exceptions | Finance (error handling on hot path) |
| `std::mdspan` | Zero-copy multi-dim views | CAD (matrices), Gaming (texture data) |
| `std::flat_map` | Cache-friendly maps | Gaming (ECS), HFT (lookup tables) |
| `std::generator` | Easy lazy sequences | All (data processing pipelines) |
| Senders/Receivers | Standard async model | All (replaces ad-hoc thread pools) |
| Reflection | Auto-serialization, ORM | Enterprise (IBM), reducing boilerplate |
| Contracts | Eliminate ad-hoc assertions | All (API boundaries, safety-critical) |

---

## Q2: What build systems and package managers are current best practice for C++?

### Answer:

**Build Systems (2025-2026):**
| Tool | Status | Best For |
|------|--------|----------|
| CMake 3.28+ | Industry standard | Most projects, modules support |
| Bazel | Growing in enterprise | Large monorepos (Google-style) |
| Meson | Simpler alternative | Small-medium projects |
| build2 | Niche but modern | New projects wanting integrated tooling |

**Package Managers:**
| Tool | Status | Best For |
|------|--------|----------|
| vcpkg | Most popular (Microsoft) | Cross-platform, CMake integration |
| Conan 2.x | Strong enterprise | Complex dependency graphs, binary caching |
| CPM.cmake | Lightweight | CMake-only projects |

**Modern CMake Best Practices:**
```cmake
cmake_minimum_required(VERSION 3.28)
project(MyCADApp VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C++20 Modules support
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# vcpkg integration
find_package(fmt CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS asio)

add_library(geometry_core)
target_sources(geometry_core
    PUBLIC FILE_SET CXX_MODULES FILES
        src/geometry.cppm
        src/topology.cppm
)
target_link_libraries(geometry_core PRIVATE fmt::fmt)

# Sanitizers for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(geometry_core PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(geometry_core PRIVATE
        -fsanitize=address,undefined
    )
endif()
```

---

## Q3: What are Senders/Receivers (std::execution) and why do they matter?

### Answer:
```cpp
// std::execution (P2300) | The future of C++ async
// Currently available via stdexec (reference implementation)
#include <stdexec/execution.hpp>

using namespace stdexec;

// Sender: describes async work (lazy, composable)
auto work = just(42)                          // Start with value 42
          | then([](int x) { return x * 2; }) // Transform
          | then([](int x) { return std::to_string(x); }); // Transform again

// Nothing has executed yet! Senders are lazy descriptions.

// Receiver: consumes the result
// Scheduler: decides WHERE to run (thread pool, GPU, etc.)
auto result = sync_wait(std::move(work));  // Execute and wait
// result = "84"

// Real-world: Async pipeline with error handling
auto pipeline = 
    schedule(threadPool.get_scheduler())   // Run on thread pool
  | then([] { return readFile("data.csv"); })
  | then([](auto data) { return parseCSV(data); })
  | then([](auto records) { return aggregate(records); })
  | upon_error([](auto err) { log("Pipeline failed: ", err); });

// Parallel fan-out
auto parallel = when_all(
    on(gpuScheduler, computePhysics()),
    on(cpuPool, computeAI()),
    on(ioScheduler, loadAssets())
);
```

### Explanation:
**Why this matters (replacing ad-hoc async):**
- **Composable**: Chain operations like ranges for data, but for async work
- **Scheduler-agnostic**: Same code runs on thread pool, GPU, or single-threaded
- **Structured concurrency**: Resources tied to scope, no leaked tasks
- **Zero allocation**: Small async operations don't need heap
- **Replaces**: `std::async` (broken), hand-rolled thread pools, Boost.Asio patterns

**Impact on your domains:**
- **Gaming**: GPU + CPU work orchestration without manual synchronization
- **Finance**: Market data processing pipelines
- **CAD**: Parallel geometry computation with structured cancellation

---

# Part B: Best Practices & Coding Standards

---

## Q4: What are the key principles of modern C++ coding standards?

### Answer:

**Core Guidelines (from ISO C++ Core Guidelines by Stroustrup & Sutter):**

```cpp
// 1. RAII everywhere | no resource leaks possible
{
    auto file = std::ifstream("data.txt");  // Closes automatically
    auto conn = ConnectionPool::acquire();  // Returns automatically
    auto lock = std::scoped_lock(mutex);    // Unlocks automatically
}  // Everything cleaned up, even with exceptions

// 2. Prefer value semantics over pointer semantics
struct Point { double x, y, z; };
Point translate(Point p, Vector v) {  // Pass by value, return by value
    return {p.x + v.dx, p.y + v.dy, p.z + v.dz};
}

// 3. Use const correctly
void process(const std::vector<int>& data);  // Won't modify
int calculateArea() const;                    // Member function won't modify object
const auto& ref = getExpensiveObject();       // Avoid copy

// 4. Prefer composition over inheritance
class Car {
    Engine engine_;          // HAS-A (composition)
    Transmission trans_;     // HAS-A
    // NOT: class Car : public Vehicle, public Engine
};

// 5. Don't use raw new/delete
auto p = std::make_unique<Widget>();     // Not: new Widget()
auto v = std::vector<int>(1000);         // Not: new int[1000]
auto s = std::string("hello");           // Not: strdup/malloc

// 6. Use string_view for non-owning string parameters
void log(std::string_view message);  // No copy, accepts string, string_view, char*

// 7. Use span for non-owning array parameters
void process(std::span<const int> data);  // Accepts vector, array, C-array

// 8. Structured error handling
std::expected<Result, Error> tryOperation();  // C++23
std::optional<int> findValue(Key k);          // C++17 | nullable return

// 9. Use enum class, not plain enum
enum class Color { RED, GREEN, BLUE };  // Scoped, type-safe
// NOT: enum Color { RED, GREEN, BLUE }; // Pollutes namespace

// 10. Mark functions noexcept when they don't throw
void swap(Widget& a, Widget& b) noexcept;
~Widget() noexcept;  // Destructors should ALWAYS be noexcept
```

---

## Q5: What testing strategies do you use for C++ projects?

### Answer:
```cpp
// Testing Pyramid for C++:
//
//        /\
//       /  \     E2E Tests (few, slow)
//      /    \    - Full system tests, integration with hardware/UI
//     /+----+\
//    /        \  Integration Tests (moderate)
//   /          \ - Component interaction, DB, network
//  /+----------+\
// /              \ Unit Tests (many, fast)
//  +------------+  - Pure logic, no I/O, fast (<1ms each)

// === Unit Testing with Google Test ===
#include <gtest/gtest.h>

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book;

    void SetUp() override {
        // Common setup
    }
};

TEST_F(OrderBookTest, MatchingBuyAndSellAtSamePrice) {
    book.addOrder({.side = Side::SELL, .price = 100.0, .quantity = 50});
    auto trades = book.addOrder({.side = Side::BUY, .price = 100.0, .quantity = 30});

    ASSERT_EQ(trades.size(), 1);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);
    EXPECT_EQ(trades[0].quantity, 30);
}

TEST_F(OrderBookTest, NoMatchWhenPricesDontCross) {
    book.addOrder({.side = Side::SELL, .price = 101.0, .quantity = 50});
    auto trades = book.addOrder({.side = Side::BUY, .price = 99.0, .quantity = 30});

    EXPECT_TRUE(trades.empty());
    EXPECT_DOUBLE_EQ(book.bestBid(), 99.0);
    EXPECT_DOUBLE_EQ(book.bestAsk(), 101.0);
}

// === Property-based testing (Fuzz testing) ===
// "For any sequence of valid orders, the book invariants hold"
TEST(OrderBookProperty, InvariantsMaintained) {
    OrderBook book;
    std::mt19937 rng(42);

    for (int i = 0; i < 10000; ++i) {
        Order order;
        order.side = (rng() % 2) ? Side::BUY : Side::SELL;
        order.price = 90.0 + (rng() % 2000) / 100.0;
        order.quantity = 1 + rng() % 1000;
        book.addOrder(order);

        // Invariant: bestBid < bestAsk (or one side is empty)
        if (book.bestBid() > 0 && book.bestAsk() > 0) {
            ASSERT_LT(book.bestBid(), book.bestAsk());
        }
    }
}

// === Benchmark tests ===
#include <benchmark/benchmark.h>

static void BM_OrderBookInsert(benchmark::State& state) {
    OrderBook book;
    for (auto _ : state) {
        Order order{.side = Side::BUY, .price = 100.0, .quantity = 100};
        benchmark::DoNotOptimize(book.addOrder(order));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBookInsert);
```

**CI/CD Pipeline:**
```yaml
# GitHub Actions / Jenkins pipeline
stages:
  - build:
      - cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZERS=ON
      - cmake --build build -j $(nproc)
  - unit_test:
      - ctest --test-dir build --output-on-failure
  - sanitizer_test:
      - ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build
  - fuzz_test:
      - ./build/fuzz_target -max_total_time=300
  - integration_test:
      - ./run_integration_tests.sh
  - benchmark:
      - ./build/benchmarks --benchmark_format=json > results.json
      - python compare_benchmarks.py results.json baseline.json
```

**Fuzz Testing (libFuzzer / AFL) ? finding bugs humans miss:**
```cpp
// Fuzz target: Find crashes in your parser
// Build: clang++ -fsanitize=fuzzer,address fuzz_target.cpp -o fuzz_target
// Run:   ./fuzz_target corpus/ -max_total_time=3600

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Feed random bytes to your parser
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        FIXMessage::parse(input);          // Test FIX parser
    } catch (...) {
        // Exceptions are OK | crashes/UB are bugs
    }
    return 0;
}

// Structured fuzzing with protobuf (more targeted):
// Define valid input structure | fuzzer generates valid-ish inputs
// Finds edge cases like:
// - Empty fields, max-length strings, integer overflow
// - Invalid UTF-8, null bytes in strings
// - Deeply nested structures, circular references
```

**Mutation Testing (assessing test quality):**
```
Concept: Modify (mutate) your source code, then run tests.
         If tests still pass | tests are weak (mutation survived)
         If tests fail | tests caught the bug (mutation killed)

Tools for C++:
  - Mull (https://github.com/mull-project/mull) | LLVM-based
  - Dextool (D-lang based, works with C/C++)
  
Example mutations:
  Original:         if (x > 0) return true;
  Mutation 1:       if (x >= 0) return true;    // Boundary mutation
  Mutation 2:       if (x < 0) return true;     // Negate condition
  Mutation 3:       if (x > 0) return false;    // Return value mutation

Mutation Score = killed_mutants / total_mutants
  70-80% = OK, 80-90% = Good, 90%+ = Excellent
  
// Run: mull-runner --test-framework=gtest ./build/my_tests
```

**Testing Pyramid for C++ projects:**
```
           /  \           E2E / Integration Tests
          /    \          (slow, broad, fewer)
         /------\         
        / System  \       System Tests
       /   Tests   \      (medium speed)
      /-------------\     
     /  Integration  \    Component Integration
    /    Tests        \   (mock external deps)
   /-------------------\  
  /   Unit Tests        \ Unit Tests
 /     (fast, focused,   \(GTest/Catch2, mock with GMock)
/       many)              \
```

---

# Part C: Live Coding Questions

---

## Q6: Implement a thread-safe Singleton Logger with log rotation (Live Coding)

### Expected Solution:
```cpp
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <filesystem>
#include <format>

class Logger {
public:
    enum class Level { DEBUG, INFO, WARN, ERROR };

    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(Level level, std::string_view message,
             std::source_location loc = std::source_location::current()) {
        if (level < minLevel_) return;

        std::lock_guard lock(mutex_);

        auto now = std::chrono::system_clock::now();
        auto timeStr = std::format("{:%Y-%m-%d %H:%M:%S}", now);

        auto entry = std::format("[{}] [{}] {} ({}:{})\n",
            timeStr, levelToStr(level), message,
            loc.file_name(), loc.line());

        checkRotation();
        file_ << entry;
        file_.flush();
    }

    void setLevel(Level level) { minLevel_ = level; }
    void setMaxFileSize(size_t bytes) { maxFileSize_ = bytes; }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() : maxFileSize_(10 * 1024 * 1024) {
        openLogFile();
    }

    void openLogFile() {
        file_.open("app.log", std::ios::app);
        currentSize_ = std::filesystem::file_size("app.log");
    }

    void checkRotation() {
        if (currentSize_ >= maxFileSize_) {
            file_.close();
            auto timestamp = std::format("{:%Y%m%d_%H%M%S}",
                std::chrono::system_clock::now());
            std::filesystem::rename("app.log", "app." + timestamp + ".log");
            openLogFile();
            currentSize_ = 0;
        }
    }

    static constexpr const char* levelToStr(Level level) {
        switch (level) {
            case Level::DEBUG: return "DEBUG";
            case Level::INFO:  return "INFO ";
            case Level::WARN:  return "WARN ";
            case Level::ERROR: return "ERROR";
        }
        return "+---+";
    }

    std::ofstream file_;
    std::mutex mutex_;
    Level minLevel_ = Level::INFO;
    size_t maxFileSize_;
    size_t currentSize_ = 0;
};

// Usage
#define LOG_DEBUG(msg) Logger::instance().log(Logger::Level::DEBUG, msg)
#define LOG_INFO(msg)  Logger::instance().log(Logger::Level::INFO, msg)
#define LOG_ERROR(msg) Logger::instance().log(Logger::Level::ERROR, msg)
```

### Interviewer Evaluation Points:
- Thread safety (mutex on write path)
- Meyer's singleton (thread-safe in C++11+)
- Log rotation to prevent disk exhaustion
- `std::source_location` (C++20) for automatic file/line
- Proper RAII for file handles
- Level filtering before taking lock (optimization)

---

## Q7: Implement `std::shared_ptr` from scratch (Live Coding)

### Expected Solution:
```cpp
#include <atomic>
#include <utility>

template<typename T>
class SharedPtr {
    struct ControlBlock {
        std::atomic<int> refCount{1};
        std::atomic<int> weakCount{0};
    };

    T* ptr_ = nullptr;
    ControlBlock* ctrl_ = nullptr;

    void release() {
        if (ctrl_) {
            if (ctrl_->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete ptr_;
                if (ctrl_->weakCount.load(std::memory_order_acquire) == 0) {
                    delete ctrl_;
                }
            }
        }
        ptr_ = nullptr;
        ctrl_ = nullptr;
    }

public:
    SharedPtr() = default;

    explicit SharedPtr(T* ptr) : ptr_(ptr), ctrl_(new ControlBlock{}) {}

    // Copy | increment ref count
    SharedPtr(const SharedPtr& other) : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        if (ctrl_) {
            ctrl_->refCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Move | transfer ownership
    SharedPtr(SharedPtr&& other) noexcept
        : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        other.ptr_ = nullptr;
        other.ctrl_ = nullptr;
    }

    // Copy assignment
    SharedPtr& operator=(const SharedPtr& other) {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ctrl_ = other.ctrl_;
            if (ctrl_) {
                ctrl_->refCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }

    // Move assignment
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ctrl_ = other.ctrl_;
            other.ptr_ = nullptr;
            other.ctrl_ = nullptr;
        }
        return *this;
    }

    ~SharedPtr() { release(); }

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }

    int use_count() const {
        return ctrl_ ? ctrl_->refCount.load(std::memory_order_relaxed) : 0;
    }

    explicit operator bool() const { return ptr_ != nullptr; }

    void reset() { release(); }
    void reset(T* ptr) {
        release();
        ptr_ = ptr;
        ctrl_ = new ControlBlock{};
    }
};

// make_shared equivalent | single allocation for object + control block
template<typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    // In production, allocate T and ControlBlock together
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}
```

### Interviewer Evaluation Points:
- Atomic reference counting (thread-safe)
- Proper copy/move semantics (Rule of 5)
- Self-assignment check
- `release()` handles deletion correctly
- Memory ordering awareness
- Knows `make_shared` optimization (single allocation)

---

## Q8: Design and implement a simple compile-time reflection system (Live Coding)

### Expected Solution:
```cpp
#include <string_view>
#include <tuple>
#include <iostream>

// Macro-based reflection (until C++26 static reflection)
#define REFLECT(Type, ...) \
    static constexpr auto reflectedFields() { \
        return std::make_tuple(__VA_ARGS__); \
    } \
    static constexpr std::string_view typeName() { return #Type; }

#define FIELD(name) \
    std::pair{std::string_view{#name}, &std::remove_pointer_t<decltype(this)>::name}

// Usage
struct Person {
    std::string name;
    int age;
    double salary;

    REFLECT(Person, FIELD(name), FIELD(age), FIELD(salary))
};

// Generic serializer using reflection
template<typename T>
void toJSON(const T& obj, std::ostream& out) {
    out << "{ ";
    bool first = true;

    std::apply([&](auto&&... fields) {
        ((
            out << (first ? "" : ", "),
            out << "\"" << fields.first << "\": ",
            printValue(out, obj.*(fields.second)),
            first = false
        ), ...);
    }, T::reflectedFields());

    out << " }";
}

template<typename T>
void printValue(std::ostream& out, const T& val) {
    if constexpr (std::is_same_v<T, std::string>) {
        out << "\"" << val << "\"";
    } else {
        out << val;
    }
}

// Usage
void example() {
    Person p{"Alice", 30, 95000.0};
    toJSON(p, std::cout);
    // Output: { "name": "Alice", "age": 30, "salary": 95000 }
}
```

### Explanation:
- Uses **fold expressions** and **`std::apply`** for iteration over tuple
- **`if constexpr`** for type-specific serialization
- **Pointer-to-member** (`&Person::name`) for field access
- **C++26 will have real reflection** | this pattern will be replaced by `std::meta::members_of(^Person)`

---

## Q9: Implement a simple coroutine-based task system (Live Coding)

### Expected Solution:
```cpp
#include <coroutine>
#include <optional>
#include <iostream>
#include <queue>
#include <functional>

// Simple Task coroutine
struct Task {
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() { if (handle) handle.destroy(); }
    Task(Task&& t) noexcept : handle(t.handle) { t.handle = nullptr; }
    Task& operator=(Task&&) = delete;
};

// Scheduler
class Scheduler {
    std::queue<std::coroutine_handle<>> ready_;

public:
    // Custom awaiter that yields to scheduler
    auto suspend() {
        struct Awaiter {
            Scheduler& sched;
            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<> h) {
                sched.ready_.push(h);  // Re-enqueue
            }
            void await_resume() {}
        };
        return Awaiter{*this};
    }

    void schedule(std::coroutine_handle<> handle) {
        ready_.push(handle);
    }

    void run() {
        while (!ready_.empty()) {
            auto handle = ready_.front();
            ready_.pop();
            if (!handle.done()) {
                handle.resume();
            }
        }
    }
};

// Usage
Scheduler scheduler;

Task taskA() {
    std::cout << "A: step 1\n";
    co_await scheduler.suspend();
    std::cout << "A: step 2\n";
    co_await scheduler.suspend();
    std::cout << "A: step 3\n";
}

Task taskB() {
    std::cout << "B: step 1\n";
    co_await scheduler.suspend();
    std::cout << "B: step 2\n";
}
```

---

# ENHANCED SECTION: Industry Trends 2025-2026

> *What senior architects and principal engineers are actually discussing and adopting in production.*

---

## Q6: How is Rust interop changing the C++ ecosystem? When should you consider Rust?

### Answer:
```cpp
// C++ calling Rust via C FFI (extern "C")
// Rust side:
// #[no_mangle]
// pub extern "C" fn rust_parse_json(input: *const c_char, len: usize) -> *mut JsonDoc

// C++ side:
extern "C" {
    void* rust_parse_json(const char* input, size_t len);
    void rust_free_json(void* doc);
}

// Modern approach: cxx bridge (type-safe, no unsafe FFI)
// Generates both C++ and Rust code from a shared definition
```

**When to use Rust vs C++:**
| Factor | Stay C++ | Consider Rust |
|--------|----------|---------------|
| Existing codebase | Large C++ codebase (iCluster: 500K+ lines) | Greenfield project |
| Team expertise | Team knows C++ deeply | Team willing to learn |
| Memory safety | Using sanitizers + static analysis | Need compile-time guarantees |
| Libraries needed | Mature C++ ecosystem (Boost, Qt) | cargo ecosystem sufficient |
| Platform | IBM i, embedded, legacy OS | Linux/macOS/Windows |
| Hiring | More C++ developers available | Growing Rust talent pool |

### Explanation:
**Senior perspective**: The question isn't "C++ or Rust" | it's "where does each fit in our stack?" Production systems increasingly use both: Rust for new security-sensitive components (parsers, network handling), C++ for existing engines (CAD kernels, game engines, trading systems). The iCluster communication layer (DMKAPI) handles encryption and protocol parsing -> these would benefit from Rust's memory safety if starting fresh.

---

## Q7: Explain Observability (Metrics, Logs, Traces) for C++ systems.

### Answer:
```
The Three Pillars:

1. METRICS (quantitative | Prometheus, Grafana)
   counter: requests_total, errors_total
   gauge: active_connections, queue_depth  
   histogram: request_latency_ms (p50, p95, p99)
   
   iCluster metrics: apply_latency, journal_position_lag,
                     staging_store_depth, link_check_failures

2. LOGS (qualitative | structured JSON, ELK stack)
   {"timestamp":"...","level":"ERROR","component":"OMMIRROR",
    "group":"PAYROLL","message":"Send failed","retry":3}
   
   iCluster uses DMKLOG with severity levels and event forwarding

3. TRACES (request flow | OpenTelemetry, Jaeger)
   Trace ID propagated: Source | Network | Target
   Span: "SAVOBJ" (200ms) | "NetworkSend" (50ms) | "RSTOBJ" (300ms)
   
   iCluster uses conversation tags (conv_slot) and packet counters
   as correlation IDs | identical concept to trace IDs
```

### Explanation:
Modern observability answers three questions: "What's broken?" (metrics), "Why?" (logs), "Where in the flow?" (traces). iCluster already implements all three, just with 1990s terminology. Understanding the mapping between iCluster's monitoring and modern observability helps explain your experience to interviewers at cloud-native companies.

---

## Q8: What is Platform Engineering and how does it affect C++ system design?

### Answer:
```
Platform Engineering = Building internal developer platforms (IDPs)
that make application teams self-service

For C++ teams:
1. BUILD PLATFORM
   - Standardized CMake templates, vcpkg manifests
   - Remote build caching (Bazel remote cache, ccache)
   - CI/CD pipelines with sanitizers + static analysis
   
2. DEPLOY PLATFORM  
   - Containerized C++ services (minimal Alpine + static binary)
   - Feature flags for runtime configuration
   - Canary deployments with automatic rollback
   
3. OBSERVE PLATFORM
   - Standardized metrics library (OpenTelemetry C++ SDK)
   - Centralized logging with structured format
   - Distributed tracing for multi-service architectures

4. TEST PLATFORM
   - Fuzz testing infrastructure (OSS-Fuzz integration)
   - Performance regression detection (automated benchmarks)
   - Chaos engineering for resilience testing
```

### Explanation:
**Senior architect note**: Platform engineering is how you scale engineering practices across hundreds of developers. Instead of each team reinventing CI/CD, build systems, and observability, a platform team provides golden paths. For iCluster, the build system (BLDDMKPGM*.CLLE files) IS a platform -> it standardizes how every module is compiled, linked, and deployed.

---
void example() {
    auto a = taskA();
    auto b = taskB();
    scheduler.schedule(a.handle);
    scheduler.schedule(b.handle);
    scheduler.run();
    // Output (interleaved):
    // A: step 1
    // B: step 1
    // A: step 2
    // B: step 2
    // A: step 3
}
```

### Explanation:
- **Cooperative multitasking** without threads
- **Custom awaiter** controls suspension/resumption behavior
- **Scheduler** maintains a ready queue of coroutine handles
- **Use cases**: Game loop (entity updates), async I/O event loops, state machines

---

## Q10: Quick-fire coding questions (expect these as warm-up or final round)

### Q10a: Implement `string_to_int` (atoi) handling edge cases
```cpp
#include <string_view>
#include <optional>
#include <limits>

std::optional<int> stringToInt(std::string_view str) {
    if (str.empty()) return std::nullopt;

    size_t i = 0;
    // Skip whitespace
    while (i < str.size() && str[i] == ' ') ++i;
    if (i == str.size()) return std::nullopt;

    // Handle sign
    bool negative = false;
    if (str[i] == '-' || str[i] == '+') {
        negative = (str[i] == '-');
        ++i;
    }

    if (i == str.size() || !std::isdigit(str[i]))
        return std::nullopt;

    long long result = 0;
    while (i < str.size() && std::isdigit(str[i])) {
        result = result * 10 + (str[i] - '0');
        // Overflow check
        if (!negative && result > std::numeric_limits<int>::max())
            return std::nullopt;
        if (negative && -result < std::numeric_limits<int>::min())
            return std::nullopt;
        ++i;
    }

    return static_cast<int>(negative -> -result : result);
}
```

### Q10b: Reverse words in a string in-place
```cpp
void reverseWords(std::string& s) {
    // Step 1: Reverse entire string
    std::reverse(s.begin(), s.end());

    // Step 2: Reverse each word
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == ' ') {
            std::reverse(s.begin() + start, s.begin() + i);
            start = i + 1;
        }
    }

    // Step 3: Clean up extra spaces (if needed)
    // "hello world" ? "world hello"
}
```

### Q10c: Check if a binary tree is balanced
```cpp
struct TreeNode {
    int val;
    TreeNode* left;
    TreeNode* right;
};

bool isBalanced(TreeNode* root) {
    return height(root) != -1;
}

int height(TreeNode* node) {
    if (!node) return 0;
    int left = height(node->left);
    if (left == -1) return -1;
    int right = height(node->right);
    if (right == -1) return -1;
    if (std::abs(left - right) > 1) return -1;
    return 1 + std::max(left, right);
}
// O(n) time, O(h) space -> single pass, not the naive O(n²) approach
```

### Q10d: Detect cycle in linked list and find the start
```cpp
ListNode* detectCycle(ListNode* head) {
    ListNode* slow = head;
    ListNode* fast = head;

    // Phase 1: Detect cycle
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) break;
    }

    if (!fast || !fast->next) return nullptr;  // No cycle

    // Phase 2: Find cycle start
    slow = head;
    while (slow != fast) {
        slow = slow->next;
        fast = fast->next;
    }
    return slow;  // Start of cycle
}
// Floyd's algorithm -> O(n) time, O(1) space
```

### Q10e: Implement a moving average calculator
```cpp
class MovingAverage {
    std::queue<double> window_;
    int maxSize_;
    double sum_ = 0;

public:
    MovingAverage(int size) : maxSize_(size) {}

    double next(double val) {
        window_.push(val);
        sum_ += val;
        if (static_cast<int>(window_.size()) > maxSize_) {
            sum_ -= window_.front();
            window_.pop();
        }
        return sum_ / window_.size();
    }
};
// O(1) per update -> used in finance for technical analysis (SMA)
```

---

# Summary: Interview Preparation Checklist

```
- Modern C++ (C++17/20/23) | concepts, ranges, coroutines, modules
- Data Structures & Algorithms -> implement from scratch, complexity analysis
- Design Patterns -> Strategy, Observer, Factory, Command, CRTP, Type Erasure
- LLD -> Parking Lot, Chess, Elevator, File System, Rate Limiter
- HLD -> CAD collaboration, Trading system, Game server, Build system
- Multithreading -> Memory model, thread pool, lock-free, atomics
- Memory & Performance -> Allocators, cache optimization, SIMD, profiling
- Domain Knowledge -> CAD geometry, Game ECS/loop, Finance FIX/OrderBook
- Projects -> 2-3 deep stories with STAR-T framework, quantified results
- Behavioral -> Disagreements, mentoring, incidents, estimation
- Latest trends -> C++23 features, build tools, Senders/Receivers
- Live coding -> Practice timed implementation (45-60 min problems)
```

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    \
// /              \ Unit Tests (many, fast)
//   
        $match = # Set 10: Latest Trends, Best Practices & Live Coding
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: Latest C++ Trends & Technologies (2024-2026)

---

## Q1: What's new in C++23 and what's coming in C++26|

### Answer:

**C++23 Key Features:**
```cpp
// 1. std::expected -> Better error handling than exceptions
#include <expected>
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) return std::unexpected("Division by zero");
    return a / b;
}
auto result = divide(10, 3);
if (result) std::cout << *result;  // 3
else std::cout << result.error();  // "Division by zero"

// 2. std::print / std::println -> Type-safe formatting
#include <print>
std::println("Hello, {}! You are {} years old.", name, age);
// Replaces: printf (unsafe), cout (verbose), fmt::print (external)

// 3. std::generator -> Coroutine-based generator
#include <generator>
std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a;
        a = b;
        b = temp + b;
    }
}
for (int n : fibonacci() | std::views::take(10))
    std::print("{} ", n);  // 0 1 1 2 3 5 8 13 21 34

// 4. std::mdspan -> Multi-dimensional array view
#include <mdspan>
std::vector<double> data(rows * cols);
std::mdspan<double, std::dextents<size_t, 2>> matrix(data.data(), rows, cols);
matrix[2, 3] = 42.0;  // Access like 2D array, data is contiguous

// 5. std::flat_map / std::flat_set -> Cache-friendly sorted containers
#include <flat_map>
std::flat_map<std::string, int> scores;  // Backed by sorted vectors
scores["Alice"] = 95;  // Cache-friendly iteration, slower insert

// 6. Deducing this (Explicit object parameter)
struct Widget {
    template<typename Self>
    auto& value(this Self&& self) {
        return std::forward<Self>(self).value_;
    }
    // Replaces 4 overloads: const&, &, const&&, &&
};

// 7. std::stacktrace -> Programmatic stack traces
#include <stacktrace>
void crashHandler() {
    auto trace = std::stacktrace::current();
    std::println("{}", std::to_string(trace));
}

// 8. Ranges improvements: ranges::to, zip, chunk, slide
auto vec = std::views::iota(1, 10)
         | std::views::filter([](int x) { return x % 2 == 0; })
         | std::ranges::to<std::vector>();  // Materialize range to container

auto pairs = std::views::zip(names, ages);  // Iterate two ranges together
```

**C++26 (Expected/In Progress):**
```
- std::execution (Senders/Receivers) -> Standard async framework
- Reflection (compile-time introspection)
- Pattern matching
- Contracts (preconditions, postconditions, assertions)
- std::inplace_vector (fixed-capacity, stack-allocated vector)
- Improved coroutine library support
- std::hive (formerly colony) -> pool-like container
```

**C++26 Reflection Deep Dive (P2996 ? approved for C++26):**
```cpp
// Compile-time inspection of types, members, and functions
#include <meta>

struct Config {
    std::string host;
    int port;
    bool useTLS;
};

// Auto-generate JSON serialization via reflection (no macros, no code gen)
template<typename T>
std::string toJSON(const T& obj) {
    std::string result = "{";
    bool first = true;
    template for (constexpr auto member : std::meta::nonstatic_data_members_of(^T)) {
        if (!first) result += ",";
        result += "\"" + std::string(std::meta::identifier_of(member)) + "\":";
        result += serialize(obj.[:member:]);  // Splice operator
        first = false;
    }
    return result + "}";
}
// toJSON(Config{"localhost", 8080, true})
// ? {"host":"localhost","port":8080,"useTLS":true}
// No boilerplate! Works for ANY struct!
```

**C++26 Contracts Deep Dive (P2900):**
```cpp
// Preconditions, postconditions, and assertions checked at runtime
int divide(int a, int b)
    pre(b != 0)                          // Precondition
    post(r: r * b == a || a % b != 0)    // Postcondition (r = return value)
{
    return a / b;
}

void processBuffer(std::span<int> buf, size_t idx)
    pre(idx < buf.size())               // Bounds check contract
    pre(!buf.empty())                    // Non-empty contract
{
    buf[idx] *= 2;
}

// Contract violation modes (configurable at build time):
// - enforce: Calls violation handler (default: abort)
// - observe: Log violation but continue (production monitoring)
// - ignore: Zero overhead (same as no contract -> release builds)
// 
// Key benefit: Contracts are part of the function signature,
// visible in documentation and IDE tooling
```

### Explanation:
| Feature | Impact | Your Domain |
|---------|--------|-------------|
| `std::expected` | Replaces error codes AND exceptions | Finance (error handling on hot path) |
| `std::mdspan` | Zero-copy multi-dim views | CAD (matrices), Gaming (texture data) |
| `std::flat_map` | Cache-friendly maps | Gaming (ECS), HFT (lookup tables) |
| `std::generator` | Easy lazy sequences | All (data processing pipelines) |
| Senders/Receivers | Standard async model | All (replaces ad-hoc thread pools) |
| Reflection | Auto-serialization, ORM | Enterprise (IBM), reducing boilerplate |
| Contracts | Eliminate ad-hoc assertions | All (API boundaries, safety-critical) |

---

## Q2: What build systems and package managers are current best practice for C++|

### Answer:

**Build Systems (2025-2026):**
| Tool | Status | Best For |
|------|--------|----------|
| CMake 3.28+ | Industry standard | Most projects, modules support |
| Bazel | Growing in enterprise | Large monorepos (Google-style) |
| Meson | Simpler alternative | Small-medium projects |
| build2 | Niche but modern | New projects wanting integrated tooling |

**Package Managers:**
| Tool | Status | Best For |
|------|--------|----------|
| vcpkg | Most popular (Microsoft) | Cross-platform, CMake integration |
| Conan 2.x | Strong enterprise | Complex dependency graphs, binary caching |
| CPM.cmake | Lightweight | CMake-only projects |

**Modern CMake Best Practices:**
```cmake
cmake_minimum_required(VERSION 3.28)
project(MyCADApp VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C++20 Modules support
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# vcpkg integration
find_package(fmt CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS asio)

add_library(geometry_core)
target_sources(geometry_core
    PUBLIC FILE_SET CXX_MODULES FILES
        src/geometry.cppm
        src/topology.cppm
)
target_link_libraries(geometry_core PRIVATE fmt::fmt)

# Sanitizers for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(geometry_core PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(geometry_core PRIVATE
        -fsanitize=address,undefined
    )
endif()
```

---

## Q3: What are Senders/Receivers (std::execution) and why do they matter|

### Answer:
```cpp
// std::execution (P2300) -> The future of C++ async
// Currently available via stdexec (reference implementation)
#include <stdexec/execution.hpp>

using namespace stdexec;

// Sender: describes async work (lazy, composable)
auto work = just(42)                          // Start with value 42
          | then([](int x) { return x * 2; }) // Transform
          | then([](int x) { return std::to_string(x); }); // Transform again

// Nothing has executed yet! Senders are lazy descriptions.

// Receiver: consumes the result
// Scheduler: decides WHERE to run (thread pool, GPU, etc.)
auto result = sync_wait(std::move(work));  // Execute and wait
// result = "84"

// Real-world: Async pipeline with error handling
auto pipeline = 
    schedule(threadPool.get_scheduler())   // Run on thread pool
  | then([] { return readFile("data.csv"); })
  | then([](auto data) { return parseCSV(data); })
  | then([](auto records) { return aggregate(records); })
  | upon_error([](auto err) { log("Pipeline failed: ", err); });

// Parallel fan-out
auto parallel = when_all(
    on(gpuScheduler, computePhysics()),
    on(cpuPool, computeAI()),
    on(ioScheduler, loadAssets())
);
```

### Explanation:
**Why this matters (replacing ad-hoc async):**
- **Composable**: Chain operations like ranges for data, but for async work
- **Scheduler-agnostic**: Same code runs on thread pool, GPU, or single-threaded
- **Structured concurrency**: Resources tied to scope, no leaked tasks
- **Zero allocation**: Small async operations don't need heap
- **Replaces**: `std::async` (broken), hand-rolled thread pools, Boost.Asio patterns

**Impact on your domains:**
- **Gaming**: GPU + CPU work orchestration without manual synchronization
- **Finance**: Market data processing pipelines
- **CAD**: Parallel geometry computation with structured cancellation

---

# Part B: Best Practices & Coding Standards

---

## Q4: What are the key principles of modern C++ coding standards|

### Answer:

**Core Guidelines (from ISO C++ Core Guidelines by Stroustrup & Sutter):**

```cpp
// 1. RAII everywhere -> no resource leaks possible
{
    auto file = std::ifstream("data.txt");  // Closes automatically
    auto conn = ConnectionPool::acquire();  // Returns automatically
    auto lock = std::scoped_lock(mutex);    // Unlocks automatically
}  // Everything cleaned up, even with exceptions

// 2. Prefer value semantics over pointer semantics
struct Point { double x, y, z; };
Point translate(Point p, Vector v) {  // Pass by value, return by value
    return {p.x + v.dx, p.y + v.dy, p.z + v.dz};
}

// 3. Use const correctly
void process(const std::vector<int>& data);  // Won't modify
int calculateArea() const;                    // Member function won't modify object
const auto& ref = getExpensiveObject();       // Avoid copy

// 4. Prefer composition over inheritance
class Car {
    Engine engine_;          // HAS-A (composition)
    Transmission trans_;     // HAS-A
    // NOT: class Car : public Vehicle, public Engine
};

// 5. Don't use raw new/delete
auto p = std::make_unique<Widget>();     // Not: new Widget()
auto v = std::vector<int>(1000);         // Not: new int[1000]
auto s = std::string("hello");           // Not: strdup/malloc

// 6. Use string_view for non-owning string parameters
void log(std::string_view message);  // No copy, accepts string, string_view, char*

// 7. Use span for non-owning array parameters
void process(std::span<const int> data);  // Accepts vector, array, C-array

// 8. Structured error handling
std::expected<Result, Error> tryOperation();  // C++23
std::optional<int> findValue(Key k);          // C++17 ? nullable return

// 9. Use enum class, not plain enum
enum class Color { RED, GREEN, BLUE };  // Scoped, type-safe
// NOT: enum Color { RED, GREEN, BLUE }; // Pollutes namespace

// 10. Mark functions noexcept when they don't throw
void swap(Widget& a, Widget& b) noexcept;
~Widget() noexcept;  // Destructors should ALWAYS be noexcept
```

---

## Q5: What testing strategies do you use for C++ projects|

### Answer:
```cpp
// Testing Pyramid for C++:
//
//        /\
//       /  \     E2E Tests (few, slow)
//      /    \    - Full system tests, integration with hardware/UI
//     /+----+\
//    /        \  Integration Tests (moderate)
//   /          \ - Component interaction, DB, network
//  /+----------+\
// /              \ Unit Tests (many, fast)
//  +------------+  - Pure logic, no I/O, fast (<1ms each)

// === Unit Testing with Google Test ===
#include <gtest/gtest.h>

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book;

    void SetUp() override {
        // Common setup
    }
};

TEST_F(OrderBookTest, MatchingBuyAndSellAtSamePrice) {
    book.addOrder({.side = Side::SELL, .price = 100.0, .quantity = 50});
    auto trades = book.addOrder({.side = Side::BUY, .price = 100.0, .quantity = 30});

    ASSERT_EQ(trades.size(), 1);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);
    EXPECT_EQ(trades[0].quantity, 30);
}

TEST_F(OrderBookTest, NoMatchWhenPricesDontCross) {
    book.addOrder({.side = Side::SELL, .price = 101.0, .quantity = 50});
    auto trades = book.addOrder({.side = Side::BUY, .price = 99.0, .quantity = 30});

    EXPECT_TRUE(trades.empty());
    EXPECT_DOUBLE_EQ(book.bestBid(), 99.0);
    EXPECT_DOUBLE_EQ(book.bestAsk(), 101.0);
}

// === Property-based testing (Fuzz testing) ===
// "For any sequence of valid orders, the book invariants hold"
TEST(OrderBookProperty, InvariantsMaintained) {
    OrderBook book;
    std::mt19937 rng(42);

    for (int i = 0; i < 10000; ++i) {
        Order order;
        order.side = (rng() % 2) ? Side::BUY : Side::SELL;
        order.price = 90.0 + (rng() % 2000) / 100.0;
        order.quantity = 1 + rng() % 1000;
        book.addOrder(order);

        // Invariant: bestBid < bestAsk (or one side is empty)
        if (book.bestBid() > 0 && book.bestAsk() > 0) {
            ASSERT_LT(book.bestBid(), book.bestAsk());
        }
    }
}

// === Benchmark tests ===
#include <benchmark/benchmark.h>

static void BM_OrderBookInsert(benchmark::State& state) {
    OrderBook book;
    for (auto _ : state) {
        Order order{.side = Side::BUY, .price = 100.0, .quantity = 100};
        benchmark::DoNotOptimize(book.addOrder(order));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBookInsert);
```

**CI/CD Pipeline:**
```yaml
# GitHub Actions / Jenkins pipeline
stages:
  - build:
      - cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZERS=ON
      - cmake --build build -j $(nproc)
  - unit_test:
      - ctest --test-dir build --output-on-failure
  - sanitizer_test:
      - ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build
  - fuzz_test:
      - ./build/fuzz_target -max_total_time=300
  - integration_test:
      - ./run_integration_tests.sh
  - benchmark:
      - ./build/benchmarks --benchmark_format=json > results.json
      - python compare_benchmarks.py results.json baseline.json
```

**Fuzz Testing (libFuzzer / AFL) ? finding bugs humans miss:**
```cpp
// Fuzz target: Find crashes in your parser
// Build: clang++ -fsanitize=fuzzer,address fuzz_target.cpp -o fuzz_target
// Run:   ./fuzz_target corpus/ -max_total_time=3600

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Feed random bytes to your parser
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        FIXMessage::parse(input);          // Test FIX parser
    } catch (...) {
        // Exceptions are OK -> crashes/UB are bugs
    }
    return 0;
}

// Structured fuzzing with protobuf (more targeted):
// Define valid input structure -> fuzzer generates valid-ish inputs
// Finds edge cases like:
// - Empty fields, max-length strings, integer overflow
// - Invalid UTF-8, null bytes in strings
// - Deeply nested structures, circular references
```

**Mutation Testing (assessing test quality):**
```
Concept: Modify (mutate) your source code, then run tests.
         If tests still pass -> tests are weak (mutation survived)
         If tests fail -> tests caught the bug (mutation killed)

Tools for C++:
  - Mull (https://github.com/mull-project/mull) -> LLVM-based
  - Dextool (D-lang based, works with C/C++)
  
Example mutations:
  Original:         if (x > 0) return true;
  Mutation 1:       if (x >= 0) return true;    // Boundary mutation
  Mutation 2:       if (x < 0) return true;     // Negate condition
  Mutation 3:       if (x > 0) return false;    // Return value mutation

Mutation Score = killed_mutants / total_mutants
  70-80% = OK, 80-90% = Good, 90%+ = Excellent
  
// Run: mull-runner --test-framework=gtest ./build/my_tests
```

**Testing Pyramid for C++ projects:**
```
           /  \           E2E / Integration Tests
          /    \          (slow, broad, fewer)
         /------\         
        / System  \       System Tests
       /   Tests   \      (medium speed)
      /-------------\     
     /  Integration  \    Component Integration
    /    Tests        \   (mock external deps)
   /-------------------\  
  /   Unit Tests        \ Unit Tests
 /     (fast, focused,   \(GTest/Catch2, mock with GMock)
/       many)              \
```

---

# Part C: Live Coding Questions

---

## Q6: Implement a thread-safe Singleton Logger with log rotation (Live Coding)

### Expected Solution:
```cpp
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <filesystem>
#include <format>

class Logger {
public:
    enum class Level { DEBUG, INFO, WARN, ERROR };

    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(Level level, std::string_view message,
             std::source_location loc = std::source_location::current()) {
        if (level < minLevel_) return;

        std::lock_guard lock(mutex_);

        auto now = std::chrono::system_clock::now();
        auto timeStr = std::format("{:%Y-%m-%d %H:%M:%S}", now);

        auto entry = std::format("[{}] [{}] {} ({}:{})\n",
            timeStr, levelToStr(level), message,
            loc.file_name(), loc.line());

        checkRotation();
        file_ << entry;
        file_.flush();
    }

    void setLevel(Level level) { minLevel_ = level; }
    void setMaxFileSize(size_t bytes) { maxFileSize_ = bytes; }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() : maxFileSize_(10 * 1024 * 1024) {
        openLogFile();
    }

    void openLogFile() {
        file_.open("app.log", std::ios::app);
        currentSize_ = std::filesystem::file_size("app.log");
    }

    void checkRotation() {
        if (currentSize_ >= maxFileSize_) {
            file_.close();
            auto timestamp = std::format("{:%Y%m%d_%H%M%S}",
                std::chrono::system_clock::now());
            std::filesystem::rename("app.log", "app." + timestamp + ".log");
            openLogFile();
            currentSize_ = 0;
        }
    }

    static constexpr const char* levelToStr(Level level) {
        switch (level) {
            case Level::DEBUG: return "DEBUG";
            case Level::INFO:  return "INFO ";
            case Level::WARN:  return "WARN ";
            case Level::ERROR: return "ERROR";
        }
        return "+---+";
    }

    std::ofstream file_;
    std::mutex mutex_;
    Level minLevel_ = Level::INFO;
    size_t maxFileSize_;
    size_t currentSize_ = 0;
};

// Usage
#define LOG_DEBUG(msg) Logger::instance().log(Logger::Level::DEBUG, msg)
#define LOG_INFO(msg)  Logger::instance().log(Logger::Level::INFO, msg)
#define LOG_ERROR(msg) Logger::instance().log(Logger::Level::ERROR, msg)
```

### Interviewer Evaluation Points:
- Thread safety (mutex on write path)
- Meyer's singleton (thread-safe in C++11+)
- Log rotation to prevent disk exhaustion
- `std::source_location` (C++20) for automatic file/line
- Proper RAII for file handles
- Level filtering before taking lock (optimization)

---

## Q7: Implement `std::shared_ptr` from scratch (Live Coding)

### Expected Solution:
```cpp
#include <atomic>
#include <utility>

template<typename T>
class SharedPtr {
    struct ControlBlock {
        std::atomic<int> refCount{1};
        std::atomic<int> weakCount{0};
    };

    T* ptr_ = nullptr;
    ControlBlock* ctrl_ = nullptr;

    void release() {
        if (ctrl_) {
            if (ctrl_->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete ptr_;
                if (ctrl_->weakCount.load(std::memory_order_acquire) == 0) {
                    delete ctrl_;
                }
            }
        }
        ptr_ = nullptr;
        ctrl_ = nullptr;
    }

public:
    SharedPtr() = default;

    explicit SharedPtr(T* ptr) : ptr_(ptr), ctrl_(new ControlBlock{}) {}

    // Copy -> increment ref count
    SharedPtr(const SharedPtr& other) : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        if (ctrl_) {
            ctrl_->refCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Move -> transfer ownership
    SharedPtr(SharedPtr&& other) noexcept
        : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        other.ptr_ = nullptr;
        other.ctrl_ = nullptr;
    }

    // Copy assignment
    SharedPtr& operator=(const SharedPtr& other) {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ctrl_ = other.ctrl_;
            if (ctrl_) {
                ctrl_->refCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }

    // Move assignment
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ctrl_ = other.ctrl_;
            other.ptr_ = nullptr;
            other.ctrl_ = nullptr;
        }
        return *this;
    }

    ~SharedPtr() { release(); }

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }

    int use_count() const {
        return ctrl_ ? ctrl_->refCount.load(std::memory_order_relaxed) : 0;
    }

    explicit operator bool() const { return ptr_ != nullptr; }

    void reset() { release(); }
    void reset(T* ptr) {
        release();
        ptr_ = ptr;
        ctrl_ = new ControlBlock{};
    }
};

// make_shared equivalent -> single allocation for object + control block
template<typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    // In production, allocate T and ControlBlock together
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}
```

### Interviewer Evaluation Points:
- Atomic reference counting (thread-safe)
- Proper copy/move semantics (Rule of 5)
- Self-assignment check
- `release()` handles deletion correctly
- Memory ordering awareness
- Knows `make_shared` optimization (single allocation)

---

## Q8: Design and implement a simple compile-time reflection system (Live Coding)

### Expected Solution:
```cpp
#include <string_view>
#include <tuple>
#include <iostream>

// Macro-based reflection (until C++26 static reflection)
#define REFLECT(Type, ...) \
    static constexpr auto reflectedFields() { \
        return std::make_tuple(__VA_ARGS__); \
    } \
    static constexpr std::string_view typeName() { return #Type; }

#define FIELD(name) \
    std::pair{std::string_view{#name}, &std::remove_pointer_t<decltype(this)>::name}

// Usage
struct Person {
    std::string name;
    int age;
    double salary;

    REFLECT(Person, FIELD(name), FIELD(age), FIELD(salary))
};

// Generic serializer using reflection
template<typename T>
void toJSON(const T& obj, std::ostream& out) {
    out << "{ ";
    bool first = true;

    std::apply([&](auto&&... fields) {
        ((
            out << (first -> "" : ", "),
            out << "\"" << fields.first << "\": ",
            printValue(out, obj.*(fields.second)),
            first = false
        ), ...);
    }, T::reflectedFields());

    out << " }";
}

template<typename T>
void printValue(std::ostream& out, const T& val) {
    if constexpr (std::is_same_v<T, std::string>) {
        out << "\"" << val << "\"";
    } else {
        out << val;
    }
}

// Usage
void example() {
    Person p{"Alice", 30, 95000.0};
    toJSON(p, std::cout);
    // Output: { "name": "Alice", "age": 30, "salary": 95000 }
}
```

### Explanation:
- Uses **fold expressions** and **`std::apply`** for iteration over tuple
- **`if constexpr`** for type-specific serialization
- **Pointer-to-member** (`&Person::name`) for field access
- **C++26 will have real reflection** | this pattern will be replaced by `std::meta::members_of(^Person)`

---

## Q9: Implement a simple coroutine-based task system (Live Coding)

### Expected Solution:
```cpp
#include <coroutine>
#include <optional>
#include <iostream>
#include <queue>
#include <functional>

// Simple Task coroutine
struct Task {
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() { if (handle) handle.destroy(); }
    Task(Task&& t) noexcept : handle(t.handle) { t.handle = nullptr; }
    Task& operator=(Task&&) = delete;
};

// Scheduler
class Scheduler {
    std::queue<std::coroutine_handle<>> ready_;

public:
    // Custom awaiter that yields to scheduler
    auto suspend() {
        struct Awaiter {
            Scheduler& sched;
            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<> h) {
                sched.ready_.push(h);  // Re-enqueue
            }
            void await_resume() {}
        };
        return Awaiter{*this};
    }

    void schedule(std::coroutine_handle<> handle) {
        ready_.push(handle);
    }

    void run() {
        while (!ready_.empty()) {
            auto handle = ready_.front();
            ready_.pop();
            if (!handle.done()) {
                handle.resume();
            }
        }
    }
};

// Usage
Scheduler scheduler;

Task taskA() {
    std::cout << "A: step 1\n";
    co_await scheduler.suspend();
    std::cout << "A: step 2\n";
    co_await scheduler.suspend();
    std::cout << "A: step 3\n";
}

Task taskB() {
    std::cout << "B: step 1\n";
    co_await scheduler.suspend();
    std::cout << "B: step 2\n";
}
```

---

# ENHANCED SECTION: Industry Trends 2025-2026

> *What senior architects and principal engineers are actually discussing and adopting in production.*

---

## Q6: How is Rust interop changing the C++ ecosystem| When should you consider Rust|

### Answer:
```cpp
// C++ calling Rust via C FFI (extern "C")
// Rust side:
// #[no_mangle]
// pub extern "C" fn rust_parse_json(input: *const c_char, len: usize) -> *mut JsonDoc

// C++ side:
extern "C" {
    void* rust_parse_json(const char* input, size_t len);
    void rust_free_json(void* doc);
}

// Modern approach: cxx bridge (type-safe, no unsafe FFI)
// Generates both C++ and Rust code from a shared definition
```

**When to use Rust vs C++:**
| Factor | Stay C++ | Consider Rust |
|--------|----------|---------------|
| Existing codebase | Large C++ codebase (iCluster: 500K+ lines) | Greenfield project |
| Team expertise | Team knows C++ deeply | Team willing to learn |
| Memory safety | Using sanitizers + static analysis | Need compile-time guarantees |
| Libraries needed | Mature C++ ecosystem (Boost, Qt) | cargo ecosystem sufficient |
| Platform | IBM i, embedded, legacy OS | Linux/macOS/Windows |
| Hiring | More C++ developers available | Growing Rust talent pool |

### Explanation:
**Senior perspective**: The question isn't "C++ or Rust" | it's "where does each fit in our stack?" Production systems increasingly use both: Rust for new security-sensitive components (parsers, network handling), C++ for existing engines (CAD kernels, game engines, trading systems). The iCluster communication layer (DMKAPI) handles encryption and protocol parsing | these would benefit from Rust's memory safety if starting fresh.

---

## Q7: Explain Observability (Metrics, Logs, Traces) for C++ systems.

### Answer:
```
The Three Pillars:

1. METRICS (quantitative -> Prometheus, Grafana)
   counter: requests_total, errors_total
   gauge: active_connections, queue_depth  
   histogram: request_latency_ms (p50, p95, p99)
   
   iCluster metrics: apply_latency, journal_position_lag,
                     staging_store_depth, link_check_failures

2. LOGS (qualitative -> structured JSON, ELK stack)
   {"timestamp":"...","level":"ERROR","component":"OMMIRROR",
    "group":"PAYROLL","message":"Send failed","retry":3}
   
   iCluster uses DMKLOG with severity levels and event forwarding

3. TRACES (request flow -> OpenTelemetry, Jaeger)
   Trace ID propagated: Source -> Network -> Target
   Span: "SAVOBJ" (200ms) -> "NetworkSend" (50ms) -> "RSTOBJ" (300ms)
   
   iCluster uses conversation tags (conv_slot) and packet counters
   as correlation IDs -> identical concept to trace IDs
```

### Explanation:
Modern observability answers three questions: "What's broken?" (metrics), "Why?" (logs), "Where in the flow?" (traces). iCluster already implements all three, just with 1990s terminology. Understanding the mapping between iCluster's monitoring and modern observability helps explain your experience to interviewers at cloud-native companies.

---

## Q8: What is Platform Engineering and how does it affect C++ system design|

### Answer:
```
Platform Engineering = Building internal developer platforms (IDPs)
that make application teams self-service

For C++ teams:
1. BUILD PLATFORM
   - Standardized CMake templates, vcpkg manifests
   - Remote build caching (Bazel remote cache, ccache)
   - CI/CD pipelines with sanitizers + static analysis
   
2. DEPLOY PLATFORM  
   - Containerized C++ services (minimal Alpine + static binary)
   - Feature flags for runtime configuration
   - Canary deployments with automatic rollback
   
3. OBSERVE PLATFORM
   - Standardized metrics library (OpenTelemetry C++ SDK)
   - Centralized logging with structured format
   - Distributed tracing for multi-service architectures

4. TEST PLATFORM
   - Fuzz testing infrastructure (OSS-Fuzz integration)
   - Performance regression detection (automated benchmarks)
   - Chaos engineering for resilience testing
```

### Explanation:
**Senior architect note**: Platform engineering is how you scale engineering practices across hundreds of developers. Instead of each team reinventing CI/CD, build systems, and observability, a platform team provides golden paths. For iCluster, the build system (BLDDMKPGM*.CLLE files) IS a platform | it standardizes how every module is compiled, linked, and deployed.

---
void example() {
    auto a = taskA();
    auto b = taskB();
    scheduler.schedule(a.handle);
    scheduler.schedule(b.handle);
    scheduler.run();
    // Output (interleaved):
    // A: step 1
    // B: step 1
    // A: step 2
    // B: step 2
    // A: step 3
}
```

### Explanation:
- **Cooperative multitasking** without threads
- **Custom awaiter** controls suspension/resumption behavior
- **Scheduler** maintains a ready queue of coroutine handles
- **Use cases**: Game loop (entity updates), async I/O event loops, state machines

---

## Q10: Quick-fire coding questions (expect these as warm-up or final round)

### Q10a: Implement `string_to_int` (atoi) handling edge cases
```cpp
#include <string_view>
#include <optional>
#include <limits>

std::optional<int> stringToInt(std::string_view str) {
    if (str.empty()) return std::nullopt;

    size_t i = 0;
    // Skip whitespace
    while (i < str.size() && str[i] == ' ') ++i;
    if (i == str.size()) return std::nullopt;

    // Handle sign
    bool negative = false;
    if (str[i] == '-' || str[i] == '+') {
        negative = (str[i] == '-');
        ++i;
    }

    if (i == str.size() || !std::isdigit(str[i]))
        return std::nullopt;

    long long result = 0;
    while (i < str.size() && std::isdigit(str[i])) {
        result = result * 10 + (str[i] - '0');
        // Overflow check
        if (!negative && result > std::numeric_limits<int>::max())
            return std::nullopt;
        if (negative && -result < std::numeric_limits<int>::min())
            return std::nullopt;
        ++i;
    }

    return static_cast<int>(negative ? -result : result);
}
```

### Q10b: Reverse words in a string in-place
```cpp
void reverseWords(std::string& s) {
    // Step 1: Reverse entire string
    std::reverse(s.begin(), s.end());

    // Step 2: Reverse each word
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == ' ') {
            std::reverse(s.begin() + start, s.begin() + i);
            start = i + 1;
        }
    }

    // Step 3: Clean up extra spaces (if needed)
    // "hello world" | "world hello"
}
```

### Q10c: Check if a binary tree is balanced
```cpp
struct TreeNode {
    int val;
    TreeNode* left;
    TreeNode* right;
};

bool isBalanced(TreeNode* root) {
    return height(root) != -1;
}

int height(TreeNode* node) {
    if (!node) return 0;
    int left = height(node->left);
    if (left == -1) return -1;
    int right = height(node->right);
    if (right == -1) return -1;
    if (std::abs(left - right) > 1) return -1;
    return 1 + std::max(left, right);
}
// O(n) time, O(h) space | single pass, not the naive O(n²) approach
```

### Q10d: Detect cycle in linked list and find the start
```cpp
ListNode* detectCycle(ListNode* head) {
    ListNode* slow = head;
    ListNode* fast = head;

    // Phase 1: Detect cycle
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) break;
    }

    if (!fast || !fast->next) return nullptr;  // No cycle

    // Phase 2: Find cycle start
    slow = head;
    while (slow != fast) {
        slow = slow->next;
        fast = fast->next;
    }
    return slow;  // Start of cycle
}
// Floyd's algorithm | O(n) time, O(1) space
```

### Q10e: Implement a moving average calculator
```cpp
class MovingAverage {
    std::queue<double> window_;
    int maxSize_;
    double sum_ = 0;

public:
    MovingAverage(int size) : maxSize_(size) {}

    double next(double val) {
        window_.push(val);
        sum_ += val;
        if (static_cast<int>(window_.size()) > maxSize_) {
            sum_ -= window_.front();
            window_.pop();
        }
        return sum_ / window_.size();
    }
};
// O(1) per update | used in finance for technical analysis (SMA)
```

---

# Summary: Interview Preparation Checklist

```
| Modern C++ (C++17/20/23) | concepts, ranges, coroutines, modules
| Data Structures & Algorithms | implement from scratch, complexity analysis
| Design Patterns | Strategy, Observer, Factory, Command, CRTP, Type Erasure
| LLD | Parking Lot, Chess, Elevator, File System, Rate Limiter
| HLD | CAD collaboration, Trading system, Game server, Build system
| Multithreading | Memory model, thread pool, lock-free, atomics
| Memory & Performance | Allocators, cache optimization, SIMD, profiling
| Domain Knowledge | CAD geometry, Game ECS/loop, Finance FIX/OrderBook
| Projects | 2-3 deep stories with STAR-T framework, quantified results
| Behavioral | Disagreements, mentoring, incidents, estimation
| Latest trends | C++23 features, build tools, Senders/Receivers
| Live coding | Practice timed implementation (45-60 min problems)
```

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
      - Pure logic, no I/O, fast (<1ms each)

// === Unit Testing with Google Test ===
#include <gtest/gtest.h>

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book;

    void SetUp() override {
        // Common setup
    }
};

TEST_F(OrderBookTest, MatchingBuyAndSellAtSamePrice) {
    book.addOrder({.side = Side::SELL, .price = 100.0, .quantity = 50});
    auto trades = book.addOrder({.side = Side::BUY, .price = 100.0, .quantity = 30});

    ASSERT_EQ(trades.size(), 1);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);
    EXPECT_EQ(trades[0].quantity, 30);
}

TEST_F(OrderBookTest, NoMatchWhenPricesDontCross) {
    book.addOrder({.side = Side::SELL, .price = 101.0, .quantity = 50});
    auto trades = book.addOrder({.side = Side::BUY, .price = 99.0, .quantity = 30});

    EXPECT_TRUE(trades.empty());
    EXPECT_DOUBLE_EQ(book.bestBid(), 99.0);
    EXPECT_DOUBLE_EQ(book.bestAsk(), 101.0);
}

// === Property-based testing (Fuzz testing) ===
// "For any sequence of valid orders, the book invariants hold"
TEST(OrderBookProperty, InvariantsMaintained) {
    OrderBook book;
    std::mt19937 rng(42);

    for (int i = 0; i < 10000; ++i) {
        Order order;
        order.side = (rng() % 2) ? Side::BUY : Side::SELL;
        order.price = 90.0 + (rng() % 2000) / 100.0;
        order.quantity = 1 + rng() % 1000;
        book.addOrder(order);

        // Invariant: bestBid < bestAsk (or one side is empty)
        if (book.bestBid() > 0 && book.bestAsk() > 0) {
            ASSERT_LT(book.bestBid(), book.bestAsk());
        }
    }
}

// === Benchmark tests ===
#include <benchmark/benchmark.h>

static void BM_OrderBookInsert(benchmark::State& state) {
    OrderBook book;
    for (auto _ : state) {
        Order order{.side = Side::BUY, .price = 100.0, .quantity = 100};
        benchmark::DoNotOptimize(book.addOrder(order));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBookInsert);
```

**CI/CD Pipeline:**
```yaml
# GitHub Actions / Jenkins pipeline
stages:
  - build:
      - cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZERS=ON
      - cmake --build build -j $(nproc)
  - unit_test:
      - ctest --test-dir build --output-on-failure
  - sanitizer_test:
      - ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build
  - fuzz_test:
      - ./build/fuzz_target -max_total_time=300
  - integration_test:
      - ./run_integration_tests.sh
  - benchmark:
      - ./build/benchmarks --benchmark_format=json > results.json
      - python compare_benchmarks.py results.json baseline.json
```

**Fuzz Testing (libFuzzer / AFL) ? finding bugs humans miss:**
```cpp
// Fuzz target: Find crashes in your parser
// Build: clang++ -fsanitize=fuzzer,address fuzz_target.cpp -o fuzz_target
// Run:   ./fuzz_target corpus/ -max_total_time=3600

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Feed random bytes to your parser
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        FIXMessage::parse(input);          // Test FIX parser
    } catch (...) {
        // Exceptions are OK -> crashes/UB are bugs
    }
    return 0;
}

// Structured fuzzing with protobuf (more targeted):
// Define valid input structure -> fuzzer generates valid-ish inputs
// Finds edge cases like:
// - Empty fields, max-length strings, integer overflow
// - Invalid UTF-8, null bytes in strings
// - Deeply nested structures, circular references
```

**Mutation Testing (assessing test quality):**
```
Concept: Modify (mutate) your source code, then run tests.
         If tests still pass -> tests are weak (mutation survived)
         If tests fail -> tests caught the bug (mutation killed)

Tools for C++:
  - Mull (https://github.com/mull-project/mull) -> LLVM-based
  - Dextool (D-lang based, works with C/C++)
  
Example mutations:
  Original:         if (x > 0) return true;
  Mutation 1:       if (x >= 0) return true;    // Boundary mutation
  Mutation 2:       if (x < 0) return true;     // Negate condition
  Mutation 3:       if (x > 0) return false;    // Return value mutation

Mutation Score = killed_mutants / total_mutants
  70-80% = OK, 80-90% = Good, 90%+ = Excellent
  
// Run: mull-runner --test-framework=gtest ./build/my_tests
```

**Testing Pyramid for C++ projects:**
```
           /  \           E2E / Integration Tests
          /    \          (slow, broad, fewer)
         /------\         
        / System  \       System Tests
       /   Tests   \      (medium speed)
      /-------------\     
     /  Integration  \    Component Integration
    /    Tests        \   (mock external deps)
   /-------------------\  
  /   Unit Tests        \ Unit Tests
 /     (fast, focused,   \(GTest/Catch2, mock with GMock)
/       many)              \
```

---

# Part C: Live Coding Questions

---

## Q6: Implement a thread-safe Singleton Logger with log rotation (Live Coding)

### Expected Solution:
```cpp
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <filesystem>
#include <format>

class Logger {
public:
    enum class Level { DEBUG, INFO, WARN, ERROR };

    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(Level level, std::string_view message,
             std::source_location loc = std::source_location::current()) {
        if (level < minLevel_) return;

        std::lock_guard lock(mutex_);

        auto now = std::chrono::system_clock::now();
        auto timeStr = std::format("{:%Y-%m-%d %H:%M:%S}", now);

        auto entry = std::format("[{}] [{}] {} ({}:{})\n",
            timeStr, levelToStr(level), message,
            loc.file_name(), loc.line());

        checkRotation();
        file_ << entry;
        file_.flush();
    }

    void setLevel(Level level) { minLevel_ = level; }
    void setMaxFileSize(size_t bytes) { maxFileSize_ = bytes; }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() : maxFileSize_(10 * 1024 * 1024) {
        openLogFile();
    }

    void openLogFile() {
        file_.open("app.log", std::ios::app);
        currentSize_ = std::filesystem::file_size("app.log");
    }

    void checkRotation() {
        if (currentSize_ >= maxFileSize_) {
            file_.close();
            auto timestamp = std::format("{:%Y%m%d_%H%M%S}",
                std::chrono::system_clock::now());
            std::filesystem::rename("app.log", "app." + timestamp + ".log");
            openLogFile();
            currentSize_ = 0;
        }
    }

    static constexpr const char* levelToStr(Level level) {
        switch (level) {
            case Level::DEBUG: return "DEBUG";
            case Level::INFO:  return "INFO ";
            case Level::WARN:  return "WARN ";
            case Level::ERROR: return "ERROR";
        }
        return " 
        $match = # Set 10: Latest Trends, Best Practices & Live Coding
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: Latest C++ Trends & Technologies (2024-2026)

---

## Q1: What's new in C++23 and what's coming in C++26?

### Answer:

**C++23 Key Features:**
```cpp
// 1. std::expected | Better error handling than exceptions
#include <expected>
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) return std::unexpected("Division by zero");
    return a / b;
}
auto result = divide(10, 3);
if (result) std::cout << *result;  // 3
else std::cout << result.error();  // "Division by zero"

// 2. std::print / std::println | Type-safe formatting
#include <print>
std::println("Hello, {}! You are {} years old.", name, age);
// Replaces: printf (unsafe), cout (verbose), fmt::print (external)

// 3. std::generator | Coroutine-based generator
#include <generator>
std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a;
        a = b;
        b = temp + b;
    }
}
for (int n : fibonacci() | std::views::take(10))
    std::print("{} ", n);  // 0 1 1 2 3 5 8 13 21 34

// 4. std::mdspan | Multi-dimensional array view
#include <mdspan>
std::vector<double> data(rows * cols);
std::mdspan<double, std::dextents<size_t, 2>> matrix(data.data(), rows, cols);
matrix[2, 3] = 42.0;  // Access like 2D array, data is contiguous

// 5. std::flat_map / std::flat_set | Cache-friendly sorted containers
#include <flat_map>
std::flat_map<std::string, int> scores;  // Backed by sorted vectors
scores["Alice"] = 95;  // Cache-friendly iteration, slower insert

// 6. Deducing this (Explicit object parameter)
struct Widget {
    template<typename Self>
    auto& value(this Self&& self) {
        return std::forward<Self>(self).value_;
    }
    // Replaces 4 overloads: const&, &, const&&, &&
};

// 7. std::stacktrace | Programmatic stack traces
#include <stacktrace>
void crashHandler() {
    auto trace = std::stacktrace::current();
    std::println("{}", std::to_string(trace));
}

// 8. Ranges improvements: ranges::to, zip, chunk, slide
auto vec = std::views::iota(1, 10)
         | std::views::filter([](int x) { return x % 2 == 0; })
         | std::ranges::to<std::vector>();  // Materialize range to container

auto pairs = std::views::zip(names, ages);  // Iterate two ranges together
```

**C++26 (Expected/In Progress):**
```
- std::execution (Senders/Receivers) | Standard async framework
- Reflection (compile-time introspection)
- Pattern matching
- Contracts (preconditions, postconditions, assertions)
- std::inplace_vector (fixed-capacity, stack-allocated vector)
- Improved coroutine library support
- std::hive (formerly colony) | pool-like container
```

**C++26 Reflection Deep Dive (P2996 ? approved for C++26):**
```cpp
// Compile-time inspection of types, members, and functions
#include <meta>

struct Config {
    std::string host;
    int port;
    bool useTLS;
};

// Auto-generate JSON serialization via reflection (no macros, no code gen)
template<typename T>
std::string toJSON(const T& obj) {
    std::string result = "{";
    bool first = true;
    template for (constexpr auto member : std::meta::nonstatic_data_members_of(^T)) {
        if (!first) result += ",";
        result += "\"" + std::string(std::meta::identifier_of(member)) + "\":";
        result += serialize(obj.[:member:]);  // Splice operator
        first = false;
    }
    return result + "}";
}
// toJSON(Config{"localhost", 8080, true})
// | {"host":"localhost","port":8080,"useTLS":true}
// No boilerplate! Works for ANY struct!
```

**C++26 Contracts Deep Dive (P2900):**
```cpp
// Preconditions, postconditions, and assertions checked at runtime
int divide(int a, int b)
    pre(b != 0)                          // Precondition
    post(r: r * b == a || a % b != 0)    // Postcondition (r = return value)
{
    return a / b;
}

void processBuffer(std::span<int> buf, size_t idx)
    pre(idx < buf.size())               // Bounds check contract
    pre(!buf.empty())                    // Non-empty contract
{
    buf[idx] *= 2;
}

// Contract violation modes (configurable at build time):
// - enforce: Calls violation handler (default: abort)
// - observe: Log violation but continue (production monitoring)
// - ignore: Zero overhead (same as no contract | release builds)
// 
// Key benefit: Contracts are part of the function signature,
// visible in documentation and IDE tooling
```

### Explanation:
| Feature | Impact | Your Domain |
|---------|--------|-------------|
| `std::expected` | Replaces error codes AND exceptions | Finance (error handling on hot path) |
| `std::mdspan` | Zero-copy multi-dim views | CAD (matrices), Gaming (texture data) |
| `std::flat_map` | Cache-friendly maps | Gaming (ECS), HFT (lookup tables) |
| `std::generator` | Easy lazy sequences | All (data processing pipelines) |
| Senders/Receivers | Standard async model | All (replaces ad-hoc thread pools) |
| Reflection | Auto-serialization, ORM | Enterprise (IBM), reducing boilerplate |
| Contracts | Eliminate ad-hoc assertions | All (API boundaries, safety-critical) |

---

## Q2: What build systems and package managers are current best practice for C++?

### Answer:

**Build Systems (2025-2026):**
| Tool | Status | Best For |
|------|--------|----------|
| CMake 3.28+ | Industry standard | Most projects, modules support |
| Bazel | Growing in enterprise | Large monorepos (Google-style) |
| Meson | Simpler alternative | Small-medium projects |
| build2 | Niche but modern | New projects wanting integrated tooling |

**Package Managers:**
| Tool | Status | Best For |
|------|--------|----------|
| vcpkg | Most popular (Microsoft) | Cross-platform, CMake integration |
| Conan 2.x | Strong enterprise | Complex dependency graphs, binary caching |
| CPM.cmake | Lightweight | CMake-only projects |

**Modern CMake Best Practices:**
```cmake
cmake_minimum_required(VERSION 3.28)
project(MyCADApp VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C++20 Modules support
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# vcpkg integration
find_package(fmt CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS asio)

add_library(geometry_core)
target_sources(geometry_core
    PUBLIC FILE_SET CXX_MODULES FILES
        src/geometry.cppm
        src/topology.cppm
)
target_link_libraries(geometry_core PRIVATE fmt::fmt)

# Sanitizers for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(geometry_core PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(geometry_core PRIVATE
        -fsanitize=address,undefined
    )
endif()
```

---

## Q3: What are Senders/Receivers (std::execution) and why do they matter?

### Answer:
```cpp
// std::execution (P2300) | The future of C++ async
// Currently available via stdexec (reference implementation)
#include <stdexec/execution.hpp>

using namespace stdexec;

// Sender: describes async work (lazy, composable)
auto work = just(42)                          // Start with value 42
          | then([](int x) { return x * 2; }) // Transform
          | then([](int x) { return std::to_string(x); }); // Transform again

// Nothing has executed yet! Senders are lazy descriptions.

// Receiver: consumes the result
// Scheduler: decides WHERE to run (thread pool, GPU, etc.)
auto result = sync_wait(std::move(work));  // Execute and wait
// result = "84"

// Real-world: Async pipeline with error handling
auto pipeline = 
    schedule(threadPool.get_scheduler())   // Run on thread pool
  | then([] { return readFile("data.csv"); })
  | then([](auto data) { return parseCSV(data); })
  | then([](auto records) { return aggregate(records); })
  | upon_error([](auto err) { log("Pipeline failed: ", err); });

// Parallel fan-out
auto parallel = when_all(
    on(gpuScheduler, computePhysics()),
    on(cpuPool, computeAI()),
    on(ioScheduler, loadAssets())
);
```

### Explanation:
**Why this matters (replacing ad-hoc async):**
- **Composable**: Chain operations like ranges for data, but for async work
- **Scheduler-agnostic**: Same code runs on thread pool, GPU, or single-threaded
- **Structured concurrency**: Resources tied to scope, no leaked tasks
- **Zero allocation**: Small async operations don't need heap
- **Replaces**: `std::async` (broken), hand-rolled thread pools, Boost.Asio patterns

**Impact on your domains:**
- **Gaming**: GPU + CPU work orchestration without manual synchronization
- **Finance**: Market data processing pipelines
- **CAD**: Parallel geometry computation with structured cancellation

---

# Part B: Best Practices & Coding Standards

---

## Q4: What are the key principles of modern C++ coding standards?

### Answer:

**Core Guidelines (from ISO C++ Core Guidelines by Stroustrup & Sutter):**

```cpp
// 1. RAII everywhere | no resource leaks possible
{
    auto file = std::ifstream("data.txt");  // Closes automatically
    auto conn = ConnectionPool::acquire();  // Returns automatically
    auto lock = std::scoped_lock(mutex);    // Unlocks automatically
}  // Everything cleaned up, even with exceptions

// 2. Prefer value semantics over pointer semantics
struct Point { double x, y, z; };
Point translate(Point p, Vector v) {  // Pass by value, return by value
    return {p.x + v.dx, p.y + v.dy, p.z + v.dz};
}

// 3. Use const correctly
void process(const std::vector<int>& data);  // Won't modify
int calculateArea() const;                    // Member function won't modify object
const auto& ref = getExpensiveObject();       // Avoid copy

// 4. Prefer composition over inheritance
class Car {
    Engine engine_;          // HAS-A (composition)
    Transmission trans_;     // HAS-A
    // NOT: class Car : public Vehicle, public Engine
};

// 5. Don't use raw new/delete
auto p = std::make_unique<Widget>();     // Not: new Widget()
auto v = std::vector<int>(1000);         // Not: new int[1000]
auto s = std::string("hello");           // Not: strdup/malloc

// 6. Use string_view for non-owning string parameters
void log(std::string_view message);  // No copy, accepts string, string_view, char*

// 7. Use span for non-owning array parameters
void process(std::span<const int> data);  // Accepts vector, array, C-array

// 8. Structured error handling
std::expected<Result, Error> tryOperation();  // C++23
std::optional<int> findValue(Key k);          // C++17 | nullable return

// 9. Use enum class, not plain enum
enum class Color { RED, GREEN, BLUE };  // Scoped, type-safe
// NOT: enum Color { RED, GREEN, BLUE }; // Pollutes namespace

// 10. Mark functions noexcept when they don't throw
void swap(Widget& a, Widget& b) noexcept;
~Widget() noexcept;  // Destructors should ALWAYS be noexcept
```

---

## Q5: What testing strategies do you use for C++ projects?

### Answer:
```cpp
// Testing Pyramid for C++:
//
//        /\
//       /  \     E2E Tests (few, slow)
//      /    \    - Full system tests, integration with hardware/UI
//     /+----+\
//    /        \  Integration Tests (moderate)
//   /          \ - Component interaction, DB, network
//  /+----------+\
// /              \ Unit Tests (many, fast)
//  +------------+  - Pure logic, no I/O, fast (<1ms each)

// === Unit Testing with Google Test ===
#include <gtest/gtest.h>

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book;

    void SetUp() override {
        // Common setup
    }
};

TEST_F(OrderBookTest, MatchingBuyAndSellAtSamePrice) {
    book.addOrder({.side = Side::SELL, .price = 100.0, .quantity = 50});
    auto trades = book.addOrder({.side = Side::BUY, .price = 100.0, .quantity = 30});

    ASSERT_EQ(trades.size(), 1);
    EXPECT_DOUBLE_EQ(trades[0].price, 100.0);
    EXPECT_EQ(trades[0].quantity, 30);
}

TEST_F(OrderBookTest, NoMatchWhenPricesDontCross) {
    book.addOrder({.side = Side::SELL, .price = 101.0, .quantity = 50});
    auto trades = book.addOrder({.side = Side::BUY, .price = 99.0, .quantity = 30});

    EXPECT_TRUE(trades.empty());
    EXPECT_DOUBLE_EQ(book.bestBid(), 99.0);
    EXPECT_DOUBLE_EQ(book.bestAsk(), 101.0);
}

// === Property-based testing (Fuzz testing) ===
// "For any sequence of valid orders, the book invariants hold"
TEST(OrderBookProperty, InvariantsMaintained) {
    OrderBook book;
    std::mt19937 rng(42);

    for (int i = 0; i < 10000; ++i) {
        Order order;
        order.side = (rng() % 2) ? Side::BUY : Side::SELL;
        order.price = 90.0 + (rng() % 2000) / 100.0;
        order.quantity = 1 + rng() % 1000;
        book.addOrder(order);

        // Invariant: bestBid < bestAsk (or one side is empty)
        if (book.bestBid() > 0 && book.bestAsk() > 0) {
            ASSERT_LT(book.bestBid(), book.bestAsk());
        }
    }
}

// === Benchmark tests ===
#include <benchmark/benchmark.h>

static void BM_OrderBookInsert(benchmark::State& state) {
    OrderBook book;
    for (auto _ : state) {
        Order order{.side = Side::BUY, .price = 100.0, .quantity = 100};
        benchmark::DoNotOptimize(book.addOrder(order));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBookInsert);
```

**CI/CD Pipeline:**
```yaml
# GitHub Actions / Jenkins pipeline
stages:
  - build:
      - cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZERS=ON
      - cmake --build build -j $(nproc)
  - unit_test:
      - ctest --test-dir build --output-on-failure
  - sanitizer_test:
      - ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build
  - fuzz_test:
      - ./build/fuzz_target -max_total_time=300
  - integration_test:
      - ./run_integration_tests.sh
  - benchmark:
      - ./build/benchmarks --benchmark_format=json > results.json
      - python compare_benchmarks.py results.json baseline.json
```

**Fuzz Testing (libFuzzer / AFL) ? finding bugs humans miss:**
```cpp
// Fuzz target: Find crashes in your parser
// Build: clang++ -fsanitize=fuzzer,address fuzz_target.cpp -o fuzz_target
// Run:   ./fuzz_target corpus/ -max_total_time=3600

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Feed random bytes to your parser
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        FIXMessage::parse(input);          // Test FIX parser
    } catch (...) {
        // Exceptions are OK | crashes/UB are bugs
    }
    return 0;
}

// Structured fuzzing with protobuf (more targeted):
// Define valid input structure | fuzzer generates valid-ish inputs
// Finds edge cases like:
// - Empty fields, max-length strings, integer overflow
// - Invalid UTF-8, null bytes in strings
// - Deeply nested structures, circular references
```

**Mutation Testing (assessing test quality):**
```
Concept: Modify (mutate) your source code, then run tests.
         If tests still pass | tests are weak (mutation survived)
         If tests fail | tests caught the bug (mutation killed)

Tools for C++:
  - Mull (https://github.com/mull-project/mull) | LLVM-based
  - Dextool (D-lang based, works with C/C++)
  
Example mutations:
  Original:         if (x > 0) return true;
  Mutation 1:       if (x >= 0) return true;    // Boundary mutation
  Mutation 2:       if (x < 0) return true;     // Negate condition
  Mutation 3:       if (x > 0) return false;    // Return value mutation

Mutation Score = killed_mutants / total_mutants
  70-80% = OK, 80-90% = Good, 90%+ = Excellent
  
// Run: mull-runner --test-framework=gtest ./build/my_tests
```

**Testing Pyramid for C++ projects:**
```
           /  \           E2E / Integration Tests
          /    \          (slow, broad, fewer)
         /------\         
        / System  \       System Tests
       /   Tests   \      (medium speed)
      /-------------\     
     /  Integration  \    Component Integration
    /    Tests        \   (mock external deps)
   /-------------------\  
  /   Unit Tests        \ Unit Tests
 /     (fast, focused,   \(GTest/Catch2, mock with GMock)
/       many)              \
```

---

# Part C: Live Coding Questions

---

## Q6: Implement a thread-safe Singleton Logger with log rotation (Live Coding)

### Expected Solution:
```cpp
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <filesystem>
#include <format>

class Logger {
public:
    enum class Level { DEBUG, INFO, WARN, ERROR };

    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(Level level, std::string_view message,
             std::source_location loc = std::source_location::current()) {
        if (level < minLevel_) return;

        std::lock_guard lock(mutex_);

        auto now = std::chrono::system_clock::now();
        auto timeStr = std::format("{:%Y-%m-%d %H:%M:%S}", now);

        auto entry = std::format("[{}] [{}] {} ({}:{})\n",
            timeStr, levelToStr(level), message,
            loc.file_name(), loc.line());

        checkRotation();
        file_ << entry;
        file_.flush();
    }

    void setLevel(Level level) { minLevel_ = level; }
    void setMaxFileSize(size_t bytes) { maxFileSize_ = bytes; }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() : maxFileSize_(10 * 1024 * 1024) {
        openLogFile();
    }

    void openLogFile() {
        file_.open("app.log", std::ios::app);
        currentSize_ = std::filesystem::file_size("app.log");
    }

    void checkRotation() {
        if (currentSize_ >= maxFileSize_) {
            file_.close();
            auto timestamp = std::format("{:%Y%m%d_%H%M%S}",
                std::chrono::system_clock::now());
            std::filesystem::rename("app.log", "app." + timestamp + ".log");
            openLogFile();
            currentSize_ = 0;
        }
    }

    static constexpr const char* levelToStr(Level level) {
        switch (level) {
            case Level::DEBUG: return "DEBUG";
            case Level::INFO:  return "INFO ";
            case Level::WARN:  return "WARN ";
            case Level::ERROR: return "ERROR";
        }
        return "+---+";
    }

    std::ofstream file_;
    std::mutex mutex_;
    Level minLevel_ = Level::INFO;
    size_t maxFileSize_;
    size_t currentSize_ = 0;
};

// Usage
#define LOG_DEBUG(msg) Logger::instance().log(Logger::Level::DEBUG, msg)
#define LOG_INFO(msg)  Logger::instance().log(Logger::Level::INFO, msg)
#define LOG_ERROR(msg) Logger::instance().log(Logger::Level::ERROR, msg)
```

### Interviewer Evaluation Points:
- Thread safety (mutex on write path)
- Meyer's singleton (thread-safe in C++11+)
- Log rotation to prevent disk exhaustion
- `std::source_location` (C++20) for automatic file/line
- Proper RAII for file handles
- Level filtering before taking lock (optimization)

---

## Q7: Implement `std::shared_ptr` from scratch (Live Coding)

### Expected Solution:
```cpp
#include <atomic>
#include <utility>

template<typename T>
class SharedPtr {
    struct ControlBlock {
        std::atomic<int> refCount{1};
        std::atomic<int> weakCount{0};
    };

    T* ptr_ = nullptr;
    ControlBlock* ctrl_ = nullptr;

    void release() {
        if (ctrl_) {
            if (ctrl_->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete ptr_;
                if (ctrl_->weakCount.load(std::memory_order_acquire) == 0) {
                    delete ctrl_;
                }
            }
        }
        ptr_ = nullptr;
        ctrl_ = nullptr;
    }

public:
    SharedPtr() = default;

    explicit SharedPtr(T* ptr) : ptr_(ptr), ctrl_(new ControlBlock{}) {}

    // Copy | increment ref count
    SharedPtr(const SharedPtr& other) : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        if (ctrl_) {
            ctrl_->refCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Move | transfer ownership
    SharedPtr(SharedPtr&& other) noexcept
        : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        other.ptr_ = nullptr;
        other.ctrl_ = nullptr;
    }

    // Copy assignment
    SharedPtr& operator=(const SharedPtr& other) {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ctrl_ = other.ctrl_;
            if (ctrl_) {
                ctrl_->refCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }

    // Move assignment
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ctrl_ = other.ctrl_;
            other.ptr_ = nullptr;
            other.ctrl_ = nullptr;
        }
        return *this;
    }

    ~SharedPtr() { release(); }

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }

    int use_count() const {
        return ctrl_ ? ctrl_->refCount.load(std::memory_order_relaxed) : 0;
    }

    explicit operator bool() const { return ptr_ != nullptr; }

    void reset() { release(); }
    void reset(T* ptr) {
        release();
        ptr_ = ptr;
        ctrl_ = new ControlBlock{};
    }
};

// make_shared equivalent | single allocation for object + control block
template<typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    // In production, allocate T and ControlBlock together
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}
```

### Interviewer Evaluation Points:
- Atomic reference counting (thread-safe)
- Proper copy/move semantics (Rule of 5)
- Self-assignment check
- `release()` handles deletion correctly
- Memory ordering awareness
- Knows `make_shared` optimization (single allocation)

---

## Q8: Design and implement a simple compile-time reflection system (Live Coding)

### Expected Solution:
```cpp
#include <string_view>
#include <tuple>
#include <iostream>

// Macro-based reflection (until C++26 static reflection)
#define REFLECT(Type, ...) \
    static constexpr auto reflectedFields() { \
        return std::make_tuple(__VA_ARGS__); \
    } \
    static constexpr std::string_view typeName() { return #Type; }

#define FIELD(name) \
    std::pair{std::string_view{#name}, &std::remove_pointer_t<decltype(this)>::name}

// Usage
struct Person {
    std::string name;
    int age;
    double salary;

    REFLECT(Person, FIELD(name), FIELD(age), FIELD(salary))
};

// Generic serializer using reflection
template<typename T>
void toJSON(const T& obj, std::ostream& out) {
    out << "{ ";
    bool first = true;

    std::apply([&](auto&&... fields) {
        ((
            out << (first ? "" : ", "),
            out << "\"" << fields.first << "\": ",
            printValue(out, obj.*(fields.second)),
            first = false
        ), ...);
    }, T::reflectedFields());

    out << " }";
}

template<typename T>
void printValue(std::ostream& out, const T& val) {
    if constexpr (std::is_same_v<T, std::string>) {
        out << "\"" << val << "\"";
    } else {
        out << val;
    }
}

// Usage
void example() {
    Person p{"Alice", 30, 95000.0};
    toJSON(p, std::cout);
    // Output: { "name": "Alice", "age": 30, "salary": 95000 }
}
```

### Explanation:
- Uses **fold expressions** and **`std::apply`** for iteration over tuple
- **`if constexpr`** for type-specific serialization
- **Pointer-to-member** (`&Person::name`) for field access
- **C++26 will have real reflection** | this pattern will be replaced by `std::meta::members_of(^Person)`

---

## Q9: Implement a simple coroutine-based task system (Live Coding)

### Expected Solution:
```cpp
#include <coroutine>
#include <optional>
#include <iostream>
#include <queue>
#include <functional>

// Simple Task coroutine
struct Task {
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() { if (handle) handle.destroy(); }
    Task(Task&& t) noexcept : handle(t.handle) { t.handle = nullptr; }
    Task& operator=(Task&&) = delete;
};

// Scheduler
class Scheduler {
    std::queue<std::coroutine_handle<>> ready_;

public:
    // Custom awaiter that yields to scheduler
    auto suspend() {
        struct Awaiter {
            Scheduler& sched;
            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<> h) {
                sched.ready_.push(h);  // Re-enqueue
            }
            void await_resume() {}
        };
        return Awaiter{*this};
    }

    void schedule(std::coroutine_handle<> handle) {
        ready_.push(handle);
    }

    void run() {
        while (!ready_.empty()) {
            auto handle = ready_.front();
            ready_.pop();
            if (!handle.done()) {
                handle.resume();
            }
        }
    }
};

// Usage
Scheduler scheduler;

Task taskA() {
    std::cout << "A: step 1\n";
    co_await scheduler.suspend();
    std::cout << "A: step 2\n";
    co_await scheduler.suspend();
    std::cout << "A: step 3\n";
}

Task taskB() {
    std::cout << "B: step 1\n";
    co_await scheduler.suspend();
    std::cout << "B: step 2\n";
}
```

---

# ENHANCED SECTION: Industry Trends 2025-2026

> *What senior architects and principal engineers are actually discussing and adopting in production.*

---

## Q6: How is Rust interop changing the C++ ecosystem? When should you consider Rust?

### Answer:
```cpp
// C++ calling Rust via C FFI (extern "C")
// Rust side:
// #[no_mangle]
// pub extern "C" fn rust_parse_json(input: *const c_char, len: usize) -> *mut JsonDoc

// C++ side:
extern "C" {
    void* rust_parse_json(const char* input, size_t len);
    void rust_free_json(void* doc);
}

// Modern approach: cxx bridge (type-safe, no unsafe FFI)
// Generates both C++ and Rust code from a shared definition
```

**When to use Rust vs C++:**
| Factor | Stay C++ | Consider Rust |
|--------|----------|---------------|
| Existing codebase | Large C++ codebase (iCluster: 500K+ lines) | Greenfield project |
| Team expertise | Team knows C++ deeply | Team willing to learn |
| Memory safety | Using sanitizers + static analysis | Need compile-time guarantees |
| Libraries needed | Mature C++ ecosystem (Boost, Qt) | cargo ecosystem sufficient |
| Platform | IBM i, embedded, legacy OS | Linux/macOS/Windows |
| Hiring | More C++ developers available | Growing Rust talent pool |

### Explanation:
**Senior perspective**: The question isn't "C++ or Rust" | it's "where does each fit in our stack?" Production systems increasingly use both: Rust for new security-sensitive components (parsers, network handling), C++ for existing engines (CAD kernels, game engines, trading systems). The iCluster communication layer (DMKAPI) handles encryption and protocol parsing -> these would benefit from Rust's memory safety if starting fresh.

---

## Q7: Explain Observability (Metrics, Logs, Traces) for C++ systems.

### Answer:
```
The Three Pillars:

1. METRICS (quantitative | Prometheus, Grafana)
   counter: requests_total, errors_total
   gauge: active_connections, queue_depth  
   histogram: request_latency_ms (p50, p95, p99)
   
   iCluster metrics: apply_latency, journal_position_lag,
                     staging_store_depth, link_check_failures

2. LOGS (qualitative | structured JSON, ELK stack)
   {"timestamp":"...","level":"ERROR","component":"OMMIRROR",
    "group":"PAYROLL","message":"Send failed","retry":3}
   
   iCluster uses DMKLOG with severity levels and event forwarding

3. TRACES (request flow | OpenTelemetry, Jaeger)
   Trace ID propagated: Source | Network | Target
   Span: "SAVOBJ" (200ms) | "NetworkSend" (50ms) | "RSTOBJ" (300ms)
   
   iCluster uses conversation tags (conv_slot) and packet counters
   as correlation IDs | identical concept to trace IDs
```

### Explanation:
Modern observability answers three questions: "What's broken?" (metrics), "Why?" (logs), "Where in the flow?" (traces). iCluster already implements all three, just with 1990s terminology. Understanding the mapping between iCluster's monitoring and modern observability helps explain your experience to interviewers at cloud-native companies.

---

## Q8: What is Platform Engineering and how does it affect C++ system design?

### Answer:
```
Platform Engineering = Building internal developer platforms (IDPs)
that make application teams self-service

For C++ teams:
1. BUILD PLATFORM
   - Standardized CMake templates, vcpkg manifests
   - Remote build caching (Bazel remote cache, ccache)
   - CI/CD pipelines with sanitizers + static analysis
   
2. DEPLOY PLATFORM  
   - Containerized C++ services (minimal Alpine + static binary)
   - Feature flags for runtime configuration
   - Canary deployments with automatic rollback
   
3. OBSERVE PLATFORM
   - Standardized metrics library (OpenTelemetry C++ SDK)
   - Centralized logging with structured format
   - Distributed tracing for multi-service architectures

4. TEST PLATFORM
   - Fuzz testing infrastructure (OSS-Fuzz integration)
   - Performance regression detection (automated benchmarks)
   - Chaos engineering for resilience testing
```

### Explanation:
**Senior architect note**: Platform engineering is how you scale engineering practices across hundreds of developers. Instead of each team reinventing CI/CD, build systems, and observability, a platform team provides golden paths. For iCluster, the build system (BLDDMKPGM*.CLLE files) IS a platform -> it standardizes how every module is compiled, linked, and deployed.

---
void example() {
    auto a = taskA();
    auto b = taskB();
    scheduler.schedule(a.handle);
    scheduler.schedule(b.handle);
    scheduler.run();
    // Output (interleaved):
    // A: step 1
    // B: step 1
    // A: step 2
    // B: step 2
    // A: step 3
}
```

### Explanation:
- **Cooperative multitasking** without threads
- **Custom awaiter** controls suspension/resumption behavior
- **Scheduler** maintains a ready queue of coroutine handles
- **Use cases**: Game loop (entity updates), async I/O event loops, state machines

---

## Q10: Quick-fire coding questions (expect these as warm-up or final round)

### Q10a: Implement `string_to_int` (atoi) handling edge cases
```cpp
#include <string_view>
#include <optional>
#include <limits>

std::optional<int> stringToInt(std::string_view str) {
    if (str.empty()) return std::nullopt;

    size_t i = 0;
    // Skip whitespace
    while (i < str.size() && str[i] == ' ') ++i;
    if (i == str.size()) return std::nullopt;

    // Handle sign
    bool negative = false;
    if (str[i] == '-' || str[i] == '+') {
        negative = (str[i] == '-');
        ++i;
    }

    if (i == str.size() || !std::isdigit(str[i]))
        return std::nullopt;

    long long result = 0;
    while (i < str.size() && std::isdigit(str[i])) {
        result = result * 10 + (str[i] - '0');
        // Overflow check
        if (!negative && result > std::numeric_limits<int>::max())
            return std::nullopt;
        if (negative && -result < std::numeric_limits<int>::min())
            return std::nullopt;
        ++i;
    }

    return static_cast<int>(negative -> -result : result);
}
```

### Q10b: Reverse words in a string in-place
```cpp
void reverseWords(std::string& s) {
    // Step 1: Reverse entire string
    std::reverse(s.begin(), s.end());

    // Step 2: Reverse each word
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == ' ') {
            std::reverse(s.begin() + start, s.begin() + i);
            start = i + 1;
        }
    }

    // Step 3: Clean up extra spaces (if needed)
    // "hello world" ? "world hello"
}
```

### Q10c: Check if a binary tree is balanced
```cpp
struct TreeNode {
    int val;
    TreeNode* left;
    TreeNode* right;
};

bool isBalanced(TreeNode* root) {
    return height(root) != -1;
}

int height(TreeNode* node) {
    if (!node) return 0;
    int left = height(node->left);
    if (left == -1) return -1;
    int right = height(node->right);
    if (right == -1) return -1;
    if (std::abs(left - right) > 1) return -1;
    return 1 + std::max(left, right);
}
// O(n) time, O(h) space -> single pass, not the naive O(n²) approach
```

### Q10d: Detect cycle in linked list and find the start
```cpp
ListNode* detectCycle(ListNode* head) {
    ListNode* slow = head;
    ListNode* fast = head;

    // Phase 1: Detect cycle
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) break;
    }

    if (!fast || !fast->next) return nullptr;  // No cycle

    // Phase 2: Find cycle start
    slow = head;
    while (slow != fast) {
        slow = slow->next;
        fast = fast->next;
    }
    return slow;  // Start of cycle
}
// Floyd's algorithm -> O(n) time, O(1) space
```

### Q10e: Implement a moving average calculator
```cpp
class MovingAverage {
    std::queue<double> window_;
    int maxSize_;
    double sum_ = 0;

public:
    MovingAverage(int size) : maxSize_(size) {}

    double next(double val) {
        window_.push(val);
        sum_ += val;
        if (static_cast<int>(window_.size()) > maxSize_) {
            sum_ -= window_.front();
            window_.pop();
        }
        return sum_ / window_.size();
    }
};
// O(1) per update -> used in finance for technical analysis (SMA)
```

---

# Summary: Interview Preparation Checklist

```
- Modern C++ (C++17/20/23) | concepts, ranges, coroutines, modules
- Data Structures & Algorithms -> implement from scratch, complexity analysis
- Design Patterns -> Strategy, Observer, Factory, Command, CRTP, Type Erasure
- LLD -> Parking Lot, Chess, Elevator, File System, Rate Limiter
- HLD -> CAD collaboration, Trading system, Game server, Build system
- Multithreading -> Memory model, thread pool, lock-free, atomics
- Memory & Performance -> Allocators, cache optimization, SIMD, profiling
- Domain Knowledge -> CAD geometry, Game ECS/loop, Finance FIX/OrderBook
- Projects -> 2-3 deep stories with STAR-T framework, quantified results
- Behavioral -> Disagreements, mentoring, incidents, estimation
- Latest trends -> C++23 features, build tools, Senders/Receivers
- Live coding -> Practice timed implementation (45-60 min problems)
```

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    ";
    }

    std::ofstream file_;
    std::mutex mutex_;
    Level minLevel_ = Level::INFO;
    size_t maxFileSize_;
    size_t currentSize_ = 0;
};

// Usage
#define LOG_DEBUG(msg) Logger::instance().log(Logger::Level::DEBUG, msg)
#define LOG_INFO(msg)  Logger::instance().log(Logger::Level::INFO, msg)
#define LOG_ERROR(msg) Logger::instance().log(Logger::Level::ERROR, msg)
```

### Interviewer Evaluation Points:
- Thread safety (mutex on write path)
- Meyer's singleton (thread-safe in C++11+)
- Log rotation to prevent disk exhaustion
- `std::source_location` (C++20) for automatic file/line
- Proper RAII for file handles
- Level filtering before taking lock (optimization)

---

## Q7: Implement `std::shared_ptr` from scratch (Live Coding)

### Expected Solution:
```cpp
#include <atomic>
#include <utility>

template<typename T>
class SharedPtr {
    struct ControlBlock {
        std::atomic<int> refCount{1};
        std::atomic<int> weakCount{0};
    };

    T* ptr_ = nullptr;
    ControlBlock* ctrl_ = nullptr;

    void release() {
        if (ctrl_) {
            if (ctrl_->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete ptr_;
                if (ctrl_->weakCount.load(std::memory_order_acquire) == 0) {
                    delete ctrl_;
                }
            }
        }
        ptr_ = nullptr;
        ctrl_ = nullptr;
    }

public:
    SharedPtr() = default;

    explicit SharedPtr(T* ptr) : ptr_(ptr), ctrl_(new ControlBlock{}) {}

    // Copy | increment ref count
    SharedPtr(const SharedPtr& other) : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        if (ctrl_) {
            ctrl_->refCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Move | transfer ownership
    SharedPtr(SharedPtr&& other) noexcept
        : ptr_(other.ptr_), ctrl_(other.ctrl_) {
        other.ptr_ = nullptr;
        other.ctrl_ = nullptr;
    }

    // Copy assignment
    SharedPtr& operator=(const SharedPtr& other) {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ctrl_ = other.ctrl_;
            if (ctrl_) {
                ctrl_->refCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }

    // Move assignment
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ctrl_ = other.ctrl_;
            other.ptr_ = nullptr;
            other.ctrl_ = nullptr;
        }
        return *this;
    }

    ~SharedPtr() { release(); }

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }

    int use_count() const {
        return ctrl_ ? ctrl_->refCount.load(std::memory_order_relaxed) : 0;
    }

    explicit operator bool() const { return ptr_ != nullptr; }

    void reset() { release(); }
    void reset(T* ptr) {
        release();
        ptr_ = ptr;
        ctrl_ = new ControlBlock{};
    }
};

// make_shared equivalent | single allocation for object + control block
template<typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    // In production, allocate T and ControlBlock together
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}
```

### Interviewer Evaluation Points:
- Atomic reference counting (thread-safe)
- Proper copy/move semantics (Rule of 5)
- Self-assignment check
- `release()` handles deletion correctly
- Memory ordering awareness
- Knows `make_shared` optimization (single allocation)

---

## Q8: Design and implement a simple compile-time reflection system (Live Coding)

### Expected Solution:
```cpp
#include <string_view>
#include <tuple>
#include <iostream>

// Macro-based reflection (until C++26 static reflection)
#define REFLECT(Type, ...) \
    static constexpr auto reflectedFields() { \
        return std::make_tuple(__VA_ARGS__); \
    } \
    static constexpr std::string_view typeName() { return #Type; }

#define FIELD(name) \
    std::pair{std::string_view{#name}, &std::remove_pointer_t<decltype(this)>::name}

// Usage
struct Person {
    std::string name;
    int age;
    double salary;

    REFLECT(Person, FIELD(name), FIELD(age), FIELD(salary))
};

// Generic serializer using reflection
template<typename T>
void toJSON(const T& obj, std::ostream& out) {
    out << "{ ";
    bool first = true;

    std::apply([&](auto&&... fields) {
        ((
            out << (first ? "" : ", "),
            out << "\"" << fields.first << "\": ",
            printValue(out, obj.*(fields.second)),
            first = false
        ), ...);
    }, T::reflectedFields());

    out << " }";
}

template<typename T>
void printValue(std::ostream& out, const T& val) {
    if constexpr (std::is_same_v<T, std::string>) {
        out << "\"" << val << "\"";
    } else {
        out << val;
    }
}

// Usage
void example() {
    Person p{"Alice", 30, 95000.0};
    toJSON(p, std::cout);
    // Output: { "name": "Alice", "age": 30, "salary": 95000 }
}
```

### Explanation:
- Uses **fold expressions** and **`std::apply`** for iteration over tuple
- **`if constexpr`** for type-specific serialization
- **Pointer-to-member** (`&Person::name`) for field access
- **C++26 will have real reflection** | this pattern will be replaced by `std::meta::members_of(^Person)`

---

## Q9: Implement a simple coroutine-based task system (Live Coding)

### Expected Solution:
```cpp
#include <coroutine>
#include <optional>
#include <iostream>
#include <queue>
#include <functional>

// Simple Task coroutine
struct Task {
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~Task() { if (handle) handle.destroy(); }
    Task(Task&& t) noexcept : handle(t.handle) { t.handle = nullptr; }
    Task& operator=(Task&&) = delete;
};

// Scheduler
class Scheduler {
    std::queue<std::coroutine_handle<>> ready_;

public:
    // Custom awaiter that yields to scheduler
    auto suspend() {
        struct Awaiter {
            Scheduler& sched;
            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<> h) {
                sched.ready_.push(h);  // Re-enqueue
            }
            void await_resume() {}
        };
        return Awaiter{*this};
    }

    void schedule(std::coroutine_handle<> handle) {
        ready_.push(handle);
    }

    void run() {
        while (!ready_.empty()) {
            auto handle = ready_.front();
            ready_.pop();
            if (!handle.done()) {
                handle.resume();
            }
        }
    }
};

// Usage
Scheduler scheduler;

Task taskA() {
    std::cout << "A: step 1\n";
    co_await scheduler.suspend();
    std::cout << "A: step 2\n";
    co_await scheduler.suspend();
    std::cout << "A: step 3\n";
}

Task taskB() {
    std::cout << "B: step 1\n";
    co_await scheduler.suspend();
    std::cout << "B: step 2\n";
}
```

---

# ENHANCED SECTION: Industry Trends 2025-2026

> *What senior architects and principal engineers are actually discussing and adopting in production.*

---

## Q6: How is Rust interop changing the C++ ecosystem? When should you consider Rust?

### Answer:
```cpp
// C++ calling Rust via C FFI (extern "C")
// Rust side:
// #[no_mangle]
// pub extern "C" fn rust_parse_json(input: *const c_char, len: usize) -> *mut JsonDoc

// C++ side:
extern "C" {
    void* rust_parse_json(const char* input, size_t len);
    void rust_free_json(void* doc);
}

// Modern approach: cxx bridge (type-safe, no unsafe FFI)
// Generates both C++ and Rust code from a shared definition
```

**When to use Rust vs C++:**
| Factor | Stay C++ | Consider Rust |
|--------|----------|---------------|
| Existing codebase | Large C++ codebase (iCluster: 500K+ lines) | Greenfield project |
| Team expertise | Team knows C++ deeply | Team willing to learn |
| Memory safety | Using sanitizers + static analysis | Need compile-time guarantees |
| Libraries needed | Mature C++ ecosystem (Boost, Qt) | cargo ecosystem sufficient |
| Platform | IBM i, embedded, legacy OS | Linux/macOS/Windows |
| Hiring | More C++ developers available | Growing Rust talent pool |

### Explanation:
**Senior perspective**: The question isn't "C++ or Rust" | it's "where does each fit in our stack?" Production systems increasingly use both: Rust for new security-sensitive components (parsers, network handling), C++ for existing engines (CAD kernels, game engines, trading systems). The iCluster communication layer (DMKAPI) handles encryption and protocol parsing -> these would benefit from Rust's memory safety if starting fresh.

---

## Q7: Explain Observability (Metrics, Logs, Traces) for C++ systems.

### Answer:
```
The Three Pillars:

1. METRICS (quantitative | Prometheus, Grafana)
   counter: requests_total, errors_total
   gauge: active_connections, queue_depth  
   histogram: request_latency_ms (p50, p95, p99)
   
   iCluster metrics: apply_latency, journal_position_lag,
                     staging_store_depth, link_check_failures

2. LOGS (qualitative | structured JSON, ELK stack)
   {"timestamp":"...","level":"ERROR","component":"OMMIRROR",
    "group":"PAYROLL","message":"Send failed","retry":3}
   
   iCluster uses DMKLOG with severity levels and event forwarding

3. TRACES (request flow | OpenTelemetry, Jaeger)
   Trace ID propagated: Source | Network | Target
   Span: "SAVOBJ" (200ms) | "NetworkSend" (50ms) | "RSTOBJ" (300ms)
   
   iCluster uses conversation tags (conv_slot) and packet counters
   as correlation IDs | identical concept to trace IDs
```

### Explanation:
Modern observability answers three questions: "What's broken?" (metrics), "Why?" (logs), "Where in the flow?" (traces). iCluster already implements all three, just with 1990s terminology. Understanding the mapping between iCluster's monitoring and modern observability helps explain your experience to interviewers at cloud-native companies.

---

## Q8: What is Platform Engineering and how does it affect C++ system design?

### Answer:
```
Platform Engineering = Building internal developer platforms (IDPs)
that make application teams self-service

For C++ teams:
1. BUILD PLATFORM
   - Standardized CMake templates, vcpkg manifests
   - Remote build caching (Bazel remote cache, ccache)
   - CI/CD pipelines with sanitizers + static analysis
   
2. DEPLOY PLATFORM  
   - Containerized C++ services (minimal Alpine + static binary)
   - Feature flags for runtime configuration
   - Canary deployments with automatic rollback
   
3. OBSERVE PLATFORM
   - Standardized metrics library (OpenTelemetry C++ SDK)
   - Centralized logging with structured format
   - Distributed tracing for multi-service architectures

4. TEST PLATFORM
   - Fuzz testing infrastructure (OSS-Fuzz integration)
   - Performance regression detection (automated benchmarks)
   - Chaos engineering for resilience testing
```

### Explanation:
**Senior architect note**: Platform engineering is how you scale engineering practices across hundreds of developers. Instead of each team reinventing CI/CD, build systems, and observability, a platform team provides golden paths. For iCluster, the build system (BLDDMKPGM*.CLLE files) IS a platform -> it standardizes how every module is compiled, linked, and deployed.

---
void example() {
    auto a = taskA();
    auto b = taskB();
    scheduler.schedule(a.handle);
    scheduler.schedule(b.handle);
    scheduler.run();
    // Output (interleaved):
    // A: step 1
    // B: step 1
    // A: step 2
    // B: step 2
    // A: step 3
}
```

### Explanation:
- **Cooperative multitasking** without threads
- **Custom awaiter** controls suspension/resumption behavior
- **Scheduler** maintains a ready queue of coroutine handles
- **Use cases**: Game loop (entity updates), async I/O event loops, state machines

---

## Q10: Quick-fire coding questions (expect these as warm-up or final round)

### Q10a: Implement `string_to_int` (atoi) handling edge cases
```cpp
#include <string_view>
#include <optional>
#include <limits>

std::optional<int> stringToInt(std::string_view str) {
    if (str.empty()) return std::nullopt;

    size_t i = 0;
    // Skip whitespace
    while (i < str.size() && str[i] == ' ') ++i;
    if (i == str.size()) return std::nullopt;

    // Handle sign
    bool negative = false;
    if (str[i] == '-' || str[i] == '+') {
        negative = (str[i] == '-');
        ++i;
    }

    if (i == str.size() || !std::isdigit(str[i]))
        return std::nullopt;

    long long result = 0;
    while (i < str.size() && std::isdigit(str[i])) {
        result = result * 10 + (str[i] - '0');
        // Overflow check
        if (!negative && result > std::numeric_limits<int>::max())
            return std::nullopt;
        if (negative && -result < std::numeric_limits<int>::min())
            return std::nullopt;
        ++i;
    }

    return static_cast<int>(negative -> -result : result);
}
```

### Q10b: Reverse words in a string in-place
```cpp
void reverseWords(std::string& s) {
    // Step 1: Reverse entire string
    std::reverse(s.begin(), s.end());

    // Step 2: Reverse each word
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == ' ') {
            std::reverse(s.begin() + start, s.begin() + i);
            start = i + 1;
        }
    }

    // Step 3: Clean up extra spaces (if needed)
    // "hello world" ? "world hello"
}
```

### Q10c: Check if a binary tree is balanced
```cpp
struct TreeNode {
    int val;
    TreeNode* left;
    TreeNode* right;
};

bool isBalanced(TreeNode* root) {
    return height(root) != -1;
}

int height(TreeNode* node) {
    if (!node) return 0;
    int left = height(node->left);
    if (left == -1) return -1;
    int right = height(node->right);
    if (right == -1) return -1;
    if (std::abs(left - right) > 1) return -1;
    return 1 + std::max(left, right);
}
// O(n) time, O(h) space -> single pass, not the naive O(n²) approach
```

### Q10d: Detect cycle in linked list and find the start
```cpp
ListNode* detectCycle(ListNode* head) {
    ListNode* slow = head;
    ListNode* fast = head;

    // Phase 1: Detect cycle
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) break;
    }

    if (!fast || !fast->next) return nullptr;  // No cycle

    // Phase 2: Find cycle start
    slow = head;
    while (slow != fast) {
        slow = slow->next;
        fast = fast->next;
    }
    return slow;  // Start of cycle
}
// Floyd's algorithm -> O(n) time, O(1) space
```

### Q10e: Implement a moving average calculator
```cpp
class MovingAverage {
    std::queue<double> window_;
    int maxSize_;
    double sum_ = 0;

public:
    MovingAverage(int size) : maxSize_(size) {}

    double next(double val) {
        window_.push(val);
        sum_ += val;
        if (static_cast<int>(window_.size()) > maxSize_) {
            sum_ -= window_.front();
            window_.pop();
        }
        return sum_ / window_.size();
    }
};
// O(1) per update -> used in finance for technical analysis (SMA)
```

---

# Summary: Interview Preparation Checklist

```
- Modern C++ (C++17/20/23) | concepts, ranges, coroutines, modules
- Data Structures & Algorithms -> implement from scratch, complexity analysis
- Design Patterns -> Strategy, Observer, Factory, Command, CRTP, Type Erasure
- LLD -> Parking Lot, Chess, Elevator, File System, Rate Limiter
- HLD -> CAD collaboration, Trading system, Game server, Build system
- Multithreading -> Memory model, thread pool, lock-free, atomics
- Memory & Performance -> Allocators, cache optimization, SIMD, profiling
- Domain Knowledge -> CAD geometry, Game ECS/loop, Finance FIX/OrderBook
- Projects -> 2-3 deep stories with STAR-T framework, quantified results
- Behavioral -> Disagreements, mentoring, incidents, estimation
- Latest trends -> C++23 features, build tools, Senders/Receivers
- Live coding -> Practice timed implementation (45-60 min problems)
```

---
