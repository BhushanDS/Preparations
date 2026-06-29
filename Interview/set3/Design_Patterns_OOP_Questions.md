# Set 3: Design Patterns & Object-Oriented Programming
## Senior-Level Interview Questions (10 Years Experience)

---

## Q1: Explain SOLID principles with C++ examples. How do you apply them in large codebases?

### Answer:

**S -> Single Responsibility Principle:**
```cpp
// BAD: One class doing too much
class CADDocument {
    void loadFromFile(const std::string& path);
    void saveToFile(const std::string& path);
    void renderToScreen(Viewport& vp);
    void exportToPDF(const std::string& path);
    void validateGeometry();
};

// GOOD: Each class has one reason to change
class CADDocument { /* holds geometry data only */ };
class DocumentLoader { void load(CADDocument&, const std::string&); };
class DocumentRenderer { void render(const CADDocument&, Viewport&); };
class PDFExporter { void exportToPDF(const CADDocument&, const std::string&); };
class GeometryValidator { bool validate(const CADDocument&); };
```

**O -> Open/Closed Principle:**
```cpp
// Open for extension, closed for modification
class Shape {
public:
    virtual ~Shape() = default;
    virtual double area() const = 0;
};

class Circle : public Shape {
    double radius_;
public:
    Circle(double r) : radius_(r) {}
    double area() const override { return 3.14159 * radius_ * radius_; }
};

// Adding new shapes doesn't modify existing code
class Polygon : public Shape {
    std::vector<Point> vertices_;
public:
    double area() const override { /* Shoelace formula */ }
};
```

**L -> Liskov Substitution Principle:**
```cpp
// BAD: Square breaks Rectangle's contract
class Rectangle {
public:
    virtual void setWidth(double w) { width_ = w; }
    virtual void setHeight(double h) { height_ = h; }
    double area() const { return width_ * height_; }
protected:
    double width_, height_;
};

class Square : public Rectangle {
public:
    void setWidth(double w) override { width_ = height_ = w; }  // Breaks LSP!
    void setHeight(double h) override { width_ = height_ = h; } // Breaks LSP!
};

// GOOD: Use a common interface without inheritance
class Shape {
public:
    virtual double area() const = 0;
    virtual ~Shape() = default;
};
```

**I -> Interface Segregation Principle:**
```cpp
// BAD: Fat interface
class IWorker {
    virtual void code() = 0;
    virtual void test() = 0;
    virtual void design() = 0;
    virtual void manage() = 0;  // Not all workers manage
};

// GOOD: Segregated interfaces
class ICoder { virtual void code() = 0; };
class ITester { virtual void test() = 0; };
class IDesigner { virtual void design() = 0; };

class SeniorDev : public ICoder, public IDesigner { /* ... */ };
```

**D -> Dependency Inversion Principle:**
```cpp
// BAD: High-level depends on low-level
class ReportGenerator {
    MySQLDatabase db_;  // Concrete dependency
public:
    void generate() { auto data = db_.query("..."); }
};

// GOOD: Depend on abstractions
class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual std::vector<Row> query(const std::string& sql) = 0;
};

class ReportGenerator {
    std::unique_ptr<IDatabase> db_;
public:
    ReportGenerator(std::unique_ptr<IDatabase> db) : db_(std::move(db)) {}
    void generate() { auto data = db_->query("..."); }
};
```

### Explanation:
SOLID keeps large C++ codebases maintainable. In CAD systems with millions of lines, violating these leads to cascading changes. In gaming, SRP helps separate rendering, physics, and AI logic cleanly.

---

## Q2: Implement the Observer Pattern. How does it apply in event-driven systems?

### Answer:
```cpp
#include <vector>
#include <algorithm>
#include <functional>
#include <string>
#include <iostream>
#include <memory>

// Modern C++ Observer using std::function (no inheritance needed)
template<typename... Args>
class Event {
    using Handler = std::function<void(Args...)>;
    using HandlerId = size_t;

    std::vector<std::pair<HandlerId, Handler>> handlers_;
    HandlerId nextId_ = 0;

public:
    HandlerId subscribe(Handler handler) {
        HandlerId id = nextId_++;
        handlers_.emplace_back(id, std::move(handler));
        return id;
    }

    void unsubscribe(HandlerId id) {
        handlers_.erase(
            std::remove_if(handlers_.begin(), handlers_.end(),
                [id](const auto& p) { return p.first == id; }),
            handlers_.end());
    }

    void emit(Args... args) const {
        for (const auto& [id, handler] : handlers_) {
            handler(args...);
        }
    }
};

// Usage in a CAD application
class Document {
public:
    Event<const std::string&> onModified;
    Event<> onSaved;

    void modify(const std::string& change) {
        // ... apply modification
        onModified.emit(change);
    }

    void save() {
        // ... save to file
        onSaved.emit();
    }
};

class UndoManager {
public:
    void onDocModified(const std::string& change) {
        std::cout << "Recording undo: " << change << "\n";
    }
};

// Wiring
void setup() {
    Document doc;
    UndoManager undoMgr;

    auto id1 = doc.onModified.subscribe(
        [&undoMgr](const std::string& c) { undoMgr.onDocModified(c); });

    auto id2 = doc.onModified.subscribe(
        [](const std::string& c) { std::cout << "UI update: " << c << "\n"; });

    doc.modify("Added circle");
    doc.onModified.unsubscribe(id2);  // UI no longer listening
}
```

### Explanation:
- **Modern approach**: `std::function` + lambda > classic virtual Observer interface
- **Type-safe**: Template parameters define event signature
- **Decoupled**: Publisher doesn't know about subscribers
- **CAD use case**: Geometry change -> triggers undo recording, UI update, constraint solver
- **Gaming use case**: Player health change -> UI health bar, sound effects, achievement checks
- **Finance**: Market data update -> strategy evaluation, risk calculations, logging
- **Thread safety note**: For multithreaded use, protect `handlers_` with a mutex or use lock-free list

**Thread-Safe Observer (production pattern):**
```cpp
template<typename... Args>
class ThreadSafeEvent {
    using Handler = std::function<void(Args...)>;
    using HandlerId = size_t;

    mutable std::shared_mutex mutex_;
    std::vector<std::pair<HandlerId, Handler>> handlers_;
    HandlerId nextId_ = 0;

public:
    HandlerId subscribe(Handler handler) {
        std::unique_lock lock(mutex_);
        HandlerId id = nextId_++;
        handlers_.emplace_back(id, std::move(handler));
        return id;
    }

    void unsubscribe(HandlerId id) {
        std::unique_lock lock(mutex_);
        std::erase_if(handlers_, [id](const auto& p) { return p.first == id; });
    }

    void emit(Args... args) const {
        std::shared_lock lock(mutex_);   // Multiple emitters can fire concurrently
        // Copy handlers to avoid holding lock during callback
        auto snapshot = handlers_;
        lock.unlock();
        for (const auto& [id, handler] : snapshot) {
            handler(args...);
        }
    }
};
// NOTE: snapshot-then-emit prevents deadlock if a handler calls subscribe/unsubscribe
```

**Weak Observer (prevents dangling callbacks):**
```cpp
template<typename... Args>
class WeakEvent {
    struct Subscription {
        size_t id;
        std::weak_ptr<void> owner;  // Track lifetime
        std::function<void(Args...)> handler;
    };
    std::vector<Subscription> handlers_;

public:
    template<typename T>
    size_t subscribe(std::shared_ptr<T> owner, std::function<void(Args...)> handler) {
        size_t id = handlers_.size();
        handlers_.push_back({id, owner, std::move(handler)});
        return id;
    }

    void emit(Args... args) {
        // Auto-cleanup expired observers
        std::erase_if(handlers_, [](const auto& s) { return s.owner.expired(); });
        for (const auto& sub : handlers_) {
            sub.handler(args...);
        }
    }
};
```

---

## Q3: Implement the Strategy Pattern and compare it with Policy-based design.

### Answer:

**Classic Strategy (Runtime polymorphism):**
```cpp
// Pricing strategy for financial instruments
class IPricingStrategy {
public:
    virtual ~IPricingStrategy() = default;
    virtual double calculatePrice(double basePrice, int quantity) const = 0;
};

class StandardPricing : public IPricingStrategy {
public:
    double calculatePrice(double basePrice, int quantity) const override {
        return basePrice * quantity;
    }
};

class DiscountPricing : public IPricingStrategy {
    double discount_;
public:
    DiscountPricing(double discount) : discount_(discount) {}
    double calculatePrice(double basePrice, int quantity) const override {
        return basePrice * quantity * (1.0 - discount_);
    }
};

class Order {
    std::unique_ptr<IPricingStrategy> pricing_;
public:
    void setPricing(std::unique_ptr<IPricingStrategy> p) { pricing_ = std::move(p); }
    double total(double price, int qty) { return pricing_->calculatePrice(price, qty); }
};
```

**Policy-based Design (Compile-time, zero overhead):**
```cpp
// Same functionality, but resolved at compile time
template<typename PricingPolicy>
class Order : private PricingPolicy {
public:
    using PricingPolicy::PricingPolicy;  // Inherit constructor

    double total(double price, int qty) {
        return this->calculatePrice(price, qty);  // Static dispatch
    }
};

struct StandardPricing {
    double calculatePrice(double basePrice, int quantity) const {
        return basePrice * quantity;
    }
};

struct DiscountPricing {
    double discount_;
    DiscountPricing(double d) : discount_(d) {}
    double calculatePrice(double basePrice, int quantity) const {
        return basePrice * quantity * (1.0 - discount_);
    }
};

// Usage
Order<StandardPricing> order1;
Order<DiscountPricing> order2(0.15);
```

### Explanation:
| Aspect | Strategy (Virtual) | Policy (Template) |
|--------|-------------------|-------------------|
| Dispatch | Runtime (vtable) | Compile-time (inlined) |
| Flexibility | Change at runtime | Fixed at compile time |
| Performance | Virtual call overhead | Zero overhead (inlined) |
| Binary size | Smaller | Larger (template bloat) |
| Use when | Behavior changes at runtime | Behavior fixed at build time |

**Choose Strategy** for: Plugin systems, user-configurable behavior, dependency injection
**Choose Policy** for: Performance-critical paths, gaming engines, HFT systems

---

## Q4: Implement the Factory Pattern family -> Simple Factory, Factory Method, Abstract Factory.

### Answer:
```cpp
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

// === Products ===
class Shape {
public:
    virtual ~Shape() = default;
    virtual void draw() const = 0;
    virtual double area() const = 0;
};

class Circle : public Shape {
    double radius_;
public:
    Circle(double r) : radius_(r) {}
    void draw() const override { /* render circle */ }
    double area() const override { return 3.14159 * radius_ * radius_; }
};

class Rectangle : public Shape {
    double w_, h_;
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    void draw() const override { /* render rect */ }
    double area() const override { return w_ * h_; }
};

// === 1. Simple Factory (not a GoF pattern, but commonly asked) ===
class ShapeFactory {
public:
    static std::unique_ptr<Shape> create(const std::string& type) {
        if (type == "circle") return std::make_unique<Circle>(1.0);
        if (type == "rectangle") return std::make_unique<Rectangle>(1.0, 1.0);
        return nullptr;
    }
};

// === 2. Self-Registering Factory (Extensible | no switch/if chain) ===
class ShapeRegistry {
    using Creator = std::function<std::unique_ptr<Shape>()>;
    std::unordered_map<std::string, Creator> creators_;

    ShapeRegistry() = default;
public:
    static ShapeRegistry& instance() {
        static ShapeRegistry reg;
        return reg;
    }

    void registerShape(const std::string& name, Creator creator) {
        creators_[name] = std::move(creator);
    }

    std::unique_ptr<Shape> create(const std::string& name) const {
        auto it = creators_.find(name);
        if (it != creators_.end()) return it->second();
        return nullptr;
    }
};

// Auto-register at static initialization
namespace {
    bool _regCircle = [] {
        ShapeRegistry::instance().registerShape("circle",
            []() { return std::make_unique<Circle>(1.0); });
        return true;
    }();
}

// === 3. Abstract Factory ===
class IUIFactory {
public:
    virtual ~IUIFactory() = default;
    virtual std::unique_ptr<class Button> createButton() = 0;
    virtual std::unique_ptr<class TextBox> createTextBox() = 0;
};

class WindowsUIFactory : public IUIFactory {
public:
    std::unique_ptr<Button> createButton() override;
    std::unique_ptr<TextBox> createTextBox() override;
};

class LinuxUIFactory : public IUIFactory {
public:
    std::unique_ptr<Button> createButton() override;
    std::unique_ptr<TextBox> createTextBox() override;
};
```

### Explanation:
- **Simple Factory**: Centralizes creation but violates Open/Closed (adding types = modifying factory)
- **Self-Registering Factory**: Types register themselves -> truly extensible, no modification needed
- **Abstract Factory**: Creates families of related objects (e.g., platform-specific UI)
- **CAD use case**: Shape factories, tool factories, import/export format factories
- **Gaming**: Entity factories, weapon factories, platform-specific renderer factories

---

## Q5: Explain CRTP (Curiously Recurring Template Pattern) and static polymorphism.

### Answer:
```cpp
// CRTP: Base class is templated on the derived class
template<typename Derived>
class Shape {
public:
    double area() const {
        return static_cast<const Derived*>(this)->areaImpl();
    }

    void draw() const {
        static_cast<const Derived*>(this)->drawImpl();
    }

    // Mixin functionality: add common behavior
    void describe() const {
        std::cout << "Shape with area: " << area() << "\n";
    }
};

class Circle : public Shape<Circle> {
    double radius_;
public:
    Circle(double r) : radius_(r) {}
    double areaImpl() const { return 3.14159 * radius_ * radius_; }
    void drawImpl() const { std::cout << "Drawing circle\n"; }
};

class Square : public Shape<Square> {
    double side_;
public:
    Square(double s) : side_(s) {}
    double areaImpl() const { return side_ * side_; }
    void drawImpl() const { std::cout << "Drawing square\n"; }
};

// Usage | no virtual dispatch, fully inlined
template<typename ShapeType>
void processShape(const Shape<ShapeType>& shape) {
    shape.draw();
    shape.describe();
}

// CRTP for counting instances
template<typename T>
class InstanceCounter {
    static inline int count_ = 0;
public:
    InstanceCounter() { ++count_; }
    ~InstanceCounter() { --count_; }
    static int getCount() { return count_; }
};

class Enemy : public InstanceCounter<Enemy> { /* ... */ };
class Bullet : public InstanceCounter<Bullet> { /* ... */ };
// Enemy::getCount() and Bullet::getCount() are independent
```

### Explanation:
**Why CRTP?**
1. **Zero overhead polymorphism**: No vtable, no virtual dispatch, calls are inlined
2. **Mixin behavior**: Add functionality to derived classes without overhead
3. **Compile-time interface enforcement**: Fails to compile if `areaImpl()` is missing

**Trade-offs vs virtual:**
| Aspect | Virtual (Dynamic) | CRTP (Static) |
|--------|-------------------|---------------|
| Performance | vtable lookup | Fully inlined |
| Heterogeneous collections | `std::vector<Shape*>` works | Need `std::variant` or type erasure |
| Runtime flexibility | Add types via plugins | All types known at compile time |

**Gaming use case**: Component systems where every frame calls `update()` on thousands of entities -> CRTP eliminates virtual call overhead.

---

## Q6: Implement the Command Pattern with undo/redo for a CAD application.

### Answer:
```cpp
#include <memory>
#include <stack>
#include <vector>
#include <iostream>

// Command interface
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string description() const = 0;
};

// Receiver
class Canvas {
    std::vector<std::string> shapes_;
public:
    void addShape(const std::string& shape) {
        shapes_.push_back(shape);
        std::cout << "Added: " << shape << "\n";
    }
    void removeLastShape() {
        if (!shapes_.empty()) {
            std::cout << "Removed: " << shapes_.back() << "\n";
            shapes_.pop_back();
        }
    }
    void moveShape(int idx, double dx, double dy) {
        std::cout << "Moved shape " << idx << " by (" << dx << ", " << dy << ")\n";
    }
};

// Concrete commands
class AddShapeCommand : public ICommand {
    Canvas& canvas_;
    std::string shape_;
public:
    AddShapeCommand(Canvas& c, std::string shape)
        : canvas_(c), shape_(std::move(shape)) {}

    void execute() override { canvas_.addShape(shape_); }
    void undo() override { canvas_.removeLastShape(); }
    std::string description() const override { return "Add " + shape_; }
};

// Command Manager with undo/redo
class CommandManager {
    std::stack<std::unique_ptr<ICommand>> undoStack_;
    std::stack<std::unique_ptr<ICommand>> redoStack_;

public:
    void execute(std::unique_ptr<ICommand> cmd) {
        cmd->execute();
        undoStack_.push(std::move(cmd));
        // Clear redo stack | new action invalidates redo history
        while (!redoStack_.empty()) redoStack_.pop();
    }

    void undo() {
        if (undoStack_.empty()) return;
        auto cmd = std::move(undoStack_.top());
        undoStack_.pop();
        cmd->undo();
        redoStack_.push(std::move(cmd));
    }

    void redo() {
        if (redoStack_.empty()) return;
        auto cmd = std::move(redoStack_.top());
        redoStack_.pop();
        cmd->execute();
        undoStack_.push(std::move(cmd));
    }
};

// Usage
void example() {
    Canvas canvas;
    CommandManager mgr;

    mgr.execute(std::make_unique<AddShapeCommand>(canvas, "Circle"));
    mgr.execute(std::make_unique<AddShapeCommand>(canvas, "Rectangle"));
    mgr.undo();   // Removes Rectangle
    mgr.undo();   // Removes Circle
    mgr.redo();   // Re-adds Circle
}
```

### Explanation:
- **Encapsulates actions as objects** ? allows undo, redo, macro recording, serialization
- **CAD essential pattern**: Every CAD application uses Command pattern for undo/redo
- **Gaming**: Input command queues, replay systems
- **Finance**: Transaction logging, audit trails
- **Macro commands**: Group multiple commands into one undoable operation (CompositeCommand)

**Macro / Composite Command:**
```cpp
class MacroCommand : public ICommand {
    std::vector<std::unique_ptr<ICommand>> commands_;
    std::string name_;
public:
    MacroCommand(std::string name) : name_(std::move(name)) {}

    void add(std::unique_ptr<ICommand> cmd) { commands_.push_back(std::move(cmd)); }

    void execute() override {
        for (auto& cmd : commands_) cmd->execute();
    }

    void undo() override {
        // Undo in REVERSE order!
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it)
            (*it)->undo();
    }

    std::string description() const override { return "Macro: " + name_; }
};

// Usage: "Add array of shapes" as a single undoable operation
auto macro = std::make_unique<MacroCommand>("Add 3 shapes");
macro->add(std::make_unique<AddShapeCommand>(canvas, "Circle"));
macro->add(std::make_unique<AddShapeCommand>(canvas, "Rectangle"));
macro->add(std::make_unique<AddShapeCommand>(canvas, "Triangle"));
mgr.execute(std::move(macro));  // All 3 added
mgr.undo();                      // All 3 removed in reverse order
```

**Memento-based undo (alternative approach):**
```cpp
// Instead of command inversion, snapshot entire state before each change
class DocumentMemento {
    std::vector<std::string> shapesSnapshot_;
    friend class Canvas;
public:
    DocumentMemento(std::vector<std::string> shapes) : shapesSnapshot_(std::move(shapes)) {}
};

class Canvas {
    std::vector<std::string> shapes_;
public:
    DocumentMemento createMemento() const { return DocumentMemento(shapes_); }
    void restoreMemento(const DocumentMemento& m) { shapes_ = m.shapesSnapshot_; }
};
// Trade-off: Simple to implement but O(n) memory per snapshot
// CAD hybrid: Snapshot for complex operations, command inversion for simple ones
```

---

## Q7: What is Type Erasure? Implement a simple `std::function`-like wrapper.

### Answer:
```cpp
#include <memory>
#include <utility>
#include <iostream>

template<typename Signature>
class Function;

template<typename Ret, typename... Args>
class Function<Ret(Args...)> {
    // Type-erased base
    struct Concept {
        virtual ~Concept() = default;
        virtual Ret invoke(Args... args) = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };

    // Model wraps any callable
    template<typename F>
    struct Model : Concept {
        F func_;
        Model(F f) : func_(std::move(f)) {}
        Ret invoke(Args... args) override {
            return func_(std::forward<Args>(args)...);
        }
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(func_);
        }
    };

    std::unique_ptr<Concept> impl_;

public:
    Function() = default;

    template<typename F>
    Function(F f) : impl_(std::make_unique<Model<F>>(std::move(f))) {}

    Function(const Function& other)
        : impl_(other.impl_ ? other.impl_->clone() : nullptr) {}

    Function(Function&&) = default;
    Function& operator=(Function&&) = default;

    Function& operator=(const Function& other) {
        if (this != &other)
            impl_ = other.impl_ ? other.impl_->clone() : nullptr;
        return *this;
    }

    Ret operator()(Args... args) {
        return impl_->invoke(std::forward<Args>(args)...);
    }

    explicit operator bool() const { return impl_ != nullptr; }
};

// Usage
void example() {
    Function<int(int, int)> add = [](int a, int b) { return a + b; };
    std::cout << add(3, 4) << "\n";  // 7

    struct Multiplier {
        int factor;
        int operator()(int a, int b) { return a * b * factor; }
    };

    Function<int(int, int)> mul = Multiplier{10};
    std::cout << mul(3, 4) << "\n";  // 120
}
```

### Explanation:
- **Type erasure** = virtual dispatch under the hood, but users see a value-type interface
- **Concept/Model idiom**: `Concept` defines the interface, `Model<F>` wraps any concrete callable
- **Where it's used**: `std::function`, `std::any`, `std::shared_ptr` deleter, `std::format`
- **SBO optimization**: Real `std::function` uses Small Buffer Optimization -> stores small callables inline instead of heap-allocating
- **Performance note**: `std::function` has ~5-10ns overhead per call due to virtual dispatch + possible heap allocation

**Small Buffer Optimization (SBO) implementation:**
```cpp
template<typename Ret, typename... Args>
class Function<Ret(Args...)> {
    // ... (Concept/Model as above) ...
    
    // SBO: store small callables inline, avoiding heap allocation
    static constexpr size_t SBO_SIZE = 32;  // Typical: 24-64 bytes
    alignas(std::max_align_t) char sboBuffer_[SBO_SIZE];
    Concept* impl_ = nullptr;
    bool usingSBO_ = false;

    template<typename F>
    void construct(F f) {
        if constexpr (sizeof(Model<F>) <= SBO_SIZE && 
                       std::is_nothrow_move_constructible_v<F>) {
            // Fits in SBO | construct in-place, NO heap allocation
            impl_ = new (sboBuffer_) Model<F>(std::move(f));
            usingSBO_ = true;
        } else {
            // Too large or may throw | heap allocate
            impl_ = new Model<F>(std::move(f));
            usingSBO_ = false;
        }
    }

    void destroy() {
        if (impl_) {
            if (usingSBO_) impl_->~Concept();  // Destroy in-place
            else delete impl_;                   // Heap delete
        }
    }
};
// lambdas with 0-2 captures typically fit in SBO
// lambdas with large captures or std::function<> wrapping | heap
```

**`std::move_only_function` (C++23):**
```cpp
// std::function requires copyability | overhead for copy constructor
// std::move_only_function relaxes this | supports unique_ptr captures
std::move_only_function<void()> task = [p = std::make_unique<int>(42)]() {
    std::cout << *p << "\n";
};
// Cannot be copied, only moved | better for thread pools, task queues
```

---

## Q8: Singleton Pattern -> implement thread-safe Singleton and discuss its problems.

### Answer:
```cpp
// C++11 and later: Meyer's Singleton | thread-safe by standard guarantee
class Logger {
public:
    static Logger& instance() {
        static Logger instance;  // Thread-safe in C++11+
        return instance;
    }

    void log(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "[LOG] " << msg << "\n";
    }

    // Delete copy/move
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    std::mutex mutex_;
};

// Usage
Logger::instance().log("Application started");

// DCLP (Double-Checked Locking Pattern) | pre-C++11 approach
class LegacySingleton {
    static std::atomic<LegacySingleton*> instance_;
    static std::mutex mutex_;

public:
    static LegacySingleton* instance() {
        LegacySingleton* tmp = instance_.load(std::memory_order_acquire);
        if (tmp == nullptr) {
            std::lock_guard<std::mutex> lock(mutex_);
            tmp = instance_.load(std::memory_order_relaxed);
            if (tmp == nullptr) {
                tmp = new LegacySingleton();
                instance_.store(tmp, std::memory_order_release);
            }
        }
        return tmp;
    }
};
```

### Explanation:
**Problems with Singleton:**
1. **Global state**: Makes testing hard (can't mock easily)
2. **Hidden dependencies**: Classes secretly depend on Singleton
3. **Destruction order**: Static destruction order is undefined across translation units
4. **Thread safety**: Pre-C++11 required complex patterns like DCLP

**Modern alternative -> Dependency Injection:**
```cpp
class Application {
    ILogger& logger_;  // Injected, not Singleton
public:
    Application(ILogger& logger) : logger_(logger) {}
};
```

**When Singleton is acceptable**: Truly global resources -> hardware interfaces, GPU context, logger in small applications.

---

## Q9: Explain the Visitor Pattern and double dispatch in C++.

### Answer:
```cpp
#include <memory>
#include <vector>
#include <iostream>

// Forward declarations
class Circle;
class Rectangle;
class Triangle;

// Visitor interface
class IShapeVisitor {
public:
    virtual ~IShapeVisitor() = default;
    virtual void visit(const Circle& c) = 0;
    virtual void visit(const Rectangle& r) = 0;
    virtual void visit(const Triangle& t) = 0;
};

// Element hierarchy
class Shape {
public:
    virtual ~Shape() = default;
    virtual void accept(IShapeVisitor& visitor) const = 0;
};

class Circle : public Shape {
public:
    double radius = 5.0;
    void accept(IShapeVisitor& visitor) const override { visitor.visit(*this); }
};

class Rectangle : public Shape {
public:
    double width = 4.0, height = 3.0;
    void accept(IShapeVisitor& visitor) const override { visitor.visit(*this); }
};

class Triangle : public Shape {
public:
    double base = 6.0, height = 4.0;
    void accept(IShapeVisitor& visitor) const override { visitor.visit(*this); }
};

// Concrete visitors | add operations without modifying shapes
class AreaCalculator : public IShapeVisitor {
    double totalArea_ = 0;
public:
    void visit(const Circle& c) override { totalArea_ += 3.14159 * c.radius * c.radius; }
    void visit(const Rectangle& r) override { totalArea_ += r.width * r.height; }
    void visit(const Triangle& t) override { totalArea_ += 0.5 * t.base * t.height; }
    double totalArea() const { return totalArea_; }
};

class SVGExporter : public IShapeVisitor {
public:
    void visit(const Circle& c) override {
        std::cout << "<circle r=\"" << c.radius << "\"/>\n";
    }
    void visit(const Rectangle& r) override {
        std::cout << "<rect width=\"" << r.width << "\" height=\"" << r.height << "\"/>\n";
    }
    void visit(const Triangle& t) override {
        std::cout << "<polygon points=\"...\"/>\n";
    }
};

// Usage
void example() {
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>());
    shapes.push_back(std::make_unique<Rectangle>());

    AreaCalculator calc;
    for (const auto& shape : shapes)
        shape->accept(calc);

    std::cout << "Total area: " << calc.totalArea() << "\n";
}
```

### Explanation:
- **Double dispatch**: `accept()` resolves the element type (1st dispatch), then calls the right `visit()` overload (2nd dispatch)
- **When to use**: Many operations on a fixed set of types (shapes in CAD, AST nodes in compilers)
- **When NOT to use**: When types change frequently (each new type = modify all visitors)
- **Modern alternative**: `std::variant` + `std::visit` (if types are known at compile time)
```cpp
// C++17 variant-based visitor (no virtual dispatch, no double dispatch)
using Shape = std::variant<Circle, Rectangle, Triangle>;

// Overloaded idiom for clean multi-type visitors
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

double area(const Shape& s) {
    return std::visit(overloaded{
        [](const Circle& c)    { return 3.14159 * c.radius * c.radius; },
        [](const Rectangle& r) { return r.width * r.height; },
        [](const Triangle& t)  { return 0.5 * t.base * t.height; }
    }, s);
}

// Variant vs classic Visitor trade-offs:
// Variant: Compiler error if you forget a type (exhaustive matching)
// Classic: Runtime error if you forget a type (or use default handler)
// Variant: Adding a type | update all visit sites (compile error guides you)
// Classic: Adding a type | add to IVisitor + all concrete visitors (may forget some)
// Variant: Better performance (no virtual dispatch), but types must be value-like
// Classic: Works with abstract hierarchies and pointer-based ownership
```

---

## Q10: What is the Entity-Component-System (ECS) architecture? How does it differ from traditional OOP?

### Answer:
```cpp
#include <vector>
#include <unordered_map>
#include <bitset>
#include <memory>

// Components are pure data | no behavior
struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };
struct Health { int current, max; };
struct Renderable { int meshId; int textureId; };

// Entity is just an ID
using Entity = uint32_t;

// Component storage | SoA (Structure of Arrays) for cache efficiency
template<typename T>
class ComponentPool {
    std::unordered_map<Entity, size_t> entityToIndex_;
    std::vector<T> data_;
    std::vector<Entity> entities_;

public:
    void add(Entity e, T component) {
        entityToIndex_[e] = data_.size();
        data_.push_back(std::move(component));
        entities_.push_back(e);
    }

    T* get(Entity e) {
        auto it = entityToIndex_.find(e);
        if (it == entityToIndex_.end()) return nullptr;
        return &data_[it->second];
    }

    // Iterate all components | cache-friendly!
    auto begin() { return data_.begin(); }
    auto end() { return data_.end(); }
    size_t size() const { return data_.size(); }
};

// Systems operate on components | contain all behavior
class PhysicsSystem {
public:
    void update(ComponentPool<Position>& positions,
                ComponentPool<Velocity>& velocities, float dt) {
        // Iterate contiguous memory | extremely cache friendly
        for (size_t i = 0; i < positions.size(); ++i) {
            auto& pos = *(positions.begin() + i);
            auto& vel = *(velocities.begin() + i);
            pos.x += vel.dx * dt;
            pos.y += vel.dy * dt;
            pos.z += vel.dz * dt;
        }
    }
};

// Usage
void gameLoop() {
    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;

    Entity player = 1;
    positions.add(player, {0, 0, 0});
    velocities.add(player, {1, 0, 0.5f});

    PhysicsSystem physics;
    physics.update(positions, velocities, 0.016f);  // 60 FPS
}
```

### Explanation:
| Aspect | Traditional OOP | ECS |
|--------|----------------|-----|
| Data layout | AoS (Array of Structures) | SoA (Structure of Arrays) |
| Cache efficiency | Poor (scattered vtables) | Excellent (contiguous data) |
| Composition | Inheritance hierarchy | Compose entities from components |
| Behavior | In objects (methods) | In systems (functions) |
| Flexibility | Diamond problem, deep hierarchies | Mix & match any components |

**Why ECS matters at 10-year level:**
- **Performance**: Processing 100K entities/frame requires cache-friendly data layout
- **Used by**: Unity DOTS, Unreal Mass Framework, EnTT library
- **CAD parallel**: Scene graph can use ECS for large assemblies (millions of parts)
- **Finance**: Entity = financial instrument, Components = price/risk/position data, Systems = calculators

**ECS vs OOP -> Real Decision Framework:**
```
Choose ECS when:
  | Processing 10K+ entities per frame
  | Entities have varied combinations of behaviors
  | Cache performance is critical
  | Systems can be parallelized independently

Choose traditional OOP when:
  | Small number of well-defined entities (<1000)
  | Deep behavior hierarchies with shared logic
  | Object identity and encapsulation matter more than performance
  | Team is more familiar with OOP patterns
```

**Production ECS libraries for C++:**
| Library | Key Feature | Used By |
|---------|------------|---------|
| EnTT | Header-only, fast, sparse sets | Minecraft Bedrock |
| flecs | Query system, REST API, multithreaded | Many indie games |
| DOTS (Unity) | Burst compiler, Jobs system | Unity games |
| Unreal Mass | Integrated with Unreal Engine | AAA games |

---

# ENHANCED SECTION: Architect-Level Pattern Questions

> *These questions test pattern selection judgment, anti-pattern recognition, and real-world trade-off analysis -> the difference between "knows patterns" and "knows WHEN and WHY to apply them."*

---

## Q9: When should you NOT use design patterns? Discuss anti-patterns and over-engineering.

### Answer:

**Over-Engineering Anti-Patterns:**
```cpp
// ANTI-PATTERN 1: "Pattern Happy" | 3 classes for what should be a function
class StringValidatorFactory {
    std::unique_ptr<IStringValidator> create(const std::string& type) { ... }
};
class EmailValidator : public IStringValidator { ... };
class PhoneValidator : public IStringValidator { ... };

// BETTER: Just use a function with a regex
bool validateEmail(std::string_view s) { return std::regex_match(...); }

// ANTI-PATTERN 2: Premature abstraction
class IDatabase { virtual Query query(string sql) = 0; };
// Created when there's only ONE database and no plan to change.
// YAGNI: You Ain't Gonna Need It.

// ANTI-PATTERN 3: God Object disguised as Mediator
class ApplicationMediator {
    void handleUserClick();
    void handleDatabaseUpdate();
    void handleNetworkMessage();
    void handleTimer();
    // 2000 lines of "mediating" everything
};
```

**When patterns HELP vs HURT:**
| Situation | Use Pattern| | Why |
|-----------|-------------|-----|
| 2 concrete types, no plan for more | No | Direct code is simpler |
| Plugin system with unknown future types | Yes (Factory) | Extensibility needed |
| One pricing algorithm, rarely changes | No | Function is fine |
| Hot path called 1M times/sec | Avoid virtual dispatch | CRTP or templates |
| API consumed by 200 teams | Yes (Pimpl, Strategy) | Stability and flexibility |
| Prototype / proof of concept | No | Speed of iteration matters more |

### Explanation:
**Senior architect perspective:** The best engineers I've hired don't recite patterns -> they explain WHY a pattern is the right choice for THIS specific context. "I used Strategy here because pricing rules change quarterly and we have 5 regulatory jurisdictions with different rules" beats "I used Strategy because the book says to."

**The Pattern Gravity Test:** If removing the pattern makes the code simpler AND you don't lose anything meaningful (extensibility, testability, decoupling) -> the pattern was unnecessary.

---

## Q10: Implement the Mediator Pattern for a complex UI with interdependent components.

### Answer:
```cpp
class IMediator;

class UIComponent {
protected:
    IMediator* mediator_;
    std::string name_;
public:
    UIComponent(std::string name, IMediator* m) : name_(std::move(name)), mediator_(m) {}
    virtual ~UIComponent() = default;
    virtual void update(const std::string& event, const std::any& data) = 0;
    void notify(const std::string& event, const std::any& data);
};

class IMediator {
public:
    virtual ~IMediator() = default;
    virtual void onEvent(UIComponent* sender, const std::string& event, 
                         const std::any& data) = 0;
};

class CADToolbarMediator : public IMediator {
    class Toolbar* toolbar_;
    class PropertyPanel* properties_;
    class Viewport* viewport_;
    class StatusBar* statusBar_;

public:
    void onEvent(UIComponent* sender, const std::string& event,
                 const std::any& data) override {
        if (event == "tool_selected") {
            auto tool = std::any_cast<std::string>(data);
            properties_->update("show_tool_properties", data);
            viewport_->update("set_cursor", data);
            statusBar_->update("show_tool_name", data);
        }
        else if (event == "selection_changed") {
            properties_->update("show_selection_properties", data);
            toolbar_->update("enable_edit_tools", data);
        }
    }
};
```

### Explanation:
- **Without Mediator**: N components with N×(N-1) direct dependencies = unmaintainable spaghetti
- **With Mediator**: N components each know only the mediator = star topology
- **iCluster analogy**: The DM_MONITOR acts as a mediator between nodes, groups, and user sessions -> all state changes flow through it
- **Anti-pattern watch**: Mediator becoming a God Object -> split into domain-specific mediators

---

## Q11: Explain the Proxy Pattern variants -> Remote, Virtual, Protection, Smart Reference.

### Answer:
```cpp
// VIRTUAL PROXY | lazy loading of expensive resources
class LazyTexture : public ITexture {
    mutable std::unique_ptr<Texture> real_;
    std::string path_;
    
    void ensureLoaded() const {
        if (!real_) real_ = std::make_unique<Texture>(path_); // Load on first use
    }
public:
    LazyTexture(std::string path) : path_(std::move(path)) {}
    void render(int x, int y) override { ensureLoaded(); real_->render(x, y); }
    int width() const override { ensureLoaded(); return real_->width(); }
};

// PROTECTION PROXY | access control
class SecureDocument : public IDocument {
    std::unique_ptr<Document> doc_;
    User currentUser_;
public:
    void write(const std::string& content) override {
        if (!currentUser_.hasPermission(Permission::WRITE))
            throw AccessDeniedException("Write access denied");
        doc_->write(content);
    }
};

// REMOTE PROXY | hides network communication
class RemoteKVStore : public IKVStore {
    TcpClient client_;
public:
    std::string get(const std::string& key) override {
        client_.send("GET " + key);
        return client_.receive(); // Looks local, but it's network call
    }
};

// SMART REFERENCE PROXY | std::shared_ptr IS a proxy!
// Adds reference counting behavior transparently
```

### Explanation:
- **iCluster uses Remote Proxy**: DMKAPI abstracts whether communication is SNA or TCP/IP -> callers use the same interface regardless of transport protocol
- **CAD use case**: Virtual proxy for large 3D models -> load geometry only when the user zooms in
- **Senior insight**: `std::shared_ptr` is technically a Smart Reference Proxy -> it adds reference counting behavior to raw pointer access transparently

---
