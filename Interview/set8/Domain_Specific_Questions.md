# Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each?

### Answer:
```
B-Rep (Boundary Representation):
 
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    
| Solid | Shells | Faces | Edges  |
|                    | Vertices    |
|                                  |
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
 
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology?

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry | regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch | slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem -> when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD?

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box | 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 | references "F2"
Step 3: User modifies box dimensions | faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face | model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD -> every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved | user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep?

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate | BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input -> same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)? How does it differ from inheritance-based game objects?

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system | only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system | only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system | only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"? Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games? Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++?

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1?s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  |                                    |
  --> Logon (A)  
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
       |  (SeqNum=1)
  |                                    |
  |    
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
     Logon (A)  
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
      (SeqNum=1)
  |                                    |
  --> NewOrderSingle (D)  
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
       |  (SeqNum=2)
  |                                    |
  |    
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
     ExecutionReport (8)  
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
      (New, SeqNum=2)
  |    
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
     ExecutionReport (8)  
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
      (Fill, SeqNum=3)
  |                                    |
  |   ... Heartbeat (0) every 30s ...  |
  |                                    |
  --> Logout (5)  
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
       |
  |    
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
     Logout (5)  
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer | no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book? Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                | incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases?

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header | ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation | can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 | no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability -> critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn -> provide migration path -> maintain for N releases -> remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 ? C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL | nullptr
- typedef | using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers | smart pointers (unique_ptr/shared_ptr)
- Manual new/delete | make_unique/make_shared
- C arrays | std::array/std::vector
- C strings | std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture -> how is it fundamentally different from Linux/Windows?

### Answer:

```
IBM i Architecture (Single-Level Storage):
 
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    
|  Applications (RPG, COBOL, C, CL, SQL)            |
 
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage | no explicit file I/O     |
 
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i | integrated)        |
 
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    
|  POWER Hardware                                    |
 
        $match = # Set 8: Domain-Specific Questions (CAD, Gaming, Finance, IBM/Enterprise)
## Senior-Level Interview Questions (10 Years Experience)

---

# Part A: CAD Domain Questions

---

## Q1: Explain B-Rep (Boundary Representation) vs CSG (Constructive Solid Geometry). When would you use each|

### Answer:
```
B-Rep (Boundary Representation):
+----------------------------------+
| Solid -> Shells -> Faces -> Edges  |
-                    ? Vertices    ?
-                                  ?
| Face = Surface + trim curves     |
| Edge = Intersection of 2 faces  |
| Vertex = Intersection of edges  |
+----------------------------------+

CSG (Constructive Solid Geometry):
         UNION
        /     \
   SUBTRACT    Cylinder
   /      \
  Box    Sphere
```

| Aspect | B-Rep | CSG |
|--------|-------|-----|
| Storage | Explicit geometry (vertices, edges, faces) | Binary tree of operations |
| Editing | Directly modify topology | Replay operations |
| Rendering | Tessellate faces directly | Convert to B-Rep first |
| Boolean ops | Complex (topology changes) | Trivial (add tree node) |
| Precision | Exact geometry | Exact (parametric) |
| File size | Larger | Smaller (just operations) |
| Query (volume, area) | Easy from geometry | Need evaluation |

### Explanation:
- **B-Rep** is the standard in production CAD (CATIA, SolidWorks, NX): explicit topology enables direct queries, meshing, FEA
- **CSG** is used for procedural modeling, OpenSCAD, and as an undo/history mechanism
- **Kernels**: Parasolid (Siemens), ACIS (Spatial), OpenCASCADE (open source)
- **Interview insight**: Most CAD systems store CSG history but work on B-Rep for visualization and analysis

---

## Q2: How would you implement undo/redo for a CAD modeling operation that changes topology|

### Answer:
```cpp
// Approach 1: Command pattern with inverse operations
class BooleanSubtractCommand : public ICommand {
    Document& doc_;
    ShapeId targetId_;
    ShapeId toolId_;
    BRepState savedState_;  // Snapshot before operation

public:
    void execute() override {
        savedState_ = doc_.getShape(targetId_).saveState();
        doc_.booleanSubtract(targetId_, toolId_);
    }

    void undo() override {
        doc_.getShape(targetId_).restoreState(savedState_);
        // Restore the tool shape that was consumed
    }
};

// Approach 2: Parametric history (preferred in modern CAD)
class FeatureHistory {
    struct Feature {
        std::string type;  // "Extrude", "Fillet", "BooleanCut"
        Parameters params;
        // No stored geometry -> regenerated on demand
    };

    std::vector<Feature> features_;
    BRepShape currentShape_;

public:
    void addFeature(Feature f) {
        features_.push_back(std::move(f));
        regenerateFrom(features_.size() - 1);
    }

    void undo() {
        features_.pop_back();
        regenerateAll();  // Rebuild from scratch -> slow but correct
    }

    void modifyFeature(int index, Parameters newParams) {
        features_[index].params = newParams;
        regenerateFrom(index);  // Only rebuild from modified feature onward
    }

private:
    void regenerateFrom(int startIndex) {
        if (startIndex == 0) currentShape_ = BRepShape();
        // Replay features from startIndex to end
        for (int i = startIndex; i < features_.size(); ++i)
            applyFeature(features_[i]);
    }
};
```

### Explanation:
- **Approach 1** (snapshot): Fast undo but high memory usage (each operation stores full geometry)
- **Approach 2** (parametric): Lower memory, supports feature editing, but slower undo (must regenerate)
- **Real CAD systems use hybrid**: Cache intermediate states at checkpoints + parametric replay
- **Key challenge**: Topological naming problem | when a feature is modified, downstream features must find the correct faces/edges to reference

---

## Q3: What is the Topological Naming Problem in CAD|

### Answer:
The topological naming problem occurs when a feature references a face/edge by index, and a preceding feature modification changes the topology, invalidating those indices.

```
Step 1: Create a box -> 6 faces (F0-F5)
Step 2: Fillet edge between F2 and F3 ? references "F2"
Step 3: User modifies box dimensions -> faces are regenerated
         F2 might now be a DIFFERENT face!
         Fillet applies to wrong face -> model breaks
```

**Solutions:**
1. **Persistent naming**: Assign stable IDs to geometric entities based on how they were created (e.g., "face created by extrusion of sketch edge E3")
2. **Name encoding**: Encode generation history in the name (like DNA)
3. **Fuzzy matching**: If exact match fails, use geometric proximity to find best candidate
4. **User intervention**: Flag ambiguous references for user resolution

**This is an unsolved problem** in general CAD | every major kernel handles it differently. It's a great topic to demonstrate deep domain knowledge.

**Persistent Naming implementation sketch:**
```cpp
// Each topological entity gets a stable name based on creation history
struct TopoName {
    std::string generatorFeature;  // "Extrude_1"
    std::string generatorEntity;   // "sketch_edge_3"  
    std::string operation;          // "face_from_extrusion"
    int index;                      // Disambiguation index
    
    // Name = "Extrude_1:face_from_extrusion:sketch_edge_3:0"
    std::string encode() const {
        return generatorFeature + ":" + operation + ":" + generatorEntity + ":" 
               + std::to_string(index);
    }
};

class TopoNameRegistry {
    std::unordered_map<std::string, TopoName> nameMap_;
    
public:
    // After regeneration, try to match old names to new topology
    void rematch(const std::vector<TopoEntity>& newEntities) {
        for (auto& entity : newEntities) {
            // Step 1: Try exact match by creation history
            auto exactMatch = findByCreationHistory(entity);
            if (exactMatch) { bind(entity, *exactMatch); continue; }
            
            // Step 2: Fuzzy match by geometric proximity
            auto fuzzyMatch = findByProximity(entity, 0.01 /*tolerance*/);
            if (fuzzyMatch) { bind(entity, *fuzzyMatch); continue; }
            
            // Step 3: Flag as unresolved -> user must disambiguate
            flagAmbiguous(entity);
        }
    }
};
```

**How major CAD kernels handle it:**
| Kernel | Approach | Stability |
|--------|----------|-----------|
| Parasolid (Siemens) | Persistent IDs + tracking | Very good |
| ACIS (Spatial) | Entity tracking with face maps | Good |
| OpenCascade | Shape evolution history | Moderate |
| SolidWorks | Face-ID reuse with heuristics | Good with quirks |
| Onshape | Part Studio deterministic regen | Excellent |

---

# Part B: Gaming Domain Questions

---

## Q4: Explain the Game Loop architecture. What is fixed vs variable timestep|

### Answer:
```cpp
// Variable timestep (simple but problematic)
void variableTimestepLoop() {
    auto lastTime = Clock::now();
    while (running) {
        auto now = Clock::now();
        float dt = duration(now - lastTime);  // Variable!
        lastTime = now;

        processInput();
        update(dt);     // Physics depends on frame rate -> BAD
        render();
    }
}
// Problem: Physics behaves differently at 30fps vs 120fps
// A jump at 30fps goes higher than at 120fps!

// Fixed timestep with interpolation (industry standard)
void fixedTimestepLoop() {
    const float FIXED_DT = 1.0f / 60.0f;  // 60 Hz physics
    float accumulator = 0.0f;
    auto lastTime = Clock::now();
    State previousState, currentState;

    while (running) {
        auto now = Clock::now();
        float frameTime = duration(now - lastTime);
        lastTime = now;

        // Cap frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.25f);
        accumulator += frameTime;

        processInput();

        // Physics runs at fixed rate
        while (accumulator >= FIXED_DT) {
            previousState = currentState;
            updatePhysics(currentState, FIXED_DT);  // Deterministic!
            accumulator -= FIXED_DT;
        }

        // Interpolate for smooth rendering
        float alpha = accumulator / FIXED_DT;
        State renderState = interpolate(previousState, currentState, alpha);
        render(renderState);
    }
}
```

### Explanation:
| Approach | Physics Determinism | Rendering Smoothness | Complexity |
|----------|-------------------|---------------------|------------|
| Variable dt | Non-deterministic | Smooth | Simple |
| Fixed dt (no interp) | Deterministic | Stuttery | Medium |
| Fixed dt + interpolation | Deterministic | Smooth | Complex |

**Why determinism matters:**
- Multiplayer synchronization (same input | same result on all clients)
- Replay systems
- Debugging (reproducible bugs)

---

## Q5: What is an Entity Component System (ECS)| How does it differ from inheritance-based game objects|

### Answer:
See Set 3, Q10 for full implementation. Additional gaming-specific context:

```cpp
// Real-world ECS usage (EnTT library style)
#include <entt/entt.hpp>

struct Transform { float x, y, z; float rotX, rotY, rotZ; };
struct RigidBody { float mass; float vx, vy, vz; };
struct MeshRenderer { int meshId; int materialId; };
struct AIController { int behaviorTreeId; float aggroRange; };
struct Health { int current; int max; };

void gameUpdate(entt::registry& registry, float dt) {
    // Physics system -> only processes entities with Transform + RigidBody
    auto physicsView = registry.view<Transform, RigidBody>();
    physicsView.each([dt](Transform& t, RigidBody& rb) {
        t.x += rb.vx * dt;
        t.y += rb.vy * dt;
        t.z += rb.vz * dt;
    });

    // Render system -> only processes entities with Transform + MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    renderView.each([](const Transform& t, const MeshRenderer& mr) {
        drawMesh(mr.meshId, mr.materialId, t);
    });

    // AI system -> only processes entities with AIController + Transform
    auto aiView = registry.view<AIController, Transform, Health>();
    aiView.each([](AIController& ai, Transform& t, Health& hp) {
        if (hp.current <= 0) return;  // Dead, skip AI
        updateBehaviorTree(ai.behaviorTreeId, t);
    });
}

// Creating entities is just composing components
entt::entity createEnemy(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<Transform>(e, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
    reg.emplace<RigidBody>(e, 80.f, 0.f, 0.f, 0.f);
    reg.emplace<MeshRenderer>(e, ENEMY_MESH, ENEMY_MATERIAL);
    reg.emplace<AIController>(e, PATROL_BEHAVIOR, 15.f);
    reg.emplace<Health>(e, 100, 100);
    return e;
}
```

### Explanation:
**Why ECS wins at scale:**
- **1M entities**: OOP = cache misses everywhere; ECS = contiguous array iteration
- **Composition over inheritance**: Want a "damageable decoration"| Just add Health component to prop entity
- **Parallelism**: Systems are embarrassingly parallel (physics doesn't touch rendering data)
- **Used by**: Unity DOTS, Unreal Mass, Overwatch, Minecraft Bedrock

---

## Q6: How does spatial partitioning work in games| Compare Grid, Quadtree, Octree, BVH.

### Answer:
| Structure | Dimensions | Best For | Build | Query |
|-----------|-----------|----------|-------|-------|
| Uniform Grid | 2D/3D | Even distribution | O(n) | O(1) per cell |
| Quadtree | 2D | Sparse 2D (RTS, top-down) | O(n log n) | O(log n) |
| Octree | 3D | Sparse 3D (open world) | O(n log n) | O(log n) |
| BVH | 2D/3D | Dynamic objects, ray tracing | O(n log n) | O(log n) |
| BSP Tree | 3D | Static indoor levels | O(n log n) | O(log n) |

```cpp
// Simple spatial hash grid for 2D collision detection
class SpatialGrid {
    float cellSize_;
    std::unordered_map<uint64_t, std::vector<Entity>> cells_;

    uint64_t hash(int x, int y) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

public:
    SpatialGrid(float cellSize) : cellSize_(cellSize) {}

    void insert(Entity e, float x, float y) {
        int cx = static_cast<int>(std::floor(x / cellSize_));
        int cy = static_cast<int>(std::floor(y / cellSize_));
        cells_[hash(cx, cy)].push_back(e);
    }

    std::vector<Entity> queryRadius(float x, float y, float radius) {
        std::vector<Entity> result;
        int minCx = static_cast<int>(std::floor((x - radius) / cellSize_));
        int maxCx = static_cast<int>(std::floor((x + radius) / cellSize_));
        int minCy = static_cast<int>(std::floor((y - radius) / cellSize_));
        int maxCy = static_cast<int>(std::floor((y + radius) / cellSize_));

        for (int cx = minCx; cx <= maxCx; ++cx)
            for (int cy = minCy; cy <= maxCy; ++cy) {
                auto it = cells_.find(hash(cx, cy));
                if (it != cells_.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        return result;
    }

    void clear() { cells_.clear(); }  // Call each frame
};
```

### Explanation:
- **Grid**: Simplest, best when entities are evenly distributed (particle systems)
- **BVH (Bounding Volume Hierarchy)**: Best for ray tracing (GPU RT cores use this), dynamic scenes
- **Octree**: Best for 3D worlds with varying density (CAD assemblies, open-world games)

---

# Part C: Finance Domain Questions

---

## Q7: Explain the FIX protocol. How would you implement a FIX engine in C++|

### Answer:
```
FIX Message Format:
8=FIX.4.4|9=176|35=D|49=CLIENT|56=EXCHANGE|34=2|52=20250615-10:30:00|
11=ORDER123|21=1|55=AAPL|54=1|60=20250615-10:30:00|38=100|40=2|44=150.50|10=128|

Tag=Value pairs separated by SOH (0x01):
8  = BeginString (protocol version)
35 = MsgType (D = New Order Single)
49 = SenderCompID
55 = Symbol
54 = Side (1=Buy, 2=Sell)
38 = OrderQty
44 = Price
40 = OrdType (2=Limit)
```

```cpp
#include <string>
#include <unordered_map>
#include <sstream>

class FIXMessage {
    std::unordered_map<int, std::string> fields_;

public:
    void setField(int tag, const std::string& value) {
        fields_[tag] = value;
    }

    std::string getField(int tag) const {
        auto it = fields_.find(tag);
        return it != fields_.end() ? it->second : "";
    }

    // Serialize to wire format
    std::string serialize() const {
        std::string body;
        for (const auto& [tag, value] : fields_) {
            if (tag == 8 || tag == 9 || tag == 10) continue;  // Header/trailer
            body += std::to_string(tag) + "=" + value + '\x01';
        }

        std::string msg;
        msg += "8=" + getField(8) + '\x01';           // BeginString
        msg += "9=" + std::to_string(body.size()) + '\x01';  // BodyLength
        msg += body;

        // Calculate checksum
        int checksum = 0;
        for (char c : msg) checksum += static_cast<unsigned char>(c);
        checksum %= 256;

        char cs[4];
        snprintf(cs, sizeof(cs), "%03d", checksum);
        msg += "10=" + std::string(cs) + '\x01';

        return msg;
    }

    // Parse from wire format
    static FIXMessage parse(const std::string& raw) {
        FIXMessage msg;
        std::istringstream ss(raw);
        std::string field;
        while (std::getline(ss, field, '\x01')) {
            auto eq = field.find('=');
            if (eq != std::string::npos) {
                int tag = std::stoi(field.substr(0, eq));
                msg.setField(tag, field.substr(eq + 1));
            }
        }
        return msg;
    }
};

// FIX Session management
class FIXSession {
    int seqNum_ = 1;
    std::string senderCompId_;
    std::string targetCompId_;

public:
    FIXMessage createNewOrderSingle(const std::string& symbol,
                                      char side, int qty, double price) {
        FIXMessage msg;
        msg.setField(8, "FIX.4.4");
        msg.setField(35, "D");           // NewOrderSingle
        msg.setField(49, senderCompId_);
        msg.setField(56, targetCompId_);
        msg.setField(34, std::to_string(seqNum_++));
        msg.setField(11, generateClOrdID());
        msg.setField(55, symbol);
        msg.setField(54, std::string(1, side));  // 1=Buy, 2=Sell
        msg.setField(38, std::to_string(qty));
        msg.setField(40, "2");           // Limit order
        msg.setField(44, std::to_string(price));
        return msg;
    }
};
```

### Explanation:
- **FIX** (Financial Information eXchange) is the industry standard for electronic trading
- **Performance critical**: Parse incoming messages in < 1|s
- **Production**: Use libraries like QuickFIX, FIX8, or custom zero-copy parsers
- **Key interview topics**: Session management, sequence number gap fill, heartbeats, message recovery

**FIX Tag Dictionary (most commonly asked tags):**
| Tag | Name | Values | Description |
|-----|------|--------|-------------|
| 8 | BeginString | FIX.4.2, FIX.4.4, FIXT.1.1 | Protocol version |
| 35 | MsgType | D=NewOrder, 8=ExecReport, 0=Heartbeat, A=Logon | Message type |
| 49 | SenderCompID | String | Sender identification |
| 56 | TargetCompID | String | Target identification |
| 34 | MsgSeqNum | Int | Sequence number (gap detection) |
| 11 | ClOrdID | String | Client order ID (unique per order) |
| 55 | Symbol | AAPL, MSFT, etc. | Instrument symbol |
| 54 | Side | 1=Buy, 2=Sell, 5=SellShort | Order direction |
| 38 | OrderQty | Int | Quantity |
| 40 | OrdType | 1=Market, 2=Limit, 3=Stop | Order type |
| 44 | Price | Decimal | Limit price |
| 39 | OrdStatus | 0=New, 1=PartFill, 2=Filled, 4=Canceled, 8=Rejected | Order state |
| 150 | ExecType | 0=New, F=Trade, 4=Canceled | Execution report type |
| 151 | LeavesQty | Int | Remaining quantity |
| 14 | CumQty | Int | Filled quantity so far |
| 31 | LastPx | Decimal | Last fill price |

**FIX Session Lifecycle:**
```
Client                              Exchange
  -                                    ?
  --- Logon (A) +------------------+   |  (SeqNum=1)
  -                                    ?
  -   +----------------+ Logon (A) +--+  (SeqNum=1)
  -                                    ?
  --- NewOrderSingle (D) +---------+   |  (SeqNum=2)
  -                                    ?
  -   +---+ ExecutionReport (8) +-----+  (New, SeqNum=2)
  -   +---+ ExecutionReport (8) +-----+  (Fill, SeqNum=3)
  -                                    ?
  -   ... Heartbeat (0) every 30s ...  |
  -                                    ?
  --- Logout (5) +-----------------+   |
  -   +---------------+ Logout (5) +--+
```

**Zero-copy FIX parser (HFT optimization):**
```cpp
// Instead of std::unordered_map + std::string copies:
struct FixFieldView {
    int tag;
    std::string_view value;  // Points into original buffer -> no copy!
};

std::vector<FixFieldView> parseZeroCopy(std::string_view raw) {
    std::vector<FixFieldView> fields;
    fields.reserve(32);
    size_t pos = 0;
    while (pos < raw.size()) {
        auto eq = raw.find('=', pos);
        auto sep = raw.find('\x01', eq);
        int tag = fastAtoi(raw.data() + pos, eq - pos);
        fields.push_back({tag, raw.substr(eq + 1, sep - eq - 1)});
        pos = sep + 1;
    }
    return fields;
}
// 5-10x faster than map-based parser for HFT use cases
```

---

## Q8: What is an Order Book| Implement a limit order book.

### Answer:
```cpp
#include <map>
#include <list>
#include <unordered_map>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    std::string orderId;
    Side side;
    double price;
    int quantity;
    int remainingQty;
};

struct Trade {
    std::string buyOrderId;
    std::string sellOrderId;
    double price;
    int quantity;
};

class OrderBook {
    // Buy side: highest price first (descending)
    std::map<double, std::list<Order*>, std::greater<>> bids_;
    // Sell side: lowest price first (ascending)
    std::map<double, std::list<Order*>> asks_;
    // Quick lookup by order ID
    std::unordered_map<std::string, Order> orders_;

    std::vector<Trade> trades_;

public:
    std::vector<Trade> addOrder(Order order) {
        trades_.clear();

        if (order.side == Side::BUY) {
            // Match against asks (sell orders)
            matchOrder(order, asks_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                bids_[stored.price].push_back(&stored);
            }
        } else {
            // Match against bids (buy orders)
            matchOrder(order, bids_);
            if (order.remainingQty > 0) {
                auto& stored = orders_[order.orderId] = std::move(order);
                asks_[stored.price].push_back(&stored);
            }
        }

        return trades_;
    }

    bool cancelOrder(const std::string& orderId) {
        auto it = orders_.find(orderId);
        if (it == orders_.end()) return false;

        auto& order = it->second;
        auto& book = (order.side == Side::BUY) ? bids_ : asks_;
        auto priceIt = book.find(order.price);
        if (priceIt != book.end()) {
            priceIt->second.remove(&order);
            if (priceIt->second.empty())
                book.erase(priceIt);
        }
        orders_.erase(it);
        return true;
    }

    double bestBid() const { return bids_.empty() ? 0 : bids_.begin()->first; }
    double bestAsk() const { return asks_.empty() ? 0 : asks_.begin()->first; }
    double spread() const { return bestAsk() - bestBid(); }

private:
    template<typename BookType>
    void matchOrder(Order& incoming, BookType& book) {
        while (incoming.remainingQty > 0 && !book.empty()) {
            auto bestIt = book.begin();
            double bestPrice = bestIt->first;

            // Check if price crosses
            bool crosses = (incoming.side == Side::BUY)
                - incoming.price >= bestPrice
                : incoming.price <= bestPrice;

            if (!crosses) break;

            auto& queue = bestIt->second;
            while (incoming.remainingQty > 0 && !queue.empty()) {
                Order* resting = queue.front();
                int fillQty = std::min(incoming.remainingQty, resting->remainingQty);

                trades_.push_back({
                    (incoming.side == Side::BUY) ? incoming.orderId : resting->orderId,
                    (incoming.side == Side::BUY) ? resting->orderId : incoming.orderId,
                    bestPrice,
                    fillQty
                });

                incoming.remainingQty -= fillQty;
                resting->remainingQty -= fillQty;

                if (resting->remainingQty == 0) {
                    queue.pop_front();
                    orders_.erase(resting->orderId);
                }
            }

            if (queue.empty()) book.erase(bestIt);
        }
    }
};
```

### Explanation:
- **Price-time priority**: Best price first, then FIFO at same price level
- **`std::map`** for price levels: O(log n) insert/find, ordered by price
- **`std::list`** for order queue at each price: O(1) insert/remove, maintains FIFO
- **Performance**: Production order books use array-based price levels (fixed tick sizes) for O(1) access
- **Throughput target**: Handle 1M+ order operations/second

---

# Part D: IBM/Enterprise Domain Questions

---

## Q9: How do you handle backward compatibility in large enterprise C++ codebases|

### Answer:

**1. API Versioning:**
```cpp
// Version-aware API
namespace api::v1 {
    struct UserRecord {
        std::string name;
        int age;
    };
    void processUser(const UserRecord& user);
}

namespace api::v2 {
    struct UserRecord {
        std::string name;
        int age;
        std::string email;       // New field
        std::optional<Address> address;  // New optional field
    };
    void processUser(const UserRecord& user);

    // Conversion from v1
    UserRecord fromV1(const api::v1::UserRecord& old) {
        return {old.name, old.age, "", std::nullopt};
    }
}
```

**2. ABI Stability (Pimpl idiom):**
```cpp
// Public header -> ABI stable (only pointer, no layout dependency)
class Database {
public:
    Database();
    ~Database();
    void query(const std::string& sql);
    // Can add new methods without breaking ABI

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;  // Size never changes
};

// Private implementation -> can change freely
struct Database::Impl {
    ConnectionPool pool;
    Cache cache;
    // Add new members without affecting clients
    Logger logger;  // Added in v2 ? no recompilation needed
};
```

**3. Feature Flags:**
```cpp
class FeatureFlags {
    std::unordered_map<std::string, bool> flags_;
public:
    bool isEnabled(const std::string& feature) const {
        auto it = flags_.find(feature);
        return it != flags_.end() && it->second;
    }
};

// Usage
if (featureFlags.isEnabled("new_pricing_engine")) {
    useNewPricingEngine();
} else {
    useLegacyPricingEngine();
}
```

### Explanation:
- **Pimpl** is THE enterprise pattern for ABI stability | critical when shipping shared libraries
- **IBM/Enterprise context**: Products with 20+ year histories, thousands of customers, can't break existing integrations
- **Deprecation strategy**: Warn | provide migration path | maintain for N releases | remove

**ABI Verification Tools (interviewers may ask "how do you verify ABI hasn't broken?"):**
```bash
# Linux: Compare symbols exported by shared library
nm -D --defined-only libmylib.so > symbols_v1.txt
nm -D --defined-only libmylib_v2.so > symbols_v2.txt
diff symbols_v1.txt symbols_v2.txt

# Linux: Check ABI compatibility with abi-compliance-checker
abi-compliance-checker -l mylib -old v1.abi -new v2.abi
# Output: Report showing broken/added/removed symbols

# readelf: Inspect ELF symbol table
readelf -Ws libmylib.so | grep FUNC

# c++filt: Demangle C++ symbols
nm libmylib.so | c++filt | grep "MyClass"

# Windows: dumpbin
dumpbin /exports mylib.dll
```

**Symbol Versioning (advanced, used by glibc):**
```cpp
// Version script (linker map file)
// mylib.map:
MYLIB_1.0 {
    global: myFunction; myClass*;
    local: *;
};
MYLIB_2.0 {
    global: newFunction;
} MYLIB_1.0;

// Compile: g++ -shared -Wl,--version-script=mylib.map -o libmylib.so
// Old clients link against MYLIB_1.0 symbols
// New clients get MYLIB_2.0 symbols
// Both versions coexist in the same .so file
```

**Inline namespace versioning (C++11):**
```cpp
namespace mylib {
    inline namespace v2 {  // Current default
        struct Config { int timeout; std::string host; bool useTLS; };
    }
    namespace v1 {         // Old version still accessible
        struct Config { int timeout; std::string host; };
    }
}
// New code: mylib::Config uses v2 automatically
// Old code: mylib::v1::Config still works
```

---

## Q10: Explain how you would modernize a legacy C++ codebase (C++03 | C++20).

### Answer:

### Phased Modernization Strategy:

**Phase 1: Safety & Build (Low risk, high value)**
```
- Enable higher warning levels (-Wall -Wextra -Werror)
- Add AddressSanitizer and UBSan to CI
- Migrate build system to CMake 3.20+
- Set C++17 as minimum standard
- Enable clang-tidy with modernize-* checks
```

**Phase 2: Quick Wins (Automated with clang-tidy)**
```
- NULL -> nullptr
- typedef -> using
- auto where it improves readability
- Range-based for loops
- override on virtual functions
- = default / = delete for special members
- Uniform initialization {}
```

**Phase 3: Resource Management (High impact)**
```
- Raw pointers -> smart pointers (unique_ptr/shared_ptr)
- Manual new/delete -> make_unique/make_shared
- C arrays -> std::array/std::vector
- C strings -> std::string/std::string_view
- Apply Rule of Zero wherever possible
```

**Phase 4: API Modernization (Incremental)**
```
- std::optional for nullable returns
- std::variant for type-safe unions
- std::string_view for non-owning string parameters
- Concepts for template constraints
- Structured bindings
```

**Phase 5: Architecture (Long-term)**
```
- Introduce modules (C++20) for build speed
- Replace callback systems with coroutines where appropriate
- Parallel algorithms for batch processing
- std::pmr for memory-critical paths
```

### Key Principle:
**Never do a big-bang rewrite.** Modernize incrementally:
1. New code uses modern C++
2. When touching old code, modernize that file
3. Use adapter layers between old and new code
4. Track metrics: build time, bug rate, sanitizer findings

---

# ENHANCED SECTION: IBM i / Enterprise Systems Domain

> *Critical for anyone with IBM i/iCluster experience interviewing at enterprise software companies (Rocket Software, IBM, HCL, Precisely, Syncsort).*

---

## Q10: Explain the IBM i architecture | how is it fundamentally different from Linux/Windows|

### Answer:

```
IBM i Architecture (Single-Level Storage):
+-------------------------------------------------+
|  Applications (RPG, COBOL, C, CL, SQL)            |
+-------------------------------------------------+
|  Machine Interface (MI) | Hardware abstraction     |
|  - All object access via system pointers           |
|  - Object types: *PGM, *FILE, *DTAQ, *USRSPC      |
|  - Single-level storage -> no explicit file I/O     |
+-------------------------------------------------+
|  Licensed Internal Code (LIC) / SLIC              |
|  - Memory management, task dispatching             |
|  - Journaling engine (QAUDJRN)                     |
|  - Database engine (DB2 for i -> integrated)        |
+-------------------------------------------------+
|  POWER Hardware                                    |
+-------------------------------------------------+
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this | OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work| Why is it critical for iCluster|

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  -?? Writes journal entry to Journal Receiver
  -?? Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  -?? Append-only log file
  -?? Entries stored in sequence number order
  -?? Receivers chained -> old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  -?? Filters by: Object, Entry type, Library
  --- Tracks position: Receiver + Sequence number
  -?? Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** | without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag | it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery | connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
.Value
        '+' + ('-' * ($match.Length - 2)) + '+'
    
```

**Key differences from Linux/Windows:**
| Aspect | IBM i | Linux/Windows |
|--------|-------|---------------|
| Database | Integrated (DB2 is THE file system) | Separate layer (PostgreSQL, MySQL) |
| Objects | Everything is a typed object | Files are byte streams |
| Security | Object-level authority | File permissions (rwx) |
| Journaling | Built into OS for all DB operations | Application must implement WAL |
| Memory model | Single-level storage (SLS) | Virtual memory with swap |
| Process model | Jobs with call stacks | Processes with threads |
| Stability | Systems run for 10+ years uninterrupted | Regular patching/rebooting |

### Explanation:
The key insight for iCluster development: **Journaling is built into the OS**. On Linux, you'd need to implement CDC yourself (reading PostgreSQL WAL). On IBM i, the OS already captures every database change in the audit journal (QAUDJRN). iCluster leverages this -> OMPROCJRN reads journal entries that the OS creates automatically. This is why IBM i HA/DR products can be so efficient.

---

## Q11: How does IBM i journaling work? Why is it critical for iCluster?

### Answer:
```
Journaling Flow:

Application writes record to PF (Physical File)
        |
        |
DB2 for i Engine
  --> Writes journal entry to Journal Receiver
  --> Entry contains: Timestamp, Job info, Object ID, 
      Entry type (PT=Put, DL=Delete, UB=Update Before, UP=Update After)
      Complete record image (before/after)
        |
        |
Journal Receiver (QAUDJRN receiver)
  --> Append-only log file
  --> Entries stored in sequence number order
  --> Receivers chained | old ones detached automatically
        |
        |
iCluster OMPROCJRN reads entries via RCVJRNE API
  --> Filters by: Object, Entry type, Library
  --> Tracks position: Receiver + Sequence number
  --> Builds change set for replication
```

**Journal Entry Types used by iCluster:**
| Code | Type | Description |
|------|------|-------------|
| PT | Put/Write | New record inserted |
| DL | Delete | Record deleted |
| UB | Update Before | Record image before update |
| UP | Update After | Record image after update |
| CL | Close | File closed |
| EE | End commitment cycle | Transaction boundary |
| SC | Start commitment control | Transaction started |
| B-CM | IFS Create | Byte stream file created |
| B-WR | IFS Write | Byte stream file written |
| B-DL | IFS Delete | Byte stream file deleted |

### Explanation:
- Journaling is the **foundation of iCluster's real-time replication** ? without it, you'd need to poll files for changes (slow, resource-intensive)
- Journal-based approach = **Change Data Capture (CDC)** built into the OS
- The receiver chain + sequence number = **exactly-once delivery guarantee** (same as Kafka consumer offsets)
- This is why iCluster can replicate with near-zero lag -> it reads changes as they happen, not by scanning files

---

## Q12: Compare iCluster's communication architecture (DMKAPI) with modern messaging systems.

### Answer:
| Aspect | iCluster DMKAPI | gRPC | Kafka | ZeroMQ |
|--------|----------------|------|-------|--------|
| Protocol | SNA + TCP/IP | HTTP/2 | TCP (custom) | TCP/IPC/inproc |
| Pattern | Request-Reply + Push | Request-Reply | Pub-Sub + Stream | All patterns |
| Encryption | Custom scramble key | TLS | TLS | CurveZMQ |
| Compression | Custom per-link | Built-in (gzip) | Configurable | No |
| Queuing | OS data queues | None (streaming) | Persistent log | In-memory |
| Sessions | 200 concurrent | Unlimited | Consumer groups | N/A |
| Max message | 64KB | 4MB default | 1MB default | Unlimited |

**DMKAPI's unique strengths:**
- **Protocol agnostic**: Same API works over SNA (legacy) and TCP/IP
- **Dual-channel**: Separate control (CI/CO) and data (DI/DO) channels prevent head-of-line blocking
- **Named opens**: Like service discovery -> connect by name, not IP address
- **Monitor process (DMKMO)**: Central orchestrator managing 200 sessions and 800 links

**Architectural lessons from DMKAPI applicable to modern systems:**
1. **Separate control and data planes** (like Kubernetes separates API server from kubelet)
2. **Queue-based async IPC** (like message brokers between microservices)
3. **Tag-slot correlation** (like request IDs / correlation IDs in distributed tracing)
4. **Protocol abstraction** (like gRPC abstracting over HTTP/2)

---
