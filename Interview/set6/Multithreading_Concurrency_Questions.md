# Set 6: Multithreading & Concurrency in C++
## Senior-Level Interview Questions (10 Years Experience)

---

## Q1: Explain the C++ Memory Model. What are memory orderings and when do you use each?

### Answer:

```cpp
#include <atomic>

// Memory orderings from weakest to strongest:

// 1. memory_order_relaxed | No ordering guarantees, only atomicity
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);  // Just need atomic increment
// Use: Simple counters, statistics

// 2. memory_order_acquire / memory_order_release | Synchronizes-with relationship
std::atomic<bool> ready{false};
int data = 0;

// Thread 1 (Producer)
void producer() {
    data = 42;                                          // Non-atomic write
    ready.store(true, std::memory_order_release);       // Release: all prior writes visible
}

// Thread 2 (Consumer)
void consumer() {
    while (!ready.load(std::memory_order_acquire)) {}   // Acquire: sees all writes before release
    assert(data == 42);  // Guaranteed to see 42!
}

// 3. memory_order_acq_rel | Both acquire and release (for read-modify-write)
std::atomic<Node*> head{nullptr};
void pushLockFree(Node* node) {
    node->next = head.load(std::memory_order_relaxed);
    while (!head.compare_exchange_weak(node->next, node,
            std::memory_order_acq_rel, std::memory_order_relaxed)) {}
}

// 4. memory_order_seq_cst | Full sequential consistency (default, strongest)
std::atomic<int> x{0}, y{0};
// Thread 1: x.store(1);  // seq_cst by default
// Thread 2: y.store(1);
// Thread 3: if (x.load() == 1 && y.load() == 0) { /* possible */ }
// Thread 4: if (y.load() == 1 && x.load() == 0) { /* possible, but... */ }
// With seq_cst: if T3 sees x=1,y=0 then T4 CANNOT see y=1,x=0
// This total ordering is expensive on ARM/POWER CPUs
```

### Explanation:
| Ordering | Cost | Guarantee | Use Case |
|----------|------|-----------|----------|
| `relaxed` | Cheapest | Atomicity only | Counters, flags (no data dependency) |
| `acquire/release` | Moderate | Synchronizes-with (producer-consumer) | Lock implementations, publish pattern |
| `acq_rel` | Moderate | Both acquire and release | CAS loops, lock-free data structures |
| `seq_cst` | Expensive | Total global ordering | When reasoning about multiple atomics |

**Key insight**: On x86, `acquire`/`release` are often free (x86 has strong memory model). On ARM/POWER, they generate barrier instructions. This matters hugely for gaming (cross-platform) and HFT (every nanosecond counts).

---

## Q2: Implement a Thread Pool with task queuing and futures.

### Answer:
```cpp
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits>

class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mutex_);
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    // Submit task and get future for result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard lock(mutex_);
            if (stop_) throw std::runtime_error("ThreadPool is stopped");
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_)
            worker.join();
    }
};

// Usage
void example() {
    ThreadPool pool(4);

    auto future1 = pool.submit([](int a, int b) { return a + b; }, 3, 4);
    auto future2 = pool.submit([] { return std::string("hello"); });

    std::cout << future1.get() << "\n";  // 7
    std::cout << future2.get() << "\n";  // "hello"

    // Parallel processing
    std::vector<std::future<double>> results;
    for (int i = 0; i < 1000; ++i) {
        results.push_back(pool.submit([i] {
            return std::sqrt(static_cast<double>(i));
        }));
    }

    double sum = 0;
    for (auto& f : results)
        sum += f.get();
}
```

### Explanation:
- **`std::packaged_task`** wraps callable and provides a `std::future` for the result
- **`std::invoke_result_t`** (C++17) deduces return type
- **Graceful shutdown**: Set `stop_`, notify all, join all threads
- **No task stealing** in this basic version -> production pools use work-stealing (like Intel TBB)
- **Follow-up**: Discuss priority queues, task dependencies (DAG scheduling), and per-thread queues

---

## Q3: What is a deadlock? How do you prevent, detect, and recover from it?

### Answer:

**Four conditions for deadlock (ALL must hold):**
1. **Mutual Exclusion**: Resource held exclusively
2. **Hold and Wait**: Thread holds one resource while waiting for another
3. **No Preemption**: Resources can't be forcibly taken
4. **Circular Wait**: A|B|C|A dependency chain

**Prevention Strategies:**
```cpp
// 1. Lock Ordering | Break circular wait
std::mutex mutexA, mutexB;

// BAD: Thread 1 locks A then B, Thread 2 locks B then A | DEADLOCK
// GOOD: Always lock in consistent order (by address or ID)
void safeOperation() {
    // std::lock locks multiple mutexes without deadlock (uses try-lock internally)
    std::scoped_lock lock(mutexA, mutexB);  // C++17 | preferred
    // ... work with both resources
}

// 2. Try-lock with backoff
bool tryOperation() {
    std::unique_lock lockA(mutexA, std::try_to_lock);
    if (!lockA.owns_lock()) return false;

    std::unique_lock lockB(mutexB, std::try_to_lock);
    if (!lockB.owns_lock()) return false;  // lockA auto-released

    // ... work with both resources
    return true;
}

// 3. Lock hierarchy (compile-time enforcement)
class HierarchicalMutex {
    std::mutex mtx_;
    int level_;
    static thread_local int currentLevel_;

public:
    HierarchicalMutex(int level) : level_(level) {}

    void lock() {
        if (level_ >= currentLevel_)
            throw std::runtime_error("Lock hierarchy violation");
        mtx_.lock();
        currentLevel_ = level_;
    }

    void unlock() {
        currentLevel_ = INT_MAX;
        mtx_.unlock();
    }
};

// Usage: High-level locks (level 1000) must be acquired before low-level (level 100)
HierarchicalMutex dbLock(1000);     // High level
HierarchicalMutex cacheLock(500);   // Mid level
HierarchicalMutex itemLock(100);    // Low level
```

**Detection (runtime):**
- Build a wait-for graph at runtime
- Periodically check for cycles (DFS)
- Log lock acquisition order for post-mortem analysis

### Explanation:
- **`std::scoped_lock`** (C++17) is the modern answer -> locks multiple mutexes atomically
- **Lock-free alternatives**: Often better for high-contention scenarios (gaming frame updates, HFT)
- **Real-world CAD example**: Geometry kernel lock + UI lock + undo stack lock -> use lock hierarchy

---

## Q4: Implement a Reader-Writer Lock and explain its use cases.

### Answer:
```cpp
#include <shared_mutex>
#include <mutex>
#include <string>
#include <unordered_map>

// Using std::shared_mutex (C++17)
class ThreadSafeConfig {
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> config_;

public:
    // Multiple readers can read simultaneously
    std::string get(const std::string& key) const {
        std::shared_lock lock(mutex_);  // Shared/read lock
        auto it = config_.find(key);
        return it != config_.end() ? it->second : "";
    }

    // Writers get exclusive access
    void set(const std::string& key, const std::string& value) {
        std::unique_lock lock(mutex_);  // Exclusive/write lock
        config_[key] = value;
    }

    // Upgrade pattern (read then conditionally write)
    void setIfAbsent(const std::string& key, const std::string& value) {
        {
            std::shared_lock readLock(mutex_);
            if (config_.count(key)) return;  // Already exists
        }
        // Must release read lock before acquiring write lock
        // (std::shared_mutex doesn't support upgrade)
        std::unique_lock writeLock(mutex_);
        config_.emplace(key, value);  // emplace won't overwrite if inserted between locks
    }
};

// Custom implementation (for interview | show understanding)
class ReadWriteLock {
    std::mutex mutex_;
    std::condition_variable readCV_, writeCV_;
    int readers_ = 0;
    int writers_ = 0;
    int waitingWriters_ = 0;

public:
    void lockRead() {
        std::unique_lock lock(mutex_);
        // Wait if a writer is active or waiting (writer preference)
        readCV_.wait(lock, [this] { return writers_ == 0 && waitingWriters_ == 0; });
        ++readers_;
    }

    void unlockRead() {
        std::unique_lock lock(mutex_);
        --readers_;
        if (readers_ == 0) writeCV_.notify_one();
    }

    void lockWrite() {
        std::unique_lock lock(mutex_);
        ++waitingWriters_;
        writeCV_.wait(lock, [this] { return readers_ == 0 && writers_ == 0; });
        --waitingWriters_;
        ++writers_;
    }

    void unlockWrite() {
        std::unique_lock lock(mutex_);
        --writers_;
        writeCV_.notify_all();   // Wake all waiting readers
        readCV_.notify_all();
    }
};
```

### Explanation:
- **Writer preference** in custom implementation: Prevents writer starvation
- **`std::shared_mutex`** doesn't support lock upgrade (read?write) -> must release and reacquire
- **Use cases**: Configuration stores, game world state (many readers, rare updates), CAD document metadata
- **When NOT to use**: If writes are frequent (overhead of shared_mutex > plain mutex)
- **Alternative**: Read-Copy-Update (RCU) for read-heavy workloads with infrequent writes

**Reader vs Writer Preference Analysis:**
```
Reader Preference:
  - Writers wait until ALL readers finish | potential writer starvation
  - Best for: Read-heavy, writes can tolerate delay (config cache)
  
Writer Preference (our implementation):
  - New readers blocked when writer is waiting | writers get priority
  - Best for: Write consistency matters (financial data, game state)
  
Fair (FIFO):
  - Requests served in arrival order | no starvation
  - Highest overhead, used when fairness is legally required
```

**Read-Copy-Update (RCU) ? lock-free alternative (advanced):**
```cpp
// RCU pattern for extreme read performance
template<typename T>
class RCU {
    std::atomic<std::shared_ptr<const T>> data_;
public:
    // Readers: Zero overhead | just atomic load
    std::shared_ptr<const T> read() const {
        return std::atomic_load(&data_);
    }
    
    // Writers: Copy-on-write
    template<typename Fn>
    void update(Fn modifier) {
        auto oldPtr = std::atomic_load(&data_);
        auto newPtr = std::make_shared<T>(*oldPtr);  // Copy
        modifier(*newPtr);                            // Modify copy
        std::atomic_store(&data_, std::move(newPtr)); // Swap atomically
        // Old data stays alive until last reader releases shared_ptr
    }
};
// Used by Linux kernel (RCU), Java CopyOnWriteArrayList
// Perfect when reads are 99%+ of operations
```

---

## Q5: Explain `std::async`, `std::future`, `std::promise`. When to use each?

### Answer:
```cpp
#include <future>
#include <thread>
#include <iostream>

// === std::async | simplest way to run async work ===
void asyncExample() {
    // launch::async = guaranteed new thread
    auto future1 = std::async(std::launch::async, [] {
        // Heavy computation
        return computeSomething();
    });

    // launch::deferred = lazy evaluation (runs when .get() called)
    auto future2 = std::async(std::launch::deferred, [] {
        return computeOther();
    });

    // Do other work while async runs...
    doOtherWork();

    int result1 = future1.get();  // Blocks until ready
    int result2 = future2.get();  // Actually runs HERE (deferred)
}

// === std::promise + std::future | producer/consumer channel ===
void promiseExample() {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();

    std::thread producer([&promise] {
        try {
            int result = performCalculation();
            promise.set_value(result);       // Fulfill promise
        } catch (...) {
            promise.set_exception(std::current_exception());  // Propagate error
        }
    });

    // Consumer
    try {
        int value = future.get();  // Blocks until promise fulfilled
        std::cout << "Got: " << value << "\n";
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }

    producer.join();
}

// === std::packaged_task | callable wrapper with future ===
void packagedTaskExample() {
    std::packaged_task<int(int, int)> task([](int a, int b) {
        return a * b;
    });

    std::future<int> future = task.get_future();

    // Can be moved to another thread or queued
    std::thread t(std::move(task), 6, 7);
    std::cout << future.get() << "\n";  // 42
    t.join();
}

// === Parallel computation pattern ===
std::vector<double> parallelMap(const std::vector<double>& input,
                                 std::function<double(double)> fn) {
    std::vector<std::future<double>> futures;
    for (double val : input) {
        futures.push_back(std::async(std::launch::async, fn, val));
    }

    std::vector<double> results;
    results.reserve(futures.size());
    for (auto& f : futures) {
        results.push_back(f.get());
    }
    return results;
}
```

### Explanation:
| Feature | Use Case | Notes |
|---------|----------|-------|
| `std::async` | Fire-and-forget parallel work | Simplest API; beware `launch::deferred` default |
| `std::promise/future` | One-shot producer-consumer | Manual thread management |
| `std::packaged_task` | Deferred execution with future | Good for thread pools |

**Gotchas:**
- `std::async` without launch policy may be deferred (never actually parallel!)
- Future from `std::async` blocks in destructor (C++ Standards defect)
- `std::future::get()` can only be called once -> use `std::shared_future` for multiple consumers

---

## Q6: What are condition variables? Implement a bounded blocking queue.

### Answer:
```cpp
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

template<typename T>
class BoundedBlockingQueue {
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    size_t capacity_;
    bool closed_ = false;

public:
    BoundedBlockingQueue(size_t capacity) : capacity_(capacity) {}

    // Blocking push | waits if queue is full
    bool push(T item) {
        std::unique_lock lock(mutex_);
        notFull_.wait(lock, [this] { return queue_.size() < capacity_ || closed_; });
        if (closed_) return false;
        queue_.push(std::move(item));
        notEmpty_.notify_one();
        return true;
    }

    // Blocking pop | waits if queue is empty
    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        notEmpty_.wait(lock, [this] { return !queue_.empty() || closed_; });
        if (queue_.empty()) return std::nullopt;  // Closed and empty
        T item = std::move(queue_.front());
        queue_.pop();
        notFull_.notify_one();
        return item;
    }

    // Timed pop
    std::optional<T> tryPopFor(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        if (!notEmpty_.wait_for(lock, timeout,
                [this] { return !queue_.empty() || closed_; })) {
            return std::nullopt;  // Timeout
        }
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        notFull_.notify_one();
        return item;
    }

    // Close the queue | unblocks all waiters
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }
};

// Producer-Consumer pattern
void producerConsumer() {
    BoundedBlockingQueue<int> queue(100);

    // Producers
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&queue, i] {
            for (int j = 0; j < 1000; ++j)
                queue.push(i * 1000 + j);
        });
    }

    // Consumers
    std::vector<std::thread> consumers;
    for (int i = 0; i < 2; ++i) {
        consumers.emplace_back([&queue] {
            while (auto item = queue.pop()) {
                process(*item);
            }
        });
    }

    for (auto& p : producers) p.join();
    queue.close();  // Signal consumers to finish
    for (auto& c : consumers) c.join();
}
```

### Explanation:
- **Spurious wakeups**: `wait` with predicate handles them automatically (loops internally)
- **Bounded**: Prevents unbounded memory growth (back-pressure)
- **`close()`**: Graceful shutdown -> producers stop pushing, consumers drain and exit
- **CAD use case**: Render job queue (producer: main thread, consumer: render threads)
- **Finance**: Market data processing pipeline
- **Alternative**: For high-throughput, use lock-free MPMC queues (e.g., `moodycamel::ConcurrentQueue`)

---

## Q7: Explain `std::jthread` and cooperative cancellation in C++20.

### Answer:
```cpp
#include <thread>
#include <stop_token>
#include <iostream>
#include <chrono>

// C++20 jthread | auto-joins and supports cancellation
void jthreadExample() {
    // jthread automatically joins on destruction (no more forgetting to join!)
    std::jthread worker([](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            doWork();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Worker stopped gracefully\n";
    });

    // ... do some work ...
    std::this_thread::sleep_for(std::chrono::seconds(2));

    worker.request_stop();  // Cooperative cancellation
    // worker automatically joins here (destructor)
}

// Stop callback | register cleanup when stop is requested
void stopCallbackExample() {
    std::jthread worker([](std::stop_token stoken) {
        // Register a callback that runs when stop is requested
        std::stop_callback callback(stoken, [] {
            std::cout << "Cancellation requested ? cleaning up resources\n";
        });

        // Simulate condition variable wait with cancellation
        std::mutex mtx;
        std::condition_variable_any cv;

        std::unique_lock lock(mtx);
        // This wait is cancellable!
        cv.wait(lock, stoken, [] { return false; /* or some condition */ });

        if (stoken.stop_requested()) {
            std::cout << "Woke up due to cancellation\n";
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    worker.request_stop();  // Triggers callback AND wakes cv.wait
}
```

### Explanation:
**`std::jthread` vs `std::thread`:**
| Feature | `std::thread` | `std::jthread` (C++20) |
|---------|--------------|----------------------|
| Destructor | `std::terminate` if not joined | Auto-joins |
| Cancellation | Manual (atomic flag) | Built-in `stop_token` |
| Exception safety | Must manually join in catch | Automatic |
| CV integration | No | `condition_variable_any` supports `stop_token` |

**Why it matters**: Eliminates entire classes of bugs:
- Forgetting to join -> crash
- Exception causes thread leak -> resource exhaustion
- Non-cooperative cancellation -> data corruption

---

## Q8: What is false sharing? How do you avoid it?

### Answer:
```cpp
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <new>  // std::hardware_destructive_interference_size

// BAD: False sharing | counters on the same cache line
struct BadCounters {
    std::atomic<int64_t> counter1;  // 8 bytes
    std::atomic<int64_t> counter2;  // 8 bytes | same 64-byte cache line!
};

// GOOD: Pad to separate cache lines
struct GoodCounters {
    alignas(64) std::atomic<int64_t> counter1;  // Own cache line
    alignas(64) std::atomic<int64_t> counter2;  // Own cache line
};

// C++17 standard way:
struct StandardCounters {
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int64_t> counter1;
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int64_t> counter2;
};

// Benchmark to demonstrate
void benchmark() {
    BadCounters bad;
    GoodCounters good;

    auto test = [](auto& counters, auto& c1, auto& c2) {
        auto start = std::chrono::high_resolution_clock::now();

        std::thread t1([&c1] {
            for (int i = 0; i < 100'000'000; ++i)
                c1.fetch_add(1, std::memory_order_relaxed);
        });

        std::thread t2([&c2] {
            for (int i = 0; i < 100'000'000; ++i)
                c2.fetch_add(1, std::memory_order_relaxed);
        });

        t1.join(); t2.join();

        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    };

    auto badTime = test(bad, bad.counter1, bad.counter2);
    auto goodTime = test(good, good.counter1, good.counter2);
    // badTime is typically 3-10x slower than goodTime!
}
```

### Explanation:
**What is false sharing?**
- CPU caches work in 64-byte cache lines
- If two threads write to different variables on the SAME cache line, the cache line bounces between cores ("ping-pong")
- Each write invalidates the other core's cache line, forcing a reload
- This is **invisible** in code but causes **3-10x slowdown**

**Where it matters:**
- **HFT**: Per-thread counters, position accumulators
- **Gaming**: Per-entity data updated by different threads
- **Thread pool**: Per-thread work-stealing deques

**Prevention:**
1. `alignas(64)` or `alignas(std::hardware_destructive_interference_size)`
2. Padding between hot variables
3. Thread-local accumulators, merge periodically
4. SoA layout with hot/cold separation

---

## Q9: Implement a Spinlock and explain when to use it over a mutex.

### Answer:
```cpp
#include <atomic>

class Spinlock {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Spin | but hint the CPU to save power
            #if defined(__x86_64__) || defined(_M_X64)
            __builtin_ia32_pause();  // or _mm_pause() with intrinsics
            #elif defined(__aarch64__)
            __asm__ volatile("yield");
            #endif
        }
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }
};

// TTAS (Test-and-Test-and-Set) | better performance under contention
class TTASSpinlock {
    std::atomic<bool> locked_{false};

public:
    void lock() {
        while (true) {
            // First, spin on read (doesn't invalidate other caches)
            while (locked_.load(std::memory_order_relaxed)) {
                #ifdef _MSC_VER
                _mm_pause();
                #endif
            }
            // Then try to acquire
            if (!locked_.exchange(true, std::memory_order_acquire))
                return;  // Got it!
        }
    }

    void unlock() {
        locked_.store(false, std::memory_order_release);
    }
};

// Usage with RAII
template<typename Lock>
class LockGuard {
    Lock& lock_;
public:
    LockGuard(Lock& lock) : lock_(lock) { lock_.lock(); }
    ~LockGuard() { lock_.unlock(); }
};
```

### Explanation:
| Aspect | Spinlock | Mutex (`std::mutex`) |
|--------|----------|---------------------|
| Wait method | Busy-spin (burns CPU) | OS sleep (context switch) |
| Latency | Very low (no syscall) | Higher (kernel transition) |
| CPU usage | 100% while waiting | 0% while sleeping |
| Best for | Very short critical sections | Longer critical sections |
| Fairness | No guarantee | OS-level fairness |

**Use spinlock when:**
- Critical section < ~1 microsecond
- Thread count -> CPU cores (no preemption)
- HFT hot path, game engine physics step, real-time audio

**Use mutex when:**
- Critical section is longer or variable
- More threads than cores
- You hold I/O or other blocking operations inside the lock

**CAS-based spinlock with backoff (production-grade):**
```cpp
class AdaptiveSpinlock {
    std::atomic<bool> locked_{false};
public:
    void lock() {
        int spinCount = 0;
        while (true) {
            if (!locked_.exchange(true, std::memory_order_acquire))
                return;
            
            if (spinCount < 16) {
                _mm_pause();  // CPU hint: "I'm spinning"
            } else if (spinCount < 64) {
                std::this_thread::yield();  // Give up time slice
            } else {
                // Exponential backoff: sleep briefly
                std::this_thread::sleep_for(
                    std::chrono::microseconds(1 << std::min(spinCount - 64, 10)));
            }
            ++spinCount;
        }
    }
    void unlock() { locked_.store(false, std::memory_order_release); }
};
// Adaptive: spins first (low latency), then yields (fairness), then sleeps (CPU savings)
```

**Fairness comparison:**
```
TAS Spinlock:    No fairness guarantee | threads near the lock win (cache line proximity)
TTAS Spinlock:   Slightly fairer | reduced cache line contention
Ticket Spinlock: FIFO fair | each thread gets a ticket number, served in order
CLH Lock:        FIFO fair | queue-based, excellent scalability on NUMA
```

---

## Q10: Explain C++17/20 parallel algorithms. How do execution policies work?

### Answer:
```cpp
#include <algorithm>
#include <execution>
#include <vector>
#include <numeric>

void parallelAlgorithms() {
    std::vector<int> data(10'000'000);
    std::iota(data.begin(), data.end(), 0);

    // Sequential (default)
    std::sort(std::execution::seq, data.begin(), data.end());

    // Parallel | uses thread pool internally
    std::sort(std::execution::par, data.begin(), data.end());

    // Parallel + Vectorized (SIMD) | strongest
    std::sort(std::execution::par_unseq, data.begin(), data.end());

    // C++20: Unsequenced (SIMD only, single thread)
    std::for_each(std::execution::unseq, data.begin(), data.end(),
                  [](int& x) { x *= 2; });

    // Parallel reduce | associative + commutative operation
    double sum = std::reduce(std::execution::par, data.begin(), data.end(), 0.0);

    // Parallel transform_reduce (map-reduce)
    double dotProduct = std::transform_reduce(
        std::execution::par,
        vec1.begin(), vec1.end(),
        vec2.begin(),
        0.0  // Initial value
    );
}
```

### Explanation:
| Policy | Threads | SIMD | Notes |
|--------|---------|------|-------|
| `seq` | 1 | No | Same as no policy |
| `par` | Multiple | No | Callbacks must be thread-safe |
| `par_unseq` | Multiple | Yes | No mutexes, no memory allocation in callback |
| `unseq` (C++20) | 1 | Yes | Vectorized but single-threaded |

**Constraints on `par_unseq`:**
- No locks (could deadlock with SIMD interleaving)
- No `new`/`delete` inside lambda
- No file I/O
- No `thread_local` access

**Real-world performance**: `par` sort on 10M elements -> 3-4x speedup on 8 cores (not linear due to merge overhead).

**Performance gotchas & when parallel algorithms hurt:**
```cpp
// BAD: Parallel overhead > work per element
std::for_each(std::execution::par, small_vec.begin(), small_vec.end(),
              [](int& x) { x *= 2; });
// For vectors < 10K elements, sequential is often faster (thread setup overhead ~10-50|s)

// BAD: Non-associative operation with reduce
double avg = std::reduce(std::execution::par, v.begin(), v.end(), 0.0) / v.size();
// reduce requires associative+commutative op; floating-point addition is NOT strictly associative
// May give slightly different result than sequential (acceptable for most uses)

// GOOD: Large data, expensive per-element work
auto results = std::transform(std::execution::par,
    models.begin(), models.end(), outputs.begin(),
    [](const Model& m) { return m.computeExpensiveResult(); });
// 8x speedup when each element takes milliseconds
```

**Custom thread pool (MSVC implementation detail):**
```
MSVC: Uses Windows Thread Pool API (IoCompletionPort-based)
GCC/libstdc++: Uses Intel TBB (Threading Building Blocks) | must link -ltbb
Clang/libc++: Limited support (falls back to sequential for some algorithms)

// To use with GCC: g++ -std=c++17 -ltbb program.cpp
```

**Benchmark template for comparing policies:**
```cpp
#include <chrono>
template<typename Policy, typename Fn>
auto benchmark(Policy policy, Fn fn) {
    auto start = std::chrono::high_resolution_clock::now();
    fn();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}
// Compare: benchmark(std::execution::seq, ...) vs benchmark(std::execution::par, ...)
```

---

# ENHANCED SECTION: Staff Engineer Concurrency Deep Dive

> *These questions test production-level concurrency expertise -> debugging real race conditions, choosing the right synchronization primitive, and understanding hardware-level behavior.*

---

## Q8: Explain the ABA Problem in lock-free programming. How do you solve it?

### Answer:
```cpp
// ABA Problem:
// Thread 1: Read head = A
// Thread 2: Pop A, Pop B, Push A back
// Thread 1: CAS(head, A, newNode) SUCCEEDS | but the list changed!

// Solution 1: Tagged pointers (generation counter)
template<typename T>
struct TaggedPtr {
    T* ptr;
    uintptr_t tag; // Incremented on every modification
};

// Solution 2: Hazard Pointers (standard in C++26)
// Readers "announce" which nodes they're accessing
// Nodes can only be reclaimed when no thread has a hazard pointer to them

// Solution 3: Epoch-Based Reclamation (used by crossbeam in Rust)
// Threads enter/exit "epochs"
// Memory freed in epoch N can be reclaimed when all threads have
// advanced past epoch N+2
```

### Explanation:
- **ABA is the #1 lock-free bug** ? CAS succeeds but the state has changed underneath
- **iCluster relevance**: The communication module uses packet counters in DMKINT.H to detect missing/reordered messages -> conceptually similar to generation counters preventing ABA
- **Production systems**: Java's `AtomicStampedReference`, RCU in Linux kernel, hazard pointers in folly (Facebook's C++ library)

---

## Q9: Design a Multi-Producer Multi-Consumer (MPMC) Lock-Free Queue.

### Answer:
```cpp
// Key insight: SPSC is straightforward, MPMC is hard
// Production approach: Use ring buffer with atomic head/tail + sequence numbers

template<typename T, size_t N>
class MPMCQueue {
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };
    
    alignas(64) std::array<Cell, N> buffer_;
    alignas(64) std::atomic<size_t> enqueue_pos_{0};
    alignas(64) std::atomic<size_t> dequeue_pos_{0};

public:
    MPMCQueue() {
        for (size_t i = 0; i < N; ++i)
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool try_enqueue(const T& data) {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        while (true) {
            auto& cell = buffer_[pos % N];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed)) {
                    cell.data = data;
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // Full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
    }
};
```

### Explanation:
- Each cell has a **sequence number** that acts as both availability flag and ABA prevention
- **alignas(64)**: Prevents false sharing between producer and consumer cache lines
- **Production alternatives**: Dmitry Vyukov's bounded MPMC queue, moodycamel::ConcurrentQueue, boost::lockfree::queue
- **iCluster analogy**: The queue-based IPC (MO, CI, CO, DI, DO queues in DMKINT) is a persistent producer-consumer system with multiple producers and consumers

---

## Q10: When should you use a mutex vs atomic vs lock-free vs wait-free?

### Answer:
```
Decision Matrix:
 
        $match = # Set 6: Multithreading & Concurrency in C++
## Senior-Level Interview Questions (10 Years Experience)

---

## Q1: Explain the C++ Memory Model. What are memory orderings and when do you use each|

### Answer:

```cpp
#include <atomic>

// Memory orderings from weakest to strongest:

// 1. memory_order_relaxed -> No ordering guarantees, only atomicity
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);  // Just need atomic increment
// Use: Simple counters, statistics

// 2. memory_order_acquire / memory_order_release -> Synchronizes-with relationship
std::atomic<bool> ready{false};
int data = 0;

// Thread 1 (Producer)
void producer() {
    data = 42;                                          // Non-atomic write
    ready.store(true, std::memory_order_release);       // Release: all prior writes visible
}

// Thread 2 (Consumer)
void consumer() {
    while (!ready.load(std::memory_order_acquire)) {}   // Acquire: sees all writes before release
    assert(data == 42);  // Guaranteed to see 42!
}

// 3. memory_order_acq_rel -> Both acquire and release (for read-modify-write)
std::atomic<Node*> head{nullptr};
void pushLockFree(Node* node) {
    node->next = head.load(std::memory_order_relaxed);
    while (!head.compare_exchange_weak(node->next, node,
            std::memory_order_acq_rel, std::memory_order_relaxed)) {}
}

// 4. memory_order_seq_cst -> Full sequential consistency (default, strongest)
std::atomic<int> x{0}, y{0};
// Thread 1: x.store(1);  // seq_cst by default
// Thread 2: y.store(1);
// Thread 3: if (x.load() == 1 && y.load() == 0) { /* possible */ }
// Thread 4: if (y.load() == 1 && x.load() == 0) { /* possible, but... */ }
// With seq_cst: if T3 sees x=1,y=0 then T4 CANNOT see y=1,x=0
// This total ordering is expensive on ARM/POWER CPUs
```

### Explanation:
| Ordering | Cost | Guarantee | Use Case |
|----------|------|-----------|----------|
| `relaxed` | Cheapest | Atomicity only | Counters, flags (no data dependency) |
| `acquire/release` | Moderate | Synchronizes-with (producer-consumer) | Lock implementations, publish pattern |
| `acq_rel` | Moderate | Both acquire and release | CAS loops, lock-free data structures |
| `seq_cst` | Expensive | Total global ordering | When reasoning about multiple atomics |

**Key insight**: On x86, `acquire`/`release` are often free (x86 has strong memory model). On ARM/POWER, they generate barrier instructions. This matters hugely for gaming (cross-platform) and HFT (every nanosecond counts).

---

## Q2: Implement a Thread Pool with task queuing and futures.

### Answer:
```cpp
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits>

class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mutex_);
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    // Submit task and get future for result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard lock(mutex_);
            if (stop_) throw std::runtime_error("ThreadPool is stopped");
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_)
            worker.join();
    }
};

// Usage
void example() {
    ThreadPool pool(4);

    auto future1 = pool.submit([](int a, int b) { return a + b; }, 3, 4);
    auto future2 = pool.submit([] { return std::string("hello"); });

    std::cout << future1.get() << "\n";  // 7
    std::cout << future2.get() << "\n";  // "hello"

    // Parallel processing
    std::vector<std::future<double>> results;
    for (int i = 0; i < 1000; ++i) {
        results.push_back(pool.submit([i] {
            return std::sqrt(static_cast<double>(i));
        }));
    }

    double sum = 0;
    for (auto& f : results)
        sum += f.get();
}
```

### Explanation:
- **`std::packaged_task`** wraps callable and provides a `std::future` for the result
- **`std::invoke_result_t`** (C++17) deduces return type
- **Graceful shutdown**: Set `stop_`, notify all, join all threads
- **No task stealing** in this basic version | production pools use work-stealing (like Intel TBB)
- **Follow-up**: Discuss priority queues, task dependencies (DAG scheduling), and per-thread queues

---

## Q3: What is a deadlock| How do you prevent, detect, and recover from it|

### Answer:

**Four conditions for deadlock (ALL must hold):**
1. **Mutual Exclusion**: Resource held exclusively
2. **Hold and Wait**: Thread holds one resource while waiting for another
3. **No Preemption**: Resources can't be forcibly taken
4. **Circular Wait**: A|B|C|A dependency chain

**Prevention Strategies:**
```cpp
// 1. Lock Ordering -> Break circular wait
std::mutex mutexA, mutexB;

// BAD: Thread 1 locks A then B, Thread 2 locks B then A -> DEADLOCK
// GOOD: Always lock in consistent order (by address or ID)
void safeOperation() {
    // std::lock locks multiple mutexes without deadlock (uses try-lock internally)
    std::scoped_lock lock(mutexA, mutexB);  // C++17 ? preferred
    // ... work with both resources
}

// 2. Try-lock with backoff
bool tryOperation() {
    std::unique_lock lockA(mutexA, std::try_to_lock);
    if (!lockA.owns_lock()) return false;

    std::unique_lock lockB(mutexB, std::try_to_lock);
    if (!lockB.owns_lock()) return false;  // lockA auto-released

    // ... work with both resources
    return true;
}

// 3. Lock hierarchy (compile-time enforcement)
class HierarchicalMutex {
    std::mutex mtx_;
    int level_;
    static thread_local int currentLevel_;

public:
    HierarchicalMutex(int level) : level_(level) {}

    void lock() {
        if (level_ >= currentLevel_)
            throw std::runtime_error("Lock hierarchy violation");
        mtx_.lock();
        currentLevel_ = level_;
    }

    void unlock() {
        currentLevel_ = INT_MAX;
        mtx_.unlock();
    }
};

// Usage: High-level locks (level 1000) must be acquired before low-level (level 100)
HierarchicalMutex dbLock(1000);     // High level
HierarchicalMutex cacheLock(500);   // Mid level
HierarchicalMutex itemLock(100);    // Low level
```

**Detection (runtime):**
- Build a wait-for graph at runtime
- Periodically check for cycles (DFS)
- Log lock acquisition order for post-mortem analysis

### Explanation:
- **`std::scoped_lock`** (C++17) is the modern answer | locks multiple mutexes atomically
- **Lock-free alternatives**: Often better for high-contention scenarios (gaming frame updates, HFT)
- **Real-world CAD example**: Geometry kernel lock + UI lock + undo stack lock | use lock hierarchy

---

## Q4: Implement a Reader-Writer Lock and explain its use cases.

### Answer:
```cpp
#include <shared_mutex>
#include <mutex>
#include <string>
#include <unordered_map>

// Using std::shared_mutex (C++17)
class ThreadSafeConfig {
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> config_;

public:
    // Multiple readers can read simultaneously
    std::string get(const std::string& key) const {
        std::shared_lock lock(mutex_);  // Shared/read lock
        auto it = config_.find(key);
        return it != config_.end() ? it->second : "";
    }

    // Writers get exclusive access
    void set(const std::string& key, const std::string& value) {
        std::unique_lock lock(mutex_);  // Exclusive/write lock
        config_[key] = value;
    }

    // Upgrade pattern (read then conditionally write)
    void setIfAbsent(const std::string& key, const std::string& value) {
        {
            std::shared_lock readLock(mutex_);
            if (config_.count(key)) return;  // Already exists
        }
        // Must release read lock before acquiring write lock
        // (std::shared_mutex doesn't support upgrade)
        std::unique_lock writeLock(mutex_);
        config_.emplace(key, value);  // emplace won't overwrite if inserted between locks
    }
};

// Custom implementation (for interview -> show understanding)
class ReadWriteLock {
    std::mutex mutex_;
    std::condition_variable readCV_, writeCV_;
    int readers_ = 0;
    int writers_ = 0;
    int waitingWriters_ = 0;

public:
    void lockRead() {
        std::unique_lock lock(mutex_);
        // Wait if a writer is active or waiting (writer preference)
        readCV_.wait(lock, [this] { return writers_ == 0 && waitingWriters_ == 0; });
        ++readers_;
    }

    void unlockRead() {
        std::unique_lock lock(mutex_);
        --readers_;
        if (readers_ == 0) writeCV_.notify_one();
    }

    void lockWrite() {
        std::unique_lock lock(mutex_);
        ++waitingWriters_;
        writeCV_.wait(lock, [this] { return readers_ == 0 && writers_ == 0; });
        --waitingWriters_;
        ++writers_;
    }

    void unlockWrite() {
        std::unique_lock lock(mutex_);
        --writers_;
        writeCV_.notify_all();   // Wake all waiting readers
        readCV_.notify_all();
    }
};
```

### Explanation:
- **Writer preference** in custom implementation: Prevents writer starvation
- **`std::shared_mutex`** doesn't support lock upgrade (read|write) | must release and reacquire
- **Use cases**: Configuration stores, game world state (many readers, rare updates), CAD document metadata
- **When NOT to use**: If writes are frequent (overhead of shared_mutex > plain mutex)
- **Alternative**: Read-Copy-Update (RCU) for read-heavy workloads with infrequent writes

**Reader vs Writer Preference Analysis:**
```
Reader Preference:
  - Writers wait until ALL readers finish -> potential writer starvation
  - Best for: Read-heavy, writes can tolerate delay (config cache)
  
Writer Preference (our implementation):
  - New readers blocked when writer is waiting -> writers get priority
  - Best for: Write consistency matters (financial data, game state)
  
Fair (FIFO):
  - Requests served in arrival order -> no starvation
  - Highest overhead, used when fairness is legally required
```

**Read-Copy-Update (RCU) ? lock-free alternative (advanced):**
```cpp
// RCU pattern for extreme read performance
template<typename T>
class RCU {
    std::atomic<std::shared_ptr<const T>> data_;
public:
    // Readers: Zero overhead -> just atomic load
    std::shared_ptr<const T> read() const {
        return std::atomic_load(&data_);
    }
    
    // Writers: Copy-on-write
    template<typename Fn>
    void update(Fn modifier) {
        auto oldPtr = std::atomic_load(&data_);
        auto newPtr = std::make_shared<T>(*oldPtr);  // Copy
        modifier(*newPtr);                            // Modify copy
        std::atomic_store(&data_, std::move(newPtr)); // Swap atomically
        // Old data stays alive until last reader releases shared_ptr
    }
};
// Used by Linux kernel (RCU), Java CopyOnWriteArrayList
// Perfect when reads are 99%+ of operations
```

---

## Q5: Explain `std::async`, `std::future`, `std::promise`. When to use each|

### Answer:
```cpp
#include <future>
#include <thread>
#include <iostream>

// === std::async -> simplest way to run async work ===
void asyncExample() {
    // launch::async = guaranteed new thread
    auto future1 = std::async(std::launch::async, [] {
        // Heavy computation
        return computeSomething();
    });

    // launch::deferred = lazy evaluation (runs when .get() called)
    auto future2 = std::async(std::launch::deferred, [] {
        return computeOther();
    });

    // Do other work while async runs...
    doOtherWork();

    int result1 = future1.get();  // Blocks until ready
    int result2 = future2.get();  // Actually runs HERE (deferred)
}

// === std::promise + std::future -> producer/consumer channel ===
void promiseExample() {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();

    std::thread producer([&promise] {
        try {
            int result = performCalculation();
            promise.set_value(result);       // Fulfill promise
        } catch (...) {
            promise.set_exception(std::current_exception());  // Propagate error
        }
    });

    // Consumer
    try {
        int value = future.get();  // Blocks until promise fulfilled
        std::cout << "Got: " << value << "\n";
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }

    producer.join();
}

// === std::packaged_task -> callable wrapper with future ===
void packagedTaskExample() {
    std::packaged_task<int(int, int)> task([](int a, int b) {
        return a * b;
    });

    std::future<int> future = task.get_future();

    // Can be moved to another thread or queued
    std::thread t(std::move(task), 6, 7);
    std::cout << future.get() << "\n";  // 42
    t.join();
}

// === Parallel computation pattern ===
std::vector<double> parallelMap(const std::vector<double>& input,
                                 std::function<double(double)> fn) {
    std::vector<std::future<double>> futures;
    for (double val : input) {
        futures.push_back(std::async(std::launch::async, fn, val));
    }

    std::vector<double> results;
    results.reserve(futures.size());
    for (auto& f : futures) {
        results.push_back(f.get());
    }
    return results;
}
```

### Explanation:
| Feature | Use Case | Notes |
|---------|----------|-------|
| `std::async` | Fire-and-forget parallel work | Simplest API; beware `launch::deferred` default |
| `std::promise/future` | One-shot producer-consumer | Manual thread management |
| `std::packaged_task` | Deferred execution with future | Good for thread pools |

**Gotchas:**
- `std::async` without launch policy may be deferred (never actually parallel!)
- Future from `std::async` blocks in destructor (C++ Standards defect)
- `std::future::get()` can only be called once | use `std::shared_future` for multiple consumers

---

## Q6: What are condition variables| Implement a bounded blocking queue.

### Answer:
```cpp
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

template<typename T>
class BoundedBlockingQueue {
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    size_t capacity_;
    bool closed_ = false;

public:
    BoundedBlockingQueue(size_t capacity) : capacity_(capacity) {}

    // Blocking push -> waits if queue is full
    bool push(T item) {
        std::unique_lock lock(mutex_);
        notFull_.wait(lock, [this] { return queue_.size() < capacity_ || closed_; });
        if (closed_) return false;
        queue_.push(std::move(item));
        notEmpty_.notify_one();
        return true;
    }

    // Blocking pop -> waits if queue is empty
    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        notEmpty_.wait(lock, [this] { return !queue_.empty() || closed_; });
        if (queue_.empty()) return std::nullopt;  // Closed and empty
        T item = std::move(queue_.front());
        queue_.pop();
        notFull_.notify_one();
        return item;
    }

    // Timed pop
    std::optional<T> tryPopFor(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        if (!notEmpty_.wait_for(lock, timeout,
                [this] { return !queue_.empty() || closed_; })) {
            return std::nullopt;  // Timeout
        }
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        notFull_.notify_one();
        return item;
    }

    // Close the queue -> unblocks all waiters
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }
};

// Producer-Consumer pattern
void producerConsumer() {
    BoundedBlockingQueue<int> queue(100);

    // Producers
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&queue, i] {
            for (int j = 0; j < 1000; ++j)
                queue.push(i * 1000 + j);
        });
    }

    // Consumers
    std::vector<std::thread> consumers;
    for (int i = 0; i < 2; ++i) {
        consumers.emplace_back([&queue] {
            while (auto item = queue.pop()) {
                process(*item);
            }
        });
    }

    for (auto& p : producers) p.join();
    queue.close();  // Signal consumers to finish
    for (auto& c : consumers) c.join();
}
```

### Explanation:
- **Spurious wakeups**: `wait` with predicate handles them automatically (loops internally)
- **Bounded**: Prevents unbounded memory growth (back-pressure)
- **`close()`**: Graceful shutdown | producers stop pushing, consumers drain and exit
- **CAD use case**: Render job queue (producer: main thread, consumer: render threads)
- **Finance**: Market data processing pipeline
- **Alternative**: For high-throughput, use lock-free MPMC queues (e.g., `moodycamel::ConcurrentQueue`)

---

## Q7: Explain `std::jthread` and cooperative cancellation in C++20.

### Answer:
```cpp
#include <thread>
#include <stop_token>
#include <iostream>
#include <chrono>

// C++20 jthread -> auto-joins and supports cancellation
void jthreadExample() {
    // jthread automatically joins on destruction (no more forgetting to join!)
    std::jthread worker([](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            doWork();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Worker stopped gracefully\n";
    });

    // ... do some work ...
    std::this_thread::sleep_for(std::chrono::seconds(2));

    worker.request_stop();  // Cooperative cancellation
    // worker automatically joins here (destructor)
}

// Stop callback -> register cleanup when stop is requested
void stopCallbackExample() {
    std::jthread worker([](std::stop_token stoken) {
        // Register a callback that runs when stop is requested
        std::stop_callback callback(stoken, [] {
            std::cout << "Cancellation requested ? cleaning up resources\n";
        });

        // Simulate condition variable wait with cancellation
        std::mutex mtx;
        std::condition_variable_any cv;

        std::unique_lock lock(mtx);
        // This wait is cancellable!
        cv.wait(lock, stoken, [] { return false; /* or some condition */ });

        if (stoken.stop_requested()) {
            std::cout << "Woke up due to cancellation\n";
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    worker.request_stop();  // Triggers callback AND wakes cv.wait
}
```

### Explanation:
**`std::jthread` vs `std::thread`:**
| Feature | `std::thread` | `std::jthread` (C++20) |
|---------|--------------|----------------------|
| Destructor | `std::terminate` if not joined | Auto-joins |
| Cancellation | Manual (atomic flag) | Built-in `stop_token` |
| Exception safety | Must manually join in catch | Automatic |
| CV integration | No | `condition_variable_any` supports `stop_token` |

**Why it matters**: Eliminates entire classes of bugs:
- Forgetting to join | crash
- Exception causes thread leak | resource exhaustion
- Non-cooperative cancellation | data corruption

---

## Q8: What is false sharing| How do you avoid it|

### Answer:
```cpp
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <new>  // std::hardware_destructive_interference_size

// BAD: False sharing -> counters on the same cache line
struct BadCounters {
    std::atomic<int64_t> counter1;  // 8 bytes
    std::atomic<int64_t> counter2;  // 8 bytes -> same 64-byte cache line!
};

// GOOD: Pad to separate cache lines
struct GoodCounters {
    alignas(64) std::atomic<int64_t> counter1;  // Own cache line
    alignas(64) std::atomic<int64_t> counter2;  // Own cache line
};

// C++17 standard way:
struct StandardCounters {
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int64_t> counter1;
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int64_t> counter2;
};

// Benchmark to demonstrate
void benchmark() {
    BadCounters bad;
    GoodCounters good;

    auto test = [](auto& counters, auto& c1, auto& c2) {
        auto start = std::chrono::high_resolution_clock::now();

        std::thread t1([&c1] {
            for (int i = 0; i < 100'000'000; ++i)
                c1.fetch_add(1, std::memory_order_relaxed);
        });

        std::thread t2([&c2] {
            for (int i = 0; i < 100'000'000; ++i)
                c2.fetch_add(1, std::memory_order_relaxed);
        });

        t1.join(); t2.join();

        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    };

    auto badTime = test(bad, bad.counter1, bad.counter2);
    auto goodTime = test(good, good.counter1, good.counter2);
    // badTime is typically 3-10x slower than goodTime!
}
```

### Explanation:
**What is false sharing|**
- CPU caches work in 64-byte cache lines
- If two threads write to different variables on the SAME cache line, the cache line bounces between cores ("ping-pong")
- Each write invalidates the other core's cache line, forcing a reload
- This is **invisible** in code but causes **3-10x slowdown**

**Where it matters:**
- **HFT**: Per-thread counters, position accumulators
- **Gaming**: Per-entity data updated by different threads
- **Thread pool**: Per-thread work-stealing deques

**Prevention:**
1. `alignas(64)` or `alignas(std::hardware_destructive_interference_size)`
2. Padding between hot variables
3. Thread-local accumulators, merge periodically
4. SoA layout with hot/cold separation

---

## Q9: Implement a Spinlock and explain when to use it over a mutex.

### Answer:
```cpp
#include <atomic>

class Spinlock {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Spin -> but hint the CPU to save power
            #if defined(__x86_64__) || defined(_M_X64)
            __builtin_ia32_pause();  // or _mm_pause() with intrinsics
            #elif defined(__aarch64__)
            __asm__ volatile("yield");
            #endif
        }
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }
};

// TTAS (Test-and-Test-and-Set) -> better performance under contention
class TTASSpinlock {
    std::atomic<bool> locked_{false};

public:
    void lock() {
        while (true) {
            // First, spin on read (doesn't invalidate other caches)
            while (locked_.load(std::memory_order_relaxed)) {
                #ifdef _MSC_VER
                _mm_pause();
                #endif
            }
            // Then try to acquire
            if (!locked_.exchange(true, std::memory_order_acquire))
                return;  // Got it!
        }
    }

    void unlock() {
        locked_.store(false, std::memory_order_release);
    }
};

// Usage with RAII
template<typename Lock>
class LockGuard {
    Lock& lock_;
public:
    LockGuard(Lock& lock) : lock_(lock) { lock_.lock(); }
    ~LockGuard() { lock_.unlock(); }
};
```

### Explanation:
| Aspect | Spinlock | Mutex (`std::mutex`) |
|--------|----------|---------------------|
| Wait method | Busy-spin (burns CPU) | OS sleep (context switch) |
| Latency | Very low (no syscall) | Higher (kernel transition) |
| CPU usage | 100% while waiting | 0% while sleeping |
| Best for | Very short critical sections | Longer critical sections |
| Fairness | No guarantee | OS-level fairness |

**Use spinlock when:**
- Critical section < ~1 microsecond
- Thread count | CPU cores (no preemption)
- HFT hot path, game engine physics step, real-time audio

**Use mutex when:**
- Critical section is longer or variable
- More threads than cores
- You hold I/O or other blocking operations inside the lock

**CAS-based spinlock with backoff (production-grade):**
```cpp
class AdaptiveSpinlock {
    std::atomic<bool> locked_{false};
public:
    void lock() {
        int spinCount = 0;
        while (true) {
            if (!locked_.exchange(true, std::memory_order_acquire))
                return;
            
            if (spinCount < 16) {
                _mm_pause();  // CPU hint: "I'm spinning"
            } else if (spinCount < 64) {
                std::this_thread::yield();  // Give up time slice
            } else {
                // Exponential backoff: sleep briefly
                std::this_thread::sleep_for(
                    std::chrono::microseconds(1 << std::min(spinCount - 64, 10)));
            }
            ++spinCount;
        }
    }
    void unlock() { locked_.store(false, std::memory_order_release); }
};
// Adaptive: spins first (low latency), then yields (fairness), then sleeps (CPU savings)
```

**Fairness comparison:**
```
TAS Spinlock:    No fairness guarantee -> threads near the lock win (cache line proximity)
TTAS Spinlock:   Slightly fairer -> reduced cache line contention
Ticket Spinlock: FIFO fair -> each thread gets a ticket number, served in order
CLH Lock:        FIFO fair -> queue-based, excellent scalability on NUMA
```

---

## Q10: Explain C++17/20 parallel algorithms. How do execution policies work|

### Answer:
```cpp
#include <algorithm>
#include <execution>
#include <vector>
#include <numeric>

void parallelAlgorithms() {
    std::vector<int> data(10'000'000);
    std::iota(data.begin(), data.end(), 0);

    // Sequential (default)
    std::sort(std::execution::seq, data.begin(), data.end());

    // Parallel -> uses thread pool internally
    std::sort(std::execution::par, data.begin(), data.end());

    // Parallel + Vectorized (SIMD) -> strongest
    std::sort(std::execution::par_unseq, data.begin(), data.end());

    // C++20: Unsequenced (SIMD only, single thread)
    std::for_each(std::execution::unseq, data.begin(), data.end(),
                  [](int& x) { x *= 2; });

    // Parallel reduce -> associative + commutative operation
    double sum = std::reduce(std::execution::par, data.begin(), data.end(), 0.0);

    // Parallel transform_reduce (map-reduce)
    double dotProduct = std::transform_reduce(
        std::execution::par,
        vec1.begin(), vec1.end(),
        vec2.begin(),
        0.0  // Initial value
    );
}
```

### Explanation:
| Policy | Threads | SIMD | Notes |
|--------|---------|------|-------|
| `seq` | 1 | No | Same as no policy |
| `par` | Multiple | No | Callbacks must be thread-safe |
| `par_unseq` | Multiple | Yes | No mutexes, no memory allocation in callback |
| `unseq` (C++20) | 1 | Yes | Vectorized but single-threaded |

**Constraints on `par_unseq`:**
- No locks (could deadlock with SIMD interleaving)
- No `new`/`delete` inside lambda
- No file I/O
- No `thread_local` access

**Real-world performance**: `par` sort on 10M elements | 3-4x speedup on 8 cores (not linear due to merge overhead).

**Performance gotchas & when parallel algorithms hurt:**
```cpp
// BAD: Parallel overhead > work per element
std::for_each(std::execution::par, small_vec.begin(), small_vec.end(),
              [](int& x) { x *= 2; });
// For vectors < 10K elements, sequential is often faster (thread setup overhead ~10-50?s)

// BAD: Non-associative operation with reduce
double avg = std::reduce(std::execution::par, v.begin(), v.end(), 0.0) / v.size();
// reduce requires associative+commutative op; floating-point addition is NOT strictly associative
// May give slightly different result than sequential (acceptable for most uses)

// GOOD: Large data, expensive per-element work
auto results = std::transform(std::execution::par,
    models.begin(), models.end(), outputs.begin(),
    [](const Model& m) { return m.computeExpensiveResult(); });
// 8x speedup when each element takes milliseconds
```

**Custom thread pool (MSVC implementation detail):**
```
MSVC: Uses Windows Thread Pool API (IoCompletionPort-based)
GCC/libstdc++: Uses Intel TBB (Threading Building Blocks) | must link -ltbb
Clang/libc++: Limited support (falls back to sequential for some algorithms)

// To use with GCC: g++ -std=c++17 -ltbb program.cpp
```

**Benchmark template for comparing policies:**
```cpp
#include <chrono>
template<typename Policy, typename Fn>
auto benchmark(Policy policy, Fn fn) {
    auto start = std::chrono::high_resolution_clock::now();
    fn();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}
// Compare: benchmark(std::execution::seq, ...) vs benchmark(std::execution::par, ...)
```

---

# ENHANCED SECTION: Staff Engineer Concurrency Deep Dive

> *These questions test production-level concurrency expertise | debugging real race conditions, choosing the right synchronization primitive, and understanding hardware-level behavior.*

---

## Q8: Explain the ABA Problem in lock-free programming. How do you solve it|

### Answer:
```cpp
// ABA Problem:
// Thread 1: Read head = A
// Thread 2: Pop A, Pop B, Push A back
// Thread 1: CAS(head, A, newNode) SUCCEEDS -> but the list changed!

// Solution 1: Tagged pointers (generation counter)
template<typename T>
struct TaggedPtr {
    T* ptr;
    uintptr_t tag; // Incremented on every modification
};

// Solution 2: Hazard Pointers (standard in C++26)
// Readers "announce" which nodes they're accessing
// Nodes can only be reclaimed when no thread has a hazard pointer to them

// Solution 3: Epoch-Based Reclamation (used by crossbeam in Rust)
// Threads enter/exit "epochs"
// Memory freed in epoch N can be reclaimed when all threads have
// advanced past epoch N+2
```

### Explanation:
- **ABA is the #1 lock-free bug** | CAS succeeds but the state has changed underneath
- **iCluster relevance**: The communication module uses packet counters in DMKINT.H to detect missing/reordered messages | conceptually similar to generation counters preventing ABA
- **Production systems**: Java's `AtomicStampedReference`, RCU in Linux kernel, hazard pointers in folly (Facebook's C++ library)

---

## Q9: Design a Multi-Producer Multi-Consumer (MPMC) Lock-Free Queue.

### Answer:
```cpp
// Key insight: SPSC is straightforward, MPMC is hard
// Production approach: Use ring buffer with atomic head/tail + sequence numbers

template<typename T, size_t N>
class MPMCQueue {
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };
    
    alignas(64) std::array<Cell, N> buffer_;
    alignas(64) std::atomic<size_t> enqueue_pos_{0};
    alignas(64) std::atomic<size_t> dequeue_pos_{0};

public:
    MPMCQueue() {
        for (size_t i = 0; i < N; ++i)
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool try_enqueue(const T& data) {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        while (true) {
            auto& cell = buffer_[pos % N];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed)) {
                    cell.data = data;
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // Full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
    }
};
```

### Explanation:
- Each cell has a **sequence number** that acts as both availability flag and ABA prevention
- **alignas(64)**: Prevents false sharing between producer and consumer cache lines
- **Production alternatives**: Dmitry Vyukov's bounded MPMC queue, moodycamel::ConcurrentQueue, boost::lockfree::queue
- **iCluster analogy**: The queue-based IPC (MO, CI, CO, DI, DO queues in DMKINT) is a persistent producer-consumer system with multiple producers and consumers

---

## Q10: When should you use a mutex vs atomic vs lock-free vs wait-free|

### Answer:
```
Decision Matrix:
+---------------------------------------------------------------------+
| Primitive     -> Latency     -> Throughput -> Complexity -> Use When           |
+---------------------------------------------------------------------+
| std::mutex    -> Moderate    -> Good       -> Low        -> Default choice     |
| shared_mutex  -> Low (reads) | High reads -> Low        -> Read-heavy (95%+)  |
| std::atomic   -> Very low    -> Very high  -> Medium     -> Simple counters    |
| Lock-free     -> Bounded     -> High       -> Very high  -> Hard RT, no block  |
| Wait-free     -> Guaranteed  -> Variable   -> Extreme    -> Safety-critical    |
+---------------------------------------------------------------------+

Rules of thumb:
1. Start with mutex. Measure. Only go lock-free if contention is proven.
2. Lock-free != faster. Lock-free means "progress guarantee" not speed.
3. Wait-free means EVERY thread completes in bounded steps -> rarely needed.
4. Cache line alignment matters more than algorithm choice for throughput.
5. The best concurrency is NO concurrency -> partition data by thread.
```

### Explanation:
- **iCluster's approach**: Uses OS-level queue primitives (data queues on IBM i) which are essentially mutex-protected FIFO buffers | simple, reliable, well-tested
- **HFT's approach**: Lock-free SPSC ring buffers on hot path, mutex on cold path (configuration changes)
- **Senior insight**: "I chose mutex because our profiling showed contention was <2% and correctness was more important than the 50ns savings" | THIS is what interviewers want to hear

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    
| Primitive     | Latency     | Throughput | Complexity | Use When           |
 
        $match = # Set 6: Multithreading & Concurrency in C++
## Senior-Level Interview Questions (10 Years Experience)

---

## Q1: Explain the C++ Memory Model. What are memory orderings and when do you use each|

### Answer:

```cpp
#include <atomic>

// Memory orderings from weakest to strongest:

// 1. memory_order_relaxed -> No ordering guarantees, only atomicity
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);  // Just need atomic increment
// Use: Simple counters, statistics

// 2. memory_order_acquire / memory_order_release -> Synchronizes-with relationship
std::atomic<bool> ready{false};
int data = 0;

// Thread 1 (Producer)
void producer() {
    data = 42;                                          // Non-atomic write
    ready.store(true, std::memory_order_release);       // Release: all prior writes visible
}

// Thread 2 (Consumer)
void consumer() {
    while (!ready.load(std::memory_order_acquire)) {}   // Acquire: sees all writes before release
    assert(data == 42);  // Guaranteed to see 42!
}

// 3. memory_order_acq_rel -> Both acquire and release (for read-modify-write)
std::atomic<Node*> head{nullptr};
void pushLockFree(Node* node) {
    node->next = head.load(std::memory_order_relaxed);
    while (!head.compare_exchange_weak(node->next, node,
            std::memory_order_acq_rel, std::memory_order_relaxed)) {}
}

// 4. memory_order_seq_cst -> Full sequential consistency (default, strongest)
std::atomic<int> x{0}, y{0};
// Thread 1: x.store(1);  // seq_cst by default
// Thread 2: y.store(1);
// Thread 3: if (x.load() == 1 && y.load() == 0) { /* possible */ }
// Thread 4: if (y.load() == 1 && x.load() == 0) { /* possible, but... */ }
// With seq_cst: if T3 sees x=1,y=0 then T4 CANNOT see y=1,x=0
// This total ordering is expensive on ARM/POWER CPUs
```

### Explanation:
| Ordering | Cost | Guarantee | Use Case |
|----------|------|-----------|----------|
| `relaxed` | Cheapest | Atomicity only | Counters, flags (no data dependency) |
| `acquire/release` | Moderate | Synchronizes-with (producer-consumer) | Lock implementations, publish pattern |
| `acq_rel` | Moderate | Both acquire and release | CAS loops, lock-free data structures |
| `seq_cst` | Expensive | Total global ordering | When reasoning about multiple atomics |

**Key insight**: On x86, `acquire`/`release` are often free (x86 has strong memory model). On ARM/POWER, they generate barrier instructions. This matters hugely for gaming (cross-platform) and HFT (every nanosecond counts).

---

## Q2: Implement a Thread Pool with task queuing and futures.

### Answer:
```cpp
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits>

class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mutex_);
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    // Submit task and get future for result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard lock(mutex_);
            if (stop_) throw std::runtime_error("ThreadPool is stopped");
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_)
            worker.join();
    }
};

// Usage
void example() {
    ThreadPool pool(4);

    auto future1 = pool.submit([](int a, int b) { return a + b; }, 3, 4);
    auto future2 = pool.submit([] { return std::string("hello"); });

    std::cout << future1.get() << "\n";  // 7
    std::cout << future2.get() << "\n";  // "hello"

    // Parallel processing
    std::vector<std::future<double>> results;
    for (int i = 0; i < 1000; ++i) {
        results.push_back(pool.submit([i] {
            return std::sqrt(static_cast<double>(i));
        }));
    }

    double sum = 0;
    for (auto& f : results)
        sum += f.get();
}
```

### Explanation:
- **`std::packaged_task`** wraps callable and provides a `std::future` for the result
- **`std::invoke_result_t`** (C++17) deduces return type
- **Graceful shutdown**: Set `stop_`, notify all, join all threads
- **No task stealing** in this basic version | production pools use work-stealing (like Intel TBB)
- **Follow-up**: Discuss priority queues, task dependencies (DAG scheduling), and per-thread queues

---

## Q3: What is a deadlock| How do you prevent, detect, and recover from it|

### Answer:

**Four conditions for deadlock (ALL must hold):**
1. **Mutual Exclusion**: Resource held exclusively
2. **Hold and Wait**: Thread holds one resource while waiting for another
3. **No Preemption**: Resources can't be forcibly taken
4. **Circular Wait**: A|B|C|A dependency chain

**Prevention Strategies:**
```cpp
// 1. Lock Ordering -> Break circular wait
std::mutex mutexA, mutexB;

// BAD: Thread 1 locks A then B, Thread 2 locks B then A -> DEADLOCK
// GOOD: Always lock in consistent order (by address or ID)
void safeOperation() {
    // std::lock locks multiple mutexes without deadlock (uses try-lock internally)
    std::scoped_lock lock(mutexA, mutexB);  // C++17 ? preferred
    // ... work with both resources
}

// 2. Try-lock with backoff
bool tryOperation() {
    std::unique_lock lockA(mutexA, std::try_to_lock);
    if (!lockA.owns_lock()) return false;

    std::unique_lock lockB(mutexB, std::try_to_lock);
    if (!lockB.owns_lock()) return false;  // lockA auto-released

    // ... work with both resources
    return true;
}

// 3. Lock hierarchy (compile-time enforcement)
class HierarchicalMutex {
    std::mutex mtx_;
    int level_;
    static thread_local int currentLevel_;

public:
    HierarchicalMutex(int level) : level_(level) {}

    void lock() {
        if (level_ >= currentLevel_)
            throw std::runtime_error("Lock hierarchy violation");
        mtx_.lock();
        currentLevel_ = level_;
    }

    void unlock() {
        currentLevel_ = INT_MAX;
        mtx_.unlock();
    }
};

// Usage: High-level locks (level 1000) must be acquired before low-level (level 100)
HierarchicalMutex dbLock(1000);     // High level
HierarchicalMutex cacheLock(500);   // Mid level
HierarchicalMutex itemLock(100);    // Low level
```

**Detection (runtime):**
- Build a wait-for graph at runtime
- Periodically check for cycles (DFS)
- Log lock acquisition order for post-mortem analysis

### Explanation:
- **`std::scoped_lock`** (C++17) is the modern answer | locks multiple mutexes atomically
- **Lock-free alternatives**: Often better for high-contention scenarios (gaming frame updates, HFT)
- **Real-world CAD example**: Geometry kernel lock + UI lock + undo stack lock | use lock hierarchy

---

## Q4: Implement a Reader-Writer Lock and explain its use cases.

### Answer:
```cpp
#include <shared_mutex>
#include <mutex>
#include <string>
#include <unordered_map>

// Using std::shared_mutex (C++17)
class ThreadSafeConfig {
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> config_;

public:
    // Multiple readers can read simultaneously
    std::string get(const std::string& key) const {
        std::shared_lock lock(mutex_);  // Shared/read lock
        auto it = config_.find(key);
        return it != config_.end() ? it->second : "";
    }

    // Writers get exclusive access
    void set(const std::string& key, const std::string& value) {
        std::unique_lock lock(mutex_);  // Exclusive/write lock
        config_[key] = value;
    }

    // Upgrade pattern (read then conditionally write)
    void setIfAbsent(const std::string& key, const std::string& value) {
        {
            std::shared_lock readLock(mutex_);
            if (config_.count(key)) return;  // Already exists
        }
        // Must release read lock before acquiring write lock
        // (std::shared_mutex doesn't support upgrade)
        std::unique_lock writeLock(mutex_);
        config_.emplace(key, value);  // emplace won't overwrite if inserted between locks
    }
};

// Custom implementation (for interview -> show understanding)
class ReadWriteLock {
    std::mutex mutex_;
    std::condition_variable readCV_, writeCV_;
    int readers_ = 0;
    int writers_ = 0;
    int waitingWriters_ = 0;

public:
    void lockRead() {
        std::unique_lock lock(mutex_);
        // Wait if a writer is active or waiting (writer preference)
        readCV_.wait(lock, [this] { return writers_ == 0 && waitingWriters_ == 0; });
        ++readers_;
    }

    void unlockRead() {
        std::unique_lock lock(mutex_);
        --readers_;
        if (readers_ == 0) writeCV_.notify_one();
    }

    void lockWrite() {
        std::unique_lock lock(mutex_);
        ++waitingWriters_;
        writeCV_.wait(lock, [this] { return readers_ == 0 && writers_ == 0; });
        --waitingWriters_;
        ++writers_;
    }

    void unlockWrite() {
        std::unique_lock lock(mutex_);
        --writers_;
        writeCV_.notify_all();   // Wake all waiting readers
        readCV_.notify_all();
    }
};
```

### Explanation:
- **Writer preference** in custom implementation: Prevents writer starvation
- **`std::shared_mutex`** doesn't support lock upgrade (read|write) | must release and reacquire
- **Use cases**: Configuration stores, game world state (many readers, rare updates), CAD document metadata
- **When NOT to use**: If writes are frequent (overhead of shared_mutex > plain mutex)
- **Alternative**: Read-Copy-Update (RCU) for read-heavy workloads with infrequent writes

**Reader vs Writer Preference Analysis:**
```
Reader Preference:
  - Writers wait until ALL readers finish -> potential writer starvation
  - Best for: Read-heavy, writes can tolerate delay (config cache)
  
Writer Preference (our implementation):
  - New readers blocked when writer is waiting -> writers get priority
  - Best for: Write consistency matters (financial data, game state)
  
Fair (FIFO):
  - Requests served in arrival order -> no starvation
  - Highest overhead, used when fairness is legally required
```

**Read-Copy-Update (RCU) ? lock-free alternative (advanced):**
```cpp
// RCU pattern for extreme read performance
template<typename T>
class RCU {
    std::atomic<std::shared_ptr<const T>> data_;
public:
    // Readers: Zero overhead -> just atomic load
    std::shared_ptr<const T> read() const {
        return std::atomic_load(&data_);
    }
    
    // Writers: Copy-on-write
    template<typename Fn>
    void update(Fn modifier) {
        auto oldPtr = std::atomic_load(&data_);
        auto newPtr = std::make_shared<T>(*oldPtr);  // Copy
        modifier(*newPtr);                            // Modify copy
        std::atomic_store(&data_, std::move(newPtr)); // Swap atomically
        // Old data stays alive until last reader releases shared_ptr
    }
};
// Used by Linux kernel (RCU), Java CopyOnWriteArrayList
// Perfect when reads are 99%+ of operations
```

---

## Q5: Explain `std::async`, `std::future`, `std::promise`. When to use each|

### Answer:
```cpp
#include <future>
#include <thread>
#include <iostream>

// === std::async -> simplest way to run async work ===
void asyncExample() {
    // launch::async = guaranteed new thread
    auto future1 = std::async(std::launch::async, [] {
        // Heavy computation
        return computeSomething();
    });

    // launch::deferred = lazy evaluation (runs when .get() called)
    auto future2 = std::async(std::launch::deferred, [] {
        return computeOther();
    });

    // Do other work while async runs...
    doOtherWork();

    int result1 = future1.get();  // Blocks until ready
    int result2 = future2.get();  // Actually runs HERE (deferred)
}

// === std::promise + std::future -> producer/consumer channel ===
void promiseExample() {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();

    std::thread producer([&promise] {
        try {
            int result = performCalculation();
            promise.set_value(result);       // Fulfill promise
        } catch (...) {
            promise.set_exception(std::current_exception());  // Propagate error
        }
    });

    // Consumer
    try {
        int value = future.get();  // Blocks until promise fulfilled
        std::cout << "Got: " << value << "\n";
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }

    producer.join();
}

// === std::packaged_task -> callable wrapper with future ===
void packagedTaskExample() {
    std::packaged_task<int(int, int)> task([](int a, int b) {
        return a * b;
    });

    std::future<int> future = task.get_future();

    // Can be moved to another thread or queued
    std::thread t(std::move(task), 6, 7);
    std::cout << future.get() << "\n";  // 42
    t.join();
}

// === Parallel computation pattern ===
std::vector<double> parallelMap(const std::vector<double>& input,
                                 std::function<double(double)> fn) {
    std::vector<std::future<double>> futures;
    for (double val : input) {
        futures.push_back(std::async(std::launch::async, fn, val));
    }

    std::vector<double> results;
    results.reserve(futures.size());
    for (auto& f : futures) {
        results.push_back(f.get());
    }
    return results;
}
```

### Explanation:
| Feature | Use Case | Notes |
|---------|----------|-------|
| `std::async` | Fire-and-forget parallel work | Simplest API; beware `launch::deferred` default |
| `std::promise/future` | One-shot producer-consumer | Manual thread management |
| `std::packaged_task` | Deferred execution with future | Good for thread pools |

**Gotchas:**
- `std::async` without launch policy may be deferred (never actually parallel!)
- Future from `std::async` blocks in destructor (C++ Standards defect)
- `std::future::get()` can only be called once | use `std::shared_future` for multiple consumers

---

## Q6: What are condition variables| Implement a bounded blocking queue.

### Answer:
```cpp
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

template<typename T>
class BoundedBlockingQueue {
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    size_t capacity_;
    bool closed_ = false;

public:
    BoundedBlockingQueue(size_t capacity) : capacity_(capacity) {}

    // Blocking push -> waits if queue is full
    bool push(T item) {
        std::unique_lock lock(mutex_);
        notFull_.wait(lock, [this] { return queue_.size() < capacity_ || closed_; });
        if (closed_) return false;
        queue_.push(std::move(item));
        notEmpty_.notify_one();
        return true;
    }

    // Blocking pop -> waits if queue is empty
    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        notEmpty_.wait(lock, [this] { return !queue_.empty() || closed_; });
        if (queue_.empty()) return std::nullopt;  // Closed and empty
        T item = std::move(queue_.front());
        queue_.pop();
        notFull_.notify_one();
        return item;
    }

    // Timed pop
    std::optional<T> tryPopFor(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        if (!notEmpty_.wait_for(lock, timeout,
                [this] { return !queue_.empty() || closed_; })) {
            return std::nullopt;  // Timeout
        }
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        notFull_.notify_one();
        return item;
    }

    // Close the queue -> unblocks all waiters
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }
};

// Producer-Consumer pattern
void producerConsumer() {
    BoundedBlockingQueue<int> queue(100);

    // Producers
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&queue, i] {
            for (int j = 0; j < 1000; ++j)
                queue.push(i * 1000 + j);
        });
    }

    // Consumers
    std::vector<std::thread> consumers;
    for (int i = 0; i < 2; ++i) {
        consumers.emplace_back([&queue] {
            while (auto item = queue.pop()) {
                process(*item);
            }
        });
    }

    for (auto& p : producers) p.join();
    queue.close();  // Signal consumers to finish
    for (auto& c : consumers) c.join();
}
```

### Explanation:
- **Spurious wakeups**: `wait` with predicate handles them automatically (loops internally)
- **Bounded**: Prevents unbounded memory growth (back-pressure)
- **`close()`**: Graceful shutdown | producers stop pushing, consumers drain and exit
- **CAD use case**: Render job queue (producer: main thread, consumer: render threads)
- **Finance**: Market data processing pipeline
- **Alternative**: For high-throughput, use lock-free MPMC queues (e.g., `moodycamel::ConcurrentQueue`)

---

## Q7: Explain `std::jthread` and cooperative cancellation in C++20.

### Answer:
```cpp
#include <thread>
#include <stop_token>
#include <iostream>
#include <chrono>

// C++20 jthread -> auto-joins and supports cancellation
void jthreadExample() {
    // jthread automatically joins on destruction (no more forgetting to join!)
    std::jthread worker([](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            doWork();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Worker stopped gracefully\n";
    });

    // ... do some work ...
    std::this_thread::sleep_for(std::chrono::seconds(2));

    worker.request_stop();  // Cooperative cancellation
    // worker automatically joins here (destructor)
}

// Stop callback -> register cleanup when stop is requested
void stopCallbackExample() {
    std::jthread worker([](std::stop_token stoken) {
        // Register a callback that runs when stop is requested
        std::stop_callback callback(stoken, [] {
            std::cout << "Cancellation requested ? cleaning up resources\n";
        });

        // Simulate condition variable wait with cancellation
        std::mutex mtx;
        std::condition_variable_any cv;

        std::unique_lock lock(mtx);
        // This wait is cancellable!
        cv.wait(lock, stoken, [] { return false; /* or some condition */ });

        if (stoken.stop_requested()) {
            std::cout << "Woke up due to cancellation\n";
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    worker.request_stop();  // Triggers callback AND wakes cv.wait
}
```

### Explanation:
**`std::jthread` vs `std::thread`:**
| Feature | `std::thread` | `std::jthread` (C++20) |
|---------|--------------|----------------------|
| Destructor | `std::terminate` if not joined | Auto-joins |
| Cancellation | Manual (atomic flag) | Built-in `stop_token` |
| Exception safety | Must manually join in catch | Automatic |
| CV integration | No | `condition_variable_any` supports `stop_token` |

**Why it matters**: Eliminates entire classes of bugs:
- Forgetting to join | crash
- Exception causes thread leak | resource exhaustion
- Non-cooperative cancellation | data corruption

---

## Q8: What is false sharing| How do you avoid it|

### Answer:
```cpp
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <new>  // std::hardware_destructive_interference_size

// BAD: False sharing -> counters on the same cache line
struct BadCounters {
    std::atomic<int64_t> counter1;  // 8 bytes
    std::atomic<int64_t> counter2;  // 8 bytes -> same 64-byte cache line!
};

// GOOD: Pad to separate cache lines
struct GoodCounters {
    alignas(64) std::atomic<int64_t> counter1;  // Own cache line
    alignas(64) std::atomic<int64_t> counter2;  // Own cache line
};

// C++17 standard way:
struct StandardCounters {
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int64_t> counter1;
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int64_t> counter2;
};

// Benchmark to demonstrate
void benchmark() {
    BadCounters bad;
    GoodCounters good;

    auto test = [](auto& counters, auto& c1, auto& c2) {
        auto start = std::chrono::high_resolution_clock::now();

        std::thread t1([&c1] {
            for (int i = 0; i < 100'000'000; ++i)
                c1.fetch_add(1, std::memory_order_relaxed);
        });

        std::thread t2([&c2] {
            for (int i = 0; i < 100'000'000; ++i)
                c2.fetch_add(1, std::memory_order_relaxed);
        });

        t1.join(); t2.join();

        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    };

    auto badTime = test(bad, bad.counter1, bad.counter2);
    auto goodTime = test(good, good.counter1, good.counter2);
    // badTime is typically 3-10x slower than goodTime!
}
```

### Explanation:
**What is false sharing|**
- CPU caches work in 64-byte cache lines
- If two threads write to different variables on the SAME cache line, the cache line bounces between cores ("ping-pong")
- Each write invalidates the other core's cache line, forcing a reload
- This is **invisible** in code but causes **3-10x slowdown**

**Where it matters:**
- **HFT**: Per-thread counters, position accumulators
- **Gaming**: Per-entity data updated by different threads
- **Thread pool**: Per-thread work-stealing deques

**Prevention:**
1. `alignas(64)` or `alignas(std::hardware_destructive_interference_size)`
2. Padding between hot variables
3. Thread-local accumulators, merge periodically
4. SoA layout with hot/cold separation

---

## Q9: Implement a Spinlock and explain when to use it over a mutex.

### Answer:
```cpp
#include <atomic>

class Spinlock {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Spin -> but hint the CPU to save power
            #if defined(__x86_64__) || defined(_M_X64)
            __builtin_ia32_pause();  // or _mm_pause() with intrinsics
            #elif defined(__aarch64__)
            __asm__ volatile("yield");
            #endif
        }
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }
};

// TTAS (Test-and-Test-and-Set) -> better performance under contention
class TTASSpinlock {
    std::atomic<bool> locked_{false};

public:
    void lock() {
        while (true) {
            // First, spin on read (doesn't invalidate other caches)
            while (locked_.load(std::memory_order_relaxed)) {
                #ifdef _MSC_VER
                _mm_pause();
                #endif
            }
            // Then try to acquire
            if (!locked_.exchange(true, std::memory_order_acquire))
                return;  // Got it!
        }
    }

    void unlock() {
        locked_.store(false, std::memory_order_release);
    }
};

// Usage with RAII
template<typename Lock>
class LockGuard {
    Lock& lock_;
public:
    LockGuard(Lock& lock) : lock_(lock) { lock_.lock(); }
    ~LockGuard() { lock_.unlock(); }
};
```

### Explanation:
| Aspect | Spinlock | Mutex (`std::mutex`) |
|--------|----------|---------------------|
| Wait method | Busy-spin (burns CPU) | OS sleep (context switch) |
| Latency | Very low (no syscall) | Higher (kernel transition) |
| CPU usage | 100% while waiting | 0% while sleeping |
| Best for | Very short critical sections | Longer critical sections |
| Fairness | No guarantee | OS-level fairness |

**Use spinlock when:**
- Critical section < ~1 microsecond
- Thread count | CPU cores (no preemption)
- HFT hot path, game engine physics step, real-time audio

**Use mutex when:**
- Critical section is longer or variable
- More threads than cores
- You hold I/O or other blocking operations inside the lock

**CAS-based spinlock with backoff (production-grade):**
```cpp
class AdaptiveSpinlock {
    std::atomic<bool> locked_{false};
public:
    void lock() {
        int spinCount = 0;
        while (true) {
            if (!locked_.exchange(true, std::memory_order_acquire))
                return;
            
            if (spinCount < 16) {
                _mm_pause();  // CPU hint: "I'm spinning"
            } else if (spinCount < 64) {
                std::this_thread::yield();  // Give up time slice
            } else {
                // Exponential backoff: sleep briefly
                std::this_thread::sleep_for(
                    std::chrono::microseconds(1 << std::min(spinCount - 64, 10)));
            }
            ++spinCount;
        }
    }
    void unlock() { locked_.store(false, std::memory_order_release); }
};
// Adaptive: spins first (low latency), then yields (fairness), then sleeps (CPU savings)
```

**Fairness comparison:**
```
TAS Spinlock:    No fairness guarantee -> threads near the lock win (cache line proximity)
TTAS Spinlock:   Slightly fairer -> reduced cache line contention
Ticket Spinlock: FIFO fair -> each thread gets a ticket number, served in order
CLH Lock:        FIFO fair -> queue-based, excellent scalability on NUMA
```

---

## Q10: Explain C++17/20 parallel algorithms. How do execution policies work|

### Answer:
```cpp
#include <algorithm>
#include <execution>
#include <vector>
#include <numeric>

void parallelAlgorithms() {
    std::vector<int> data(10'000'000);
    std::iota(data.begin(), data.end(), 0);

    // Sequential (default)
    std::sort(std::execution::seq, data.begin(), data.end());

    // Parallel -> uses thread pool internally
    std::sort(std::execution::par, data.begin(), data.end());

    // Parallel + Vectorized (SIMD) -> strongest
    std::sort(std::execution::par_unseq, data.begin(), data.end());

    // C++20: Unsequenced (SIMD only, single thread)
    std::for_each(std::execution::unseq, data.begin(), data.end(),
                  [](int& x) { x *= 2; });

    // Parallel reduce -> associative + commutative operation
    double sum = std::reduce(std::execution::par, data.begin(), data.end(), 0.0);

    // Parallel transform_reduce (map-reduce)
    double dotProduct = std::transform_reduce(
        std::execution::par,
        vec1.begin(), vec1.end(),
        vec2.begin(),
        0.0  // Initial value
    );
}
```

### Explanation:
| Policy | Threads | SIMD | Notes |
|--------|---------|------|-------|
| `seq` | 1 | No | Same as no policy |
| `par` | Multiple | No | Callbacks must be thread-safe |
| `par_unseq` | Multiple | Yes | No mutexes, no memory allocation in callback |
| `unseq` (C++20) | 1 | Yes | Vectorized but single-threaded |

**Constraints on `par_unseq`:**
- No locks (could deadlock with SIMD interleaving)
- No `new`/`delete` inside lambda
- No file I/O
- No `thread_local` access

**Real-world performance**: `par` sort on 10M elements | 3-4x speedup on 8 cores (not linear due to merge overhead).

**Performance gotchas & when parallel algorithms hurt:**
```cpp
// BAD: Parallel overhead > work per element
std::for_each(std::execution::par, small_vec.begin(), small_vec.end(),
              [](int& x) { x *= 2; });
// For vectors < 10K elements, sequential is often faster (thread setup overhead ~10-50?s)

// BAD: Non-associative operation with reduce
double avg = std::reduce(std::execution::par, v.begin(), v.end(), 0.0) / v.size();
// reduce requires associative+commutative op; floating-point addition is NOT strictly associative
// May give slightly different result than sequential (acceptable for most uses)

// GOOD: Large data, expensive per-element work
auto results = std::transform(std::execution::par,
    models.begin(), models.end(), outputs.begin(),
    [](const Model& m) { return m.computeExpensiveResult(); });
// 8x speedup when each element takes milliseconds
```

**Custom thread pool (MSVC implementation detail):**
```
MSVC: Uses Windows Thread Pool API (IoCompletionPort-based)
GCC/libstdc++: Uses Intel TBB (Threading Building Blocks) | must link -ltbb
Clang/libc++: Limited support (falls back to sequential for some algorithms)

// To use with GCC: g++ -std=c++17 -ltbb program.cpp
```

**Benchmark template for comparing policies:**
```cpp
#include <chrono>
template<typename Policy, typename Fn>
auto benchmark(Policy policy, Fn fn) {
    auto start = std::chrono::high_resolution_clock::now();
    fn();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}
// Compare: benchmark(std::execution::seq, ...) vs benchmark(std::execution::par, ...)
```

---

# ENHANCED SECTION: Staff Engineer Concurrency Deep Dive

> *These questions test production-level concurrency expertise | debugging real race conditions, choosing the right synchronization primitive, and understanding hardware-level behavior.*

---

## Q8: Explain the ABA Problem in lock-free programming. How do you solve it|

### Answer:
```cpp
// ABA Problem:
// Thread 1: Read head = A
// Thread 2: Pop A, Pop B, Push A back
// Thread 1: CAS(head, A, newNode) SUCCEEDS -> but the list changed!

// Solution 1: Tagged pointers (generation counter)
template<typename T>
struct TaggedPtr {
    T* ptr;
    uintptr_t tag; // Incremented on every modification
};

// Solution 2: Hazard Pointers (standard in C++26)
// Readers "announce" which nodes they're accessing
// Nodes can only be reclaimed when no thread has a hazard pointer to them

// Solution 3: Epoch-Based Reclamation (used by crossbeam in Rust)
// Threads enter/exit "epochs"
// Memory freed in epoch N can be reclaimed when all threads have
// advanced past epoch N+2
```

### Explanation:
- **ABA is the #1 lock-free bug** | CAS succeeds but the state has changed underneath
- **iCluster relevance**: The communication module uses packet counters in DMKINT.H to detect missing/reordered messages | conceptually similar to generation counters preventing ABA
- **Production systems**: Java's `AtomicStampedReference`, RCU in Linux kernel, hazard pointers in folly (Facebook's C++ library)

---

## Q9: Design a Multi-Producer Multi-Consumer (MPMC) Lock-Free Queue.

### Answer:
```cpp
// Key insight: SPSC is straightforward, MPMC is hard
// Production approach: Use ring buffer with atomic head/tail + sequence numbers

template<typename T, size_t N>
class MPMCQueue {
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };
    
    alignas(64) std::array<Cell, N> buffer_;
    alignas(64) std::atomic<size_t> enqueue_pos_{0};
    alignas(64) std::atomic<size_t> dequeue_pos_{0};

public:
    MPMCQueue() {
        for (size_t i = 0; i < N; ++i)
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool try_enqueue(const T& data) {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        while (true) {
            auto& cell = buffer_[pos % N];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed)) {
                    cell.data = data;
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // Full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
    }
};
```

### Explanation:
- Each cell has a **sequence number** that acts as both availability flag and ABA prevention
- **alignas(64)**: Prevents false sharing between producer and consumer cache lines
- **Production alternatives**: Dmitry Vyukov's bounded MPMC queue, moodycamel::ConcurrentQueue, boost::lockfree::queue
- **iCluster analogy**: The queue-based IPC (MO, CI, CO, DI, DO queues in DMKINT) is a persistent producer-consumer system with multiple producers and consumers

---

## Q10: When should you use a mutex vs atomic vs lock-free vs wait-free|

### Answer:
```
Decision Matrix:
+---------------------------------------------------------------------+
| Primitive     -> Latency     -> Throughput -> Complexity -> Use When           |
+---------------------------------------------------------------------+
| std::mutex    -> Moderate    -> Good       -> Low        -> Default choice     |
| shared_mutex  -> Low (reads) | High reads -> Low        -> Read-heavy (95%+)  |
| std::atomic   -> Very low    -> Very high  -> Medium     -> Simple counters    |
| Lock-free     -> Bounded     -> High       -> Very high  -> Hard RT, no block  |
| Wait-free     -> Guaranteed  -> Variable   -> Extreme    -> Safety-critical    |
+---------------------------------------------------------------------+

Rules of thumb:
1. Start with mutex. Measure. Only go lock-free if contention is proven.
2. Lock-free != faster. Lock-free means "progress guarantee" not speed.
3. Wait-free means EVERY thread completes in bounded steps -> rarely needed.
4. Cache line alignment matters more than algorithm choice for throughput.
5. The best concurrency is NO concurrency -> partition data by thread.
```

### Explanation:
- **iCluster's approach**: Uses OS-level queue primitives (data queues on IBM i) which are essentially mutex-protected FIFO buffers | simple, reliable, well-tested
- **HFT's approach**: Lock-free SPSC ring buffers on hot path, mutex on cold path (configuration changes)
- **Senior insight**: "I chose mutex because our profiling showed contention was <2% and correctness was more important than the 50ns savings" | THIS is what interviewers want to hear

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    
| std::mutex    | Moderate    | Good       | Low        | Default choice     |
| shared_mutex  | Low (reads) | High reads | Low        | Read-heavy (95%+)  |
| std::atomic   | Very low    | Very high  | Medium     | Simple counters    |
| Lock-free     | Bounded     | High       | Very high  | Hard RT, no block  |
| Wait-free     | Guaranteed  | Variable   | Extreme    | Safety-critical    |
 
        $match = # Set 6: Multithreading & Concurrency in C++
## Senior-Level Interview Questions (10 Years Experience)

---

## Q1: Explain the C++ Memory Model. What are memory orderings and when do you use each|

### Answer:

```cpp
#include <atomic>

// Memory orderings from weakest to strongest:

// 1. memory_order_relaxed -> No ordering guarantees, only atomicity
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);  // Just need atomic increment
// Use: Simple counters, statistics

// 2. memory_order_acquire / memory_order_release -> Synchronizes-with relationship
std::atomic<bool> ready{false};
int data = 0;

// Thread 1 (Producer)
void producer() {
    data = 42;                                          // Non-atomic write
    ready.store(true, std::memory_order_release);       // Release: all prior writes visible
}

// Thread 2 (Consumer)
void consumer() {
    while (!ready.load(std::memory_order_acquire)) {}   // Acquire: sees all writes before release
    assert(data == 42);  // Guaranteed to see 42!
}

// 3. memory_order_acq_rel -> Both acquire and release (for read-modify-write)
std::atomic<Node*> head{nullptr};
void pushLockFree(Node* node) {
    node->next = head.load(std::memory_order_relaxed);
    while (!head.compare_exchange_weak(node->next, node,
            std::memory_order_acq_rel, std::memory_order_relaxed)) {}
}

// 4. memory_order_seq_cst -> Full sequential consistency (default, strongest)
std::atomic<int> x{0}, y{0};
// Thread 1: x.store(1);  // seq_cst by default
// Thread 2: y.store(1);
// Thread 3: if (x.load() == 1 && y.load() == 0) { /* possible */ }
// Thread 4: if (y.load() == 1 && x.load() == 0) { /* possible, but... */ }
// With seq_cst: if T3 sees x=1,y=0 then T4 CANNOT see y=1,x=0
// This total ordering is expensive on ARM/POWER CPUs
```

### Explanation:
| Ordering | Cost | Guarantee | Use Case |
|----------|------|-----------|----------|
| `relaxed` | Cheapest | Atomicity only | Counters, flags (no data dependency) |
| `acquire/release` | Moderate | Synchronizes-with (producer-consumer) | Lock implementations, publish pattern |
| `acq_rel` | Moderate | Both acquire and release | CAS loops, lock-free data structures |
| `seq_cst` | Expensive | Total global ordering | When reasoning about multiple atomics |

**Key insight**: On x86, `acquire`/`release` are often free (x86 has strong memory model). On ARM/POWER, they generate barrier instructions. This matters hugely for gaming (cross-platform) and HFT (every nanosecond counts).

---

## Q2: Implement a Thread Pool with task queuing and futures.

### Answer:
```cpp
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits>

class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mutex_);
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    // Submit task and get future for result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard lock(mutex_);
            if (stop_) throw std::runtime_error("ThreadPool is stopped");
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_)
            worker.join();
    }
};

// Usage
void example() {
    ThreadPool pool(4);

    auto future1 = pool.submit([](int a, int b) { return a + b; }, 3, 4);
    auto future2 = pool.submit([] { return std::string("hello"); });

    std::cout << future1.get() << "\n";  // 7
    std::cout << future2.get() << "\n";  // "hello"

    // Parallel processing
    std::vector<std::future<double>> results;
    for (int i = 0; i < 1000; ++i) {
        results.push_back(pool.submit([i] {
            return std::sqrt(static_cast<double>(i));
        }));
    }

    double sum = 0;
    for (auto& f : results)
        sum += f.get();
}
```

### Explanation:
- **`std::packaged_task`** wraps callable and provides a `std::future` for the result
- **`std::invoke_result_t`** (C++17) deduces return type
- **Graceful shutdown**: Set `stop_`, notify all, join all threads
- **No task stealing** in this basic version | production pools use work-stealing (like Intel TBB)
- **Follow-up**: Discuss priority queues, task dependencies (DAG scheduling), and per-thread queues

---

## Q3: What is a deadlock| How do you prevent, detect, and recover from it|

### Answer:

**Four conditions for deadlock (ALL must hold):**
1. **Mutual Exclusion**: Resource held exclusively
2. **Hold and Wait**: Thread holds one resource while waiting for another
3. **No Preemption**: Resources can't be forcibly taken
4. **Circular Wait**: A|B|C|A dependency chain

**Prevention Strategies:**
```cpp
// 1. Lock Ordering -> Break circular wait
std::mutex mutexA, mutexB;

// BAD: Thread 1 locks A then B, Thread 2 locks B then A -> DEADLOCK
// GOOD: Always lock in consistent order (by address or ID)
void safeOperation() {
    // std::lock locks multiple mutexes without deadlock (uses try-lock internally)
    std::scoped_lock lock(mutexA, mutexB);  // C++17 ? preferred
    // ... work with both resources
}

// 2. Try-lock with backoff
bool tryOperation() {
    std::unique_lock lockA(mutexA, std::try_to_lock);
    if (!lockA.owns_lock()) return false;

    std::unique_lock lockB(mutexB, std::try_to_lock);
    if (!lockB.owns_lock()) return false;  // lockA auto-released

    // ... work with both resources
    return true;
}

// 3. Lock hierarchy (compile-time enforcement)
class HierarchicalMutex {
    std::mutex mtx_;
    int level_;
    static thread_local int currentLevel_;

public:
    HierarchicalMutex(int level) : level_(level) {}

    void lock() {
        if (level_ >= currentLevel_)
            throw std::runtime_error("Lock hierarchy violation");
        mtx_.lock();
        currentLevel_ = level_;
    }

    void unlock() {
        currentLevel_ = INT_MAX;
        mtx_.unlock();
    }
};

// Usage: High-level locks (level 1000) must be acquired before low-level (level 100)
HierarchicalMutex dbLock(1000);     // High level
HierarchicalMutex cacheLock(500);   // Mid level
HierarchicalMutex itemLock(100);    // Low level
```

**Detection (runtime):**
- Build a wait-for graph at runtime
- Periodically check for cycles (DFS)
- Log lock acquisition order for post-mortem analysis

### Explanation:
- **`std::scoped_lock`** (C++17) is the modern answer | locks multiple mutexes atomically
- **Lock-free alternatives**: Often better for high-contention scenarios (gaming frame updates, HFT)
- **Real-world CAD example**: Geometry kernel lock + UI lock + undo stack lock | use lock hierarchy

---

## Q4: Implement a Reader-Writer Lock and explain its use cases.

### Answer:
```cpp
#include <shared_mutex>
#include <mutex>
#include <string>
#include <unordered_map>

// Using std::shared_mutex (C++17)
class ThreadSafeConfig {
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> config_;

public:
    // Multiple readers can read simultaneously
    std::string get(const std::string& key) const {
        std::shared_lock lock(mutex_);  // Shared/read lock
        auto it = config_.find(key);
        return it != config_.end() ? it->second : "";
    }

    // Writers get exclusive access
    void set(const std::string& key, const std::string& value) {
        std::unique_lock lock(mutex_);  // Exclusive/write lock
        config_[key] = value;
    }

    // Upgrade pattern (read then conditionally write)
    void setIfAbsent(const std::string& key, const std::string& value) {
        {
            std::shared_lock readLock(mutex_);
            if (config_.count(key)) return;  // Already exists
        }
        // Must release read lock before acquiring write lock
        // (std::shared_mutex doesn't support upgrade)
        std::unique_lock writeLock(mutex_);
        config_.emplace(key, value);  // emplace won't overwrite if inserted between locks
    }
};

// Custom implementation (for interview -> show understanding)
class ReadWriteLock {
    std::mutex mutex_;
    std::condition_variable readCV_, writeCV_;
    int readers_ = 0;
    int writers_ = 0;
    int waitingWriters_ = 0;

public:
    void lockRead() {
        std::unique_lock lock(mutex_);
        // Wait if a writer is active or waiting (writer preference)
        readCV_.wait(lock, [this] { return writers_ == 0 && waitingWriters_ == 0; });
        ++readers_;
    }

    void unlockRead() {
        std::unique_lock lock(mutex_);
        --readers_;
        if (readers_ == 0) writeCV_.notify_one();
    }

    void lockWrite() {
        std::unique_lock lock(mutex_);
        ++waitingWriters_;
        writeCV_.wait(lock, [this] { return readers_ == 0 && writers_ == 0; });
        --waitingWriters_;
        ++writers_;
    }

    void unlockWrite() {
        std::unique_lock lock(mutex_);
        --writers_;
        writeCV_.notify_all();   // Wake all waiting readers
        readCV_.notify_all();
    }
};
```

### Explanation:
- **Writer preference** in custom implementation: Prevents writer starvation
- **`std::shared_mutex`** doesn't support lock upgrade (read|write) | must release and reacquire
- **Use cases**: Configuration stores, game world state (many readers, rare updates), CAD document metadata
- **When NOT to use**: If writes are frequent (overhead of shared_mutex > plain mutex)
- **Alternative**: Read-Copy-Update (RCU) for read-heavy workloads with infrequent writes

**Reader vs Writer Preference Analysis:**
```
Reader Preference:
  - Writers wait until ALL readers finish -> potential writer starvation
  - Best for: Read-heavy, writes can tolerate delay (config cache)
  
Writer Preference (our implementation):
  - New readers blocked when writer is waiting -> writers get priority
  - Best for: Write consistency matters (financial data, game state)
  
Fair (FIFO):
  - Requests served in arrival order -> no starvation
  - Highest overhead, used when fairness is legally required
```

**Read-Copy-Update (RCU) ? lock-free alternative (advanced):**
```cpp
// RCU pattern for extreme read performance
template<typename T>
class RCU {
    std::atomic<std::shared_ptr<const T>> data_;
public:
    // Readers: Zero overhead -> just atomic load
    std::shared_ptr<const T> read() const {
        return std::atomic_load(&data_);
    }
    
    // Writers: Copy-on-write
    template<typename Fn>
    void update(Fn modifier) {
        auto oldPtr = std::atomic_load(&data_);
        auto newPtr = std::make_shared<T>(*oldPtr);  // Copy
        modifier(*newPtr);                            // Modify copy
        std::atomic_store(&data_, std::move(newPtr)); // Swap atomically
        // Old data stays alive until last reader releases shared_ptr
    }
};
// Used by Linux kernel (RCU), Java CopyOnWriteArrayList
// Perfect when reads are 99%+ of operations
```

---

## Q5: Explain `std::async`, `std::future`, `std::promise`. When to use each|

### Answer:
```cpp
#include <future>
#include <thread>
#include <iostream>

// === std::async -> simplest way to run async work ===
void asyncExample() {
    // launch::async = guaranteed new thread
    auto future1 = std::async(std::launch::async, [] {
        // Heavy computation
        return computeSomething();
    });

    // launch::deferred = lazy evaluation (runs when .get() called)
    auto future2 = std::async(std::launch::deferred, [] {
        return computeOther();
    });

    // Do other work while async runs...
    doOtherWork();

    int result1 = future1.get();  // Blocks until ready
    int result2 = future2.get();  // Actually runs HERE (deferred)
}

// === std::promise + std::future -> producer/consumer channel ===
void promiseExample() {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();

    std::thread producer([&promise] {
        try {
            int result = performCalculation();
            promise.set_value(result);       // Fulfill promise
        } catch (...) {
            promise.set_exception(std::current_exception());  // Propagate error
        }
    });

    // Consumer
    try {
        int value = future.get();  // Blocks until promise fulfilled
        std::cout << "Got: " << value << "\n";
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }

    producer.join();
}

// === std::packaged_task -> callable wrapper with future ===
void packagedTaskExample() {
    std::packaged_task<int(int, int)> task([](int a, int b) {
        return a * b;
    });

    std::future<int> future = task.get_future();

    // Can be moved to another thread or queued
    std::thread t(std::move(task), 6, 7);
    std::cout << future.get() << "\n";  // 42
    t.join();
}

// === Parallel computation pattern ===
std::vector<double> parallelMap(const std::vector<double>& input,
                                 std::function<double(double)> fn) {
    std::vector<std::future<double>> futures;
    for (double val : input) {
        futures.push_back(std::async(std::launch::async, fn, val));
    }

    std::vector<double> results;
    results.reserve(futures.size());
    for (auto& f : futures) {
        results.push_back(f.get());
    }
    return results;
}
```

### Explanation:
| Feature | Use Case | Notes |
|---------|----------|-------|
| `std::async` | Fire-and-forget parallel work | Simplest API; beware `launch::deferred` default |
| `std::promise/future` | One-shot producer-consumer | Manual thread management |
| `std::packaged_task` | Deferred execution with future | Good for thread pools |

**Gotchas:**
- `std::async` without launch policy may be deferred (never actually parallel!)
- Future from `std::async` blocks in destructor (C++ Standards defect)
- `std::future::get()` can only be called once | use `std::shared_future` for multiple consumers

---

## Q6: What are condition variables| Implement a bounded blocking queue.

### Answer:
```cpp
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

template<typename T>
class BoundedBlockingQueue {
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    size_t capacity_;
    bool closed_ = false;

public:
    BoundedBlockingQueue(size_t capacity) : capacity_(capacity) {}

    // Blocking push -> waits if queue is full
    bool push(T item) {
        std::unique_lock lock(mutex_);
        notFull_.wait(lock, [this] { return queue_.size() < capacity_ || closed_; });
        if (closed_) return false;
        queue_.push(std::move(item));
        notEmpty_.notify_one();
        return true;
    }

    // Blocking pop -> waits if queue is empty
    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        notEmpty_.wait(lock, [this] { return !queue_.empty() || closed_; });
        if (queue_.empty()) return std::nullopt;  // Closed and empty
        T item = std::move(queue_.front());
        queue_.pop();
        notFull_.notify_one();
        return item;
    }

    // Timed pop
    std::optional<T> tryPopFor(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        if (!notEmpty_.wait_for(lock, timeout,
                [this] { return !queue_.empty() || closed_; })) {
            return std::nullopt;  // Timeout
        }
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        notFull_.notify_one();
        return item;
    }

    // Close the queue -> unblocks all waiters
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }
};

// Producer-Consumer pattern
void producerConsumer() {
    BoundedBlockingQueue<int> queue(100);

    // Producers
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&queue, i] {
            for (int j = 0; j < 1000; ++j)
                queue.push(i * 1000 + j);
        });
    }

    // Consumers
    std::vector<std::thread> consumers;
    for (int i = 0; i < 2; ++i) {
        consumers.emplace_back([&queue] {
            while (auto item = queue.pop()) {
                process(*item);
            }
        });
    }

    for (auto& p : producers) p.join();
    queue.close();  // Signal consumers to finish
    for (auto& c : consumers) c.join();
}
```

### Explanation:
- **Spurious wakeups**: `wait` with predicate handles them automatically (loops internally)
- **Bounded**: Prevents unbounded memory growth (back-pressure)
- **`close()`**: Graceful shutdown | producers stop pushing, consumers drain and exit
- **CAD use case**: Render job queue (producer: main thread, consumer: render threads)
- **Finance**: Market data processing pipeline
- **Alternative**: For high-throughput, use lock-free MPMC queues (e.g., `moodycamel::ConcurrentQueue`)

---

## Q7: Explain `std::jthread` and cooperative cancellation in C++20.

### Answer:
```cpp
#include <thread>
#include <stop_token>
#include <iostream>
#include <chrono>

// C++20 jthread -> auto-joins and supports cancellation
void jthreadExample() {
    // jthread automatically joins on destruction (no more forgetting to join!)
    std::jthread worker([](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
            doWork();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Worker stopped gracefully\n";
    });

    // ... do some work ...
    std::this_thread::sleep_for(std::chrono::seconds(2));

    worker.request_stop();  // Cooperative cancellation
    // worker automatically joins here (destructor)
}

// Stop callback -> register cleanup when stop is requested
void stopCallbackExample() {
    std::jthread worker([](std::stop_token stoken) {
        // Register a callback that runs when stop is requested
        std::stop_callback callback(stoken, [] {
            std::cout << "Cancellation requested ? cleaning up resources\n";
        });

        // Simulate condition variable wait with cancellation
        std::mutex mtx;
        std::condition_variable_any cv;

        std::unique_lock lock(mtx);
        // This wait is cancellable!
        cv.wait(lock, stoken, [] { return false; /* or some condition */ });

        if (stoken.stop_requested()) {
            std::cout << "Woke up due to cancellation\n";
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    worker.request_stop();  // Triggers callback AND wakes cv.wait
}
```

### Explanation:
**`std::jthread` vs `std::thread`:**
| Feature | `std::thread` | `std::jthread` (C++20) |
|---------|--------------|----------------------|
| Destructor | `std::terminate` if not joined | Auto-joins |
| Cancellation | Manual (atomic flag) | Built-in `stop_token` |
| Exception safety | Must manually join in catch | Automatic |
| CV integration | No | `condition_variable_any` supports `stop_token` |

**Why it matters**: Eliminates entire classes of bugs:
- Forgetting to join | crash
- Exception causes thread leak | resource exhaustion
- Non-cooperative cancellation | data corruption

---

## Q8: What is false sharing| How do you avoid it|

### Answer:
```cpp
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <new>  // std::hardware_destructive_interference_size

// BAD: False sharing -> counters on the same cache line
struct BadCounters {
    std::atomic<int64_t> counter1;  // 8 bytes
    std::atomic<int64_t> counter2;  // 8 bytes -> same 64-byte cache line!
};

// GOOD: Pad to separate cache lines
struct GoodCounters {
    alignas(64) std::atomic<int64_t> counter1;  // Own cache line
    alignas(64) std::atomic<int64_t> counter2;  // Own cache line
};

// C++17 standard way:
struct StandardCounters {
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int64_t> counter1;
    alignas(std::hardware_destructive_interference_size)
        std::atomic<int64_t> counter2;
};

// Benchmark to demonstrate
void benchmark() {
    BadCounters bad;
    GoodCounters good;

    auto test = [](auto& counters, auto& c1, auto& c2) {
        auto start = std::chrono::high_resolution_clock::now();

        std::thread t1([&c1] {
            for (int i = 0; i < 100'000'000; ++i)
                c1.fetch_add(1, std::memory_order_relaxed);
        });

        std::thread t2([&c2] {
            for (int i = 0; i < 100'000'000; ++i)
                c2.fetch_add(1, std::memory_order_relaxed);
        });

        t1.join(); t2.join();

        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    };

    auto badTime = test(bad, bad.counter1, bad.counter2);
    auto goodTime = test(good, good.counter1, good.counter2);
    // badTime is typically 3-10x slower than goodTime!
}
```

### Explanation:
**What is false sharing|**
- CPU caches work in 64-byte cache lines
- If two threads write to different variables on the SAME cache line, the cache line bounces between cores ("ping-pong")
- Each write invalidates the other core's cache line, forcing a reload
- This is **invisible** in code but causes **3-10x slowdown**

**Where it matters:**
- **HFT**: Per-thread counters, position accumulators
- **Gaming**: Per-entity data updated by different threads
- **Thread pool**: Per-thread work-stealing deques

**Prevention:**
1. `alignas(64)` or `alignas(std::hardware_destructive_interference_size)`
2. Padding between hot variables
3. Thread-local accumulators, merge periodically
4. SoA layout with hot/cold separation

---

## Q9: Implement a Spinlock and explain when to use it over a mutex.

### Answer:
```cpp
#include <atomic>

class Spinlock {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Spin -> but hint the CPU to save power
            #if defined(__x86_64__) || defined(_M_X64)
            __builtin_ia32_pause();  // or _mm_pause() with intrinsics
            #elif defined(__aarch64__)
            __asm__ volatile("yield");
            #endif
        }
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }
};

// TTAS (Test-and-Test-and-Set) -> better performance under contention
class TTASSpinlock {
    std::atomic<bool> locked_{false};

public:
    void lock() {
        while (true) {
            // First, spin on read (doesn't invalidate other caches)
            while (locked_.load(std::memory_order_relaxed)) {
                #ifdef _MSC_VER
                _mm_pause();
                #endif
            }
            // Then try to acquire
            if (!locked_.exchange(true, std::memory_order_acquire))
                return;  // Got it!
        }
    }

    void unlock() {
        locked_.store(false, std::memory_order_release);
    }
};

// Usage with RAII
template<typename Lock>
class LockGuard {
    Lock& lock_;
public:
    LockGuard(Lock& lock) : lock_(lock) { lock_.lock(); }
    ~LockGuard() { lock_.unlock(); }
};
```

### Explanation:
| Aspect | Spinlock | Mutex (`std::mutex`) |
|--------|----------|---------------------|
| Wait method | Busy-spin (burns CPU) | OS sleep (context switch) |
| Latency | Very low (no syscall) | Higher (kernel transition) |
| CPU usage | 100% while waiting | 0% while sleeping |
| Best for | Very short critical sections | Longer critical sections |
| Fairness | No guarantee | OS-level fairness |

**Use spinlock when:**
- Critical section < ~1 microsecond
- Thread count | CPU cores (no preemption)
- HFT hot path, game engine physics step, real-time audio

**Use mutex when:**
- Critical section is longer or variable
- More threads than cores
- You hold I/O or other blocking operations inside the lock

**CAS-based spinlock with backoff (production-grade):**
```cpp
class AdaptiveSpinlock {
    std::atomic<bool> locked_{false};
public:
    void lock() {
        int spinCount = 0;
        while (true) {
            if (!locked_.exchange(true, std::memory_order_acquire))
                return;
            
            if (spinCount < 16) {
                _mm_pause();  // CPU hint: "I'm spinning"
            } else if (spinCount < 64) {
                std::this_thread::yield();  // Give up time slice
            } else {
                // Exponential backoff: sleep briefly
                std::this_thread::sleep_for(
                    std::chrono::microseconds(1 << std::min(spinCount - 64, 10)));
            }
            ++spinCount;
        }
    }
    void unlock() { locked_.store(false, std::memory_order_release); }
};
// Adaptive: spins first (low latency), then yields (fairness), then sleeps (CPU savings)
```

**Fairness comparison:**
```
TAS Spinlock:    No fairness guarantee -> threads near the lock win (cache line proximity)
TTAS Spinlock:   Slightly fairer -> reduced cache line contention
Ticket Spinlock: FIFO fair -> each thread gets a ticket number, served in order
CLH Lock:        FIFO fair -> queue-based, excellent scalability on NUMA
```

---

## Q10: Explain C++17/20 parallel algorithms. How do execution policies work|

### Answer:
```cpp
#include <algorithm>
#include <execution>
#include <vector>
#include <numeric>

void parallelAlgorithms() {
    std::vector<int> data(10'000'000);
    std::iota(data.begin(), data.end(), 0);

    // Sequential (default)
    std::sort(std::execution::seq, data.begin(), data.end());

    // Parallel -> uses thread pool internally
    std::sort(std::execution::par, data.begin(), data.end());

    // Parallel + Vectorized (SIMD) -> strongest
    std::sort(std::execution::par_unseq, data.begin(), data.end());

    // C++20: Unsequenced (SIMD only, single thread)
    std::for_each(std::execution::unseq, data.begin(), data.end(),
                  [](int& x) { x *= 2; });

    // Parallel reduce -> associative + commutative operation
    double sum = std::reduce(std::execution::par, data.begin(), data.end(), 0.0);

    // Parallel transform_reduce (map-reduce)
    double dotProduct = std::transform_reduce(
        std::execution::par,
        vec1.begin(), vec1.end(),
        vec2.begin(),
        0.0  // Initial value
    );
}
```

### Explanation:
| Policy | Threads | SIMD | Notes |
|--------|---------|------|-------|
| `seq` | 1 | No | Same as no policy |
| `par` | Multiple | No | Callbacks must be thread-safe |
| `par_unseq` | Multiple | Yes | No mutexes, no memory allocation in callback |
| `unseq` (C++20) | 1 | Yes | Vectorized but single-threaded |

**Constraints on `par_unseq`:**
- No locks (could deadlock with SIMD interleaving)
- No `new`/`delete` inside lambda
- No file I/O
- No `thread_local` access

**Real-world performance**: `par` sort on 10M elements | 3-4x speedup on 8 cores (not linear due to merge overhead).

**Performance gotchas & when parallel algorithms hurt:**
```cpp
// BAD: Parallel overhead > work per element
std::for_each(std::execution::par, small_vec.begin(), small_vec.end(),
              [](int& x) { x *= 2; });
// For vectors < 10K elements, sequential is often faster (thread setup overhead ~10-50?s)

// BAD: Non-associative operation with reduce
double avg = std::reduce(std::execution::par, v.begin(), v.end(), 0.0) / v.size();
// reduce requires associative+commutative op; floating-point addition is NOT strictly associative
// May give slightly different result than sequential (acceptable for most uses)

// GOOD: Large data, expensive per-element work
auto results = std::transform(std::execution::par,
    models.begin(), models.end(), outputs.begin(),
    [](const Model& m) { return m.computeExpensiveResult(); });
// 8x speedup when each element takes milliseconds
```

**Custom thread pool (MSVC implementation detail):**
```
MSVC: Uses Windows Thread Pool API (IoCompletionPort-based)
GCC/libstdc++: Uses Intel TBB (Threading Building Blocks) | must link -ltbb
Clang/libc++: Limited support (falls back to sequential for some algorithms)

// To use with GCC: g++ -std=c++17 -ltbb program.cpp
```

**Benchmark template for comparing policies:**
```cpp
#include <chrono>
template<typename Policy, typename Fn>
auto benchmark(Policy policy, Fn fn) {
    auto start = std::chrono::high_resolution_clock::now();
    fn();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}
// Compare: benchmark(std::execution::seq, ...) vs benchmark(std::execution::par, ...)
```

---

# ENHANCED SECTION: Staff Engineer Concurrency Deep Dive

> *These questions test production-level concurrency expertise | debugging real race conditions, choosing the right synchronization primitive, and understanding hardware-level behavior.*

---

## Q8: Explain the ABA Problem in lock-free programming. How do you solve it|

### Answer:
```cpp
// ABA Problem:
// Thread 1: Read head = A
// Thread 2: Pop A, Pop B, Push A back
// Thread 1: CAS(head, A, newNode) SUCCEEDS -> but the list changed!

// Solution 1: Tagged pointers (generation counter)
template<typename T>
struct TaggedPtr {
    T* ptr;
    uintptr_t tag; // Incremented on every modification
};

// Solution 2: Hazard Pointers (standard in C++26)
// Readers "announce" which nodes they're accessing
// Nodes can only be reclaimed when no thread has a hazard pointer to them

// Solution 3: Epoch-Based Reclamation (used by crossbeam in Rust)
// Threads enter/exit "epochs"
// Memory freed in epoch N can be reclaimed when all threads have
// advanced past epoch N+2
```

### Explanation:
- **ABA is the #1 lock-free bug** | CAS succeeds but the state has changed underneath
- **iCluster relevance**: The communication module uses packet counters in DMKINT.H to detect missing/reordered messages | conceptually similar to generation counters preventing ABA
- **Production systems**: Java's `AtomicStampedReference`, RCU in Linux kernel, hazard pointers in folly (Facebook's C++ library)

---

## Q9: Design a Multi-Producer Multi-Consumer (MPMC) Lock-Free Queue.

### Answer:
```cpp
// Key insight: SPSC is straightforward, MPMC is hard
// Production approach: Use ring buffer with atomic head/tail + sequence numbers

template<typename T, size_t N>
class MPMCQueue {
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
    };
    
    alignas(64) std::array<Cell, N> buffer_;
    alignas(64) std::atomic<size_t> enqueue_pos_{0};
    alignas(64) std::atomic<size_t> dequeue_pos_{0};

public:
    MPMCQueue() {
        for (size_t i = 0; i < N; ++i)
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool try_enqueue(const T& data) {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        while (true) {
            auto& cell = buffer_[pos % N];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed)) {
                    cell.data = data;
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // Full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
    }
};
```

### Explanation:
- Each cell has a **sequence number** that acts as both availability flag and ABA prevention
- **alignas(64)**: Prevents false sharing between producer and consumer cache lines
- **Production alternatives**: Dmitry Vyukov's bounded MPMC queue, moodycamel::ConcurrentQueue, boost::lockfree::queue
- **iCluster analogy**: The queue-based IPC (MO, CI, CO, DI, DO queues in DMKINT) is a persistent producer-consumer system with multiple producers and consumers

---

## Q10: When should you use a mutex vs atomic vs lock-free vs wait-free|

### Answer:
```
Decision Matrix:
+---------------------------------------------------------------------+
| Primitive     -> Latency     -> Throughput -> Complexity -> Use When           |
+---------------------------------------------------------------------+
| std::mutex    -> Moderate    -> Good       -> Low        -> Default choice     |
| shared_mutex  -> Low (reads) | High reads -> Low        -> Read-heavy (95%+)  |
| std::atomic   -> Very low    -> Very high  -> Medium     -> Simple counters    |
| Lock-free     -> Bounded     -> High       -> Very high  -> Hard RT, no block  |
| Wait-free     -> Guaranteed  -> Variable   -> Extreme    -> Safety-critical    |
+---------------------------------------------------------------------+

Rules of thumb:
1. Start with mutex. Measure. Only go lock-free if contention is proven.
2. Lock-free != faster. Lock-free means "progress guarantee" not speed.
3. Wait-free means EVERY thread completes in bounded steps -> rarely needed.
4. Cache line alignment matters more than algorithm choice for throughput.
5. The best concurrency is NO concurrency -> partition data by thread.
```

### Explanation:
- **iCluster's approach**: Uses OS-level queue primitives (data queues on IBM i) which are essentially mutex-protected FIFO buffers | simple, reliable, well-tested
- **HFT's approach**: Lock-free SPSC ring buffers on hot path, mutex on cold path (configuration changes)
- **Senior insight**: "I chose mutex because our profiling showed contention was <2% and correctness was more important than the 50ns savings" | THIS is what interviewers want to hear

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    

Rules of thumb:
1. Start with mutex. Measure. Only go lock-free if contention is proven.
2. Lock-free != faster. Lock-free means "progress guarantee" not speed.
3. Wait-free means EVERY thread completes in bounded steps | rarely needed.
4. Cache line alignment matters more than algorithm choice for throughput.
5. The best concurrency is NO concurrency | partition data by thread.
```

### Explanation:
- **iCluster's approach**: Uses OS-level queue primitives (data queues on IBM i) which are essentially mutex-protected FIFO buffers -> simple, reliable, well-tested
- **HFT's approach**: Lock-free SPSC ring buffers on hot path, mutex on cold path (configuration changes)
- **Senior insight**: "I chose mutex because our profiling showed contention was <2% and correctness was more important than the 50ns savings" ? THIS is what interviewers want to hear

---
