# Set 7: Memory Management & Performance Optimization
## Senior-Level Interview Questions (10 Years Experience)

---

## Q1: Explain the C++ memory layout. Where are stack, heap, BSS, data, and text segments?

### Answer:

```
High Address +------------------------+
|                                      |
|           Stack                      |  | Local variables, function params, return addresses
|           (grows |)                  |  | Fixed size per thread (default ~1-8MB)
|                                      |
+--------------------------------------+
|                                      |
|           | Free Space |             |
|                                      |
+--------------------------------------+
|                                      |
|           Heap                       |  | Dynamic allocation (new/malloc)
|           (grows |)                  |  | Managed by allocator (ptmalloc, jemalloc, tcmalloc)
|                                      |
+--------------------------------------+
|           BSS Segment                |  | Uninitialized globals/statics (zeroed by OS)
+--------------------------------------+
|           Data Segment               |  | Initialized globals/statics
+--------------------------------------+
|           Text (Code) Segment        |  | Machine instructions (read-only)
Low Address +-------------------------+
```

```cpp
int globalInit = 42;              // Data segment
int globalUninit;                 // BSS segment
static int staticVar = 10;       // Data segment
const char* str = "hello";       // "hello" in Text (read-only), str pointer in Data

void function() {
    int localVar = 5;             // Stack
    static int staticLocal = 0;   // Data segment (persists across calls)
    int* heapVar = new int(10);   // Pointer on stack, int on heap
    delete heapVar;

    char buffer[1024];            // Stack (1KB) → fast
    // char hugeBuffer[10'000'000]; // Stack overflow! Use heap for large allocations
}
```

### Explanation:
| Segment | Allocation | Lifetime | Speed |
|---------|-----------|----------|-------|
| Stack | Automatic (compiler) | Function scope | Fastest (pointer bump) |
| Heap | Manual (new/malloc) | Until delete/free | Slower (allocator overhead) |
| Data/BSS | At program start | Entire program | N/A (static) |
| Text | At load time | Entire program | N/A (read-only) |

**Interview key points:**
- Stack size is limited (~1MB default on Windows, ~8MB on Linux) → don't allocate large arrays on stack
- Heap fragmentation degrades performance over time
- `thread_local` variables get a copy per thread (in TLS segment)
- Virtual memory means addresses are virtual → physical pages allocated on first access (page fault)

**ASLR (Address Space Layout Randomization):**
```
Without ASLR:                    With ASLR:
+----------+ 0xFFFF             +----------+ 0xFFFF
|  Stack   | (fixed)            |  Stack   | (random offset)
+----------+                    +----------+
|  | grows |                    |  | grows |
|          |                    |          |
|  | grows |                    |  | grows |
+----------+                    +----------+
|  Heap    | (fixed)            |  Heap    | (random offset)
+----------+                    +----------+
|  BSS     |                    |  BSS     |
+----------+                    +----------+
|  Data    |                    |  Data    |
+----------+                    +----------+
|  Text    | (fixed)            |  Text    | (random, PIE)
+----------+ 0x0000             +----------+ 0x0000

// ASLR randomizes base addresses of stack, heap, shared libraries, and executable
// Security: Makes return-oriented programming (ROP) attacks much harder
// Performance: No impact → virtual addresses are translated via page table regardless
// Disable for debugging: setarch -R ./program (Linux)
```

**Thread-Local Storage (TLS) segment detail:**
```cpp
thread_local int perThreadCounter = 0;  // Each thread gets its own copy

// Implementation:
// - ELF: __thread attribute → TLS block in thread control block (TCB)
// - Windows: __declspec(thread) → TLS directory in PE header
// - Access: fs/gs segment register on x86-64 ? zero overhead for read/write
// - Size: Each thread_local variable adds to per-thread memory footprint
// - Gotcha: thread_local + DLL = initialization order issues on Windows
```

---

## Q2: What are custom allocators? Implement a pool allocator and explain when to use one.

### Answer:
```cpp
#include <cstdlib>
#include <vector>
#include <memory>
#include <cassert>

// Fixed-size block pool allocator → O(1) alloc/dealloc, zero fragmentation
template<typename T>
class PoolAllocator {
    struct Block {
        Block* next;
    };

    std::vector<void*> chunks_;  // Owns allocated chunks
    Block* freeList_ = nullptr;
    size_t blockSize_;
    size_t chunkSize_;

    void allocateChunk() {
        blockSize_ = std::max(sizeof(T), sizeof(Block));
        void* chunk = std::aligned_alloc(alignof(T), blockSize_ * chunkSize_);
        chunks_.push_back(chunk);

        // Build free list
        char* p = static_cast<char*>(chunk);
        for (size_t i = 0; i < chunkSize_ - 1; ++i) {
            reinterpret_cast<Block*>(p + i * blockSize_)->next =
                reinterpret_cast<Block*>(p + (i + 1) * blockSize_);
        }
        reinterpret_cast<Block*>(p + (chunkSize_ - 1) * blockSize_)->next = freeList_;
        freeList_ = reinterpret_cast<Block*>(chunk);
    }

public:
    PoolAllocator(size_t chunkSize = 1024) : chunkSize_(chunkSize) {
        blockSize_ = std::max(sizeof(T), sizeof(Block));
        allocateChunk();
    }

    ~PoolAllocator() {
        for (void* chunk : chunks_)
            std::free(chunk);
    }

    T* allocate() {
        if (!freeList_) allocateChunk();
        Block* block = freeList_;
        freeList_ = block->next;
        return reinterpret_cast<T*>(block);
    }

    void deallocate(T* ptr) {
        Block* block = reinterpret_cast<Block*>(ptr);
        block->next = freeList_;
        freeList_ = block;
    }
};

// STL-compatible allocator wrapper
template<typename T>
class StlPoolAllocator {
    static PoolAllocator<T>& pool() {
        static PoolAllocator<T> p;
        return p;
    }
public:
    using value_type = T;

    StlPoolAllocator() = default;
    template<typename U>
    StlPoolAllocator(const StlPoolAllocator<U>&) {}

    T* allocate(size_t n) {
        assert(n == 1);  // Pool allocator only does single objects
        return pool().allocate();
    }

    void deallocate(T* p, size_t) {
        pool().deallocate(p);
    }
};

// Usage
void example() {
    // Custom allocator for linked list nodes → huge performance gain
    std::list<int, StlPoolAllocator<int>> myList;
    for (int i = 0; i < 100000; ++i)
        myList.push_back(i);  // Each node allocated from pool in O(1)
}
```

### Explanation:
| Allocator | Alloc Time | Dealloc Time | Fragmentation | Use Case |
|-----------|-----------|-------------|---------------|----------|
| Default (malloc) | O(1) amortized | O(1) amortized | Yes | General purpose |
| Pool | O(1) guaranteed | O(1) guaranteed | None | Fixed-size objects (particles, entities) |
| Arena/Linear | O(1) pointer bump | Free all at once | None | Per-frame game allocations |
| Stack | O(1) LIFO only | O(1) LIFO only | None | Temporary computations |

**When to use pool allocators:**
- **Gaming**: Particle systems (millions of same-size objects), entity components
- **CAD**: Geometry nodes, constraint objects
- **Finance**: Order objects, market data entries
- **General**: Any hot path with frequent alloc/dealloc of same-size objects

---

## Q3: How does virtual memory work? What are page faults, TLB, and huge pages?

### Answer:

```
Virtual Address Space (per process)     Physical Memory (RAM)
+------------------+                    +------------------+
|  Page 0 (4KB)    +---+ Page Table +---+  Frame 47        |
|  Page 1 (4KB)    +---+ Page Table +---+  Frame 12        |
|  Page 2 (4KB)    +---+ NOT MAPPED --> |  (disk/swap)     |  | Page fault!
|  Page 3 (4KB)    +---+ Page Table +---+  Frame 98        |
|  ...             |                    |  ...             |
+------------------+                    +------------------+
```

```cpp
// Page fault demonstration
void pageFaultExample() {
    // Allocating 1GB doesn't immediately use physical memory
    char* buffer = new char[1'000'000'000];  // Virtual pages allocated, not physical

    buffer[0] = 'A';          // PAGE FAULT → OS maps physical page
    buffer[4096] = 'B';       // PAGE FAULT → another physical page
    buffer[999'999'999] = 'C'; // PAGE FAULT → yet another

    // Only 3 physical pages used, not 1GB!
}

// Huge pages → reduce TLB misses
void hugePageExample() {
    #ifdef __linux__
    // Request 2MB huge pages (vs default 4KB)
    void* ptr = mmap(nullptr, 2 * 1024 * 1024,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                     -1, 0);
    #endif

    // Windows: VirtualAlloc with MEM_LARGE_PAGES
    #ifdef _WIN32
    void* ptr = VirtualAlloc(nullptr, 2 * 1024 * 1024,
                             MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
    #endif
}
```

### Explanation:
**TLB (Translation Lookaside Buffer):**
- CPU cache for virtual?physical page mappings
- TLB miss = expensive page table walk (100+ cycles)
- Default pages: 4KB → 1GB data = 256K pages = frequent TLB misses
- **Huge pages**: 2MB (or 1GB) → same 1GB = 512 (or 1) entries = far fewer TLB misses

**Performance impact:**
| Scenario | 4KB Pages | 2MB Huge Pages |
|----------|-----------|---------------|
| TLB entries needed for 1GB | 262,144 | 512 |
| TLB miss rate | High | Very low |
| Performance gain | Baseline | 5-30% for large datasets |

**When to use huge pages:**
- **HFT**: Market data buffers, order books (latency-critical)
- **Gaming**: Texture pools, large world data
- **CAD**: Geometry buffers for large assemblies
- **Databases**: Buffer pool management

---

## Q4: What are cache-friendly data structures? Explain SoA vs AoS.

### Answer:
```cpp
#include <vector>
#include <chrono>

// AoS → Array of Structures (traditional OOP)
struct ParticleAoS {
    float x, y, z;        // Position
    float vx, vy, vz;     // Velocity
    float r, g, b, a;     // Color
    float size;
    float lifetime;
    bool active;
    // ... 52+ bytes per particle
};
std::vector<ParticleAoS> particlesAoS(1'000'000);

// When updating positions, we load 64-byte cache lines
// but only use 24 bytes (x,y,z,vx,vy,vz) → 63% wasted!
void updatePositionsAoS(std::vector<ParticleAoS>& particles, float dt) {
    for (auto& p : particles) {
        if (!p.active) continue;
        p.x += p.vx * dt;  // Cache line loaded: x,y,z,vx,vy,vz,r,g,b,a,size...
        p.y += p.vy * dt;  // All that color/size data pollutes cache
        p.z += p.vz * dt;
    }
}

// SoA → Structure of Arrays (data-oriented design)
struct ParticlesSoA {
    std::vector<float> x, y, z;        // Positions packed together
    std::vector<float> vx, vy, vz;     // Velocities packed together
    std::vector<float> r, g, b, a;     // Colors packed together
    std::vector<float> size;
    std::vector<float> lifetime;
    std::vector<bool> active;
};

void updatePositionsSoA(ParticlesSoA& p, float dt) {
    size_t n = p.x.size();
    for (size_t i = 0; i < n; ++i) {
        p.x[i] += p.vx[i] * dt;  // x[] and vx[] are contiguous in memory
        p.y[i] += p.vy[i] * dt;  // Every byte in cache line is useful!
        p.z[i] += p.vz[i] * dt;
    }
    // Also enables SIMD auto-vectorization!
}

// Hot/Cold splitting → hybrid approach
struct ParticleHot {
    float x, y, z;
    float vx, vy, vz;
};
struct ParticleCold {
    float r, g, b, a;
    float size, lifetime;
    bool active;
};
// Hot data updated every frame; cold data rarely accessed
std::vector<ParticleHot> hotData(1'000'000);
std::vector<ParticleCold> coldData(1'000'000);
```

### Explanation:
| Layout | Cache Efficiency | SIMD Friendly | Code Simplicity |
|--------|-----------------|---------------|-----------------|
| AoS | Poor (loads unused fields) | No | Natural OOP |
| SoA | Excellent (contiguous) | Yes | Less intuitive |
| Hot/Cold | Good compromise | Partial | Moderate |

**Performance difference**: SoA can be **2-5x faster** than AoS for bulk operations on large datasets.

**Why it matters:**
- L1 cache line = 64 bytes, L1 cache → 32-48KB
- L1 cache miss = ~4ns, L2 miss = ~12ns, L3 miss = ~40ns, RAM = ~100ns
- **1M particles at 60fps**: Every cache miss counts

---

## Q5: What tools do you use for profiling and performance analysis in C++?

### Answer:

### Profiling Tools Taxonomy:
```
+--------------------------------------------------+
|                 Profiling Tools                   |
+--------------------------------------------------+
|   Sampling     |  Instrumentation |   Hardware    |
|   Profilers    |  Profilers       |   Counters    |
+--------------------------------------------------+
| perf (Linux)   | Valgrind        | Intel VTune   |
| Instruments    | (Callgrind)     | perf stat     |
| (macOS)        |                 | PAPI          |
| Visual Studio  | gprof           |               |
| Profiler       | Tracy           | AMD uProf     |
| Orbit (Google) | (game focused)  |               |
+--------------------------------------------------+
```

### Practical Usage:
```bash
# 1. perf → CPU profiling (sampling)
perf record -g ./my_app           # Record with call graph
perf report                       # Interactive analysis
perf stat ./my_app                # Hardware counters summary

# 2. Valgrind → Memory and cache analysis
valgrind --tool=cachegrind ./my_app   # Cache miss analysis
valgrind --tool=massif ./my_app        # Heap profiling
valgrind --tool=memcheck ./my_app      # Memory leak detection

# 3. Google Benchmark → Micro-benchmarking
# 4. Address/Thread Sanitizers → Runtime error detection
```

```cpp
// Compile-time: AddressSanitizer, ThreadSanitizer
// g++ -fsanitize=address -g my_app.cpp     # Memory errors
// g++ -fsanitize=thread -g my_app.cpp      # Data races
// g++ -fsanitize=undefined -g my_app.cpp   # UB detection

// Google Benchmark
#include <benchmark/benchmark.h>

static void BM_VectorPush(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        for (int i = 0; i < state.range(0); ++i)
            v.push_back(i);
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_VectorPush)->Range(8, 1 << 20);

static void BM_VectorReserved(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        v.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i)
            v.push_back(i);
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_VectorReserved)->Range(8, 1 << 20);

// In-code profiling with chrono
class ScopedTimer {
    const char* name_;
    std::chrono::high_resolution_clock::time_point start_;
public:
    ScopedTimer(const char* name) : name_(name),
        start_(std::chrono::high_resolution_clock::now()) {}
    ~ScopedTimer() {
        auto elapsed = std::chrono::high_resolution_clock::now() - start_;
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        std::cout << name_ << ": " << us << " ?s\n";
    }
};

#define PROFILE_SCOPE(name) ScopedTimer _timer##__LINE__(name)
```

### Explanation:
| Tool | What it finds | Overhead | When to use |
|------|-------------|----------|-------------|
| perf | CPU hotspots, cache misses | ~2% | First pass profiling |
| VTune | Deep microarch analysis | ~5% | Optimization phase |
| Valgrind/memcheck | Memory leaks, UB | 10-50x | Debug builds |
| ASan/TSan | Memory/thread bugs | 2-3x | CI pipeline |
| Tracy | Frame-by-frame (games) | ~1% | Game/real-time profiling |
| Benchmark | Micro-benchmarks | N/A | Algorithm comparison |

---

## Q6: What are common memory bugs in C++? How do sanitizers help?

### Answer:
```cpp
// 1. Use-after-free
int* p = new int(42);
delete p;
*p = 10;  // UNDEFINED BEHAVIOR → ASan catches this

// 2. Double free
int* q = new int(5);
delete q;
delete q;  // CRASH or heap corruption → ASan catches

// 3. Buffer overflow
int arr[10];
arr[10] = 42;  // Stack buffer overflow → ASan catches
int* heap = new int[10];
heap[15] = 1;  // Heap buffer overflow → ASan catches

// 4. Memory leak
void leak() {
    int* p = new int(42);
    // Forgot delete → LSan (part of ASan) catches
}

// 5. Uninitialized memory read
int x;
if (x > 0) { /* UB */ }  // MSan catches

// 6. Stack use after return
int* dangling() {
    int local = 42;
    return &local;  // Dangling pointer → compiler warning + ASan
}

// 7. Data race
int shared = 0;
// Thread 1: shared++;
// Thread 2: shared++;
// RACE CONDITION → TSan catches

// 8. Integer overflow (signed)
int a = INT_MAX;
a++;  // UNDEFINED BEHAVIOR → UBSan catches
```

### Sanitizer Flags:
```bash
# AddressSanitizer (ASan) → memory errors
g++ -fsanitize=address -fno-omit-frame-pointer -g app.cpp

# ThreadSanitizer (TSan) → data races
g++ -fsanitize=thread -g app.cpp

# MemorySanitizer (MSan) → uninitialized reads (Clang only)
clang++ -fsanitize=memory -g app.cpp

# UndefinedBehaviorSanitizer (UBSan) → undefined behavior
g++ -fsanitize=undefined -g app.cpp

# LeakSanitizer (LSan) → memory leaks (included in ASan)
g++ -fsanitize=leak -g app.cpp
```

### Explanation:
**Best practice**: Run ASan + UBSan in CI for every commit. TSan on separate build (incompatible with ASan).

---

## Q7: Explain SIMD and how to use it in C++.

### Answer:
```cpp
#include <immintrin.h>  // Intel intrinsics
#include <vector>

// Scalar version → processes 1 element at a time
void addArraysScalar(const float* a, const float* b, float* result, size_t n) {
    for (size_t i = 0; i < n; ++i)
        result[i] = a[i] + b[i];
}

// SIMD version (AVX2) → processes 8 floats at a time
void addArraysSIMD(const float* a, const float* b, float* result, size_t n) {
    size_t i = 0;
    // Process 8 floats per iteration
    for (; i + 7 < n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);    // Load 8 floats from a
        __m256 vb = _mm256_loadu_ps(b + i);    // Load 8 floats from b
        __m256 vr = _mm256_add_ps(va, vb);     // Add 8 pairs simultaneously
        _mm256_storeu_ps(result + i, vr);      // Store 8 results
    }
    // Handle remaining elements
    for (; i < n; ++i)
        result[i] = a[i] + b[i];
}

// Dot product with SIMD
float dotProductSIMD(const float* a, const float* b, size_t n) {
    __m256 sum = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sum = _mm256_fmadd_ps(va, vb, sum);  // Fused multiply-add
    }
    // Horizontal sum of 8 floats in sum register
    __m128 hi = _mm256_extractf128_ps(sum, 1);
    __m128 lo = _mm256_castps256_ps128(sum);
    __m128 s = _mm_add_ps(hi, lo);
    s = _mm_hadd_ps(s, s);
    s = _mm_hadd_ps(s, s);

    float result;
    _mm_store_ss(&result, s);

    // Remainder
    for (; i < n; ++i)
        result += a[i] * b[i];
    return result;
}

// Auto-vectorization hints (let compiler do SIMD)
void autoVectorized(float* __restrict a, const float* __restrict b, size_t n) {
    #pragma omp simd
    for (size_t i = 0; i < n; ++i)
        a[i] += b[i];
}
```

### Explanation:
| SIMD ISA | Width | Floats/op | Available |
|----------|-------|-----------|-----------|
| SSE | 128-bit | 4 | All modern x86 |
| AVX | 256-bit | 8 | Since ~2011 |
| AVX-512 | 512-bit | 16 | Server CPUs, some laptops |
| NEON | 128-bit | 4 | ARM (mobile, Apple Silicon) |

**When to use SIMD:**
- **Finance**: Portfolio risk calculations, Monte Carlo simulations
- **CAD**: Mesh transformations, distance calculations
- **Gaming**: Physics, animation blending, particle updates
- **Tip**: Try auto-vectorization first (`-O3 -march=native`), use intrinsics only when compiler fails

---

## Q8: What is RVO/NRVO? How does copy elision work?

### Answer:
```cpp
#include <vector>
#include <iostream>

class BigObject {
    std::vector<int> data_;
public:
    BigObject() : data_(1'000'000) {
        std::cout << "Constructor\n";
    }
    BigObject(const BigObject&) {
        std::cout << "Copy constructor\n";
    }
    BigObject(BigObject&&) noexcept {
        std::cout << "Move constructor\n";
    }
};

// RVO (Return Value Optimization) → guaranteed in C++17
BigObject createRVO() {
    return BigObject();  // Constructed directly in caller's memory
    // Output: "Constructor" (no copy, no move!)
}

// NRVO (Named Return Value Optimization) → NOT guaranteed but usually happens
BigObject createNRVO() {
    BigObject obj;
    // ... populate obj ...
    return obj;  // Usually constructed directly in caller's memory
    // Output: "Constructor" (no copy, no move → if NRVO applies)
}

// NRVO BREAKS when there are multiple return paths with different objects
BigObject brokenNRVO(bool flag) {
    BigObject a, b;
    if (flag) return a;  // Which one to construct in caller's space?
    return b;            // Compiler can't decide → move instead of elide
    // Output: "Constructor", "Constructor", "Move constructor"
}

// Best practice: Return by value → modern C++ handles it efficiently
std::vector<int> getResults() {
    std::vector<int> results;
    results.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        results.push_back(i);
    return results;  // RVO or move → never expensive copy
}
```

### Explanation:
| Scenario | C++11/14 | C++17+ |
|----------|---------|--------|
| `return Type()` (RVO) | Allowed but not guaranteed | **Guaranteed** (mandatory) |
| `return named_var` (NRVO) | Allowed, not guaranteed | Allowed, not guaranteed |
| Multiple return paths | Likely move | Likely move |

**Key rules:**
1. **Always return by value** ? let the compiler optimize
2. **Don't `std::move` the return value** ? it prevents NRVO!
   ```cpp
   return std::move(obj);  // BAD → disables NRVO, forces move
   return obj;             // GOOD → enables NRVO, zero cost
   ```
3. **Single return variable** helps NRVO succeed
4. Mark move constructors `noexcept` ? enables move as fallback when NRVO fails

---

## Q9: What is `std::pmr` (Polymorphic Memory Resource)?

### Answer:
```cpp
#include <memory_resource>
#include <vector>
#include <string>
#include <array>

void pmrExample() {
    // 1. Stack-based buffer → no heap allocation at all!
    std::array<std::byte, 4096> buffer;
    std::pmr::monotonic_buffer_resource pool(buffer.data(), buffer.size());

    // Vector uses the stack buffer for storage
    std::pmr::vector<int> vec(&pool);
    for (int i = 0; i < 100; ++i)
        vec.push_back(i);  // All allocations from buffer, zero heap allocation!

    // 2. Monotonic buffer → fast allocation, bulk deallocation
    std::pmr::monotonic_buffer_resource mono;
    {
        std::pmr::vector<std::pmr::string> names(&mono);
        names.push_back("Alice");   // String allocated from mono
        names.push_back("Bob");
    }
    // mono destructor frees everything at once → very fast

    // 3. Unsynchronized pool → for single-threaded, fixed-size allocations
    std::pmr::unsynchronized_pool_resource unsyncPool;
    std::pmr::vector<int> fast_vec(&unsyncPool);  // No locking overhead

    // 4. Synchronized pool → for multi-threaded
    std::pmr::synchronized_pool_resource syncPool;

    // 5. Chained: Try stack buffer first, fall back to heap
    std::array<std::byte, 1024> smallBuf;
    std::pmr::monotonic_buffer_resource stackResource(
        smallBuf.data(), smallBuf.size(),
        std::pmr::new_delete_resource());  // Fallback to heap
}

// Real-world: Per-frame allocator for games
class FrameAllocator {
    std::pmr::monotonic_buffer_resource resource_;
    std::pmr::polymorphic_allocator<std::byte> allocator_;

public:
    FrameAllocator(size_t size)
        : resource_(size), allocator_(&resource_) {}

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocator_.allocate_bytes(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    void resetFrame() {
        resource_.release();  // Deallocate everything at once → O(1)!
    }
};
```

### Explanation:
| Resource | Thread-safe | Dealloc | Use Case |
|----------|-----------|---------|----------|
| `monotonic_buffer_resource` | No | All at once (release()) | Per-frame/per-request allocations |
| `unsynchronized_pool_resource` | No | Individual | Single-threaded pools |
| `synchronized_pool_resource` | Yes | Individual | Multi-threaded pools |
| `new_delete_resource` | Yes | Individual | Default (global new/delete) |

**Why `std::pmr` matters:**
- Swap allocators at **runtime** (not template parameter → no code bloat)
- **Composable**: Chain allocators (try fast path, fall back to slow)
- **Gaming**: Per-frame monotonic allocator → zero fragmentation, bulk free
- **Finance**: Per-request allocators for stateless pricing calculations

---

## Q10: Explain compiler optimizations you should know about.

### Answer:

### Key Compiler Optimizations:
```cpp
// 1. Inlining → Compiler replaces function call with body
inline int square(int x) { return x * x; }
// With -O2+, compiler inlines aggressively even without keyword

// 2. Loop unrolling
for (int i = 0; i < 4; ++i)  // Compiler may transform to:
    arr[i] *= 2;
// arr[0] *= 2; arr[1] *= 2; arr[2] *= 2; arr[3] *= 2;

// 3. Dead code elimination
if (false) {
    expensiveFunction();  // Removed entirely
}

// 4. Constant folding / propagation
const int x = 5;
int y = x * 3 + 2;  // Computed at compile time: y = 17

// 5. Loop-invariant code motion (LICM)
for (int i = 0; i < n; ++i) {
    int len = str.length();  // Hoisted out of loop if str doesn't change
    // ...
}

// 6. Vectorization (auto-SIMD)
// Compiler generates SIMD when it can prove no aliasing:
void add(float* __restrict a, const float* __restrict b, size_t n) {
    for (size_t i = 0; i < n; ++i)
        a[i] += b[i];  // Vectorized with -O3 -march=native
}

// 7. Devirtualization → compiler resolves virtual calls when type is known
class Base { virtual void f(); };
class Derived : public Base { void f() override; };

void test() {
    Derived d;
    d.f();           // Direct call → compiler knows exact type
    Base& b = d;
    b.f();           // May be devirtualized if compiler can prove type
}

// 8. Tail call optimization (TCO)
int factorial(int n, int acc = 1) {
    if (n <= 1) return acc;
    return factorial(n - 1, n * acc);  // Tail call → compiler uses jump instead of call
}
```

### Compiler Flags:
```bash
# GCC/Clang
-O0    # No optimization (debug)
-O1    # Basic optimization
-O2    # Standard optimization (recommended for production)
-O3    # Aggressive (may increase binary size)
-Ofast # O3 + fast-math (may change floating-point results!)
-Os    # Optimize for size
-march=native  # Use CPU-specific instructions (AVX2, etc.)
-flto          # Link-time optimization (cross-TU inlining)
-fprofile-generate / -fprofile-use  # Profile-guided optimization (PGO)

# MSVC
/O2   # Maximize speed
/Ox   # Full optimization
/GL   # Whole program optimization (LTO equivalent)
/arch:AVX2  # Enable AVX2
```

### Explanation:
**PGO (Profile-Guided Optimization)** can give 10-20% additional speedup by:
- Optimizing hot paths
- Better branch prediction hints
- Optimal function layout (hot functions together)
- Used by: Chrome, Firefox, LLVM itself, game engines

**LTO (Link-Time Optimization) in detail:**
```
Without LTO:                           With LTO (-flto):
+----------+  +----------+           +----------------------+
| file1.cpp |  | file2.cpp|           | Merged IR            |
| compiled  |  | compiled |           | (all translation     |
| to .o     |  | to .o    |           |  units visible)      |
+-----------+  +----------+           +----------------------+
      |              |                            |
      |              |                            |
   Linker (just      →                    LTO Optimizer
   symbol resolution)                     - Cross-TU inlining
      |                                   - Cross-TU dead code elimination
      |                                   - Whole-program devirtualization
   Binary                                 - Global constant propagation
                                                  |
                                                  |
                                               Binary (5-15% faster)
```

**Build flags cheat sheet for senior interviews:**
```
Debug build:   -g -O0 -fsanitize=address,undefined -D_GLIBCXX_DEBUG
Release build: -O2 -DNDEBUG -march=native -flto
Profile build: -O2 -g -pg                    (gprof compatible)
PGO step 1:    -O2 -fprofile-generate        (instrumented binary)
PGO step 2:    -O2 -fprofile-use=prof.data   (optimized with profile)
Size-optimized: -Os -ffunction-sections -fdata-sections -Wl,--gc-sections
```

**Compiler-specific advanced options:**
| Flag | What it does | When to use |
|------|-------------|-------------|
| `-ffast-math` | Relax IEEE 754 compliance | Non-scientific computation (games) |
| `-fno-exceptions` | Disable exceptions | Embedded, game engines |
| `-fno-rtti` | Disable RTTI | Reduce binary size, embedded |
| `-fvisibility=hidden` | Hide symbols by default | Shared libraries (faster linking) |
| `-fsanitize=memory` | Detect uninitialized reads | Debugging memory issues |
| `-fbranch-probabilities` | Use PGO branch data | After PGO step 1 |

---

## Q11: Compare jemalloc, tcmalloc, and mimalloc. When would you replace the default allocator?

### Answer:

**Why Default malloc() Is Often Not Enough:**
```
Default glibc malloc problems at scale:
  - Lock contention: single arena lock under heavy threading
  - Fragmentation: long-running servers waste 30-50% memory
  - Cache unfriendliness: scattered allocations hurt locality
  - No metrics: can't measure allocation patterns
```

**Comparison Table:**
| Feature | glibc malloc | jemalloc | tcmalloc | mimalloc |
|---------|-------------|----------|----------|----------|
| **Origin** | GNU | FreeBSD/Meta | Google | Microsoft Research |
| **Thread scaling** | Poor (arena lock) | Excellent (per-thread cache) | Excellent (per-thread cache) | Excellent |
| **Fragmentation** | High | Low (size classes) | Medium | Very low |
| **Memory overhead** | Low | Medium (metadata) | Low | Low |
| **Large allocs** | mmap threshold | Dedicated huge pages | Spans | Segments |
| **Profiling** | None | Built-in heap profiling | Built-in (pprof) | Stats API |
| **Used by** | Default Linux | Meta, Redis, Rust | Go runtime, Chrome | .NET, Zig |

**jemalloc Architecture:**
```cpp
// LD_PRELOAD=libjemalloc.so ./myapp  (zero code changes!)

// Or link directly:
#include <jemalloc/jemalloc.h>

// jemalloc arena structure:
// Thread ? tcache (thread-local) ? bin (size class) ? run (contiguous pages)
//
// Size classes: 8, 16, 32, 48, 64, 80, 96, ..., 4096, 8192, ...
// Small: < 14KB ? bins with bitmap allocation
// Large: 14KB - 4MB ? individual page runs
// Huge: > 4MB ? direct mmap

// Runtime profiling (no recompile):
// MALLOC_CONF="prof:true,prof_prefix:heap" ./myapp
// Generates heap profiles compatible with jeprof/pprof
```

**tcmalloc Architecture:**
```cpp
// Google's approach: per-thread + central transfer caches
//
// Allocation fast path (no locks!):
//   1. Check thread-local FreeList for size class ? O(1)
//   2. If empty, grab batch from CentralFreeList ? one lock
//   3. If empty, grab span from PageHeap ? one lock
//
// Key insight: batch transfers reduce lock frequency
//   Thread gets 32 objects at once from central, returns batch when full

// Use with: -ltcmalloc or LD_PRELOAD=libtcmalloc.so
// Profiling: HEAPPROFILE=/tmp/prof ./myapp
// Analysis: google-pprof --svg ./myapp /tmp/prof.0001.heap > heap.svg
```

**mimalloc ? Best for Many Small Allocations:**
```cpp
#include <mimalloc.h>

// Override globally:
// Link with -lmimalloc-override (replaces malloc/free/new/delete)

// Key innovation: "free list sharding" ? each page has its own free list
// Result: almost zero fragmentation even after billions of alloc/free cycles
// Benchmark: 2-3x faster than jemalloc for workloads with many small objects

// Per-heap isolation (security):
mi_heap_t* heap = mi_heap_new();
void* p = mi_heap_malloc(heap, 256);
mi_heap_destroy(heap);  // Frees ALL allocations in this heap at once
```

**When to Switch:**
```
? Server with >8 threads doing frequent allocation ? jemalloc or tcmalloc
? Need heap profiling without recompile ? tcmalloc (pprof)
? Long-running service with fragmentation ? jemalloc (arenas auto-decay)
? Many small objects (< 256 bytes), high churn ? mimalloc
? Security isolation (untrusted plugins) ? mimalloc (per-heap)
? Already using: Redis, Rust, Firefox ? jemalloc; Go, Chrome ? tcmalloc
? Embedded with limited RAM ? stick with default or custom pool
? Real-time with strict determinism ? custom pool allocator (fixed blocks)
```

### Explanation:
At principal engineer level, choosing and tuning the allocator is a key performance decision. The #1 optimization for many server applications is simply switching to jemalloc (LD_PRELOAD, zero code changes, often 10-30% throughput improvement). Know which one fits your workload and why. This is asked at companies like Meta (jemalloc), Google (tcmalloc), and any latency-sensitive system.

---

## Q12: Explain NUMA architecture and its impact on C++ application performance.

### Answer:

**NUMA (Non-Uniform Memory Access):**
```
Uniform Memory Access (UMA):         Non-Uniform Memory Access (NUMA):
??????????????????????????          ????????????????    ????????????????
?  CPU0  CPU1  CPU2  CPU3?          ? Node 0       ?    ? Node 1       ?
?         ?               ?          ? CPU0 CPU1    ?    ? CPU2 CPU3    ?
?    Shared Memory Bus    ?          ?   ?          ?    ?   ?          ?
?         ?               ?          ? Local RAM    ?    ? Local RAM    ?
?    [ DRAM  DRAM ]       ?          ? (64GB, 80ns) ?????? (64GB, 130ns)?
??????????????????????????          ????????????????    ????????????????
                                         Inter-node link (QPI/UPI): +50ns penalty

Modern servers: 2-8 NUMA nodes, each with its own memory controller
Accessing remote memory: 1.5-2x slower than local memory!
```

**Impact on C++ Performance:**
```cpp
#include <numa.h>      // libnuma on Linux
#include <sched.h>     // CPU affinity

// Problem: Default allocator uses "first-touch" policy
// Thread on Node 0 allocates ? memory on Node 0
// If Thread migrates to Node 1 ? all accesses are remote (slow!)

// Solution 1: Pin threads to NUMA nodes
void pinToNode(int node) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    // Get CPUs belonging to this NUMA node
    struct bitmask* cpus = numa_allocate_cpumask();
    numa_node_to_cpus(node, cpus);
    
    for (int i = 0; i < numa_num_configured_cpus(); i++) {
        if (numa_bitmask_isbitset(cpus, i)) {
            CPU_SET(i, &cpuset);
        }
    }
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    numa_free_cpumask(cpus);
}

// Solution 2: Allocate memory on specific node
void* numaAlloc(size_t size, int node) {
    return numa_alloc_onnode(size, node);  // Guaranteed local to node
}

// Solution 3: Interleave for shared read-only data
void* interleavedAlloc(size_t size) {
    // Spread pages across all nodes ? good for read-mostly shared data
    return numa_alloc_interleaved(size);
}
```

**NUMA-Aware Data Structure Design:**
```cpp
// Pattern: Per-node sharding
template<typename T>
class NumaAwarePool {
    struct alignas(64) NodePool {  // Cache-line aligned
        std::vector<T> objects;
        std::mutex lock;
        // Allocated on the NUMA node it serves
    };
    
    std::vector<NodePool*> nodePools_;  // One per NUMA node
    
public:
    NumaAwarePool() {
        int numNodes = numa_num_configured_nodes();
        nodePools_.resize(numNodes);
        for (int n = 0; n < numNodes; n++) {
            // Allocate pool metadata ON that node
            void* mem = numa_alloc_onnode(sizeof(NodePool), n);
            nodePools_[n] = new (mem) NodePool();
        }
    }
    
    T* allocate() {
        int node = numa_node_of_cpu(sched_getcpu());
        auto& pool = *nodePools_[node];
        std::lock_guard lock(pool.lock);
        // Always allocate from local node ? fast access
        pool.objects.emplace_back();
        return &pool.objects.back();
    }
};

// Impact example (real measurements on 2-socket Xeon):
// Sequential access, local memory:  80ns per cache miss
// Sequential access, remote memory: 130ns per cache miss ? 62% slower!
// Random access amplifies this: 3-4x difference observed
```

**Diagnostic Tools:**
```bash
# Check NUMA topology
numactl --hardware
# node 0: cpus: 0-15, size: 64GB
# node 1: cpus: 16-31, size: 64GB

# Run pinned to node 0
numactl --cpunodebind=0 --membind=0 ./myapp

# Check memory placement at runtime
numastat -p $(pidof myapp)

# perf counters for NUMA misses
perf stat -e numa-loads,numa-load-misses,numa-stores ./myapp
```

### Explanation:
NUMA awareness separates good performance engineers from great ones. On a 2-socket server (standard for databases, trading systems), ignoring NUMA means leaving 30-60% performance on the table. Key pattern: pin threads, allocate locally, shard data structures by node. This is asked at any company running performance-critical servers (trading, databases, game servers, ML inference).

---

## Q13: Explain branch prediction and how to write branch-friendly C++ code.

### Answer:

**How Branch Prediction Works:**
```
Modern CPUs pipeline 15-20+ stages. A branch stalls the pipeline
unless the CPU can PREDICT which way it goes.

Branch Predictor accuracy: ~95% for typical code
Misprediction penalty: 15-20 cycles (entire pipeline flush!)

Types of prediction:
  1. Static: forward branches predicted not-taken, backward taken (loops)
  2. Dynamic: Pattern History Table (PHT) ? learns from recent outcomes
  3. TAGE: Tagged Geometric predictor (modern Intel/AMD) ? multiple history lengths
```

**Branch-Friendly Patterns:**
```cpp
// BAD: Unpredictable branch (random data)
int sumPositive_branchy(const std::vector<int>& data) {
    int sum = 0;
    for (int x : data) {
        if (x > 0) sum += x;  // 50% taken ? terrible prediction
    }
    return sum;
}

// GOOD: Branchless with conditional move
int sumPositive_branchless(const std::vector<int>& data) {
    int sum = 0;
    for (int x : data) {
        sum += (x > 0) ? x : 0;  // Compiler may use cmov
        // Or explicitly: sum += x & -(x > 0);  // Bit trick
    }
    return sum;
}

// BEST: Sort data first if possible (makes branch perfectly predictable)
// sorted: [-5, -3, -1, 2, 4, 7, 9] ? branch taken for last 4 ? predictor learns
// Sorting + branchy can be FASTER than branchless on sorted data!

// Benchmark on random data (10M elements):
// Branchy:     45ms (50% misprediction rate)
// Branchless:  12ms (no branches to mispredict)
// Sorted+branchy: 8ms (nearly 100% prediction rate)
```

**Compiler Hints:**
```cpp
// [[likely]] and [[unlikely]] attributes (C++20)
void processPacket(Packet& p) {
    if (p.isValid()) [[likely]] {
        route(p);           // Common path: predictor favor this
    } else [[unlikely]] {
        logError(p);        // Rare path
        dropPacket(p);
    }
}

// GCC/Clang builtin (pre-C++20)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

// Impact: compiler reorders basic blocks to keep hot path linear (no jumps)
```

**Data-Oriented Design to Avoid Branches:**
```cpp
// BAD: Virtual dispatch = indirect branch (hard to predict)
struct Shape { virtual double area() = 0; };
// Processing mixed vector<Shape*> ? indirect branch mispredict at every element

// GOOD: Separate containers by type (Structure of Arrays)
struct Shapes {
    std::vector<Circle> circles;
    std::vector<Rectangle> rectangles;
    
    double totalArea() {
        double sum = 0;
        for (auto& c : circles) sum += 3.14159 * c.r * c.r;     // No branches!
        for (auto& r : rectangles) sum += r.w * r.h;             // No branches!
        return sum;
    }
};

// Lookup tables replace branches:
// BAD:
int categoryScore(int category) {
    switch (category) {  // N-way branch
        case 0: return 10;
        case 1: return 25;
        case 2: return 50;
        // ...
    }
}

// GOOD:
static constexpr int SCORES[] = {10, 25, 50, 75, 100};
int categoryScore(int category) {
    return SCORES[category];  // Single memory access, no branch
}
```

**Measuring Branch Mispredictions:**
```bash
# perf (Linux)
perf stat -e branches,branch-misses ./myapp
# Output: 1,234,567 branch-misses (2.34% of all branches)
# > 5% = investigate; > 10% = significant problem

# Intel VTune: "Microarchitecture Exploration" ? Bad Speculation metric
# AMD uProf: "Branch Misprediction" events
```

### Explanation:
Branch prediction is the #1 microarchitectural effect that senior C++ engineers must understand. A single tight loop with 50% misprediction burns 15-20 cycles per iteration ? this dominates the profile. Three strategies: (1) sort data to make branches predictable, (2) use branchless code (cmov, bit tricks, SIMD), (3) eliminate branches via data-oriented design. This is asked at game studios, HFT firms, and any company doing performance-critical C++.

---

# ?? INTERVIEWER GUIDE ? Set 7 Scoring & Evaluation

---

## Memory & Performance Depth Check

| Topic | Senior Must Know | Staff Must Know |
|-------|-----------------|------------------|
| **Memory Layout** | Stack vs heap, RAII | ASLR, page tables, TLB misses, huge pages |
| **Allocators** | Pool allocator concept | Implement one, discuss arena vs slab vs buddy |
| **Cache** | L1/L2/L3 hierarchy | False sharing, prefetch, data-oriented design |
| **Profiling** | Use perf/VTune | Interpret PMU counters, flamegraphs, roofline model |
| **SIMD** | Know it exists | Write AVX2 code, understand lane operations |
| **Optimization** | Avoid premature optimization | Systematic: measure ? identify ? fix ? verify |

## ?? Green Flags

```
? First response to "optimize this" is "show me the profiler output"
? Knows sizeof and alignment rules for structs (padding!)
? Can explain why vector<bool> is problematic
? Discusses SoA vs AoS with cache implications
? Mentions compiler explorer (godbolt.org) for checking codegen
? Knows jemalloc/tcmalloc trade-offs vs glibc malloc
? Can calculate cache miss cost in nanoseconds
? Understands memory-mapped files and when to use them
? Discusses compile-time computation (constexpr) as optimization
```

## ?? Red Flags

```
? Optimizes without measuring (premature optimization)
? Doesn't know the difference between stack and heap allocation cost
? Thinks "new is slow" without understanding why (syscall, fragmentation)
? Can't explain cache line size (64 bytes) or its implications
? Uses std::list everywhere (terrible cache performance)
? Doesn't understand move semantics as a performance feature
? Never heard of perf, VTune, or Valgrind
? Thinks virtual functions are "too slow" without measurement
```

## Performance Interview: The 3-Step Process

```
Step 1: MEASURE (don't guess)
  ? perf stat, perf record, VTune, Google Benchmark
  ? "What's the P99 latency? What's the bottleneck?"

Step 2: IDENTIFY (algorithmic vs micro)
  ? O(n�) ? O(n log n)? (algorithmic)
  ? Cache miss? Branch mispredict? (micro-architectural)
  ? Memory allocation? (system)

Step 3: FIX & VERIFY
  ? Apply targeted fix
  ? Measure again to confirm improvement
  ? Check for regressions in other metrics
```

---
