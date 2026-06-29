# Set 12: Ultimate Senior C++ Interview Questions (10+ Years Experience)

> **Target**: Staff/Principal Engineer level — questions that separate 10+ year veterans from mid-level developers.
> These cover topics NOT in sets 1-10: ABI design, production debugging, cross-language interop, networking, serialization, static analysis, security hardening, GPU computing, and architectural decision-making.

---

# Part A: Library Design & ABI Stability

---

## Q1: You maintain a shared library (.so/.dll) used by 200+ internal teams. How do you evolve the API without breaking existing consumers?

### Answer:

**The ABI Problem:**
```
Source compatibility → Binary compatibility

Source compatible: Recompile client code → works
Binary compatible: Old client binary + new library binary → works WITHOUT recompile

Breaking ABI changes:
  ✗ Adding virtual functions (vtable layout changes)
  ✗ Changing class size (adding/removing members)
  ✗ Changing function signatures
  ✗ Changing enum values
  ✗ Reordering class members
  ✗ Changing template instantiations
  
Non-breaking changes:
  ✓ Adding new free functions
  ✓ Adding new classes
  ✓ Adding static methods
  ✓ Changing function implementation (not signature)
  ✓ Adding enum values AT THE END
```

**Technique 1: Pimpl (Pointer to Implementation)**
```cpp
// === public header (stable ABI) ===
// widget.h → This header NEVER changes
class Widget {
public:
    Widget();
    ~Widget();
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;
    
    void draw();
    void resize(int w, int h);
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    
    // V2.0 → New method added (ABI safe: no vtable, no size change)
    void setOpacity(float alpha);

private:
    struct Impl;                    // Forward declaration only
    std::unique_ptr<Impl> pImpl_;   // Fixed size: sizeof(pointer)
};

// === private implementation (can change freely) ===
// widget.cpp
struct Widget::Impl {
    int width_ = 0;
    int height_ = 0;
    float opacity_ = 1.0f;         // V2.0: Added without breaking ABI
    std::string name_;             // V2.1: Added without breaking ABI
    RenderContext* ctx_ = nullptr; // V3.0: Added without breaking ABI
    // Can add ANY members → sizeof(Widget) doesn't change
};

Widget::Widget() : pImpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

void Widget::draw() { /* uses pImpl_->ctx_ */ }
void Widget::setOpacity(float alpha) { pImpl_->opacity_ = alpha; }
```

**Technique 2: Inline Namespaces for Versioning**
```cpp
// V1
namespace mylib {
inline namespace v1 {
    struct Config { int timeout; };
    void connect(const Config& cfg);
}}

// V2 → old clients still link to v1 symbols
namespace mylib {
inline namespace v2 {
    struct Config { int timeout; int retries; bool useTLS; };
    void connect(const Config& cfg);    // Different mangled name!
}
namespace v1 {
    // Keep old version for binary compat
    void connect(const Config& cfg);
}}
```

**Technique 3: C API Wrapper (ultimate ABI stability)**
```cpp
// Pure C interface → ABI stable across compilers, platforms, versions
extern "C" {
    typedef void* widget_handle;
    
    widget_handle widget_create();
    void widget_destroy(widget_handle w);
    void widget_draw(widget_handle w);
    void widget_resize(widget_handle w, int width, int height);
    int widget_get_width(widget_handle w);
    
    // V2.0 → just add new functions (always safe)
    void widget_set_opacity(widget_handle w, float alpha);
    
    // Error handling via error codes + thread-local message
    int widget_last_error();
    const char* widget_last_error_msg();
}
```

**Technique 4: Symbol Versioning (Linux .so)**
```
# version.map → GNU linker version script
MYLIB_1.0 {
    global:
        widget_create;
        widget_destroy;
        widget_draw;
    local:
        *;  # Hide everything else
};

MYLIB_2.0 {
    global:
        widget_set_opacity;   # New in V2
} MYLIB_1.0;                  # Inherits V1 symbols

# Build: g++ -shared -Wl,--version-script=version.map -o libwidget.so.2 widget.cpp
```

**ABI Compliance Checklist:**
```
Before every release:
  ✓ Run abi-compliance-checker (compares old vs new .so)
  ✓ Run abi-dumper to generate ABI dump
  ✓ Check no virtual functions added/removed/reordered
  ✓ Check no public struct sizes changed
  ✓ Verify symbol visibility (only export what's needed)
  ✓ Test: link old test binary against new .so → must work
  
Tools:
  - abi-compliance-checker   → comprehensive ABI diff
  - abidiff (libabigail)     → Red Hat's ABI checker
  - nm -D libfoo.so          → list exported symbols
  - c++filt                  → demangle C++ symbols
  - readelf -Ws libfoo.so    → symbol table with versions
```

### Why this matters at 10+ years:
Junior devs write code. Senior devs maintain libraries consumed by hundreds of teams. One ABI break = hundreds of broken builds. You must understand the difference between source and binary compatibility and use Pimpl, C wrappers, or symbol versioning proactively.

---

## Q2: Design a plugin architecture using dynamic loading. How do you handle versioning, error isolation, and ABI safety?

### Answer:

```cpp
// === Plugin Interface (pure C for ABI stability) ===
// plugin_api.h → shipped to plugin developers

#pragma once

#define PLUGIN_API_VERSION 2

#ifdef _WIN32
  #define PLUGIN_EXPORT __declspec(dllexport)
#else
  #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

struct PluginInfo {
    int         apiVersion;     // Must match host's PLUGIN_API_VERSION
    const char* name;
    const char* version;
    const char* author;
};

// Every plugin MUST implement these 4 functions:
PLUGIN_EXPORT PluginInfo  plugin_get_info();
PLUGIN_EXPORT int         plugin_init(void* hostContext);
PLUGIN_EXPORT void        plugin_shutdown();
PLUGIN_EXPORT int         plugin_execute(const char* command, 
                                          char* resultBuf, int bufSize);
}

// === Host Application → Plugin Loader ===
// plugin_loader.h

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>

#ifdef _WIN32
  #include <windows.h>
  using LibHandle = HMODULE;
  #define LoadLib(path)    LoadLibraryA(path)
  #define GetSym(h, name)  GetProcAddress(h, name)
  #define FreeLib(h)       FreeLibrary(h)
  #define LibError()       std::to_string(GetLastError())
#else
  #include <dlfcn.h>
  using LibHandle = void*;
  #define LoadLib(path)    dlopen(path, RTLD_NOW | RTLD_LOCAL)
  #define GetSym(h, name)  dlsym(h, name)
  #define FreeLib(h)       dlclose(h)
  #define LibError()       std::string(dlerror() ? dlerror() : "unknown")
#endif

class PluginLoader {
public:
    struct LoadedPlugin {
        LibHandle handle = nullptr;
        PluginInfo info{};
        // Function pointers
        std::function<int(void*)>                         init;
        std::function<void()>                             shutdown;
        std::function<int(const char*, char*, int)>       execute;
    };
    
    // Load plugin with version check and error isolation
    std::expected<std::string, std::string> loadPlugin(const std::filesystem::path& path) {
        // 1. Load shared library
        LibHandle handle = LoadLib(path.string().c_str());
        if (!handle) {
            return std::unexpected("Failed to load " + path.string() + ": " + LibError());
        }
        
        // 2. Resolve symbols
        auto getInfo = reinterpret_cast<PluginInfo(*)()>(GetSym(handle, "plugin_get_info"));
        if (!getInfo) {
            FreeLib(handle);
            return std::unexpected("Missing plugin_get_info in " + path.string());
        }
        
        // 3. Version check
        PluginInfo info = getInfo();
        if (info.apiVersion != PLUGIN_API_VERSION) {
            FreeLib(handle);
            return std::unexpected(
                std::format("Plugin '{}' API version {} != host version {}",
                            info.name, info.apiVersion, PLUGIN_API_VERSION));
        }
        
        // 4. Resolve remaining symbols
        LoadedPlugin plugin;
        plugin.handle = handle;
        plugin.info = info;
        plugin.init = reinterpret_cast<int(*)(void*)>(GetSym(handle, "plugin_init"));
        plugin.shutdown = reinterpret_cast<void(*)()>(GetSym(handle, "plugin_shutdown"));
        plugin.execute = reinterpret_cast<int(*)(const char*, char*, int)>(
            GetSym(handle, "plugin_execute"));
        
        if (!plugin.init || !plugin.shutdown || !plugin.execute) {
            FreeLib(handle);
            return std::unexpected("Missing required exports in " + path.string());
        }
        
        // 5. Initialize plugin (catch crashes)
        int rc = plugin.init(hostContext_);
        if (rc != 0) {
            FreeLib(handle);
            return std::unexpected(std::format("Plugin '{}' init failed: {}", info.name, rc));
        }
        
        std::string name = info.name;
        plugins_[name] = std::move(plugin);
        return name;
    }
    
    // Auto-discover plugins from directory
    int loadAllPlugins(const std::filesystem::path& pluginDir) {
        int loaded = 0;
        for (const auto& entry : std::filesystem::directory_iterator(pluginDir)) {
            auto ext = entry.path().extension().string();
            #ifdef _WIN32
            if (ext != ".dll") continue;
            #elif __APPLE__
            if (ext != ".dylib") continue;
            #else
            if (ext != ".so") continue;
            #endif
            
            auto result = loadPlugin(entry.path());
            if (result) {
                ++loaded;
                spdlog::info("Loaded plugin: {}", *result);
            } else {
                spdlog::warn("Failed: {}", result.error());
            }
        }
        return loaded;
    }
    
    void unloadAll() {
        for (auto& [name, plugin] : plugins_) {
            plugin.shutdown();
            FreeLib(plugin.handle);
        }
        plugins_.clear();
    }
    
    ~PluginLoader() { unloadAll(); }
    
private:
    void* hostContext_ = nullptr;
    std::unordered_map<std::string, LoadedPlugin> plugins_;
};
```

**Error Isolation → protect host from faulty plugins:**
```cpp
// On Linux: use signal handlers to catch SIGSEGV in plugins
// On Windows: use SEH (__try/__except)
// Better: run plugin in separate process with IPC

// Process-isolated plugin (production-grade):
class IsolatedPlugin {
    pid_t childPid_;
    int   ipcFd_;      // Unix domain socket or pipe
    
public:
    int execute(const std::string& cmd) {
        // Send command via IPC to child process
        write(ipcFd_, cmd.data(), cmd.size());
        
        // Wait for response with timeout
        struct pollfd pfd = {ipcFd_, POLLIN, 0};
        int ready = poll(&pfd, 1, 5000);  // 5 second timeout
        
        if (ready <= 0) {
            // Plugin hung → kill and restart
            kill(childPid_, SIGKILL);
            respawnPlugin();
            return -1;
        }
        // Read result...
        return 0;
    }
};
```

### Why this matters:
At 10+ years, you've likely designed or maintained plugin systems. Key decisions: C vs C++ interface (C for ABI stability), in-process vs isolated (trade performance for safety), version negotiation (don't break old plugins). Real examples: VSCode extensions, Chrome plugins, game engine mods, CAD add-ins.

---

# Part B: Production Debugging & Cross-Language Interop

---

## Q3: You get a core dump from a production server crash that only happens under high load (1 in 10,000 requests). Walk through your debugging process.

### Answer:

**Step 1: Preserve the evidence**
```bash
# Ensure core dumps are enabled and stored
ulimit -c unlimited
echo "/var/coredumps/core.%e.%p.%t" > /proc/sys/kernel/core_pattern

# On crash: collect core dump + binary + shared libraries
# (symbols MUST match the exact binary that crashed)
tar czf crash_bundle.tar.gz \
    /var/coredumps/core.myapp.12345.* \
    /usr/local/bin/myapp \
    /usr/local/lib/libmylib.so* \
    /proc/12345/maps    # Memory map at crash time
```

**Step 2: Initial core dump analysis**
```bash
# Load in GDB
gdb /usr/local/bin/myapp /var/coredumps/core.myapp.12345

# First commands:
(gdb) bt                    # Backtrace → WHERE did it crash?
(gdb) bt full               # Backtrace with local variables
(gdb) info threads           # All threads at crash time
(gdb) thread apply all bt    # Backtrace for EVERY thread

# Example crash output:
#0  0x00007f3a2c1a3b42 in OrderBook::matchOrder(Order&) at orderbook.cpp:147
#1  0x00007f3a2c1a4890 in TradingEngine::processMessage(FIXMessage&) at engine.cpp:89
#2  0x00007f3a2c1a5120 in NetworkHandler::onData(Buffer&) at network.cpp:234
```

**Step 3: Inspect the crash site**
```bash
(gdb) frame 0                    # Go to crash frame
(gdb) list                       # Show source code around crash
(gdb) info locals                # Local variables
(gdb) info args                  # Function arguments
(gdb) print *this                # Object state at crash

# For segfault → what address was accessed?
(gdb) info signal SIGSEGV
(gdb) print $_siginfo._sifields._sigfault.si_addr
# Output: 0x0000000000000048 → nullptr + offset 0x48
# This means: this->someField where 'this' was null

# Inspect memory around the crash
(gdb) x/32xg $rsp               # Stack contents (hex, 8-byte units)
(gdb) x/10i $rip                # Disassembly at crash point
```

**Step 4: Reproduce with sanitizers**
```bash
# Rebuild with Address Sanitizer
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
cmake --build build

# Run under load
./build/myapp --threads=64 --load-test

# ASan output will be much more informative:
# ==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x60300000efa0
# READ of size 8 at 0x60300000efa0 thread T5
#     #0 OrderBook::matchOrder orderbook.cpp:147
#     freed by thread T12 here:
#     #0 operator delete
#     #1 OrderBook::cancelOrder orderbook.cpp:201
# → Thread T12 deleted the order while T5 was still reading it!
```

**Step 5: If not reproducible → reverse debugging**
```bash
# Use 'rr' → record & replay debugger (Linux only)
rr record ./myapp --load-test
rr replay

# Now you can step BACKWARDS from the crash
(rr) reverse-continue          # Run backwards to previous breakpoint
(rr) reverse-step              # Step backwards one line
(rr) watch -l order->status    # When did this memory last change?
(rr) reverse-continue          # → Takes you to the writer!
```

**Step 6: For intermittent bugs → targeted instrumentation**
```cpp
// Add tracing that activates only at high load
#include <atomic>
static std::atomic<int> activeRequests{0};

void processRequest(Request& req) {
    auto guard = activeRequests.fetch_add(1);
    SCOPE_EXIT { activeRequests.fetch_sub(1); };
    
    // Enable detailed tracing when load is high
    if (activeRequests > 100) {
        TRACE_LOG("High load path: req={} thread={} order={}",
                  req.id, std::this_thread::get_id(), req.orderId);
    }
    // ...
}
```

**Common 10+ year patterns:**
```
Crash type              | First suspect
+---------------------+ +-------------------------------+
SIGSEGV at 0x0+offset  | Null pointer dereference
SIGSEGV at random addr  | Use-after-free or wild pointer
SIGABRT                 | assert() failed or double-free
SIGBUS                  | Misaligned access or mmap issue
Stack smashing detected | Buffer overflow on stack
Only under load         | Race condition (use TSan)
Only after N hours      | Memory leak (use ASan/Valgrind)
Random data corruption  | Data race (use TSan + atomics)
```

### Why this matters:
Production debugging is THE skill that separates senior from staff engineers. You can't attach a debugger to prod — you work with core dumps, logs, and metrics. At 10+ years, you've built the muscle memory of: preserve evidence → analyze core dump → reproduce with sanitizers → fix → add regression test.

---

## Q4: How do you expose a C++ library to Python? Compare pybind11, SWIG, ctypes, and Cython.

### Answer:

**Comparison Matrix:**
| Feature | pybind11 | SWIG | ctypes/cffi | Cython |
|---------|----------|------|-------------|--------|
| **Language** | C++ header-only | Code generator | Python-side | Cython (.pyx) |
| **ABI** | C++ (compiler-specific) | C wrapper generated | C only | C/C++ |
| **Ease of use** | +---+ | --> | +--+ | --> |
| **Performance** | Excellent | Good | Good (FFI overhead) | Excellent |
| **C++ support** | Full (templates, STL) | Good | C only | Partial |
| **Build** | CMake native | Separate step | No build needed | Separate step |
| **Debugging** | GDB works | Complex | Python-side only | GDB works |
| **Maintenance** | Low | Medium (SWIG files) | Low | Medium |
| **Best for** | C++ libs to Python | Multi-language (Java, C#, Python) | Simple C libs | Performance-critical Python |

**pybind11 → the modern standard:**
```cpp
// geometry.h
#include <vector>
#include <string>
#include <cmath>

class Point {
public:
    double x, y, z;
    Point(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    
    double distanceTo(const Point& other) const {
        return std::sqrt(std::pow(x - other.x, 2) + 
                         std::pow(y - other.y, 2) + 
                         std::pow(z - other.z, 2));
    }
    
    Point operator+(const Point& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }
    
    std::string repr() const {
        return std::format("Point({}, {}, {})", x, y, z);
    }
};

class Mesh {
    std::vector<Point> vertices_;
    std::vector<std::array<int, 3>> faces_;
public:
    void addVertex(const Point& p) { vertices_.push_back(p); }
    void addFace(int v0, int v1, int v2) { faces_.push_back({v0, v1, v2}); }
    
    size_t vertexCount() const { return vertices_.size(); }
    size_t faceCount() const { return faces_.size(); }
    
    // Return numpy-compatible buffer
    const Point* vertexData() const { return vertices_.data(); }
};

// bindings.cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>           // Automatic STL conversions
#include <pybind11/numpy.h>         // NumPy array support
#include <pybind11/operators.h>     // Operator overloading
#include "geometry.h"

namespace py = pybind11;

PYBIND11_MODULE(geometry, m) {
    m.doc() = "3D Geometry library";
    
    py::class_<Point>(m, "Point")
        .def(py::init<double, double, double>(),
             py::arg("x") = 0, py::arg("y") = 0, py::arg("z") = 0)
        .def("distance_to", &Point::distanceTo)
        .def(py::self + py::self)       // __add__
        .def("__repr__", &Point::repr)
        .def_readwrite("x", &Point::x)  // Expose as Python properties
        .def_readwrite("y", &Point::y)
        .def_readwrite("z", &Point::z)
        // NumPy buffer protocol → zero-copy access from Python
        .def_buffer([](Point& p) {
            return py::buffer_info(
                &p.x, sizeof(double),
                py::format_descriptor<double>::format(),
                1, {3}, {sizeof(double)});
        });
    
    py::class_<Mesh>(m, "Mesh")
        .def(py::init<>())
        .def("add_vertex", &Mesh::addVertex)
        .def("add_face", &Mesh::addFace)
        .def_property_readonly("vertex_count", &Mesh::vertexCount)
        .def_property_readonly("face_count", &Mesh::faceCount)
        // Return vertices as NumPy array (zero-copy!)
        .def("vertices_as_numpy", [](const Mesh& mesh) {
            return py::array_t<double>(
                {static_cast<py::ssize_t>(mesh.vertexCount()), py::ssize_t(3)},
                {sizeof(Point), sizeof(double)},
                reinterpret_cast<const double*>(mesh.vertexData()),
                py::cast(mesh)  // prevent mesh from being garbage collected
            );
        });
}

// CMakeLists.txt
// find_package(pybind11 REQUIRED)
// pybind11_add_module(geometry bindings.cpp)
```

**Usage in Python:**
```python
import geometry
import numpy as np

p1 = geometry.Point(1, 2, 3)
p2 = geometry.Point(4, 5, 6)
print(p1.distance_to(p2))  # 5.196
print(p1 + p2)              # Point(5, 7, 9)

mesh = geometry.Mesh()
mesh.add_vertex(p1)
mesh.add_vertex(p2)

# Zero-copy access to C++ memory as NumPy array!
verts = mesh.vertices_as_numpy()
print(verts.shape)  # (2, 3)
print(verts[0])     # [1. 2. 3.]
```

**Advanced: Handling GIL, callbacks, and async:**
```cpp
// Release GIL for long-running C++ operations
m.def("heavy_computation", [](const Mesh& mesh) {
    py::gil_scoped_release release;  // Release GIL → other Python threads can run
    auto result = mesh.computeNormals();  // Pure C++ → no Python calls
    py::gil_scoped_acquire acquire;  // Re-acquire before returning Python objects
    return result;
});

// Python callback from C++
m.def("iterate_vertices", [](Mesh& mesh, py::function callback) {
    for (size_t i = 0; i < mesh.vertexCount(); ++i) {
        callback(i, mesh.vertex(i));  // Calls Python function → GIL must be held
    }
});
```

**ctypes → for simple C interop (no build step):**
```python
# If your lib exports C functions:
import ctypes

lib = ctypes.CDLL("./libgeometry.so")
lib.widget_create.restype = ctypes.c_void_p
lib.widget_draw.argtypes = [ctypes.c_void_p]

w = lib.widget_create()
lib.widget_draw(w)
lib.widget_destroy(w)
```

### Decision Framework:
```
Need full C++ support (templates, STL, inheritance)?  | pybind11
Need multi-language (Python + Java + C#)|             | SWIG
Simple C library, no build step needed?              | ctypes/cffi
Writing performance-critical Python with C calls?    | Cython
Need zero-copy NumPy interop?                        | pybind11 (buffer protocol)
```

---

# Part C: Networking & Protocol Design

---

## Q5: Implement an async TCP echo server using Boost.Asio with C++20 coroutines. Then compare with io_uring.

### Answer:

```cpp
// Modern Boost.Asio with C++20 coroutines
// Build: g++ -std=c++20 -lboost_system -lpthread server.cpp

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <iostream>
#include <format>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Per-connection coroutine → reads data, echoes it back
asio::awaitable<void> handleClient(tcp::socket socket) {
    auto endpoint = socket.remote_endpoint();
    std::cout << std::format("Client connected: {}:{}\n", 
                              endpoint.address().to_string(), endpoint.port());
    
    try {
        char buffer[4096];
        for (;;) {
            // co_await suspends here → no thread blocked!
            auto bytesRead = co_await socket.async_read_some(
                asio::buffer(buffer), asio::use_awaitable);
            
            // Echo back
            co_await asio::async_write(
                socket, asio::buffer(buffer, bytesRead), asio::use_awaitable);
        }
    } catch (const std::exception& e) {
        std::cout << std::format("Client disconnected: {} ({})\n",
                                  endpoint.address().to_string(), e.what());
    }
}

// Acceptor coroutine → listens for new connections
asio::awaitable<void> listener(tcp::acceptor acceptor) {
    for (;;) {
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);
        
        // Spawn a new coroutine for each client (no thread per connection!)
        auto executor = co_await asio::this_coro::executor;
        asio::co_spawn(executor, handleClient(std::move(socket)), asio::detached);
    }
}

int main() {
    asio::io_context io(/*concurrency_hint=*/std::thread::hardware_concurrency());
    
    tcp::acceptor acceptor(io, {tcp::v4(), 8080});
    acceptor.set_option(tcp::acceptor::reuse_address(true));
    
    asio::co_spawn(io, listener(std::move(acceptor)), asio::detached);
    
    // Run on multiple threads
    std::vector<std::jthread> threads;
    for (unsigned i = 1; i < std::thread::hardware_concurrency(); ++i) {
        threads.emplace_back([&io] { io.run(); });
    }
    io.run();  // Main thread also processes events
}
```

**Custom binary protocol on top:**
```cpp
// Protocol: [4-byte length][payload]
// TLV (Type-Length-Value) framing

struct MessageHeader {
    uint32_t type;       // MESSAGE, HEARTBEAT, AUTH, etc.
    uint32_t length;     // Payload length in bytes
    uint32_t sequence;   // Monotonic sequence number
    uint32_t checksum;   // CRC32 of payload
};

asio::awaitable<void> handleClientProtocol(tcp::socket socket) {
    try {
        for (;;) {
            // 1. Read fixed-size header
            MessageHeader header;
            co_await asio::async_read(
                socket, asio::buffer(&header, sizeof(header)),
                asio::use_awaitable);
            
            header.length = ntohl(header.length);   // Network byte order
            header.type = ntohl(header.type);
            
            // 2. Validate
            if (header.length > MAX_MESSAGE_SIZE) {
                throw std::runtime_error("Message too large");
            }
            
            // 3. Read payload
            std::vector<char> payload(header.length);
            co_await asio::async_read(
                socket, asio::buffer(payload),
                asio::use_awaitable);
            
            // 4. Verify checksum
            uint32_t computed = crc32(payload.data(), payload.size());
            if (computed != ntohl(header.checksum)) {
                throw std::runtime_error("Checksum mismatch");
            }
            
            // 5. Dispatch based on type
            co_await processMessage(header.type, std::move(payload));
        }
    } catch (...) {
        // Connection error handling
    }
}
```

**Comparison: Boost.Asio vs io_uring vs epoll:**
```
+----------------------------------------------------------------------------+
| Feature        | Boost.Asio        | io_uring           | epoll            |
+----------------------------------------------------------------------------+
| Model          | Proactor          | Proactor           | Reactor          |
| API level      | High (C++ lib)    | Low (syscall)      | Low (syscall)    |
| Zero-copy      | No                | Yes (registered    | No               |
|                |                   |  buffers)          |                  |
| Syscalls/op    | 1 (epoll backend) | 0 (submission      | 1+               |
|                |                   |  queue batching)   |                  |
| Kernel bypass  | No                | Yes (SQPOLL mode)  | No               |
| File I/O       | Separate API      | Unified            | Not supported    |
| Platform       | Cross-platform    | Linux 5.1+         | Linux only       |
| Coroutines     | C++20 co_await    | liburing + manual  | Manual           |
| Throughput     | ~500K msg/s       | ~2M msg/s          | ~800K msg/s      |
| Best for       | Portable apps     | Maximum Linux perf | Legacy Linux     |
+----------------------------------------------------------------------------+
```

**io_uring example (for contrast):**
```cpp
// Raw io_uring → maximum performance, Linux only
#include <liburing.h>

struct io_uring ring;
io_uring_queue_init(256, &ring, 0);

// Submit read operation (no syscall until io_uring_submit!)
struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
io_uring_prep_recv(sqe, client_fd, buffer, sizeof(buffer), 0);
io_uring_sqe_set_data(sqe, conn);  // Associate with connection

// Batch submit (one syscall for N operations!)
io_uring_submit(&ring);

// Reap completions
struct io_uring_cqe* cqe;
io_uring_wait_cqe(&ring, &cqe);
int bytesRead = cqe->res;
auto* conn = (Connection*)io_uring_cqe_get_data(cqe);
io_uring_cqe_seen(&ring, cqe);
```

### When to use what:
```
Cross-platform application          | Boost.Asio
Maximum throughput on Linux          | io_uring
Legacy codebase, already using epoll → Keep epoll
Portable + coroutines               | Boost.Asio + C++20 co_await
Need file I/O + network unified     | io_uring
```

---

## Q6: Compare serialization frameworks: Protocol Buffers, FlatBuffers, Cap'n Proto, and MessagePack. When would you choose each?

### Answer:

**Head-to-head comparison:**
```
+------------------------------------------------------------------+
|              | Protobuf   | FlatBuffers| Cap'n Proto| MessagePack|
+------------------------------------------------------------------+
| Encode speed | Medium     | Fast       | Zero-copy  | Fast       |
| Decode speed | Medium     | Zero-copy  | Zero-copy  | Fast       |
| Wire size    | Small      | Medium     | Medium     | Small      |
| Schema       | .proto     | .fbs       | .capnp     | None       |
| Backward     | Excellent  | Good       | Excellent  | N/A        |
|  compat      |            |            |            |            |
| Zero-copy    | No (decode)| Yes (read) | Yes (r+w)  | No         |
| RPC support  | gRPC       | gRPC       | Built-in   | msgpack-rpc|
| Languages    | 12+        | 15+        | 7          | 50+        |
| Mutable msg  | Yes        | Limited    | Builder    | Yes        |
| Memory usage | High       | Low        | Low        | Medium     |
| Learning     | Easy       | Medium     | Medium     | Easy       |
+------------------------------------------------------------------+
```

**Protocol Buffers (most popular, best ecosystem):**
```protobuf
// order.proto
syntax = "proto3";
package trading;

message Order {
    string order_id = 1;
    enum Side { BUY = 0; SELL = 1; }
    Side side = 2;
    double price = 3;
    int64 quantity = 4;
    google.protobuf.Timestamp created_at = 5;
    
    // Backward compatible: adding new field
    optional string client_tag = 6;  // Old code ignores this
}
```
```cpp
// C++ usage
trading::Order order;
order.set_order_id("ORD-001");
order.set_side(trading::Order::BUY);
order.set_price(150.25);

// Serialize
std::string wire;
order.SerializeToString(&wire);   // ~40 bytes (compact!)

// Deserialize
trading::Order decoded;
decoded.ParseFromString(wire);    // Allocates memory, copies data
```

**FlatBuffers (zero-copy read, game engines):**
```cpp
// Access data directly from wire buffer → NO deserialization step
auto order = GetOrder(wire_buffer);  // Just a pointer cast!
double price = order->price();       // Direct memory read
// Total decode cost: 0 allocations, 0 copies
// Perfect for: game networking, real-time systems

// Trade-off: wire size is larger, mutation is limited
```

**Cap'n Proto (zero-copy read AND write):**
```cpp
// Both serialization and deserialization are zero-copy
// The in-memory representation IS the wire format
capnp::MallocMessageBuilder builder;
auto order = builder.initRoot<Order>();
order.setPrice(150.25);

// "Serialize" → just write the memory directly (no encoding!)
auto segments = builder.getSegmentsForOutput();
write(fd, segments[0].begin(), segments[0].size());
```

**MessagePack (schema-less, JSON alternative):**
```cpp
// No schema needed → like binary JSON
msgpack::sbuffer buffer;
msgpack::packer<msgpack::sbuffer> pk(&buffer);

pk.pack_map(3);
pk.pack("price"); pk.pack(150.25);
pk.pack("qty");   pk.pack(100);
pk.pack("side");  pk.pack("BUY");
// ~25 bytes (vs 60 bytes JSON)
```

**Decision Framework:**
```
Microservices with gRPC                     | Protobuf (gRPC native)
Game engine / real-time (read-heavy)        | FlatBuffers (zero-copy read)
High-performance IPC / RPC                  | Cap'n Proto (zero-copy both)
Config files, logs, dynamic data            | MessagePack (schema-free)
Need maximum language support               | Protobuf or MessagePack
Embedded / constrained memory               | FlatBuffers (no allocations)
Finance (every microsecond counts)          | FlatBuffers or Cap'n Proto
Need backward + forward compatibility       | Protobuf (field numbering)
```

**Benchmark (encode + decode 1 million messages):**
```
Framework       Encode     Decode     Wire Size   Allocations
+-------------+ +--------+ +--------+ +---------+ +---------+
Protobuf        320ms      280ms      42 bytes    2M (decode)
FlatBuffers     150ms       15ms      64 bytes    1M (encode only)
Cap'n Proto      80ms       10ms      56 bytes    1M (builder only)
MessagePack     200ms      180ms      38 bytes    2M
JSON (nlohmann) 800ms      600ms      95 bytes    5M
```

---

# Part D: Static Analysis, Security & Quality at Scale

---

## Q7: How do you enforce code quality across a 5-million-line C++ codebase with 100+ developers|

### Answer:

**Quality Enforcement Pyramid:**
```
                   /\
                  /  \        Code Review
                 /    \       (human, design-level)
                /+----+\
               / Static  \    Clang-Tidy, SonarQube, PVS-Studio
              /  Analysis  \  (automated, pattern-level)
             /+------------+\
            /  Sanitizers &   \  ASan, TSan, UBSan, MSan
           /   Dynamic Analysis \  (runtime, bug-level)
          /+----------------------+\
         /  Compiler Warnings       \  -Wall -Wextra -Werror -Wpedantic
        /   (-Werror = no warnings)  \  (compile-time, syntax-level)
       /+----------------------------+\
      / Formatting & Style              \  clang-format, .clang-format
     /  (automated, style-level)         \
    /+------------------------------------+\
```

**Layer 1: clang-format (zero-argument style)**
```yaml
# .clang-format → committed to repo, enforced in CI
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Empty
BreakBeforeBraces: Attach
SortIncludes: CaseInsensitive
IncludeBlocks: Regroup

# Pre-commit hook:
# git diff --cached --name-only | grep -E '\.(cpp|h)$' | xargs clang-format --dry-run --Werror
```

**Layer 2: Compiler Warnings as Errors**
```cmake
# CMakeLists.txt → enforced project-wide
add_compile_options(
    -Wall -Wextra -Wpedantic -Werror    # Treat all warnings as errors
    -Wshadow                             # Variable shadowing
    -Wnon-virtual-dtor                   # Missing virtual dtor
    -Wold-style-cast                     # C-style casts
    -Wcast-align                         # Misaligned casts
    -Wunused                             # Unused variables/functions
    -Woverloaded-virtual                 # Accidental hiding
    -Wconversion                         # Implicit conversions
    -Wsign-conversion                    # Signed/unsigned conversion
    -Wnull-dereference                   # Null pointer dereference
    -Wdouble-promotion                   # Float → double
    -Wformat=2                           # printf format errors
    -Wimplicit-fallthrough               # Switch fallthrough
)
```

**Layer 3: Custom clang-tidy checks**
```yaml
# .clang-tidy → project-specific rules
Checks: >
    -*,
    bugprone-*,
    cert-*,
    cppcoreguidelines-*,
    misc-*,
    modernize-*,
    performance-*,
    readability-*,
    -modernize-use-trailing-return-type,
    -readability-magic-numbers

WarningsAsErrors: >
    bugprone-use-after-move,
    bugprone-dangling-handle,
    cppcoreguidelines-owning-memory,
    misc-use-after-move,
    performance-unnecessary-copy-initialization

# Writing CUSTOM checks:
# When a project-specific pattern needs enforcement
# Example: "All public API functions must log entry/exit"
```

```cpp
// Custom clang-tidy check (AST matcher)
// Finds: raw 'new' expressions not wrapped in make_unique/make_shared
void RawNewCheck::registerMatchers(ast_matchers::MatchFinder* finder) {
    finder->addMatcher(
        cxxNewExpr(
            unless(hasAncestor(callExpr(callee(
                functionDecl(hasName("make_unique"))))))
        ).bind("raw_new"),
        this);
}

void RawNewCheck::check(const MatchFinder::MatchResult& result) {
    if (const auto* newExpr = result.Nodes.getNodeAs<CXXNewExpr>("raw_new")) {
        diag(newExpr->getBeginLoc(), 
             "use std::make_unique instead of raw new") 
            << FixItHint::CreateReplacement(/* ... */);
    }
}
```

**Layer 4: CI Pipeline Integration**
```yaml
# .github/workflows/quality.yml
quality-gate:
  steps:
    - name: Format Check
      run: |
        find src -name "*.cpp" -o -name "*.h" | 
          xargs clang-format --dry-run --Werror

    - name: Build with Warnings
      run: cmake --build build --target all 2>&1 | tee build.log
      # -Werror means any warning = build failure

    - name: Clang-Tidy
      run: |
        run-clang-tidy -p build -j $(nproc) src/
      # Returns non-zero if any check fails

    - name: Sanitizer Tests
      run: |
        cmake -B build-san -DSANITIZER=address,undefined
        cmake --build build-san
        ctest --test-dir build-san --output-on-failure

    - name: Coverage Gate (minimum 80%)
      run: |
        lcov --capture --directory build --output-file coverage.info
        lcov --summary coverage.info | grep -oP 'lines\.\.\.\.\.\.: \K[\d.]+' |
          awk '{if ($1 < 80) exit 1}'
```

**Layer 5: SonarQube for Enterprise-wide tracking**
```
Dashboard metrics tracked:
  - Code coverage trend (per team, per module)
  - Technical debt (hours to fix all issues)
  - Bug density (bugs per 1000 lines)
  - Code duplication %
  - Complexity hotspots (cyclomatic complexity > 20)
  - Security vulnerabilities (OWASP top 10)
  
Quality Gate (blocks merge if ANY fail):
  - Coverage on new code >= 80%
  - No new bugs (severity: critical or blocker)
  - No new security vulnerabilities
  - Duplication on new code < 3%
  - Maintainability rating >= A
```

### Why this matters at 10+ years:
You don't just write quality code — you build systems that enforce quality across 100+ developers who have varying skill levels. The key insight: automate everything you can. Humans are bad at catching formatting issues, missing includes, or subtle bugs during code review — machines are perfect at it.

---

## Q8: What C++ security vulnerabilities have you encountered, and how do you harden a C++ codebase?

### Answer:

**Top C++ Security Vulnerabilities (OWASP Native):**
```
+-----------------------------------------------------------------------------+
| #  | Vulnerability                | C++ Mitigation                          |
+-----------------------------------------------------------------------------+
|  1 | Buffer Overflow              | std::span, std::array, bounds checking  |
|  2 | Use-After-Free               | Smart pointers, ASan in CI              |
|  3 | Integer Overflow              | SafeInt<T>, -ftrapv, checked arithmetic |
|  4 | Format String Attacks         | std::format (not printf), -Wformat=2   |
|  5 | Double Free                   | unique_ptr (single owner), ASan         |
|  6 | Uninitialized Memory          | -Wuninitialized, MSan, = {} everywhere |
|  7 | Race Conditions               | TSan, std::mutex, atomics              |
|  8 | Null Pointer Deref            | std::optional, contracts (C++26)       |
|  9 | Type Confusion                | dynamic_cast checks, std::variant      |
| 10 | Stack Overflow (recursion)    | -fstack-protector-all, iteration       |
+-----------------------------------------------------------------------------+
```

**Secure coding patterns:**
```cpp
// 1. Buffer safety → std::span instead of raw pointer + size
void processData(std::span<const std::byte> data) {  // Bounds-checked
    if (data.size() < sizeof(Header)) {
        throw std::invalid_argument("Buffer too small for header");
    }
    auto header = std::bit_cast<Header>(data.first<sizeof(Header)>());
    auto payload = data.subspan(sizeof(Header));
    // No possibility of buffer overflow
}

// 2. Integer overflow protection
template<typename T>
std::optional<T> safeAdd(T a, T b) {
    if constexpr (std::is_signed_v<T>) {
        if ((b > 0 && a > std::numeric_limits<T>::max() - b) ||
            (b < 0 && a < std::numeric_limits<T>::min() - b))
            return std::nullopt;
    } else {
        if (a > std::numeric_limits<T>::max() - b)
            return std::nullopt;
    }
    return a + b;
}

// Or use compiler built-in:
int result;
if (__builtin_add_overflow(a, b, &result)) {
    handleOverflow();
}

// 3. Constant-time comparison (prevents timing attacks)
bool secureCompare(std::span<const uint8_t> a, std::span<const uint8_t> b) {
    if (a.size() != b.size()) return false;
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;  // No early exit → constant time
}

// 4. Secure memory wiping (prevent compiler from optimizing out)
void secureZero(void* ptr, size_t size) {
    volatile auto* p = static_cast<volatile unsigned char*>(ptr);
    while (size--) *p++ = 0;
    // volatile prevents compiler from removing the zeroing
}
// Or: explicit_bzero() on Linux, SecureZeroMemory() on Windows

// 5. RAII for sensitive data
class SecureString {
    std::string data_;
public:
    ~SecureString() { secureZero(data_.data(), data_.size()); }
    // Move only → no copies of sensitive data
    SecureString(SecureString&&) noexcept = default;
    SecureString& operator=(SecureString&&) noexcept = default;
    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;
};
```

**Build hardening flags:**
```cmake
# Compile-time
-fstack-protector-strong          # Stack canaries (detect buffer overflow)
-D_FORTIFY_SOURCE=2               # Fortified libc functions (bounds-checked memcpy, etc.)
-fPIE                             # Position Independent Executable (for ASLR)
-Wformat -Wformat-security        # Format string checks

# Link-time
-Wl,-z,relro,-z,now               # Full RELRO (read-only GOT)
-Wl,-z,noexecstack                # Non-executable stack
-pie                              # PIE binary (ASLR for main executable)

# Runtime
-fsanitize=safe-stack              # Separate stack for return addresses
-fsanitize=cfi                     # Control Flow Integrity (clang)
```

---

# Part E: Advanced Architecture & Decision Making

---

## Q9: You're leading the migration of a 2M-line C++ codebase from C++11 to C++20. How do you plan and execute this|

### Answer:

**Phase 0: Assessment (2-4 weeks)**
```
1. Inventory:
   - Compilers in use (GCC, Clang, MSVC versions)
   - Build systems (CMake, Make, Bazel)
   - Third-party dependencies (do they support C++20|)
   - CI/CD pipeline capabilities
   - Team C++20 knowledge level

2. Risk analysis:
   +------------------------------------------------------------------------+
   | Risk                        | Level | Mitigation                       |
   +------------------------------------------------------------------------+
   | Compiler bugs in C++20      | High  | Use latest stable compilers      |
   | Breaking changes from C++17 | Med   | -std=c++20 compile test first    |
   | Template code breaks        | Med   | Concept checks may reject old    |
   |                             |       | code that worked by accident     |
   | Third-party lib incompat    | High  | Test each dependency separately  |
   | Build time increase         | Med   | Modules, PCH, distributed build  |
   | Team unfamiliarity          | High  | Training, coding guidelines      |
   +------------------------------------------------------------------------+

3. Compiler support matrix:
   Feature          GCC    Clang   MSVC
   +-------------+ +----+ +-----+ +----+
   Concepts         10+    12+     19.30+
   Ranges           10+    13+     19.30+
   Coroutines       10+    14+     19.29+
   Modules          11+    16+     19.28+ (partial)
   constexpr new    12+    14+     19.33+
   Three-way comp.  10+    10+     19.28+
```

**Phase 1: Infrastructure (4-8 weeks)**
```
1. Upgrade compilers on ALL platforms (dev, CI, prod)
2. Add -std=c++20 flag to CMake
   cmake_minimum_required(VERSION 3.20)
   set(CMAKE_CXX_STANDARD 20)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
3. Fix ALL compilation errors (usually < 100 for 2M lines)
   - Most common: std::result_of → std::invoke_result
   - Most common: implicit conversions now rejected
   - Most common: reserved identifiers (requires, concept, co_await)
4. Green CI with C++20 standard → NO new features used yet
```

**Phase 2: Incremental Modernization (ongoing, 6-18 months)**
```
Priority 1 → Easy wins (grep & replace):
  std::make_unique already used?     → Good
  nullptr used everywhere?           → Should be since C++11
  auto used appropriately?           → Review
  Range-for loops everywhere|        → Should be since C++11
  
Priority 2 → Mechanical modernization:
  Rule                              Command/Tool
  +------------------------------+ +--------------------+
  std::bind → lambda                clang-tidy modernize-avoid-bind
  typedef → using                   clang-tidy modernize-use-using
  NULL → nullptr                    clang-tidy modernize-use-nullptr
  push_back(T(...)) → emplace_back  clang-tidy modernize-use-emplace
  enable_if → concepts              Manual (design review needed)
  virtual + override → override     clang-tidy modernize-use-override
  
  # Run modernization:
  run-clang-tidy -checks='modernize-*' -fix -j $(nproc) src/

Priority 3 → Strategic adoption (requires design review):
  - SFINAE | Concepts (clearer error messages)
  - Callback hell | Coroutines
  - Ad-hoc serialization | Structured Bindings
  - Iterator pairs | Ranges/Views
  - Mutex + condition_variable | jthread + stop_token
```

**Phase 3: Module Migration (12-24 months, if desired)**
```
Strategy: Bottom-up, leaf modules first

  1. Identify leaf modules (no internal dependencies)
  2. Convert header | module interface
  3. Update consumers one at a time
  4. Repeat with next layer
  
  +--------------------------+
  | Application Layer        | | Convert LAST
  |   imports core, network  |
  +--------------------------+
  | Network Module           | | Convert SECOND
  |   imports core           |
  +--------------------------+
  | Core Module              | | Convert FIRST
  |   (no internal deps)     |
  +--------------------------+
  
  Tip: Keep header versions alongside modules during transition
       #ifdef USE_MODULES
       import core;
       #else
       #include "core/core.h"
       #endif
```

**Governance:**
```
- Coding guidelines document updated with C++20 rules
- Code review checklist includes "used C++20 feature appropriately?"
- Brown-bag sessions: 1 feature per week (concepts, ranges, etc.)
- "Migration Champions" per team (trained first, help others)
- Metrics: % of files compiled as C++20, # of modernization PRs/month
- NO big-bang migration | incremental, team by team, module by module
```

### Why this matters:
At 10+ years, you've done migrations before. The interviewer wants to hear: pragmatic planning (not "just flip the switch"), risk assessment, incremental approach, tooling leverage (clang-tidy --fix), team enablement, and governance. The worst answer is "we'll just add -std=c++20 and fix what breaks."

---

## Q10: Explain expression templates. Where are they used and why?

### Answer:

**The problem → unnecessary temporaries:**
```cpp
// Naive vector math:
Vector a, b, c, d;
Vector result = a + b + c + d;

// What actually happens without expression templates:
// temp1 = a + b;           // Allocate + compute
// temp2 = temp1 + c;       // Allocate + compute
// temp3 = temp2 + d;       // Allocate + compute
// result = temp3;           // Copy
// 
// That's 3 temporary allocations and 4 loops through the data!
```

**Expression templates → compile-time expression tree:**
```cpp
// Instead of computing immediately, build an expression at compile time
// Then evaluate the ENTIRE expression in ONE pass

// Step 1: Expression node types
template<typename L, typename R>
struct VecAdd {
    const L& lhs;
    const R& rhs;
    
    double operator[](size_t i) const {
        return lhs[i] + rhs[i];  // Lazy → computed on access
    }
    size_t size() const { return lhs.size(); }
};

template<typename L, typename R>
struct VecMul {
    const L& lhs;
    const R& rhs;
    
    double operator[](size_t i) const {
        return lhs[i] * rhs[i];
    }
    size_t size() const { return lhs.size(); }
};

// Step 2: The actual Vector class
class Vector {
    std::vector<double> data_;
public:
    Vector(size_t n) : data_(n) {}
    
    double& operator[](size_t i) { return data_[i]; }
    double operator[](size_t i) const { return data_[i]; }
    size_t size() const { return data_.size(); }
    
    // Assign from ANY expression (the magic happens here)
    template<typename Expr>
    Vector& operator=(const Expr& expr) {
        for (size_t i = 0; i < data_.size(); ++i) {
            data_[i] = expr[i];  // Evaluates the ENTIRE expression per element
        }
        return *this;
    }
};

// Step 3: Operators return expression objects (NOT computed results)
template<typename L, typename R>
VecAdd<L, R> operator+(const L& lhs, const R& rhs) {
    return {lhs, rhs};
}

// Step 4: Usage
Vector a(1000), b(1000), c(1000), d(1000);
Vector result(1000);
result = a + b + c + d;

// What the compiler sees after template instantiation:
// result = VecAdd<VecAdd<VecAdd<Vector, Vector>, Vector>, Vector>
// 
// Which evaluates as:
// for (i = 0; i < 1000; ++i)
//     result[i] = a[i] + b[i] + c[i] + d[i];  // ONE loop, ZERO temporaries!
```

**Real-world usage:**
```
Library         Uses Expression Templates For
+------------+  +--------------------------------+
Eigen           Matrix/vector math (THE gold standard)
Blaze           Dense/sparse linear algebra
Boost.uBLAS     Linear algebra
xtensor         NumPy-like tensor operations
Armadillo       Linear algebra (MATLAB-like API)
```

**Modern C++20 version with concepts:**
```cpp
template<typename T>
concept VectorExpression = requires(T t, size_t i) {
    { t[i] } -> std::convertible_to<double>;
    { t.size() } -> std::convertible_to<size_t>;
};

template<VectorExpression L, VectorExpression R>
auto operator+(const L& lhs, const R& rhs) {
    return VecAdd<L, R>{lhs, rhs};
}
```

**Performance: Expression templates vs naive**
```
Operation: result = a + b * c + d  (size = 1,000,000)

Naive:              Expression Templates:
4 loops             1 loop
3 temporaries       0 temporaries
12MB memory         4MB memory (result only)
~3.2ms              ~0.8ms (4x faster)
```

### Why this matters:
Expression templates are a uniquely C++ technique — no other language can do this at compile time. They show deep understanding of templates, lazy evaluation, and zero-overhead abstraction. Eigen (the most used linear algebra library) is entirely built on this pattern. At 10+ years, you should understand how libraries like Eigen achieve their performance.

---

# Part F: Advanced Topics

---

## Q11: Explain CRTP (Curiously Recurring Template Pattern) advanced uses beyond static polymorphism.

### Answer:

```cpp
// === Use 1: Mixin → inject functionality into derived classes ===
template<typename Derived>
class Printable {
public:
    void print() const {
        // Access derived class without virtual dispatch
        const auto& self = static_cast<const Derived&>(*this);
        std::cout << self.toString() << "\n";
    }
    
    void printN(int n) const {
        for (int i = 0; i < n; ++i) print();
    }
};

template<typename Derived>
class Comparable {
public:
    bool operator!=(const Derived& other) const {
        const auto& self = static_cast<const Derived&>(*this);
        return !(self == other);
    }
    bool operator>(const Derived& other) const {
        return other < static_cast<const Derived&>(*this);
    }
    bool operator<=(const Derived& other) const {
        return !(static_cast<const Derived&>(*this) > other);
    }
    bool operator>=(const Derived& other) const {
        return !(static_cast<const Derived&>(*this) < other);
    }
    // Derived only needs to implement == and <
};

// Stack multiple mixins:
class Temperature : public Printable<Temperature>,
                    public Comparable<Temperature> {
    double celsius_;
public:
    Temperature(double c) : celsius_(c) {}
    std::string toString() const { return std::format("{}Â°C", celsius_); }
    bool operator==(const Temperature& o) const { return celsius_ == o.celsius_; }
    bool operator<(const Temperature& o) const { return celsius_ < o.celsius_; }
};

Temperature t1(20), t2(30);
t1.print();           // "20°C" → no virtual call
bool b = (t1 >= t2);  // Mixin-generated operator

// === Use 2: Object Counter (per-type) ===
template<typename Derived>
class Counter {
    static inline std::atomic<int> count_{0};
public:
    Counter() { ++count_; }
    Counter(const Counter&) { ++count_; }
    ~Counter() { --count_; }
    static int alive() { return count_; }
};

class Widget : public Counter<Widget> {};
class Gadget : public Counter<Gadget> {};

Widget w1, w2, w3;
Gadget g1;
Widget::alive();  // 3 → separate count per type!
Gadget::alive();  // 1

// === Use 3: Static Interface Enforcement ===
template<typename Derived>
class Serializable {
public:
    std::string serialize() const {
        // Compile error if Derived doesn't implement serializeImpl
        return static_cast<const Derived*>(this)->serializeImpl();
    }
    
    static Derived deserialize(const std::string& data) {
        return Derived::deserializeImpl(data);
    }
};

// === Use 4: Method Chaining (Fluent API) for inheritance ===
template<typename Derived>
class QueryBuilder {
public:
    Derived& select(std::string_view cols) {
        query_ += std::format("SELECT {} ", cols);
        return self();
    }
    Derived& from(std::string_view table) {
        query_ += std::format("FROM {} ", table);
        return self();
    }
    Derived& where(std::string_view cond) {
        query_ += std::format("WHERE {} ", cond);
        return self();
    }
protected:
    Derived& self() { return static_cast<Derived&>(*this); }
    std::string query_;
};

// Derived adds more fluent methods without breaking the chain:
class PgQueryBuilder : public QueryBuilder<PgQueryBuilder> {
public:
    PgQueryBuilder& returning(std::string_view cols) {
        query_ += std::format("RETURNING {} ", cols);
        return *this;
    }
};

// Fluent chain works seamlessly:
auto q = PgQueryBuilder()
    .select("*")
    .from("users")
    .where("age > 21")
    .returning("id");    // This works because base returns PgQueryBuilder&!
```

**CRTP vs Virtual vs C++20 Concepts:**
```
+------------------------------------------------------------------------+
|              | Virtual       | CRTP             | Concepts (C++20)     |
+------------------------------------------------------------------------+
| Dispatch     | Runtime (vtbl)| Compile-time     | Compile-time         |
| Overhead     | vtable ptr    | Zero             | Zero                 |
| Heterogeneous| Yes (base*)   | No (diff types)  | No (diff types)      |
| Inlining     | Usually no    | Yes              | Yes                  |
| Error msgs   | Good          | Terrible          | Excellent            |
| Add new type | Easy          | Easy             | Easy                 |
| Best for     | Runtime poly  | Mixins, counters | Constraints, traits  |
+------------------------------------------------------------------------+

Rule of thumb:
  Need runtime polymorphism (vector<Base*>)?  → virtual
  Need compile-time mixin/injection?          → CRTP
  Need compile-time constraint/requirement?   → Concepts
```

---

## Q12: How do you design for testability in C++? Dependency injection without a framework.

### Answer:

```cpp
// === Technique 1: Constructor Injection (most common) ===

// Interface (pure abstract class)
class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual std::optional<User> findUser(int id) = 0;
    virtual bool saveUser(const User& user) = 0;
};

class IEmailService {
public:
    virtual ~IEmailService() = default;
    virtual bool send(const std::string& to, const std::string& body) = 0;
};

// Production implementation
class PostgresDB : public IDatabase {
    pqxx::connection conn_;
public:
    PostgresDB(std::string_view connStr) : conn_(connStr) {}
    std::optional<User> findUser(int id) override { /* real SQL */ }
    bool saveUser(const User& user) override { /* real SQL */ }
};

// The class under test → depends on abstractions, not implementations
class UserService {
public:
    // Constructor injection → dependencies provided from outside
    UserService(std::unique_ptr<IDatabase> db, 
                std::unique_ptr<IEmailService> email)
        : db_(std::move(db)), email_(std::move(email)) {}
    
    bool registerUser(const std::string& name, const std::string& email) {
        User user{.name = name, .email = email};
        if (!db_->saveUser(user)) return false;
        email_->send(email, "Welcome!");
        return true;
    }

private:
    std::unique_ptr<IDatabase> db_;
    std::unique_ptr<IEmailService> email_;
};

// === Test with mocks (GoogleMock) ===
class MockDatabase : public IDatabase {
public:
    MOCK_METHOD(std::optional<User>, findUser, (int id), (override));
    MOCK_METHOD(bool, saveUser, (const User&), (override));
};

class MockEmailService : public IEmailService {
public:
    MOCK_METHOD(bool, send, (const std::string&, const std::string&), (override));
};

TEST(UserServiceTest, RegisterSendsWelcomeEmail) {
    auto mockDb = std::make_unique<MockDatabase>();
    auto mockEmail = std::make_unique<MockEmailService>();
    
    EXPECT_CALL(*mockDb, saveUser(_)).WillOnce(Return(true));
    EXPECT_CALL(*mockEmail, send("a@b.com", "Welcome!")).WillOnce(Return(true));
    
    UserService svc(std::move(mockDb), std::move(mockEmail));
    EXPECT_TRUE(svc.registerUser("Alice", "a@b.com"));
}

// === Technique 2: Template-based DI (zero virtual overhead) ===
template<typename Database, typename Email>
class UserServiceT {
    Database db_;
    Email email_;
public:
    UserServiceT(Database db, Email email) 
        : db_(std::move(db)), email_(std::move(email)) {}
    
    bool registerUser(const std::string& name, const std::string& email) {
        User user{.name = name, .email = email};
        if (!db_.saveUser(user)) return false;
        email_.send(email, "Welcome!");
        return true;
    }
};

// Production:
using ProductionUserService = UserServiceT<PostgresDB, SmtpEmailService>;

// Test:
using TestUserService = UserServiceT<FakeDatabase, FakeEmailService>;

// === Technique 3: Simple DI Container (no framework) ===
class ServiceLocator {
    std::unordered_map<std::type_index, std::shared_ptr<void>> services_;
    
public:
    template<typename T>
    void bind(std::shared_ptr<T> service) {
        services_[std::type_index(typeid(T))] = std::move(service);
    }
    
    template<typename T>
    std::shared_ptr<T> resolve() {
        auto it = services_.find(std::type_index(typeid(T)));
        if (it == services_.end()) 
            throw std::runtime_error("Service not registered: " + 
                                      std::string(typeid(T).name()));
        return std::static_pointer_cast<T>(it->second);
    }
};

// Setup:
ServiceLocator container;
container.bind<IDatabase>(std::make_shared<PostgresDB>("host=localhost"));
container.bind<IEmailService>(std::make_shared<SmtpEmailService>());

// Resolve:
auto db = container.resolve<IDatabase>();
auto email = container.resolve<IEmailService>();
```

**Testability Principles:**
```
1. Depend on abstractions (interfaces), not implementations
2. Inject dependencies | never create them internally
3. Single Responsibility | small classes are easier to test
4. No global state (singletons are testing enemies)
5. Pure functions where possible (no side effects = easy to test)

Anti-patterns that kill testability:
  | new PostgresDB() inside UserService constructor
  | Singleton::instance() called anywhere
  | static methods that access global state
  | Hard-coded file paths, URLs, or connection strings
  | System clock calls (use injectable Clock interface)
```

---

## Q13: What are the key differences in C++ development across platforms (Windows, Linux, macOS)? How do you write truly portable code?

### Answer:

**Platform Pitfalls Cheatsheet:**
```
+--------------------------------------------------------------------------+
| Issue             | Windows          | Linux          | macOS            |
+--------------------------------------------------------------------------+
| Path separator    | \ (backslash)    | / (forward)    | / (forward)      |
| Line endings      | \r\n             | \n             | \n               |
| Shared lib ext    | .dll             | .so            | .dylib           |
| Static lib ext    | .lib             | .a             | .a               |
| Symbol export     | __declspec       | visibility     | visibility       |
| Thread local      | __declspec(thrd) | __thread       | __thread         |
| Filesystem        | Case-insensitive | Case-sensitive | Case-insensitive |
|                   |                  |                | (HFS+)           |
| Max path          | 260 (legacy)     | 4096           | 1024             |
| ABI               | MSVC ABI         | Itanium ABI   | Itanium ABI      |
| Debugger          | WinDbg / VS      | GDB / LLDB    | LLDB             |
| Package manager   | vcpkg / Conan    | apt/dnf+Conan | brew + Conan     |
| Default alloc     | Windows Heap     | glibc malloc   | libmalloc        |
| Time resolution   | ~15ms (default)  | ~1ns           | ~1ns             |
| Socket API        | Winsock2         | POSIX          | POSIX            |
| File locking      | LockFileEx       | flock/fcntl    | flock/fcntl      |
| Async I/O         | IOCP             | epoll/io_uring | kqueue           |

---

# ENHANCED SECTION: Distinguished Engineer Technical Depth

> *These questions are asked at L7+/Principal/Distinguished level — they test decade-spanning experience and system-level thinking.*

---

## Q16: How do you design for crash recovery in a systems program?

### Answer:
```cpp
// Write-Ahead Log (WAL) -> the universal crash recovery pattern
// Rule: Log the operation BEFORE performing it

class WAL {
    std::ofstream logFile_;
    uint64_t sequenceNum_ = 0;
    
public:
    uint64_t appendEntry(const std::string& operation, 
                          const std::string& data) {
        uint64_t seq = ++sequenceNum_;
        logFile_ << seq << "|" << operation << "|" << data << "\n";
        logFile_.flush(); // fsync -> durable on disk
        return seq; // NOW safe to perform the actual operation
    }
    
    void checkpoint(uint64_t lastApplied) {
        // Snapshot current state
        // Truncate old log entries
    }
    
    void recover() {
        // 1. Load last checkpoint
        // 2. Replay all WAL entries after checkpoint
        // 3. System is now consistent
    }
};
```

**Systems using WAL:**
- **PostgreSQL**: WAL for crash recovery + streaming replication
- **iCluster**: Journal receivers ARE the WAL; staging stores checkpoint applied positions
- **SQLite**: WAL mode for concurrent reads during writes
- **Kafka**: Append-only log IS the data structure
- **Our KVStore (Set 13)**: WAL for crash recovery of key-value operations

**iCluster's crash recovery protocol:**
```
1. Source crashes: Resume from last committed journal position (jrnCurrentPos)
2. Target crashes: Restart OMTARGET, re-apply from jrnApplyPos
3. Network failure: Source retains SAVFs in staging store, retransmit on reconnect
```

### Explanation:
Every senior engineer should know: **WAL guarantees durability**, **checkpoints bound recovery time**, **idempotent operations enable safe replay**. iCluster embodies all three | the journal IS the WAL, staging stores ARE checkpoints, and RSTOBJ IS idempotent.

---

## Q17: Design a versioned configuration system for distributed deployment.

### Answer:
```
Design (inspired by iCluster's DM layer):

1. VERSIONED STORAGE
   v1: {timeout: 30, retries: 3}
   v2: {timeout: 60, retries: 3}  → changed timeout

2. TWO-PHASE ROLLOUT
   Phase 1: Distribute to all nodes (PREPARE)
   Phase 2: All nodes switch atomically (COMMIT)
   If any node fails Phase 1: ABORT, keep old version

3. ROLLBACK = simply switch back to previous version number

4. LIVE RELOAD via atomic pointer swap:
   std::atomic<std::shared_ptr<const Config>> activeConfig_;

iCluster's approach (DM layer):
- DMNODES, DMGROUPS, DMREPL* files store configuration
- Changes propagated via DMKAPI to all nodes
- Two-phase protocol ensures consistency
- Registry keys (dmks_put_key/dmks_get_key) for runtime params
```

---

## Q18: Security hardening for C++ systems at architectural level.

### Answer:
```
LAYERED DEFENSE:

1. INPUT VALIDATION
   - Validate ALL external input at system boundary
   - Bounds-check array access (std::span, std::at())
   - Never trust deserialized data without validation

2. MEMORY SAFETY
   - Smart pointers for ownership
   - Sanitizers in CI (ASan, UBSan, TSan, MSan)
   - Stack canaries, ASLR, PIE, W^X

3. CRYPTOGRAPHY
   - Use established libraries (OpenSSL, libsodium)
   - TLS 1.3 for network, AES-256-GCM for data at rest
   - Constant-time comparison for secrets

4. PRIVILEGE MINIMIZATION
   - Drop privileges after initialization
   - Seccomp-BPF syscall filtering
   - Separate processes for untrusted data parsing

5. SUPPLY CHAIN
   - Pin dependency versions
   - Reproducible builds, SBOM, binary signing

iCluster security: Encrypted DMKCOMM channels, authority
replication (HABSFAUTH), object-level access control,
audit journal for all operations.
```

---
| Exec format       | PE (.exe)        | ELF            | Mach-O           |
+--------------------------------------------------------------------------+
```

**Portable code patterns:**
```cpp
// 1. Use std::filesystem (C++17) → eliminates path issues
#include <filesystem>
namespace fs = std::filesystem;

fs::path config = fs::current_path() / "config" / "app.json";
// Windows: "C:\MyApp\config\app.json"
// Linux:   "/home/user/config/app.json"
// Correct on all platforms → fs::path handles separators

// 2. Platform-specific code via CMake + headers
// platform.h
#if defined(_WIN32)
    #define PLATFORM_WINDOWS
    #define EXPORT __declspec(dllexport)
    #define IMPORT __declspec(dllimport)
#elif defined(__APPLE__)
    #define PLATFORM_MACOS
    #define EXPORT __attribute__((visibility("default")))
    #define IMPORT
#elif defined(__linux__)
    #define PLATFORM_LINUX
    #define EXPORT __attribute__((visibility("default")))
    #define IMPORT
#endif

// 3. Abstract platform differences behind interfaces
class FileWatcher {
public:
    virtual ~FileWatcher() = default;
    virtual void watch(const fs::path& dir, Callback cb) = 0;
    
    static std::unique_ptr<FileWatcher> create(); // Factory
};

// Implementations in separate .cpp files:
// file_watcher_win32.cpp   → ReadDirectoryChangesW
// file_watcher_linux.cpp   → inotify
// file_watcher_macos.cpp   → FSEvents / kqueue

// CMakeLists.txt:
// if(WIN32)
//     target_sources(mylib PRIVATE file_watcher_win32.cpp)
// elseif(APPLE)
//     target_sources(mylib PRIVATE file_watcher_macos.cpp)
// else()
//     target_sources(mylib PRIVATE file_watcher_linux.cpp)
// endif()

// 4. Time → use <chrono>, never platform-specific APIs
auto start = std::chrono::steady_clock::now();
doWork();
auto elapsed = std::chrono::steady_clock::now() - start;
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
```

---

## Q14: What is policy-based design? How does it compare to strategy pattern and when do you use each?

### Answer:

```cpp
// Strategy pattern: RUNTIME selection via virtual dispatch
// Policy-based design: COMPILE-TIME selection via templates

// === Policy-Based Design (Alexandrescu) ===

// Policies are template parameters that customize behavior
template<
    typename ThreadingPolicy,   // How to handle thread safety
    typename LoggingPolicy,     // How to log
    typename AllocationPolicy   // How to allocate memory
>
class SmartPointer : private ThreadingPolicy, 
                      private LoggingPolicy,
                      private AllocationPolicy {
    using ThreadingPolicy::lock;
    using ThreadingPolicy::unlock;
    using LoggingPolicy::log;
    using AllocationPolicy::allocate;
    using AllocationPolicy::deallocate;
    
    void* ptr_ = nullptr;
    
public:
    void reset(void* newPtr) {
        lock();
        log("SmartPointer::reset");
        if (ptr_) deallocate(ptr_);
        ptr_ = newPtr;
        unlock();
    }
};

// Policy implementations:
struct SingleThreaded {
    void lock() {}    // No-op
    void unlock() {}
};

struct MultiThreaded {
    std::mutex mtx_;
    void lock() { mtx_.lock(); }
    void unlock() { mtx_.unlock(); }
};

struct NoLogging {
    void log(const char*) {}
};

struct ConsoleLogging {
    void log(const char* msg) { std::cout << msg << "\n"; }
};

struct HeapAllocation {
    void* allocate(size_t n) { return ::operator new(n); }
    void deallocate(void* p) { ::operator delete(p); }
};

struct PoolAllocation {
    MemoryPool pool_;
    void* allocate(size_t n) { return pool_.alloc(n); }
    void deallocate(void* p) { pool_.free(p); }
};

// Compose policies at compile time → zero overhead!
using FastPtr = SmartPointer<SingleThreaded, NoLogging, PoolAllocation>;
using DebugPtr = SmartPointer<MultiThreaded, ConsoleLogging, HeapAllocation>;
using ProdPtr = SmartPointer<MultiThreaded, NoLogging, PoolAllocation>;

// Each combination is a DIFFERENT TYPE → compiler optimizes away unused code
// FastPtr::reset() has NO mutex calls and NO logging calls in the binary
```

**Comparison:**
```
+-------------------------------------------------------------+
|                | Strategy Pattern    | Policy-Based Design  |
+-------------------------------------------------------------+
| Selection      | Runtime             | Compile-time         |
| Dispatch       | Virtual (vtable)    | Static (inlined)     |
| Overhead       | Virtual call + ptr  | Zero                 |
| Flexibility    | Can change at runtime| Fixed at compile    |
| Types          | Same type (Base*)   | Different types      |
| Error detection| Runtime             | Compile-time         |
| Best for       | User-selectable     | Library design       |
|                | algorithms          | (performance-critical|
|                |                     |  customization)      |
+-------------------------------------------------------------+

Decision:
  Need to change behavior at runtime?           → Strategy
  Need maximum performance, known at compile?   → Policy
  Need heterogeneous container?                 → Strategy
  Building a library with customization points? → Policy
```

**Real-world policy-based design:**
```cpp
// std::allocator is a policy!
std::vector<int, MyPoolAllocator<int>> vec;

// std::char_traits is a policy!
std::basic_string<char, CaseInsensitiveTraits> ciString;

// std::hash + std::equal_to are policies!
std::unordered_map<Key, Value, MyHash, MyEqual, MyAlloc> map;
```

---

## Q15: You're asked to review a pull request with 3000 lines of C++ changes. What's your approach?

### Answer:

**Systematic Code Review Framework:**
```
Phase 1: Big Picture (5 minutes)
  | Read PR description and linked ticket/issue
  | Understand the WHAT and WHY before looking at code
  | Check: Does the approach make sense architecturally|
  | Check: Should this be split into smaller PRs|

Phase 2: Structure (10 minutes)
  | Look at file list | which modules are touched|
  | Are there new files| Do they belong where they are|
  | Is there a test file for each production file changed|
  | Check CMakeLists.txt changes | new dependencies|

Phase 3: Detailed Review | prioritized (30-60 minutes)
  Priority 1 → CORRECTNESS:
    | Logic errors (off-by-one, wrong operator, missing case)
    | Concurrency bugs (races, deadlocks, missing locks)
    | Memory bugs (leaks, use-after-free, dangling refs)
    | Error handling (what happens when X fails|)
    | Edge cases (empty input, max values, null)
    
  Priority 2 → SECURITY:
    | Input validation at system boundaries
    | Buffer overflow potential
    | SQL/command injection (if applicable)
    | Sensitive data exposure in logs
    
  Priority 3 → DESIGN:
    | Does it follow existing patterns in the codebase|
    | Is it testable| (dependency injection, no globals)
    | Single responsibility | is any class doing too much|
    | API design | will callers find this intuitive|
    
  Priority 4 → MAINTAINABILITY:
    | Naming | are variables/functions self-documenting|
    | Complexity | can any function be simplified|
    | Duplication | is code copy-pasted from elsewhere|
    
  Priority 5 → PERFORMANCE (only if on hot path):
    | Unnecessary copies (pass by const ref, move semantics)
    | Algorithmic complexity (O(nÂ²) where O(n log n) exists)
    | Memory allocation in loops
```

**What separates senior from junior code review:**
```
Junior reviewer:
  "Line 42: use auto instead of std::vector<int>::iterator"
  "Line 89: missing space after if"
  | Nitpicks that a linter should catch

Senior reviewer:
  "This lock is held across the network call on line 120-145.
   Under load, this will serialize all requests through one mutex.
   Consider: read the shared state under lock, release lock, 
   then do the network call, then re-acquire to update state."
  
  "The Observer here holds a raw pointer to the subject.
   If the subject is destroyed first, all observers have dangling ptrs.
   Consider weak_ptr or a deregistration callback in destructor."
  
  | Architectural issues, concurrency concerns, lifecycle problems
```

**Giving constructive feedback:**
```
Bad:   "This is wrong."
Good:  "This will deadlock when called from thread A because..."

Bad:   "Use X instead."
Good:  "Consider using X here because it avoids the copy on line 50
        and makes the ownership transfer explicit."

Bad:   "This is messy."
Good:  "This function handles validation, transformation, and persistence.
        Would it be clearer as three functions? Happy to discuss."

Always:
  - Prefix blocking issues: "BLOCKER: This has a race condition..."
  - Prefix suggestions: "NIT: Could simplify with ranges..."
  - Prefix questions: "Q: What happens if the callback throws?"
  - Acknowledge good work: "Nice use of RAII here → clean approach"
```

---



# 🎯 INTERVIEWER GUIDE — Set 12 Scoring & Evaluation

---

## Staff/Principal Engineer Bar

> **These questions separate 10-year veterans from 5-year seniors.**

| Topic | Senior Expectation | Staff/Principal Expectation |
|-------|-------------------|----------------------------|
| **ABI** | Knows Pimpl | Designs library versioning strategy for 200+ consumers |
| **Plugins** | Can load a DLL | Designs version negotiation, error isolation, discovery |
| **Debugging** | Uses GDB basics | Analyzes core dumps from 1-in-10K race condition crashes |
| **Interop** | Used pybind11 | Designs cross-language architectures with zero-copy semantics |

## Green Flags for Staff+ Candidates

- Discusses production incidents they diagnosed and fixed
- Thinks about API consumers, not just implementation
- Mentions backward compatibility and migration paths
- Can explain WHY they chose an architecture, not just WHAT it is
- Has experience maintaining code for years (not just writing it)
- Has shipped to production and dealt with real user feedback

## Red Flags for Staff+ Candidates

- All experience is greenfield — never maintained/evolved existing systems
- Can't discuss a production incident they resolved
- Over-engineers everything (creates abstractions for one-time operations)
- Has never profiled code or analyzed performance
- Can't name a technology choice they regret and what they learned

## The 10-Year Question (Final Assessment)

> **Would I trust this person to design a critical system, ship it, and maintain it for 5 years while the team changes around them?**

1. **Judgment** — Making good decisions with incomplete information
2. **Ownership** — Taking responsibility for outcomes, not just code
3. **Influence** — Making the team better through technical leadership
4. **Sustainability** — Building systems that survive personnel changes

---