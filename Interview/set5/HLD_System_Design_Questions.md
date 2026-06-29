# Set 5: High Level Design (HLD) / System Design
## Senior-Level Interview Questions (10 Years Experience)

---

## Q1: Design a Real-Time Collaborative CAD System (like Figma for 3D)

### Requirements:
- Multiple users edit the same 3D model simultaneously
- Real-time synchronization with conflict resolution
- Version history and undo per user
- Support large assemblies (millions of polygons)

### High Level Architecture:

```
+----------+     +----------+     +----------+
|  Client   |     |  Client   |     |  Client   |
|  (C++ App)|     |  (C++ App)|     |  (Web GL) |
+-----------+     +-----------+     +-----------+
      | WebSocket        | WebSocket        | WebSocket
      +-------------------------------------+
                         |
              +---------------------+
              |   API Gateway /     |
              |   Load Balancer     |
              +---------------------+
                         |
         +-------------------------------+
         |               |               |
+----------------+ +------------+ +-------------+
|  Collaboration | |  Geometry   | |  Rendering  |
|  Service       | |  Service    | |  Service    |
|  (CRDT/OT)     | |  (B-Rep)    | |  (Tiles)    |
+----------------+ +------------+ +-------------+
         |               |               |
    +---------+     +---------+     +---------+
    | Redis   |     |PostgreSQL|    |  Object  |
    | Pub/Sub |     | + PostGIS|    |  Storage |
    +---------+     +---------+    |  (S3)    |
                                    +----------+
```

### Key Components:

**1. Real-Time Collaboration Engine:**
- **CRDT (Conflict-free Replicated Data Types)** over OT (Operational Transform)
- Why CRDT? No central server needed for conflict resolution, eventual consistency guaranteed
- Operations: AddShape, DeleteShape, MoveVertex, SetProperty
- Each operation has a Lamport timestamp + user ID for ordering

**2. Geometry Service:**
- B-Rep (Boundary Representation) kernel → like OpenCASCADE
- Operations: Boolean (union, subtract, intersect), fillet, chamfer
- Heavy computation offloaded to server-side workers
- Results cached and streamed back as tessellated meshes

**3. Rendering Pipeline:**
- **Level of Detail (LOD)**: Server pre-generates LOD0-LOD3
- **Tile-based streaming**: Only send visible geometry to client
- **Frustum culling** on server: Don't transmit what client can't see
- **Progressive loading**: Low-res first, then refine

**4. Data Storage:**
- **Git-like versioning**: Each save = snapshot + delta
- **PostgreSQL** for metadata, constraints, BOM (Bill of Materials)
- **Object Storage (S3)** for geometry blobs, textures, renders
- **Redis** for real-time session state, presence, cursors

### Scalability Considerations:
| Challenge | Solution |
|-----------|----------|
| Large assemblies (1M+ parts) | Spatial partitioning (Octree), stream visible only |
| Many concurrent editors | Shard by document, horizontal scale collab service |
| Network latency | Client-side prediction, reconcile on server response |
| Geometry computation | GPU compute farm, job queue with priority |
| Version storage | Delta compression, deduplication |

### Explanation:
This design is asked at companies like Autodesk, PTC, Dassault, Figma, and Onshape. Key talking points:
- **CRDT vs OT**: CRDT is mathematically guaranteed to converge without coordination; OT needs a central server
- **B-Rep vs Mesh**: CAD uses B-Rep for precision; mesh is for visualization only
- **Spatial indexing**: Octree/BVH for 3D, R-Tree for 2D (PostGIS uses R-Trees)

---

## Q2: Design a High-Frequency Trading (HFT) System

### Requirements:
- Sub-microsecond latency for order execution
- Process 1M+ market data messages/second
- Multiple trading strategies running simultaneously
- Risk management and position tracking in real-time

### Architecture:

```
+-------------+
|  Exchange    +----+ Co-located servers (same data center)
|  (FIX/ITCH) |
+-------------+
       | Raw market data (UDP multicast)
       |
+-----------------------------------------------------+
|               FPGA / Kernel Bypass NIC              |
|          (Solarflare/Mellanox + DPDK/ef_vi)         |
+-----------------------------------------------------+
       |
+-------------+     +--------------+     +------------+
|  Market Data +-----+   Strategy   +-----+   Order    |
|  Handler     |     |   Engine     |     |   Manager  |
|  (Lock-free) |     |  (C++ Core)  |     | (FIX/OUCH) |
+--------------+     +--------------+     +------------+
                            |                    |
                     +--------------+     +-------------+
                     |    Risk      |     |  Position   |
                     |   Manager    |     |  Tracker    |
                     +--------------+     +-------------+
                            |
                     +--------------+
                     |   Logging    |
                     | (Memory-     |
                     |  mapped file)|
                     +--------------+
```

### Key Design Decisions:

**1. Latency-Critical Path (Hot Path):**
```
Market Data | Parse | Strategy Signal | Risk Check | Order Submit
Target: < 5 microseconds end-to-end
```

**C++ Techniques for Low Latency:**
```cpp
// 1. Lock-free ring buffer for market data
template<typename T, size_t N>
class SPSCRingBuffer {
    alignas(64) std::atomic<size_t> head_{0};  // Separate cache lines
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) std::array<T, N> buffer_;

public:
    bool push(const T& item) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t next = (h + 1) % N;
        if (next == tail_.load(std::memory_order_acquire)) return false;
        buffer_[h] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return false;
        item = buffer_[t];
        tail_.store((t + 1) % N, std::memory_order_release);
        return true;
    }
};

// 2. Pre-allocated object pool (no malloc on hot path)
// 3. Busy-spin instead of condition variables
// 4. CPU pinning + isolated cores (isolcpus)
// 5. Huge pages for TLB efficiency
// 6. Compile-time polymorphism (CRTP) → no virtual calls
```

**2. Strategy Engine:**
- Multiple strategies as compile-time plugins (templates, not virtual)
- Signal: BUY/SELL/HOLD with confidence score
- Strategies: Market Making, Statistical Arbitrage, Momentum

**3. Risk Management:**
- Pre-trade: Position limits, order size limits, price bands
- Real-time: P&L tracking, exposure limits, kill switch
- Must be on the hot path but extremely fast → lookup tables, not computation

**4. Logging:**
- **Memory-mapped files** for zero-copy logging
- Binary format, not text → parse offline
- Circular buffer in shared memory for crash recovery

### Key Metrics:
| Metric | Target |
|--------|--------|
| Tick-to-trade latency | < 5 |s |
| Market data throughput | 10M msg/sec |
| Order rate | 100K orders/sec |
| Uptime | 99.999% during market hours |

### Explanation:
This is THE signature system design question for finance. Key points:
- **No heap allocation** on hot path → everything pre-allocated
- **No system calls** on hot path → kernel bypass networking
- **No virtual dispatch** ? everything resolved at compile time
- **Cache-conscious design** ? data aligned to cache lines, avoid false sharing
- **Deterministic latency** > average latency → jitter matters more than throughput

---

## Q3: Design a Multiplayer Game Server (like Fortnite/PUBG)

### Architecture:

```
+---------+  +---------+  +---------+
| Client 1 |  | Client 2 |  |Client N  |
+----------+  +----------+  +----------+
     | UDP          | UDP         | UDP
     +----------------------------+
                    |
         +---------------------+
         |   Game Server       |
         |   (Dedicated C++)   |
         |                     |
         | +-----------------+ |
         | |  Game Loop      | +---+ 60 tick/sec
         | |  (Fixed Step)   | |
         | +-----------------+ |
         |          |          |
         | +-----------------+ |
         | | World State     | |
         | | - Spatial Grid  | |
         | | - Entity Pool   | |
         | | - Physics       | |
         | +-----------------+ |
         |          |          |
         | +-----------------+ |
         | | Net Serializer  | |
         | | - Delta Compress| |
         | | - Priority      | |
         | +-----------------+ |
         +---------------------+
                    |
         +---------------------+
         |   Backend Services  |
         +---------------------+
         | Matchmaking Service |
         | Auth Service        |
         | Leaderboard (Redis) |
         | Analytics (Kafka)   |
         | Chat (WebSocket)    |
         +---------------------+
```

### Key Components:

**1. Game Loop (Fixed Timestep):**
```cpp
void GameServer::run() {
    const auto TICK_RATE = std::chrono::milliseconds(16);  // ~60 Hz

    while (running_) {
        auto tickStart = std::chrono::steady_clock::now();

        processInput();       // Read client inputs from network buffer
        updatePhysics();      // Deterministic physics step
        resolveCollisions();  // Spatial hash grid
        updateGameLogic();    // Damage, scoring, items
        sendSnapshots();      // Delta-compressed state to clients

        auto elapsed = std::chrono::steady_clock::now() - tickStart;
        if (elapsed < TICK_RATE)
            busyWait(TICK_RATE - elapsed);  // Precise timing
    }
}
```

**2. Networking ? Client-Side Prediction & Server Reconciliation:**
- **Client-side prediction**: Client applies input immediately (responsive feel)
- **Server authority**: Server validates and broadcasts authoritative state
- **Reconciliation**: Client replays unacknowledged inputs on receiving server state
- **Entity interpolation**: Smooth movement for other players (buffer 2-3 snapshots)
- **Lag compensation**: Server rewinds time to validate hit detection

**3. Spatial Partitioning:**
- **Grid-based** for flat worlds (battle royale maps)
- **Octree** for 3D environments
- Interest management: Only replicate entities within player's relevance radius

**4. Network Optimization:**
```
Bandwidth budget per player: ~20 KB/s
- Delta compression: Only send what changed (80% reduction)
- Quantization: Positions as int16 (mm precision) vs float64
- Priority: Nearby entities update more frequently
- Packet aggregation: Bundle updates per tick
```

### Scalability:
| Players | Architecture |
|---------|-------------|
| 2-16 (arena) | Single game server process |
| 100 (battle royale) | Single beefy server, spatial streaming |
| 1000+ (MMO) | Sharded world, seamless server boundaries |
| Millions concurrent | Matchmaking distributes across thousands of game servers |

---

## Q4: Design a Distributed Build System (like Bazel/DistCC)

### Architecture:

```
+----------------------------------------------------------+
|                    Build Client (CLI)                     |
|  +----------+  +--------------+  +-------------------+  |
|  | Parser   |  | Dep Graph    |  | Action Scheduler   |  |
|  | (BUILD   -- | Builder      -- | (Local / Remote)   |  |
|  |  files)  |  | (DAG)        |  |                    |  |
|  +----------+  +--------------+  +-------------------+  |
+----------------------------------------------------------+
                                             |
                    +------------------------+
                    |                        |
           +---------------+      +---------------------+
           | Remote Cache  |      |  Remote Execution   |
           | (Content-     |      |  Service             |
           |  Addressable) |      |  +----------------+  |
           |               |      |  | Worker Pool    |  |
           |  key = hash(  |      |  | (100s of cores)|  |
           |   inputs +    |      |  +----------------+  |
           |   command)    |      +---------------------+
           +---------------+
```

### Key Concepts:

**1. Dependency Graph (DAG):**
```cpp
// Each build target is a node
struct BuildTarget {
    std::string name;
    std::vector<std::string> sources;
    std::vector<std::string> dependencies;
    std::string command;  // Compile/link command

    // Content hash = hash(sources + deps_hashes + command)
    std::string contentHash() const;
};
```

**2. Content-Addressable Cache:**
- Key = hash(all inputs + command + compiler version)
- If hash matches → skip compilation, use cached output
- Shared across all developers → dramatic speedup
- **CAD build systems**: Compile 10,000+ source files | cache cuts build from 2h to 5min

**3. Remote Execution:**
- Send hermetic action (inputs + command) to worker
- Worker executes in sandboxed environment
- Return outputs (object files, artifacts)
- **Horizontal scaling**: 1000 cores for compilation

**4. Incremental Builds:**
- File-level dependency tracking (header includes)
- Only rebuild changed files + their dependents
- **C++ challenge**: Header changes can cascade widely | precompiled headers, modules help

### Design Decisions:
| Decision | Choice | Why |
|----------|--------|-----|
| Hashing | SHA-256 | Collision-resistant, fast with hardware acceleration |
| Cache storage | CAS (Content-Addressable) | Deduplication, immutable |
| Scheduling | Critical-path first | Minimize wall-clock time |
| Sandboxing | Filesystem namespace | Hermetic builds, reproducible |
| Network protocol | gRPC | Streaming, efficient, typed |

---

## Q5: Design a Real-Time Risk Management System (Finance)

### Architecture:

```
Market Data Feed --> Pricing Engine --> Risk Calculator --> Dashboard
     |                    |                    |                |
     |                    |                    |                |
 Tick Store          Price Cache          Risk Store        Alerting
 (Time-series DB)   (Redis)              (In-memory)       (PagerDuty)
```

### Key Components:

**1. Risk Metrics (calculated in real-time):**
- **VaR (Value at Risk)**: Max expected loss at confidence level
- **Greeks**: Delta, Gamma, Vega, Theta for options
- **P&L**: Real-time profit/loss per position, desk, firm
- **Exposure**: Net/gross by asset class, currency, counterparty

**2. Pricing Engine:**
```cpp
// Pricing different instruments
class IPricer {
public:
    virtual ~IPricer() = default;
    virtual double price(const Instrument& inst, const MarketData& mkt) = 0;
    virtual Greeks greeks(const Instrument& inst, const MarketData& mkt) = 0;
};

class BlackScholesPricer : public IPricer { /* Options */ };
class MonteCarloPricer : public IPricer { /* Complex derivatives */ };
class DiscountCashFlowPricer : public IPricer { /* Bonds */ };
```

**3. Aggregation Hierarchy:**
```
Position | Book | Desk | Business Unit | Firm
Each level aggregates risk metrics from below
```

**4. Performance Requirements:**
| Metric | Target |
|--------|--------|
| Position repricing | < 1ms per position |
| Portfolio risk update | < 100ms for 100K positions |
| End-of-day VaR | < 5 min (Monte Carlo with 10K scenarios) |
| Market data latency | < 10|s from feed to engine |

**5. Key Technical Decisions:**
- **In-memory computation**: All positions and market data in RAM
- **Columnar data layout**: SoA for SIMD-friendly risk calculation
- **GPU acceleration**: Monte Carlo simulations on GPU (CUDA)
- **Streaming architecture**: Kafka for event sourcing, replay capability
- **Time-series DB**: InfluxDB/TimescaleDB for historical risk metrics

### Explanation:
This is asked at banks (Goldman, JPMorgan), hedge funds (Citadel, Two Sigma), and fintech. Key points:
- **Consistency vs Speed**: Risk can tolerate eventual consistency (last-look model)
- **Regulatory**: Must store all calculations for audit (MiFID II, Dodd-Frank)
- **Disaster recovery**: Active-active across data centers
- **Intraday vs End-of-day**: Different compute budgets and accuracy requirements

---

## Q6: Design a Plugin Architecture for a CAD Application

### Architecture:

```
+---------------------------------------------------+
|                 CAD Application                    |
|  +----------+  +----------+  +--------------+    |
|  | Core     |  | UI       |  | Document     |    |
|  | Geometry |  | Framework |  | Manager      |    |
|  +----------+  +----------+  +--------------+    |
|       |              |               |            |
|  +-------------------------------------------+    |
|  |              Plugin API (Stable ABI)       |    |
|  |  - IGeometryPlugin                         |    |
|  |  - IToolPlugin                             |    |
|  |  - IImportExportPlugin                     |    |
|  |  - IRendererPlugin                         |    |
|  +-------------------------------------------+    |
+---------------------------------------------------+
                      | Dynamic Loading (dlopen/LoadLibrary)
        +----------------------------+
        |             |              |
   +---------+  +----------+  +----------+
   | STL     |  | IGES     |  | Custom   |
   | Import  |  | Export   |  | Analysis |
   | Plugin  |  | Plugin   |  | Plugin   |
   +---------+  +----------+  +----------+
```

### Implementation:
```cpp
// Stable C ABI for plugin interface (avoids C++ ABI issues)
extern "C" {
    struct PluginInfo {
        const char* name;
        const char* version;
        const char* author;
    };

    // Every plugin DLL exports these functions
    typedef PluginInfo (*GetPluginInfoFn)();
    typedef void* (*CreatePluginFn)();
    typedef void (*DestroyPluginFn)(void*);
}

// C++ interface used internally
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual void initialize(class PluginContext& ctx) = 0;
    virtual void shutdown() = 0;
};

class IImportPlugin : public IPlugin {
public:
    virtual std::vector<std::string> supportedFormats() const = 0;
    virtual bool import(const std::string& filePath, Document& doc) = 0;
};

// Plugin loader
class PluginManager {
    struct LoadedPlugin {
        void* handle;           // DLL handle
        DestroyPluginFn destroy;
        IPlugin* instance;
    };
    std::vector<LoadedPlugin> plugins_;

public:
    void loadPlugin(const std::string& path) {
        #ifdef _WIN32
        void* handle = LoadLibraryA(path.c_str());
        #else
        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        #endif

        auto getInfo = (GetPluginInfoFn)getProcAddress(handle, "GetPluginInfo");
        auto create = (CreatePluginFn)getProcAddress(handle, "CreatePlugin");
        auto destroy = (DestroyPluginFn)getProcAddress(handle, "DestroyPlugin");

        auto info = getInfo();
        auto* plugin = static_cast<IPlugin*>(create());

        PluginContext ctx{/* provide app services */};
        plugin->initialize(ctx);

        plugins_.push_back({handle, destroy, plugin});
    }

    ~PluginManager() {
        for (auto& p : plugins_) {
            p.instance->shutdown();
            p.destroy(p.instance);
            #ifdef _WIN32
            FreeLibrary((HMODULE)p.handle);
            #else
            dlclose(p.handle);
            #endif
        }
    }
};
```

### Key Design Decisions:
1. **C ABI at boundary** ? avoids C++ name mangling and ABI compatibility issues across compilers
2. **Version negotiation** ? plugins declare required API version
3. **Sandboxing** ? plugins run in separate process for crash isolation (optional)
4. **Hot reload** ? unload/reload plugins without restarting app
5. **Dependency injection** ? PluginContext provides services (logging, UI, geometry kernel)

---

## Q7: Design YouTube/Netflix Video Streaming Architecture

### Architecture:

```
+----------+     +--------------+     +-------------+
|  Upload  +-----+ Transcoding  +-----+    CDN       |
|  Service |     | Pipeline     |     | (Edge Cache) |
+----------+     | (FFmpeg farm)|     +--------------+
                 +--------------+            |
                                     +---------------+
                                     |   Client App   |
                                     | Adaptive       |
                                     | Bitrate Player |
                                     +----------------+
```

### Key Components:

**1. Transcoding Pipeline:**
- Input: Original video (any format)
- Output: Multiple resolutions (240p, 480p, 720p, 1080p, 4K) � codecs (H.264, H.265, VP9, AV1)
- Each resolution split into 2-10 second segments (HLS/DASH)
- Distributed across GPU farm → parallelize per segment

**2. Adaptive Bitrate Streaming (ABR):**
```
Client measures bandwidth → requests appropriate quality
Segment 1: 1080p (good bandwidth)
Segment 2: 1080p
Segment 3: 720p  (bandwidth dropped)
Segment 4: 480p  (still dropping)
Segment 5: 720p  (recovering)
```

**3. CDN Strategy:**
- **Hot content** (new releases, trending): Cached at edge (500+ PoPs globally)
- **Long tail** (old/niche content): Origin fetch on demand, regional cache
- **Cache hit ratio target**: 95%+ for video segments

**4. Scalability:**
| Metric | Scale |
|--------|-------|
| Concurrent streams | 100M+ |
| Storage | Exabytes |
| Bandwidth | Terabits/sec |
| Encoding throughput | 1000s of hours/day |

### Explanation:
- **Why segments?** Enable seeking, ABR switching, parallel CDN distribution
- **Why multiple codecs|** Browser/device compatibility + newer codecs = smaller files
- **Pre-compute vs on-the-fly?** Pre-compute popular, on-the-fly for rare combinations

**DRM (Digital Rights Management) ? frequently asked follow-up:**
```
Content Protection Pipeline:
+----------+    +-----------+    +--------------+    +------------+
| Original  +----+ Encrypt   +----+ License      +----+ Client     |
| Content   |    | (AES-128) |    | Server       |    | Decrypts   |
|           |    | per-key   |    | (Widevine/   |    | in TEE/    |
|           |    | rotation  |    | FairPlay/    |    | CDM/HW     |
+----------+    +-----------+    | PlayReady)   |    +------------+
                                  +--------------+
```
| DRM System | Platform | Owner |
|------------|----------|-------|
| Widevine | Android, Chrome, Firefox | Google |
| FairPlay | iOS, Safari, Apple TV | Apple |
| PlayReady | Windows, Xbox, Smart TVs | Microsoft |

**Live Streaming addition:**
```
Live Source | Ingest Server | Real-time transcoder | Edge CDN | Viewer
                                    |
                           Low-latency HLS/DASH
                           (sub-3-second latency
                            via CMAF chunked transfer)
```
- **VOD vs Live**: VOD pre-encodes everything; Live must encode in real-time
- **Ultra-low latency**: WebRTC (sub-second) for interactive use cases

---

## Q8: Design a Distributed Cache System (like Redis/Memcached)

### Requirements:
- Key-value store with sub-millisecond reads
- Distributed across multiple nodes
- Support for TTL, eviction policies
- High availability and partition tolerance

### Architecture:
```
+-------------------------------------------------------------+
|                      CLIENT LIBRARY                          |
|  +----------+  +----------------+  +--------------------+  |
|  | Consistent|  | Connection     |  | Retry / Circuit   |  |
|  | Hashing   |  | Pool per node  |  | Breaker           |  |
|  +-----------+  +----------------+  +-------------------+  |
+-------------------------------------------------------------+
         |               |                       |
    +---------+    +----------+           +------------+
    | Node 1  |    | Node 2   |           | Node 3     |
    | (Hash   |    | (Hash    |           | (Hash      |
    |  0-120) |    | 121-240) |           | 241-360)   |
    |         |    |          |           |            |
    | Memory: |    | Memory:  |           | Memory:    |
    | HashMap |    | HashMap  |           | HashMap    |
    | + LRU   |    | + LRU    |           | + LRU      |
    | eviction|    | eviction |           | eviction   |
    |         |    |          |           |            |
    | Replica |    | Replica  |           | Replica    |
    | | Node 2|    | | Node 3 |           | | Node 1   |
    +---------+    +----------+           +------------+
```

### Core Design Decisions:

**1. Partitioning ? Consistent Hashing with Virtual Nodes:**
```cpp
class ConsistentHashRing {
    std::map<uint32_t, std::string> ring_;  // hash → node_id
    int virtualNodes_ = 150;                // Per physical node

public:
    void addNode(const std::string& nodeId) {
        for (int i = 0; i < virtualNodes_; ++i) {
            uint32_t hash = murmur3(nodeId + ":" + std::to_string(i));
            ring_[hash] = nodeId;
        }
    }

    std::string getNode(const std::string& key) const {
        uint32_t hash = murmur3(key);
        auto it = ring_.lower_bound(hash);
        if (it == ring_.end()) it = ring_.begin();
        return it->second;
    }

    void removeNode(const std::string& nodeId) {
        for (int i = 0; i < virtualNodes_; ++i)
            ring_.erase(murmur3(nodeId + ":" + std::to_string(i)));
    }
};
// Adding/removing a node only remaps ~1/N of keys (N = nodes)
```

**2. Eviction Policies:**
| Policy | Algorithm | Use Case |
|--------|-----------|----------|
| LRU | Doubly-linked list + HashMap | General purpose (Redis default) |
| LFU | Min-heap by frequency | Frequency-skewed workloads |
| TTL | Lazy expiry + periodic sweep | Time-bound caches |
| Random | Random eviction | Simple, surprisingly effective |
| ARC (Adaptive) | Two LRU lists (recent + frequent) | Self-tuning workloads |

**3. Replication & Consistency:**
```
Write path (primary ? replica):
  Client | Primary node | ACK to client | Async replicate to replica
  
  Trade-off: Fast writes (eventual consistency) vs. Strong consistency (wait for replica ACK)
  
  Redis approach: Async replication, accept data loss on failover
  Production: Choose based on use case (session cache = eventual, rate limiter = strong)
```

**4. Cache-aside vs Read-through vs Write-through:**
```
Cache-Aside (most common):
  Read: App checks cache | miss | read DB | write to cache | return
  Write: App writes DB → invalidate cache

Read-Through:
  Read: App asks cache → cache loads from DB on miss → return
  
Write-Through:
  Write: App writes cache → cache synchronously writes DB
  
Write-Behind (Write-Back):
  Write: App writes cache → cache async writes DB (batched)
```

### Scalability:
| Concern | Solution |
|---------|----------|
| Hot keys (celebrity problem) | Local client-side cache + replicated hot key copies |
| Node failure | Replica promotion, consistent hashing remaps minimal keys |
| Cache stampede | Distributed lock on cache miss (only 1 thread fetches) |
| Memory pressure | TTL + eviction + monitoring |
| Network partition | CAP trade-off: Available but potentially stale (AP) |

---

## Q9: Design a Notification System (Push, Email, SMS, In-App)

### Architecture:
```
+-----------------------------------------------------------------+
|                    NOTIFICATION SERVICE                          |
|                                                                  |
|  +----------+    +--------------+    +------------------------+ |
|  | REST API  +----+ Priority     +----+ Message Queue          | |
|  | /notify   |    | Router       |    | (Kafka / RabbitMQ)     | |
|  |           |    |              |    |                        | |
|  | gRPC API  |    | HIGH | fast  |    | Topic: push.high      | |
|  |           |    | NORMAL | std |    | Topic: push.normal     | |
|  |           |    | LOW | batch  |    | Topic: email.batch     | |
|  +----------+    +--------------+    | Topic: sms.critical    | |
|                                      +------------------------+ |
|                                                 |                |
|  +-------------------------------------------------------------+ |
|  |              DELIVERY WORKERS                 |              | |
|  |                                               |              | |
|  |  +----------+  +----------+  +----------+  +-----------+  | |
|  |  | Push     |  | Email    |  | SMS      |  | In-App    |  | |
|  |  | Worker   |  | Worker   |  | Worker   |  | Worker    |  | |
|  |  |          |  |          |  |          |  |           |  | |
|  |  | FCM/APNS |  | SES/     |  | Twilio/  |  | WebSocket |  | |
|  |  |          |  | SendGrid |  | Vonage   |  | SSE       |  | |
|  |  +----------+  +----------+  +----------+  +-----------+  | |
|  +------------------------------------------------------------+ |
|                                                                  |
|  +-------------------------------------------------------------+ |
|  | SUPPORTING SERVICES                                         | |
|  | | User Preferences DB (channels, DND hours, frequency cap)  | |
|  | | Template Engine (localization, personalization)            | |
|  | | Rate Limiter (per user, per channel)                      | |
|  | | Delivery Status Tracker (sent, delivered, read, failed)   | |
|  | | Analytics (open rates, click rates, engagement)           | |
|  | | Deduplication (idempotency key | prevent double-send)     | |
|  +-------------------------------------------------------------+ |
+-----------------------------------------------------------------+
```

### Key Design Decisions:
1. **Channel abstraction**: Each delivery channel is a separate worker type → Strategy pattern
2. **Priority routing**: Critical alerts (fraud, OTP) bypass batching → separate high-priority queue
3. **User preference engine**: Users control which channels, DND hours, frequency caps
4. **Deduplication**: Idempotency keys prevent duplicate sends on retry
5. **Template engine**: Same notification → different format per channel

**Delivery Guarantees:**
```
At-most-once:  Send and forget (low-priority marketing)
At-least-once: Retry on failure with idempotency (transactional alerts)
Exactly-once:  Idempotency key + deduplication store (OTP, payment confirmations)
```

**Rate Limiting (prevent spam):**
```
Per-user limits:
  - Push: max 10/hour
  - Email: max 5/day
  - SMS: max 3/day (cost-sensitive)
  
Global limits:
  - FCM: 240 messages/min per device
  - SES: 14 emails/sec (configurable)
  - Twilio: account-specific limits
```

### Explanation:
This system design tests understanding of: asynchronous processing (message queues), multi-channel delivery (push, email, SMS), user preferences, rate limiting, and delivery tracking. Key insight: notifications seem simple but have complex edge cases (timezone-aware DND, frequency caps, channel fallback on failure).

---

## Q10: Design a Distributed Task/Workflow Engine (like Airflow/Temporal)

### Architecture:
```
+-----------------------------------------------------------------+
|                   WORKFLOW ENGINE                                |
|                                                                  |
|  +--------------+    +---------------+    +------------------+  |
|  | API Server   |    | Scheduler     |    | History Service  |  |
|  |              |    |               |    |                  |  |
|  | - Submit     |    | - Timer-based |    | - Event sourcing |  |
|  |   workflow   |    | - Cron        |    | - Replay/debug   |  |
|  | - Query      |    | - Trigger     |    | - Audit trail    |  |
|  |   status     |    |               |    |                  |  |
|  +--------------+    +---------------+    +------------------+  |
|         |                    |                     |             |
|  +-------------------------------------------------------------+ |
|  |                    TASK QUEUE (Kafka/Redis Streams)          | |
|  |  Partitioned by workflow_id for ordering                    | |
|  +-------------------------------------------------------------+ |
|         |              |              |              |            |
|  +------------+ +------------+ +------------+ +------------+   |
|  | Worker 1   | | Worker 2   | | Worker 3   | | Worker N   |   |
|  | (Python)   | | (C++)      | | (Java)     | | (Go)       |   |
|  | ML tasks   | | Compute    | | ETL tasks  | | Network    |   |
|  +------------+ +------------+ +------------+ +------------+   |
|                                                                  |
|  +-------------------------------------------------------------+ |
|  | WORKFLOW STATE MACHINE                                      | |
|  |                                                             | |
|  |  CREATED | RUNNING | COMPLETED                              | |
|  |                |         |                                  | |
|  |            TASK_FAILED | RETRYING (with backoff)            | |
|  |                |                                            | |
|  |            WORKFLOW_FAILED (max retries exceeded)           | |
|  |                |                                            | |
|  |            COMPENSATING | COMPENSATED (saga rollback)       | |
|  +-------------------------------------------------------------+ |
+-----------------------------------------------------------------+
```

### Core Concepts:

**Workflow Definition (DAG):**
```cpp
// Workflow as a DAG of tasks
struct TaskDef {
    std::string id;
    std::string type;                    // "http_call", "ml_inference", "etl"
    std::map<std::string, std::string> params;
    std::vector<std::string> dependsOn;  // DAG edges
    int maxRetries = 3;
    std::chrono::seconds retryDelay{30};
    std::chrono::seconds timeout{300};
};

struct WorkflowDef {
    std::string name;
    std::string version;
    std::vector<TaskDef> tasks;
    std::string schedule;  // Cron expression: "0 */6 * * *"
};
```

**Saga Pattern for distributed transactions:**
```
Payment Workflow (Saga):
  1. Reserve Inventory  --(success)--> 2. Charge Payment  --(success)--> 3. Ship Order
           |                                    |
       (compensate)                         (compensate)
           |                                    |
    Release Inventory  -->(failure)+--+ Refund Payment
```

**Key Design Decisions:**
1. **Event sourcing**: Every state change is an immutable event → full replay capability
2. **Idempotent tasks**: Every task can be safely retried (network failures are common)
3. **Language-agnostic workers**: Workers communicate via queue protocol, not language bindings
4. **Saga compensation**: For distributed transactions, compensating actions undo partial work
5. **Deterministic replay**: Workflow execution is deterministic → mocking side effects enables debugging

### Explanation:
Workflow engines are the backbone of modern data platforms (Airflow, Temporal, Step Functions). Key interview points: DAG-based scheduling, exactly-once execution via idempotency, saga pattern for distributed transactions, event sourcing for audit trails. This combines many system design concepts: distributed queues, state machines, retry strategies, and consistency models.

---

## Q11: Explain the Raft consensus algorithm. How would you implement leader election and log replication in C++?

### Answer:

**Why Consensus Matters:**
```
Problem: N servers must agree on a sequence of commands
         even if some servers crash or network partitions occur.

Real uses: 
  - etcd (Kubernetes control plane) ? Raft
  - ZooKeeper (distributed coordination) ? ZAB (Paxos variant)
  - CockroachDB ? Raft per range
  - iCluster (IBM i HA) ? Custom consensus for switchover
```

**Raft Core Concepts:**
```
+-------------------+     +-------------------+     +-------------------+
|     LEADER        |     |    FOLLOWER       |     |    FOLLOWER       |
|  (handles writes) |     |  (replicates)     |     |  (replicates)     |
|                   |     |                   |     |                   |
| Term: 3           |     | Term: 3           |     | Term: 3           |
| Log: [1,2,3,4,5]  |?????| Log: [1,2,3,4,5]  |     | Log: [1,2,3,4]    |
|                   |     |                   |     |   (catching up)   |
+-------------------+     +-------------------+     +-------------------+
        |                                                    ?
        ??????????????????????????????????????????????????????
                        AppendEntries RPC

States: FOLLOWER ? CANDIDATE ? LEADER
Transitions:
  Follower ? Candidate:  Election timeout expires (no heartbeat from leader)
  Candidate ? Leader:    Receives majority votes
  Candidate ? Follower:  Discovers leader with higher term
  Leader ? Follower:     Discovers higher term
```

**C++ Implementation Skeleton:**
```cpp
enum class State { FOLLOWER, CANDIDATE, LEADER };

struct LogEntry {
    uint64_t term;
    uint64_t index;
    std::string command;  // Serialized state machine command
};

class RaftNode {
    // Persistent state (must survive crashes ? write to disk before responding)
    uint64_t currentTerm_ = 0;
    std::optional<NodeId> votedFor_;
    std::vector<LogEntry> log_;
    
    // Volatile state
    State state_ = State::FOLLOWER;
    uint64_t commitIndex_ = 0;      // Highest log entry known to be committed
    uint64_t lastApplied_ = 0;      // Highest log entry applied to state machine
    
    // Leader-only state
    std::unordered_map<NodeId, uint64_t> nextIndex_;   // For each follower
    std::unordered_map<NodeId, uint64_t> matchIndex_;  // Highest replicated
    
    // Timers
    std::chrono::milliseconds electionTimeout_;   // 150-300ms (randomized!)
    std::chrono::milliseconds heartbeatInterval_; // 50ms
    
public:
    // === Leader Election ===
    void onElectionTimeout() {
        currentTerm_++;
        state_ = State::CANDIDATE;
        votedFor_ = myId_;
        int votesReceived = 1;  // Vote for self
        
        // Request votes from all other nodes
        RequestVoteRPC request{
            .term = currentTerm_,
            .candidateId = myId_,
            .lastLogIndex = log_.size(),
            .lastLogTerm = log_.empty() ? 0 : log_.back().term,
        };
        
        for (auto& peer : peers_) {
            auto response = peer.requestVote(request);
            if (response.voteGranted) {
                votesReceived++;
                if (votesReceived > peers_.size() / 2) {
                    becomeLeader();
                    return;
                }
            }
            if (response.term > currentTerm_) {
                stepDown(response.term);
                return;
            }
        }
    }
    
    // === Log Replication ===
    void appendEntry(const std::string& command) {
        assert(state_ == State::LEADER);
        
        log_.push_back({.term = currentTerm_, 
                        .index = log_.size() + 1, 
                        .command = command});
        
        // Replicate to all followers
        for (auto& [peerId, peer] : peers_) {
            replicateTo(peerId);
        }
    }
    
    void replicateTo(NodeId peer) {
        uint64_t prevIndex = nextIndex_[peer] - 1;
        AppendEntriesRPC request{
            .term = currentTerm_,
            .leaderId = myId_,
            .prevLogIndex = prevIndex,
            .prevLogTerm = prevIndex > 0 ? log_[prevIndex-1].term : 0,
            .entries = getEntriesFrom(nextIndex_[peer]),
            .leaderCommit = commitIndex_,
        };
        
        auto response = sendRPC(peer, request);
        if (response.success) {
            matchIndex_[peer] = prevIndex + request.entries.size();
            nextIndex_[peer] = matchIndex_[peer] + 1;
            advanceCommitIndex();
        } else {
            // Log inconsistency ? decrement and retry
            nextIndex_[peer]--;
            replicateTo(peer);  // Retry with earlier entry
        }
    }
    
    void advanceCommitIndex() {
        // Find N such that majority of matchIndex[i] >= N
        for (uint64_t n = commitIndex_ + 1; n <= log_.size(); n++) {
            if (log_[n-1].term != currentTerm_) continue;  // Only commit current term
            
            int count = 1;  // Leader has it
            for (auto& [_, idx] : matchIndex_) {
                if (idx >= n) count++;
            }
            if (count > (peers_.size() + 1) / 2) {
                commitIndex_ = n;
            }
        }
    }
};
```

**Split-Brain Prevention:**
```
Scenario: Network partition splits cluster [A,B] | [C,D,E]
  - [A,B] side: A was leader, but can't get majority ? stops accepting writes
  - [C,D,E] side: Election timeout ? C becomes leader (has majority of 3/5)
  - Healing: A discovers higher term from C ? steps down
  - Result: NO split-brain, NO conflicting writes

Key safety property: Only ONE leader per term (guaranteed by majority vote)
```

**Raft vs Paxos vs ZAB:**
| Feature | Raft | Paxos | ZAB |
|---------|------|-------|-----|
| Understandability | High (designed for clarity) | Low (academic) | Medium |
| Leader election | Integrated | Separate | Integrated |
| Log compaction | Snapshots | Not specified | Snapshots |
| Membership changes | Joint consensus | Not specified | Reconfiguration |
| Production use | etcd, CockroachDB, TiKV | Chubby (Google) | ZooKeeper |

### Explanation:
Raft is THE consensus algorithm you must know for senior interviews. It's asked at Google (etcd), Meta (distributed KV), and any company building distributed systems. Key insight: Raft sacrifices some theoretical elegance for understandability ? the paper is 18 pages vs Paxos's 30+. At 10+ years, you should be able to sketch leader election, log replication, and explain how split-brain is prevented.

---

## Q12: Design an observability platform for a C++ microservices system. Cover metrics, tracing, and logging.

### Answer:

**The Three Pillars:**
```
+------------------------------------------------------------------+
|                    OBSERVABILITY PLATFORM                         |
|                                                                  |
|  +----------+    +-------------+    +------------------+         |
|  | METRICS  |    | TRACES      |    | STRUCTURED LOGS  |         |
|  | (numbers)|    | (requests)  |    | (events)         |         |
|  +----------+    +-------------+    +------------------+         |
|       |                |                    |                    |
|  Prometheus      OpenTelemetry         Elasticsearch            |
|  + Grafana       + Jaeger/Tempo        + Kibana/Loki           |
+------------------------------------------------------------------+
        ?                ?                    ?
   "What's broken?"  "Where's it slow?"  "Why did it happen?"
```

**C++ Integration (OpenTelemetry SDK):**
```cpp
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>

// Initialize once at startup
void initObservability() {
    // Traces ? OTLP exporter to Jaeger/Tempo
    auto exporter = otlp::OtlpGrpcExporterFactory::Create();
    auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
    auto provider = trace_sdk::TracerProviderFactory::Create(std::move(processor));
    trace::Provider::SetTracerProvider(std::move(provider));
    
    // Metrics ? Prometheus endpoint
    auto prometheus_exporter = metrics_sdk::PrometheusExporterFactory::Create();
    auto meter_provider = metrics_sdk::MeterProviderFactory::Create();
    metrics::Provider::SetMeterProvider(std::move(meter_provider));
}

// Tracing: wrap every external call
class OrderService {
    opentelemetry::nostd::shared_ptr<trace::Tracer> tracer_;
    
public:
    void processOrder(const Order& order) {
        auto span = tracer_->StartSpan("OrderService::processOrder", {
            {"order.id", order.id},
            {"order.type", order.type},
            {"order.size", order.quantity},
        });
        auto scope = trace::Scope(span);
        
        try {
            validateOrder(order);     // Child span auto-created
            checkRisk(order);         // Child span
            submitToExchange(order);  // Child span + external call
            span->SetStatus(trace::StatusCode::kOk);
        } catch (const std::exception& e) {
            span->SetStatus(trace::StatusCode::kError, e.what());
            span->RecordException(e);
            throw;
        }
    }
};

// Metrics: counters, histograms, gauges
class MetricsCollector {
    metrics::Counter<uint64_t>* requestCounter_;
    metrics::Histogram<double>* latencyHistogram_;
    metrics::UpDownCounter<int64_t>* activeConnections_;
    
public:
    void recordRequest(const std::string& method, int statusCode, double latencyMs) {
        requestCounter_->Add(1, {{"method", method}, {"status", std::to_string(statusCode)}});
        latencyHistogram_->Record(latencyMs, {{"method", method}});
    }
};
```

**Structured Logging (Zero-overhead when disabled):**
```cpp
// spdlog + JSON formatter for machine-readable logs
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

// Context propagation: trace_id in every log line
#define LOG_WITH_CONTEXT(level, ...) \
    spdlog::level(fmt::format("[trace_id={}] " __VA_ARGS__, \
        getCurrentTraceId()))

// SLO monitoring: track error budgets
class SLOMonitor {
    std::atomic<uint64_t> totalRequests_{0};
    std::atomic<uint64_t> failedRequests_{0};
    
    static constexpr double TARGET_AVAILABILITY = 0.999;  // 99.9%
    
public:
    bool isWithinErrorBudget() const {
        double errorRate = static_cast<double>(failedRequests_) / totalRequests_;
        return errorRate <= (1.0 - TARGET_AVAILABILITY);
    }
    
    double remainingErrorBudget() const {
        double allowed = totalRequests_ * (1.0 - TARGET_AVAILABILITY);
        return (allowed - failedRequests_) / allowed;
    }
};
```

**Alert Strategy (SLO-based):**
```
SLO: 99.9% of requests complete in < 200ms

Multi-window alerting:
  - 5min burn rate > 14.4x ? Page (will exhaust budget in 1 hour)
  - 1hr burn rate > 6x    ? Page (will exhaust budget in 6 hours)
  - 6hr burn rate > 1x    ? Ticket (trending toward budget exhaustion)
```

### Explanation:
Observability is a critical gap in many C++ engineer interviews. Modern systems are too complex to debug with printf. At principal engineer level, you're expected to design observability INTO the system from day one ? not bolt it on later. Key concepts: distributed tracing (correlate requests across services), SLO-based alerting (not threshold-based), structured logging (machine-parseable), and error budgets (balance reliability vs velocity).

---

# ?? INTERVIEWER GUIDE ? Set 5 Scoring & Evaluation

---

## System Design Interview Framework (How Interviewers Score)

| Phase | Time | What Senior Interviewers Assess |
|-------|------|-------------------------------|
| **Requirements** | 5 min | Do they ask clarifying questions? Do they identify scale requirements? |
| **High-Level Design** | 10 min | Can they draw the architecture on a whiteboard? Correct component identification? |
| **Deep Dive** | 15 min | Can they go deep on 1-2 components? Data model? Algorithm? |
| **Trade-offs** | 10 min | Do they discuss alternatives? CAP theorem? Cost vs latency? |
| **Scaling/Fault Tolerance** | 5 min | How does it handle 10x growth? What fails and how to recover? |

## ?? Green Flags for HLD

```
? Starts with functional AND non-functional requirements
? Calculates back-of-envelope (QPS, storage, bandwidth) BEFORE designing
? Draws clear diagrams with data flow arrows
? Identifies the hardest sub-problem and focuses there
? Discusses CAP theorem trade-offs for distributed components
? Mentions specific technologies with justification (not just buzzwords)
? Handles failure scenarios proactively ("What if this service goes down?")
? Discusses data partitioning strategy (hash vs range)
? Mentions caching strategy (read-through, write-behind, invalidation)
? Can discuss consistency models (strong, eventual, causal)
```

## ?? Red Flags for HLD

```
? Jumps to solution without understanding requirements
? Can't calculate basic numbers (QPS, storage for 1B users)
? Names technologies without understanding why ("just use Kafka")
? No consideration of failure modes
? Single point of failure in design
? Can't explain data model or schema
? Doesn't discuss monitoring or alerting
? Over-engineers for the scale (uses Kafka for 100 requests/day)
? Under-engineers for the scale (SQLite for 1M concurrent users)
```

## Back-of-Envelope Calculations Cheat Sheet

```
READ LATENCY:
  L1 cache:                 0.5 ns
  L2 cache:                 7 ns
  Main memory:              100 ns
  SSD random read:          150 �s
  HDD random read:          10 ms
  Network round trip (DC):  500 �s
  Network round trip (cross-continent): 150 ms

THROUGHPUT:
  SSD sequential read:      1 GB/s
  Network (10 Gbps):        1 GB/s
  HDD sequential read:      100 MB/s
  Compress 1KB (Snappy):    3 �s

STORAGE RULES:
  1 million = 10^6
  1 billion = 10^9
  1 trillion = 10^12
  ASCII char = 1 byte
  UUID = 128 bits = 16 bytes
  Timestamp = 8 bytes
  IPv4 = 4 bytes, IPv6 = 16 bytes

DAILY ESTIMATES (for 100M DAU):
  100M DAU � 10 actions/day = 1B actions/day
  1B actions/day � 86400 sec = ~12K QPS
  Peak = 3� average = ~36K QPS
```

## CAP Theorem Quick Reference

```
CP Systems (Consistency + Partition tolerance):
  ? HBase, MongoDB (with majority reads), ZooKeeper, etcd
  ? Choose when: Financial transactions, inventory, leader election

AP Systems (Availability + Partition tolerance):  
  ? Cassandra, DynamoDB, CouchDB, DNS
  ? Choose when: Social feeds, analytics, session stores

CA Systems (theoretical ? no network partition):
  ? Traditional RDBMS (single node PostgreSQL)
  ? Not possible in distributed systems (partitions WILL happen)
```

---
