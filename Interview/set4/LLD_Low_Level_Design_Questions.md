# Set 4: Low Level Design (LLD)
## Senior-Level Interview Questions (10 Years Experience)

---

## Q1: Design a Parking Lot System

### Requirements:
- Multiple floors, each with different spot sizes (Compact, Regular, Large)
- Support for different vehicle types (Motorcycle, Car, Truck)
- Track available spots, assign nearest spot
- Calculate parking fee based on duration

### Answer:

```cpp
#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <chrono>
#include <string>
#include <mutex>
#include <optional>

// === Enums ===
enum class VehicleType { MOTORCYCLE, CAR, TRUCK };
enum class SpotSize { COMPACT, REGULAR, LARGE };

// === Vehicle ===
class Vehicle {
    std::string licensePlate_;
    VehicleType type_;
public:
    Vehicle(std::string plate, VehicleType type)
        : licensePlate_(std::move(plate)), type_(type) {}
    VehicleType type() const { return type_; }
    const std::string& plate() const { return licensePlate_; }

    SpotSize requiredSpotSize() const {
        switch (type_) {
            case VehicleType::MOTORCYCLE: return SpotSize::COMPACT;
            case VehicleType::CAR:        return SpotSize::REGULAR;
            case VehicleType::TRUCK:      return SpotSize::LARGE;
        }
        return SpotSize::REGULAR;
    }
};

// === Parking Spot ===
class ParkingSpot {
    int id_;
    int floor_;
    SpotSize size_;
    Vehicle* vehicle_ = nullptr;

public:
    ParkingSpot(int id, int floor, SpotSize size)
        : id_(id), floor_(floor), size_(size) {}

    bool isAvailable() const { return vehicle_ == nullptr; }
    bool canFit(const Vehicle& v) const {
        return isAvailable() && v.requiredSpotSize() <= size_;
    }
    void park(Vehicle& v) { vehicle_ = &v; }
    void vacate() { vehicle_ = nullptr; }

    int id() const { return id_; }
    int floor() const { return floor_; }
    SpotSize size() const { return size_; }
};

// === Ticket ===
struct ParkingTicket {
    int ticketId;
    std::string licensePlate;
    int spotId;
    std::chrono::steady_clock::time_point entryTime;
};

// === Fee Strategy ===
class IFeeStrategy {
public:
    virtual ~IFeeStrategy() = default;
    virtual double calculate(VehicleType type, std::chrono::minutes duration) const = 0;
};

class HourlyFeeStrategy : public IFeeStrategy {
public:
    double calculate(VehicleType type, std::chrono::minutes duration) const override {
        int hours = static_cast<int>(std::ceil(duration.count() / 60.0));
        double rate = 0;
        switch (type) {
            case VehicleType::MOTORCYCLE: rate = 1.0; break;
            case VehicleType::CAR:        rate = 2.0; break;
            case VehicleType::TRUCK:      rate = 3.0; break;
        }
        return hours * rate;
    }
};

// === Parking Lot ===
class ParkingLot {
    std::vector<ParkingSpot> spots_;
    std::unordered_map<std::string, ParkingTicket> activeTickets_;  // plate -> ticket
    std::unordered_map<int, int> spotIndexById_;  // spotId -> index
    std::unique_ptr<IFeeStrategy> feeStrategy_;
    int nextTicketId_ = 1;
    mutable std::mutex mutex_;

    // Available spots by size (min-heap by spot ID for nearest spot)
    std::unordered_map<SpotSize, std::priority_queue<int, std::vector<int>, std::greater<>>> available_;

public:
    ParkingLot(std::unique_ptr<IFeeStrategy> fee) : feeStrategy_(std::move(fee)) {}

    void addSpot(int id, int floor, SpotSize size) {
        spotIndexById_[id] = spots_.size();
        spots_.emplace_back(id, floor, size);
        available_[size].push(id);
    }

    std::optional<ParkingTicket> parkVehicle(Vehicle& vehicle) {
        std::lock_guard lock(mutex_);

        // Find best fit: try exact size first, then larger
        SpotSize needed = vehicle.requiredSpotSize();
        int spotId = -1;

        for (int s = static_cast<int>(needed); s <= static_cast<int>(SpotSize::LARGE); ++s) {
            auto size = static_cast<SpotSize>(s);
            if (!available_[size].empty()) {
                spotId = available_[size].top();
                available_[size].pop();
                break;
            }
        }

        if (spotId == -1) return std::nullopt;  // No spot available

        auto& spot = spots_[spotIndexById_[spotId]];
        spot.park(vehicle);

        ParkingTicket ticket{nextTicketId_++, vehicle.plate(), spotId,
                             std::chrono::steady_clock::now()};
        activeTickets_[vehicle.plate()] = ticket;
        return ticket;
    }

    double unparkVehicle(const std::string& licensePlate) {
        std::lock_guard lock(mutex_);

        auto it = activeTickets_.find(licensePlate);
        if (it == activeTickets_.end()) return -1;

        auto& ticket = it->second;
        auto& spot = spots_[spotIndexById_[ticket.spotId]];

        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            std::chrono::steady_clock::now() - ticket.entryTime);

        double fee = feeStrategy_->calculate(VehicleType::CAR, duration);

        spot.vacate();
        available_[spot.size()].push(spot.id());
        activeTickets_.erase(it);

        return fee;
    }

    int availableSpots() const {
        std::lock_guard lock(mutex_);
        int count = 0;
        for (const auto& [size, pq] : available_)
            count += pq.size();
        return count;
    }
};
```

### Design Decisions & Explanation:
1. **Strategy Pattern** for fee calculation → easy to swap hourly/daily/tiered pricing
2. **Priority Queue** for spot assignment → O(log n) nearest spot allocation
3. **Thread-safe** with mutex → required for multi-entrance parking lots
4. **Vehicle-Spot compatibility** uses enum ordering (COMPACT < REGULAR < LARGE)
5. **Ticket system** links vehicle to spot and records entry time

**Follow-up questions to expect:**
- How to handle multiple entrances/exits? (Distributed lock or sharding by floor)
- How to add electric vehicle charging spots? (Add EVChargingSpot subclass or component)
- How to display available spots on LED boards? (Observer pattern on spot count changes)

---

## Q2: Design a Chess Game

### Answer:
```cpp
#include <memory>
#include <vector>
#include <array>
#include <optional>
#include <iostream>

enum class Color { WHITE, BLACK };
enum class PieceType { KING, QUEEN, ROOK, BISHOP, KNIGHT, PAWN };

struct Position {
    int row, col;
    bool isValid() const { return row >= 0 && row < 8 && col >= 0 && col < 8; }
    bool operator==(const Position& o) const { return row == o.row && col == o.col; }
};

// Forward declaration
class Board;

// === Piece ===
class Piece {
protected:
    Color color_;
    PieceType type_;
    bool hasMoved_ = false;

public:
    Piece(Color c, PieceType t) : color_(c), type_(t) {}
    virtual ~Piece() = default;

    Color color() const { return color_; }
    PieceType type() const { return type_; }
    bool hasMoved() const { return hasMoved_; }
    void setMoved() { hasMoved_ = true; }

    virtual std::vector<Position> getValidMoves(Position from, const Board& board) const = 0;
    virtual char symbol() const = 0;
};

// === Board ===
class Board {
    std::array<std::array<std::unique_ptr<Piece>, 8>, 8> grid_;

public:
    Piece* at(Position pos) const {
        if (!pos.isValid()) return nullptr;
        return grid_[pos.row][pos.col].get();
    }

    void place(Position pos, std::unique_ptr<Piece> piece) {
        grid_[pos.row][pos.col] = std::move(piece);
    }

    std::unique_ptr<Piece> remove(Position pos) {
        return std::move(grid_[pos.row][pos.col]);
    }

    bool isOccupied(Position pos) const {
        return pos.isValid() && grid_[pos.row][pos.col] != nullptr;
    }

    bool isEnemy(Position pos, Color myColor) const {
        auto* p = at(pos);
        return p && p->color() != myColor;
    }

    bool isEmptyOrEnemy(Position pos, Color myColor) const {
        return pos.isValid() && (!isOccupied(pos) || isEnemy(pos, myColor));
    }

    Position findKing(Color color) const {
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c)
                if (auto* p = at({r, c}); p && p->type() == PieceType::KING && p->color() == color)
                    return {r, c};
        return {-1, -1};
    }
};

// === Concrete Pieces (showing Knight and Rook as examples) ===
class Knight : public Piece {
public:
    Knight(Color c) : Piece(c, PieceType::KNIGHT) {}
    char symbol() const override { return color_ == Color::WHITE ? 'N' : 'n'; }

    std::vector<Position> getValidMoves(Position from, const Board& board) const override {
        std::vector<Position> moves;
        int offsets[][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
        for (auto [dr, dc] : offsets) {
            Position to{from.row + dr, from.col + dc};
            if (board.isEmptyOrEnemy(to, color_))
                moves.push_back(to);
        }
        return moves;
    }
};

class Rook : public Piece {
public:
    Rook(Color c) : Piece(c, PieceType::ROOK) {}
    char symbol() const override { return color_ == Color::WHITE ? 'R' : 'r'; }

    std::vector<Position> getValidMoves(Position from, const Board& board) const override {
        std::vector<Position> moves;
        int dirs[][2] = {{0,1},{0,-1},{1,0},{-1,0}};
        for (auto [dr, dc] : dirs) {
            for (int i = 1; i < 8; ++i) {
                Position to{from.row + dr*i, from.col + dc*i};
                if (!to.isValid()) break;
                if (!board.isOccupied(to)) {
                    moves.push_back(to);
                } else {
                    if (board.isEnemy(to, color_)) moves.push_back(to);
                    break;
                }
            }
        }
        return moves;
    }
};

// === Game ===
class ChessGame {
    Board board_;
    Color currentTurn_ = Color::WHITE;
    bool gameOver_ = false;

public:
    ChessGame() { setupBoard(); }

    void setupBoard() {
        // Place pieces... (abbreviated for brevity)
        // Row 0: Black back rank, Row 1: Black pawns
        // Row 6: White pawns, Row 7: White back rank
    }

    bool makeMove(Position from, Position to) {
        auto* piece = board_.at(from);
        if (!piece || piece->color() != currentTurn_) return false;

        auto validMoves = piece->getValidMoves(from, board_);
        bool isValid = false;
        for (const auto& m : validMoves)
            if (m == to) { isValid = true; break; }

        if (!isValid) return false;

        // Execute move
        auto captured = board_.remove(to);
        auto movingPiece = board_.remove(from);
        movingPiece->setMoved();
        board_.place(to, std::move(movingPiece));

        // Check if move puts own king in check → if so, undo
        if (isInCheck(currentTurn_)) {
            // Undo
            board_.place(from, board_.remove(to));
            if (captured) board_.place(to, std::move(captured));
            return false;
        }

        // Switch turns
        currentTurn_ = (currentTurn_ == Color::WHITE) ? Color::BLACK : Color::WHITE;

        // Check for checkmate
        if (isCheckmate(currentTurn_)) {
            gameOver_ = true;
        }

        return true;
    }

    bool isInCheck(Color color) const {
        Position kingPos = board_.findKing(color);
        Color enemy = (color == Color::WHITE) ? Color::BLACK : Color::WHITE;

        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c) {
                auto* p = board_.at({r, c});
                if (p && p->color() == enemy) {
                    auto moves = p->getValidMoves({r, c}, board_);
                    for (const auto& m : moves)
                        if (m == kingPos) return true;
                }
            }
        return false;
    }

    bool isCheckmate(Color color) {
        if (!isInCheck(color)) return false;
        // Try all possible moves for this color
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c) {
                auto* p = board_.at({r, c});
                if (p && p->color() == color) {
                    auto moves = p->getValidMoves({r, c}, board_);
                    for (const auto& to : moves)
                        // Try move and see if still in check
                        // (simplified → actual implementation uses makeMove/undo)
                        return false;  // Found at least one legal move
                }
            }
        return true;  // No legal moves while in check = checkmate
    }
};
```

### Key Design Decisions:
1. **Polymorphism for pieces** ? each piece knows its valid moves
2. **Board owns pieces** via `unique_ptr` ? clear ownership
3. **Move validation** separated from move execution
4. **Check detection** by iterating opponent pieces
5. **Extensible**: Easy to add special moves (castling, en passant, promotion)

**Special Moves (interviewers often ask about these):**
```cpp
// === Castling ===
bool canCastle(Color color, bool kingSide) const {
    int row = (color == Color::WHITE) ? 7 : 0;
    auto* king = board_.at({row, 4});
    int rookCol = kingSide ? 7 : 0;
    auto* rook = board_.at({row, rookCol});
    
    if (!king || king->hasMoved() || !rook || rook->hasMoved()) return false;
    if (isInCheck(color)) return false;
    
    // Check all squares between king and rook are empty
    int startCol = kingSide ? 5 : 1;
    int endCol = kingSide ? 6 : 3;
    for (int c = startCol; c <= endCol; ++c)
        if (board_.isOccupied({row, c})) return false;
    
    // King must not pass through check
    int step = kingSide ? 1 : -1;
    for (int c = 4; c != (kingSide ? 7 : 1); c += step)
        if (isSquareAttacked({row, c}, color)) return false;
    
    return true;
}

// === En Passant ===
// Requires tracking last move
struct LastMove {
    Position from, to;
    PieceType type;
};
std::optional<LastMove> lastMove_;

bool isEnPassant(Position from, Position to) const {
    auto* pawn = board_.at(from);
    if (!pawn || pawn->type() != PieceType::PAWN) return false;
    if (!lastMove_) return false;
    
    // Opponent pawn just moved 2 squares
    if (lastMove_->type != PieceType::PAWN) return false;
    if (std::abs(lastMove_->from.row - lastMove_->to.row) != 2) return false;
    
    // Capture square is where opponent pawn passed through
    return to.col == lastMove_->to.col &&
           to.row == (from.row + (pawn->color() == Color::WHITE | -1 : 1));
}

// === Pawn Promotion ===
void handlePromotion(Position pos, Color color) {
    // In real LLD: ask player for piece choice
    // Default to Queen (most common)
    board_.place(pos, std::make_unique<Queen>(color));
}

bool isPawnPromotion(Position to, Color color) const {
    return (color == Color::WHITE && to.row == 0) ||
           (color == Color::BLACK && to.row == 7);
}
```

**Additional features to mention in interviews:**
- **Stalemate detection**: No legal moves but not in check → draw
- **Threefold repetition**: Same position 3 times → draw (need position hash history)
- **50-move rule**: 50 moves without capture or pawn move → draw
- **FEN notation**: Serialize/deserialize board state for save/load
- **Move notation**: Algebraic notation (e.g., "Nf3", "O-O")

---

## Q3: Design an Elevator System

### Answer:
```cpp
#include <queue>
#include <vector>
#include <set>
#include <mutex>
#include <functional>

enum class Direction { UP, DOWN, IDLE };

class Elevator {
    int id_;
    int currentFloor_ = 0;
    Direction direction_ = Direction::IDLE;
    int capacity_;
    int currentLoad_ = 0;
    std::set<int> destinations_;  // Sorted floors to visit
    mutable std::mutex mutex_;

public:
    Elevator(int id, int capacity = 10)
        : id_(id), capacity_(capacity) {}

    int currentFloor() const { return currentFloor_; }
    Direction direction() const { return direction_; }
    bool isFull() const { return currentLoad_ >= capacity_; }
    bool isIdle() const { return direction_ == Direction::IDLE; }

    void addDestination(int floor) {
        std::lock_guard lock(mutex_);
        destinations_.insert(floor);
        updateDirection();
    }

    void step() {
        std::lock_guard lock(mutex_);
        if (destinations_.empty()) {
            direction_ = Direction::IDLE;
            return;
        }

        if (direction_ == Direction::UP) currentFloor_++;
        else if (direction_ == Direction::DOWN) currentFloor_--;

        // Check if we've arrived at a destination
        if (destinations_.count(currentFloor_)) {
            destinations_.erase(currentFloor_);
            // Open doors, load/unload
        }

        updateDirection();
    }

    int distanceTo(int floor) const {
        return std::abs(currentFloor_ - floor);
    }

    // Cost function for scheduler
    int estimatedCost(int pickupFloor, Direction requestDir) const {
        if (isIdle()) return distanceTo(pickupFloor);

        bool sameDirection = (direction_ == Direction::UP && pickupFloor >= currentFloor_) ||
                             (direction_ == Direction::DOWN && pickupFloor <= currentFloor_);

        if (sameDirection) return distanceTo(pickupFloor);
        return distanceTo(pickupFloor) + 20;  // Penalty for direction change
    }

private:
    void updateDirection() {
        if (destinations_.empty()) {
            direction_ = Direction::IDLE;
        } else if (*destinations_.rbegin() > currentFloor_) {
            direction_ = Direction::UP;
        } else {
            direction_ = Direction::DOWN;
        }
    }
};

// === Scheduling Strategy ===
class ISchedulingStrategy {
public:
    virtual ~ISchedulingStrategy() = default;
    virtual int selectElevator(const std::vector<Elevator>& elevators,
                               int floor, Direction dir) = 0;
};

// LOOK Algorithm (similar to disk scheduling)
class LOOKScheduler : public ISchedulingStrategy {
public:
    int selectElevator(const std::vector<Elevator>& elevators,
                       int floor, Direction dir) override {
        int bestIdx = 0;
        int bestCost = INT_MAX;

        for (int i = 0; i < static_cast<int>(elevators.size()); ++i) {
            if (elevators[i].isFull()) continue;
            int cost = elevators[i].estimatedCost(floor, dir);
            if (cost < bestCost) {
                bestCost = cost;
                bestIdx = i;
            }
        }
        return bestIdx;
    }
};

// === Controller ===
class ElevatorController {
    std::vector<Elevator> elevators_;
    std::unique_ptr<ISchedulingStrategy> scheduler_;
    int totalFloors_;

public:
    ElevatorController(int numElevators, int totalFloors,
                       std::unique_ptr<ISchedulingStrategy> scheduler)
        : totalFloors_(totalFloors), scheduler_(std::move(scheduler)) {
        for (int i = 0; i < numElevators; ++i)
            elevators_.emplace_back(i);
    }

    void requestElevator(int floor, Direction dir) {
        int elevIdx = scheduler_->selectElevator(elevators_, floor, dir);
        elevators_[elevIdx].addDestination(floor);
    }

    void selectFloor(int elevatorId, int floor) {
        elevators_[elevatorId].addDestination(floor);
    }

    void tick() {
        for (auto& elev : elevators_)
            elev.step();
    }
};
```

### Explanation:
- **LOOK scheduling** (elevator algorithm) → serves requests in current direction, then reverses
- **Strategy pattern** for scheduling → can swap to SCAN, SSTF (Shortest Seek Time First)
- **Cost function** considers: distance, current direction, load
- **Thread-safe** ? each elevator has its own mutex
- **Extensible**: Add VIP priority, express elevators, maintenance mode

---

## Q4: Design an In-Memory File System

### Answer:
```cpp
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <sstream>
#include <stdexcept>

class FileSystemNode {
public:
    std::string name;
    bool isDirectory;
    std::string content;  // Only for files
    std::unordered_map<std::string, std::unique_ptr<FileSystemNode>> children;
    FileSystemNode* parent = nullptr;

    FileSystemNode(std::string n, bool isDir, FileSystemNode* par = nullptr)
        : name(std::move(n)), isDirectory(isDir), parent(par) {}
};

class FileSystem {
    std::unique_ptr<FileSystemNode> root_;

    std::vector<std::string> parsePath(const std::string& path) {
        std::vector<std::string> parts;
        std::istringstream ss(path);
        std::string token;
        while (std::getline(ss, token, '/'))
            if (!token.empty()) parts.push_back(token);
        return parts;
    }

    FileSystemNode* navigate(const std::string& path) {
        auto parts = parsePath(path);
        FileSystemNode* curr = root_.get();
        for (const auto& part : parts) {
            auto it = curr->children.find(part);
            if (it == curr->children.end()) return nullptr;
            curr = it->second.get();
        }
        return curr;
    }

    FileSystemNode* navigateParent(const std::string& path, std::string& childName) {
        auto parts = parsePath(path);
        if (parts.empty()) return nullptr;
        childName = parts.back();
        parts.pop_back();

        FileSystemNode* curr = root_.get();
        for (const auto& part : parts) {
            auto it = curr->children.find(part);
            if (it == curr->children.end()) return nullptr;
            curr = it->second.get();
        }
        return curr;
    }

public:
    FileSystem() : root_(std::make_unique<FileSystemNode>("/", true)) {}

    void mkdir(const std::string& path) {
        std::string name;
        auto* parent = navigateParent(path, name);
        if (!parent || !parent->isDirectory)
            throw std::runtime_error("Invalid path");
        if (parent->children.count(name))
            throw std::runtime_error("Already exists");

        parent->children[name] = std::make_unique<FileSystemNode>(name, true, parent);
    }

    void createFile(const std::string& path, const std::string& content = "") {
        std::string name;
        auto* parent = navigateParent(path, name);
        if (!parent || !parent->isDirectory)
            throw std::runtime_error("Invalid path");

        auto node = std::make_unique<FileSystemNode>(name, false, parent);
        node->content = content;
        parent->children[name] = std::move(node);
    }

    std::string readFile(const std::string& path) {
        auto* node = navigate(path);
        if (!node || node->isDirectory)
            throw std::runtime_error("Not a file");
        return node->content;
    }

    void writeFile(const std::string& path, const std::string& content) {
        auto* node = navigate(path);
        if (!node || node->isDirectory)
            throw std::runtime_error("Not a file");
        node->content = content;
    }

    std::vector<std::string> ls(const std::string& path) {
        auto* node = navigate(path);
        if (!node) throw std::runtime_error("Path not found");

        if (!node->isDirectory) return {node->name};

        std::vector<std::string> result;
        for (const auto& [name, child] : node->children)
            result.push_back(name);
        std::sort(result.begin(), result.end());
        return result;
    }

    void rm(const std::string& path) {
        std::string name;
        auto* parent = navigateParent(path, name);
        if (!parent) throw std::runtime_error("Path not found");
        parent->children.erase(name);  // unique_ptr handles recursive cleanup
    }
};
```

### Explanation:
- **Tree structure** with `unique_ptr` for automatic cleanup
- **Path parsing** splits on `/` ? handles absolute paths
- **Recursive delete** is free thanks to `unique_ptr` ownership
- **Interview extensions**: Add permissions, symlinks, file size tracking, search

---

## Q5: Design a Rate Limiter (Token Bucket + Sliding Window)

### Answer:
```cpp
#include <chrono>
#include <deque>
#include <mutex>

// Token Bucket Algorithm
class TokenBucketLimiter {
    double tokens_;
    double maxTokens_;
    double refillRate_;  // tokens per second
    std::chrono::steady_clock::time_point lastRefill_;
    mutable std::mutex mutex_;

    void refill() {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastRefill_).count();
        tokens_ = std::min(maxTokens_, tokens_ + elapsed * refillRate_);
        lastRefill_ = now;
    }

public:
    TokenBucketLimiter(double maxTokens, double refillRate)
        : tokens_(maxTokens), maxTokens_(maxTokens), refillRate_(refillRate),
          lastRefill_(std::chrono::steady_clock::now()) {}

    bool tryConsume(double tokens = 1.0) {
        std::lock_guard lock(mutex_);
        refill();
        if (tokens_ >= tokens) {
            tokens_ -= tokens;
            return true;
        }
        return false;
    }
};

// Sliding Window Counter
class SlidingWindowLimiter {
    int maxRequests_;
    std::chrono::seconds windowSize_;
    std::deque<std::chrono::steady_clock::time_point> requests_;
    mutable std::mutex mutex_;

    void evictExpired() {
        auto cutoff = std::chrono::steady_clock::now() - windowSize_;
        while (!requests_.empty() && requests_.front() < cutoff)
            requests_.pop_front();
    }

public:
    SlidingWindowLimiter(int maxRequests, std::chrono::seconds window)
        : maxRequests_(maxRequests), windowSize_(window) {}

    bool tryRequest() {
        std::lock_guard lock(mutex_);
        evictExpired();
        if (static_cast<int>(requests_.size()) >= maxRequests_)
            return false;
        requests_.push_back(std::chrono::steady_clock::now());
        return true;
    }
};

// === API Gateway combining both ===
class APIRateLimiter {
    std::unordered_map<std::string, TokenBucketLimiter> perUserLimiters_;
    TokenBucketLimiter globalLimiter_;
    std::mutex mapMutex_;

public:
    APIRateLimiter(double globalRate)
        : globalLimiter_(globalRate * 10, globalRate) {}

    bool allowRequest(const std::string& userId) {
        if (!globalLimiter_.tryConsume()) return false;

        std::lock_guard lock(mapMutex_);
        auto [it, inserted] = perUserLimiters_.try_emplace(
            userId, 100.0, 10.0);  // 100 burst, 10/sec per user
        return it->second.tryConsume();
    }
};
```

### Explanation:
| Algorithm | Pros | Cons | Use Case |
|-----------|------|------|----------|
| Token Bucket | Allows bursts, smooth rate | Memory efficient | API rate limiting |
| Sliding Window | Exact count in window | More memory (stores timestamps) | Strict compliance |
| Fixed Window | Simplest | Boundary burst problem | Simple counters |

**Finance**: Order rate limiting (exchange limits), API call budgets
**Gaming**: Action rate limiting (anti-cheat), message throttling

---

## Q6: Design a Logger Framework

### Answer:
```cpp
#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <functional>

enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string file;
    int line;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id threadId;
};

// === Sink Interface ===
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void write(const LogEntry& entry) = 0;
    virtual void flush() = 0;
};

class ConsoleSink : public ILogSink {
    std::mutex mutex_;
public:
    void write(const LogEntry& entry) override {
        std::lock_guard lock(mutex_);
        std::cout << formatEntry(entry) << "\n";
    }
    void flush() override { std::cout.flush(); }

private:
    std::string formatEntry(const LogEntry& entry) {
        auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time), "%H:%M:%S")
           << " [" << levelToString(entry.level) << "] "
           << entry.message
           << " (" << entry.file << ":" << entry.line << ")";
        return ss.str();
    }

    static const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
        }
        return "+---+";
    }
};

class FileSink : public ILogSink {
    std::ofstream file_;
    std::mutex mutex_;
    size_t maxSize_;
    size_t currentSize_ = 0;
    std::string basePath_;
    int rotationCount_ = 0;

public:
    FileSink(const std::string& path, size_t maxSize = 10 * 1024 * 1024)
        : basePath_(path), maxSize_(maxSize) {
        file_.open(path, std::ios::app);
    }

    void write(const LogEntry& entry) override {
        std::lock_guard lock(mutex_);
        // Rotation check
        if (currentSize_ > maxSize_) rotate();

        std::string msg = /* format entry */ entry.message + "\n";
        file_ << msg;
        currentSize_ += msg.size();
    }

    void flush() override { file_.flush(); }

private:
    void rotate() {
        file_.close();
        std::string newName = basePath_ + "." + std::to_string(++rotationCount_);
        std::rename(basePath_.c_str(), newName.c_str());
        file_.open(basePath_, std::ios::app);
        currentSize_ = 0;
    }
};

// === Logger ===
class Logger {
    LogLevel minLevel_ = LogLevel::INFO;
    std::vector<std::unique_ptr<ILogSink>> sinks_;

public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void setLevel(LogLevel level) { minLevel_ = level; }
    void addSink(std::unique_ptr<ILogSink> sink) {
        sinks_.push_back(std::move(sink));
    }

    void log(LogLevel level, const std::string& msg,
             const char* file = "", int line = 0) {
        if (level < minLevel_) return;

        LogEntry entry{level, msg, file, line,
                       std::chrono::system_clock::now(),
                       std::this_thread::get_id()};

        for (auto& sink : sinks_)
            sink->write(entry);
    }
};

// Convenience macros
#define LOG_INFO(msg)  Logger::instance().log(LogLevel::INFO, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, msg, __FILE__, __LINE__)
```

### Explanation:
- **Multiple sinks** (Console, File, Network) → open for extension
- **Log rotation** ? prevents disk exhaustion
- **Thread-safe** per sink
- **Level filtering** ? compile-time level can also be added with `if constexpr`
- **Production features to discuss**: Async logging with ring buffer, structured logging (JSON), log correlation IDs

**Async Logger (production-grade):**
```cpp
class AsyncLogger {
    struct LogMessage { LogLevel level; std::string message; /* ... */ };
    
    std::queue<LogMessage> buffer_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::jthread workerThread_;
    std::vector<std::unique_ptr<ILogSink>> sinks_;
    bool stopping_ = false;
    static constexpr size_t MAX_QUEUE = 10000;

public:
    AsyncLogger() : workerThread_([this](std::stop_token st) { workerLoop(st); }) {}

    void log(LogLevel level, std::string msg) {
        std::unique_lock lock(mutex_);
        if (buffer_.size() >= MAX_QUEUE) {
            buffer_.pop();  // Drop oldest (or block → policy choice)
        }
        buffer_.push({level, std::move(msg)});
        cv_.notify_one();
    }

private:
    void workerLoop(std::stop_token st) {
        while (!st.stop_requested()) {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return !buffer_.empty() || st.stop_requested(); });
            
            // Drain all pending messages (batch write)
            std::queue<LogMessage> batch;
            std::swap(batch, buffer_);
            lock.unlock();
            
            while (!batch.empty()) {
                for (auto& sink : sinks_) sink->write(batch.front());
                batch.pop();
            }
        }
    }
};
// Performance: ~10x faster than sync logging in high-throughput scenarios
// Trade-off: Messages may be lost on crash (not yet flushed)
```

**Structured Logging (JSON format for ELK/Splunk):**
```cpp
class JsonSink : public ILogSink {
    void write(const LogEntry& entry) override {
        // Output: {"timestamp":"2026-06-15T10:30:00Z","level":"ERROR",
        //          "msg":"Connection failed","file":"net.cpp","line":42,
        //          "thread":"0x1234","correlation_id":"abc-123"}
        std::cout << "{\"timestamp\":\"" << formatISO8601(entry.timestamp)
                  << "\",\"level\":\"" << levelStr(entry.level)
                  << "\",\"msg\":\"" << escapeJson(entry.message)
                  << "\",\"file\":\"" << entry.file
                  << "\",\"line\":" << entry.line << "}\n";
    }
};
```

---

## Q7: Design a Connection Pool

### Answer:
```cpp
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>
#include <chrono>

template<typename Connection>
class ConnectionPool {
    using ConnectionPtr = std::shared_ptr<Connection>;
    using Factory = std::function<ConnectionPtr()>;
    using Validator = std::function<bool(const ConnectionPtr&)>;

    Factory factory_;
    Validator validator_;
    std::queue<ConnectionPtr> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    int maxSize_;
    int activeCount_ = 0;

public:
    ConnectionPool(Factory factory, Validator validator, int maxSize)
        : factory_(std::move(factory)),
          validator_(std::move(validator)),
          maxSize_(maxSize) {}

    // RAII wrapper that returns connection to pool on destruction
    class ConnectionGuard {
        ConnectionPool& pool_;
        ConnectionPtr conn_;
    public:
        ConnectionGuard(ConnectionPool& pool, ConnectionPtr conn)
            : pool_(pool), conn_(std::move(conn)) {}
        ~ConnectionGuard() { pool_.release(std::move(conn_)); }

        Connection& operator*() { return *conn_; }
        Connection* operator->() { return conn_.get(); }

        ConnectionGuard(const ConnectionGuard&) = delete;
        ConnectionGuard& operator=(const ConnectionGuard&) = delete;
        ConnectionGuard(ConnectionGuard&&) = default;
    };

    ConnectionGuard acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        std::unique_lock lock(mutex_);

        // Try to get an existing connection
        while (!pool_.empty()) {
            auto conn = std::move(pool_.front());
            pool_.pop();
            if (validator_(conn)) {
                return ConnectionGuard(*this, std::move(conn));
            }
            --activeCount_;  // Invalid connection, discard
        }

        // Create new if under limit
        if (activeCount_ < maxSize_) {
            ++activeCount_;
            lock.unlock();
            return ConnectionGuard(*this, factory_());
        }

        // Wait for a connection to be returned
        if (!cv_.wait_for(lock, timeout, [this] { return !pool_.empty(); })) {
            throw std::runtime_error("Connection pool timeout");
        }

        auto conn = std::move(pool_.front());
        pool_.pop();
        return ConnectionGuard(*this, std::move(conn));
    }

private:
    void release(ConnectionPtr conn) {
        std::lock_guard lock(mutex_);
        if (validator_(conn)) {
            pool_.push(std::move(conn));
            cv_.notify_one();
        } else {
            --activeCount_;
        }
    }
};
```

### Explanation:
- **RAII ConnectionGuard** ? connection auto-returns to pool (no leak possible)
- **Health check** via validator → dead connections are discarded
- **Bounded pool** with timeout → prevents resource exhaustion
- **Condition variable** ? efficient waiting (no busy-spin)
- **Used in**: Database connections, network sockets, GPU resource pools in CAD/Gaming

**Eviction Policies (common follow-up):**
| Policy | How it works | When to use |
|--------|-------------|-------------|
| FIFO | Evict oldest idle connection | Simple, good default |
| LRU | Evict least recently used | When fresh connections are preferred |
| Max Idle Time | Evict if idle > threshold | Prevent stale connections |
| Max Lifetime | Evict after total age > limit | Handle server-side timeouts |
| Min Idle | Keep at least N connections warm | Avoid cold-start latency |

```cpp
// Idle timeout eviction
void evictStale(std::chrono::seconds maxIdle) {
    std::lock_guard lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    while (!pool_.empty()) {
        auto& [conn, lastUsed] = pool_.front();
        if (now - lastUsed > maxIdle) {
            pool_.pop();
            --activeCount_;
        } else break;
    }
}
```

---

## Q8: Design a Vending Machine (State Machine)

### Answer:
```cpp
#include <unordered_map>
#include <string>
#include <memory>
#include <iostream>
#include <stdexcept>

// Products
struct Product {
    std::string name;
    double price;
    int quantity;
};

// Forward declaration
class VendingMachine;

// === State Interface ===
class IVendingState {
public:
    virtual ~IVendingState() = default;
    virtual void insertMoney(VendingMachine& vm, double amount) = 0;
    virtual void selectProduct(VendingMachine& vm, const std::string& code) = 0;
    virtual void dispense(VendingMachine& vm) = 0;
    virtual void cancelTransaction(VendingMachine& vm) = 0;
    virtual std::string stateName() const = 0;
};

// === Concrete States ===
class IdleState : public IVendingState {
public:
    void insertMoney(VendingMachine& vm, double amount) override;
    void selectProduct(VendingMachine& vm, const std::string& code) override {
        std::cout << "Please insert money first.\n";
    }
    void dispense(VendingMachine& vm) override {
        std::cout << "Please insert money and select product.\n";
    }
    void cancelTransaction(VendingMachine& vm) override {
        std::cout << "No transaction to cancel.\n";
    }
    std::string stateName() const override { return "IDLE"; }
};

class HasMoneyState : public IVendingState {
public:
    void insertMoney(VendingMachine& vm, double amount) override;
    void selectProduct(VendingMachine& vm, const std::string& code) override;
    void dispense(VendingMachine& vm) override {
        std::cout << "Please select a product first.\n";
    }
    void cancelTransaction(VendingMachine& vm) override;
    std::string stateName() const override { return "HAS_MONEY"; }
};

class DispensingState : public IVendingState {
public:
    void insertMoney(VendingMachine& vm, double amount) override {
        std::cout << "Please wait, dispensing in progress.\n";
    }
    void selectProduct(VendingMachine& vm, const std::string& code) override {
        std::cout << "Please wait, dispensing in progress.\n";
    }
    void dispense(VendingMachine& vm) override;
    void cancelTransaction(VendingMachine& vm) override {
        std::cout << "Cannot cancel during dispensing.\n";
    }
    std::string stateName() const override { return "DISPENSING"; }
};

// === Vending Machine ===
class VendingMachine {
    std::unordered_map<std::string, Product> inventory_;
    std::unique_ptr<IVendingState> state_;
    double currentBalance_ = 0;
    std::string selectedProduct_;

    friend class IdleState;
    friend class HasMoneyState;
    friend class DispensingState;

public:
    VendingMachine() : state_(std::make_unique<IdleState>()) {}

    void addProduct(std::string code, Product product) {
        inventory_[std::move(code)] = std::move(product);
    }

    void insertMoney(double amount) { state_->insertMoney(*this, amount); }
    void selectProduct(const std::string& code) { state_->selectProduct(*this, code); }
    void dispense() { state_->dispense(*this); }
    void cancel() { state_->cancelTransaction(*this); }

    void setState(std::unique_ptr<IVendingState> newState) {
        std::cout << "State: " << state_->stateName() << " ? " << newState->stateName() << "\n";
        state_ = std::move(newState);
    }
};

// State implementations
void IdleState::insertMoney(VendingMachine& vm, double amount) {
    vm.currentBalance_ += amount;
    std::cout << "Inserted $" << amount << ". Balance: $" << vm.currentBalance_ << "\n";
    vm.setState(std::make_unique<HasMoneyState>());
}

void HasMoneyState::insertMoney(VendingMachine& vm, double amount) {
    vm.currentBalance_ += amount;
    std::cout << "Added $" << amount << ". Balance: $" << vm.currentBalance_ << "\n";
}

void HasMoneyState::selectProduct(VendingMachine& vm, const std::string& code) {
    auto it = vm.inventory_.find(code);
    if (it == vm.inventory_.end()) { std::cout << "Invalid product.\n"; return; }
    if (it->second.quantity == 0) { std::cout << "Out of stock.\n"; return; }
    if (vm.currentBalance_ < it->second.price) {
        std::cout << "Insufficient funds. Need $" << it->second.price << "\n"; return;
    }
    vm.selectedProduct_ = code;
    vm.setState(std::make_unique<DispensingState>());
    vm.dispense();  // Auto-dispense
}

void HasMoneyState::cancelTransaction(VendingMachine& vm) {
    std::cout << "Refunding $" << vm.currentBalance_ << "\n";
    vm.currentBalance_ = 0;
    vm.setState(std::make_unique<IdleState>());
}

void DispensingState::dispense(VendingMachine& vm) {
    auto& product = vm.inventory_[vm.selectedProduct_];
    product.quantity--;
    double change = vm.currentBalance_ - product.price;
    std::cout << "Dispensing: " << product.name << "\n";
    if (change > 0) std::cout << "Change: $" << change << "\n";
    vm.currentBalance_ = 0;
    vm.selectedProduct_.clear();
    vm.setState(std::make_unique<IdleState>());
}
```

### Explanation:
- **State Pattern** ? each state handles events differently, no giant if/else chain
- **State transitions** are explicit and validated
- **Classic LLD interview question** ? tests state machine design skills
- **Extended for real world**: Add payment types (card, cash, mobile), maintenance state, admin refill state

```
State Diagram:
  IDLE --(insertMoney)--> HAS_MONEY --(selectProduct)--> DISPENSING
   |                         |                              |
   |                    (cancel/refund)                  (dispense)
   |                         |                              |
   +--------------------------------------------------------+
```

---

## Q9: Design an In-Memory Pub/Sub Message Broker

### Answer:
```cpp
#include <string>
#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <memory>
#include <atomic>

struct Message {
    std::string topic;
    std::string payload;
    std::chrono::steady_clock::time_point timestamp;
    uint64_t sequenceId;
};

class Topic {
    std::string name_;
    mutable std::mutex mutex_;
    std::vector<Message> messages_;         // Persistent log (like Kafka)
    uint64_t nextSeqId_ = 0;

    struct Subscriber {
        std::string id;
        std::function<void(const Message&)> handler;
        uint64_t lastConsumedSeqId = 0;     // Consumer offset
    };
    std::vector<Subscriber> subscribers_;

public:
    Topic(std::string name) : name_(std::move(name)) {}

    void subscribe(std::string subscriberId, std::function<void(const Message&)> handler) {
        std::lock_guard lock(mutex_);
        subscribers_.push_back({std::move(subscriberId), std::move(handler), nextSeqId_});
    }

    void unsubscribe(const std::string& subscriberId) {
        std::lock_guard lock(mutex_);
        std::erase_if(subscribers_, [&](const auto& s) { return s.id == subscriberId; });
    }

    void publish(std::string payload) {
        std::lock_guard lock(mutex_);
        Message msg{name_, std::move(payload), std::chrono::steady_clock::now(), nextSeqId_++};
        messages_.push_back(msg);

        // Fan-out to all subscribers
        for (auto& sub : subscribers_) {
            sub.handler(msg);
            sub.lastConsumedSeqId = msg.sequenceId;
        }
    }

    // Replay from offset (like Kafka consumer seek)
    void replayFrom(const std::string& subscriberId, uint64_t fromSeqId) {
        std::lock_guard lock(mutex_);
        for (auto& sub : subscribers_) {
            if (sub.id == subscriberId) {
                for (const auto& msg : messages_) {
                    if (msg.sequenceId >= fromSeqId)
                        sub.handler(msg);
                }
                break;
            }
        }
    }
};

class MessageBroker {
    std::unordered_map<std::string, std::unique_ptr<Topic>> topics_;
    mutable std::shared_mutex mutex_;

public:
    void createTopic(const std::string& name) {
        std::unique_lock lock(mutex_);
        topics_[name] = std::make_unique<Topic>(name);
    }

    void publish(const std::string& topicName, std::string payload) {
        std::shared_lock lock(mutex_);
        auto it = topics_.find(topicName);
        if (it != topics_.end()) it->second->publish(std::move(payload));
    }

    void subscribe(const std::string& topicName, const std::string& subId,
                   std::function<void(const Message&)> handler) {
        std::shared_lock lock(mutex_);
        auto it = topics_.find(topicName);
        if (it != topics_.end()) it->second->subscribe(subId, std::move(handler));
    }
};

// Usage
void example() {
    MessageBroker broker;
    broker.createTopic("trades");

    broker.subscribe("trades", "risk-engine", [](const Message& msg) {
        std::cout << "Risk check: " << msg.payload << "\n";
    });
    broker.subscribe("trades", "audit-log", [](const Message& msg) {
        std::cout << "Logged: " << msg.payload << "\n";
    });

    broker.publish("trades", "{\"symbol\":\"AAPL\",\"qty\":100}");
    // Both risk-engine and audit-log receive the message
}
```

### Explanation:
- **Fan-out delivery**: Each message delivered to ALL subscribers (pub/sub, not queue)
- **Message log with offsets**: Like Kafka → consumers can replay from any point
- **Thread-safe**: `shared_mutex` for broker (many readers, few writers), `mutex` per topic
- **Key patterns**: Observer (pub/sub), Iterator (replay), Repository (message log)
- **Follow-ups**: Consumer groups (load balancing), dead-letter queue, TTL expiry, partitioning, back-pressure

---

## Q10: Design a Task Scheduler with Priority and Dependencies

### Answer:
```cpp
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <atomic>

enum class TaskStatus { PENDING, READY, RUNNING, COMPLETED, FAILED };

struct Task {
    std::string id;
    int priority;                           // Higher = more important
    std::function<void()> work;
    std::vector<std::string> dependencies;  // Task IDs this depends on
    TaskStatus status = TaskStatus::PENDING;
    std::chrono::steady_clock::time_point scheduledTime;
};

class TaskScheduler {
    struct TaskCompare {
        bool operator()(const Task* a, const Task* b) {
            if (a->priority != b->priority) return a->priority < b->priority; // Max-heap
            return a->scheduledTime > b->scheduledTime; // Earlier first
        }
    };

    std::unordered_map<std::string, std::unique_ptr<Task>> tasks_;
    std::priority_queue<Task*, std::vector<Task*>, TaskCompare> readyQueue_;
    std::unordered_map<std::string, std::unordered_set<std::string>> dependents_; // task → who depends on it
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::jthread> workers_;
    std::atomic<bool> stopping_{false};

public:
    TaskScheduler(int numWorkers = 4) {
        for (int i = 0; i < numWorkers; ++i) {
            workers_.emplace_back([this](std::stop_token st) { workerLoop(st); });
        }
    }

    void addTask(std::string id, int priority, std::function<void()> work,
                 std::vector<std::string> deps = {}) {
        std::lock_guard lock(mutex_);
        auto task = std::make_unique<Task>();
        task->id = id;
        task->priority = priority;
        task->work = std::move(work);
        task->dependencies = std::move(deps);
        task->scheduledTime = std::chrono::steady_clock::now();

        // Build reverse dependency map
        for (const auto& dep : task->dependencies)
            dependents_[dep].insert(id);

        // Check if all dependencies are met
        bool allDepsComplete = true;
        for (const auto& dep : task->dependencies) {
            auto it = tasks_.find(dep);
            if (it == tasks_.end() || it->second->status != TaskStatus::COMPLETED) {
                allDepsComplete = false;
                break;
            }
        }

        if (allDepsComplete) {
            task->status = TaskStatus::READY;
            readyQueue_.push(task.get());
            cv_.notify_one();
        }

        tasks_[id] = std::move(task);
    }

    ~TaskScheduler() {
        stopping_ = true;
        cv_.notify_all();
    }

private:
    void workerLoop(std::stop_token st) {
        while (!st.stop_requested()) {
            Task* task = nullptr;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [&] { return !readyQueue_.empty() || stopping_; });
                if (stopping_ && readyQueue_.empty()) return;
                task = readyQueue_.top();
                readyQueue_.pop();
                task->status = TaskStatus::RUNNING;
            }

            try {
                task->work();
                markCompleted(task->id);
            } catch (...) {
                std::lock_guard lock(mutex_);
                task->status = TaskStatus::FAILED;
            }
        }
    }

    void markCompleted(const std::string& taskId) {
        std::lock_guard lock(mutex_);
        tasks_[taskId]->status = TaskStatus::COMPLETED;

        // Check if any dependent tasks can now run
        auto it = dependents_.find(taskId);
        if (it != dependents_.end()) {
            for (const auto& depId : it->second) {
                auto& depTask = tasks_[depId];
                bool allReady = true;
                for (const auto& dep : depTask->dependencies) {
                    if (tasks_[dep]->status != TaskStatus::COMPLETED) {
                        allReady = false;
                        break;
                    }
                }
                if (allReady) {
                    depTask->status = TaskStatus::READY;
                    readyQueue_.push(depTask.get());
                    cv_.notify_one();
                }
            }
        }
    }
};

// Usage
void example() {
    TaskScheduler scheduler(4);

    scheduler.addTask("compile", 5, [] { /* compile source */ });
    scheduler.addTask("link", 5, [] { /* link objects */ }, {"compile"});
    scheduler.addTask("test", 3, [] { /* run tests */ }, {"link"});
    scheduler.addTask("deploy", 1, [] { /* deploy binary */ }, {"test"});
    // compile | link | test | deploy (respecting dependencies)
    // Independent tasks with higher priority run first
}
```

### Explanation:
- **DAG-based scheduling**: Tasks form a Directed Acyclic Graph via dependencies
- **Priority queue**: Among ready tasks, higher priority runs first
- **Worker pool**: Multiple tasks can run in parallel if no dependency conflicts
- **Dependency resolution**: When a task completes, unlock all dependents whose deps are fully met
- **Used in**: Build systems (Make, Bazel), CI/CD pipelines, workflow engines, game asset loading
- **Follow-ups**: Circular dependency detection (DFS), task cancellation propagation, retry policies, task timeouts

---

## Q11: Design a Distributed Lock Manager with fencing tokens.

### Answer:

```cpp
#include <chrono>
#include <optional>
#include <string>
#include <atomic>
#include <unordered_map>
#include <mutex>

// Fencing token prevents stale lock holders from making writes
// Scenario: Client A gets lock ? GC pause ? lock expires ? Client B gets lock
//           Client A wakes up, thinks it still has lock ? STALE WRITE!
// Solution: Monotonically increasing fencing token ? storage rejects stale tokens

struct LockGrant {
    std::string resourceId;
    std::string ownerId;
    uint64_t fencingToken;  // Monotonically increasing
    std::chrono::steady_clock::time_point expiresAt;
    
    bool isExpired() const { return std::chrono::steady_clock::now() >= expiresAt; }
};

class DistributedLockManager {
    struct LockState {
        std::string currentOwner;
        uint64_t fencingToken;
        std::chrono::steady_clock::time_point expiresAt;
    };
    
    std::mutex mutex_;
    std::unordered_map<std::string, LockState> locks_;
    std::atomic<uint64_t> tokenCounter_{0};
    
public:
    // Try to acquire lock ? returns grant if successful
    std::optional<LockGrant> tryAcquire(
        const std::string& resourceId, 
        const std::string& ownerId,
        std::chrono::milliseconds ttl) 
    {
        std::lock_guard lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        
        auto it = locks_.find(resourceId);
        if (it != locks_.end() && !isExpired(it->second, now)) {
            // Lock held by someone else
            if (it->second.currentOwner != ownerId) {
                return std::nullopt;
            }
            // Reentrant: same owner ? extend TTL
        }
        
        uint64_t token = tokenCounter_.fetch_add(1) + 1;
        auto expiresAt = now + ttl;
        
        locks_[resourceId] = {ownerId, token, expiresAt};
        return LockGrant{resourceId, ownerId, token, expiresAt};
    }
    
    // Release lock ? only owner can release
    bool release(const std::string& resourceId, const std::string& ownerId) {
        std::lock_guard lock(mutex_);
        auto it = locks_.find(resourceId);
        if (it == locks_.end() || it->second.currentOwner != ownerId) {
            return false;
        }
        locks_.erase(it);
        return true;
    }
    
    // Validate fencing token ? storage layer calls this
    bool validateFencingToken(const std::string& resourceId, uint64_t token) {
        std::lock_guard lock(mutex_);
        auto it = locks_.find(resourceId);
        if (it == locks_.end()) return false;
        // Only accept if token matches current (or newer)
        return token >= it->second.fencingToken;
    }
    
private:
    bool isExpired(const LockState& state, std::chrono::steady_clock::time_point now) {
        return now >= state.expiresAt;
    }
};

// RAII lock guard for distributed lock
class DistributedLockGuard {
    DistributedLockManager& manager_;
    std::optional<LockGrant> grant_;
    std::jthread renewalThread_;
    
public:
    DistributedLockGuard(DistributedLockManager& mgr, const std::string& resource,
                         const std::string& owner, std::chrono::milliseconds ttl)
        : manager_(mgr) 
    {
        grant_ = manager_.tryAcquire(resource, owner, ttl);
        if (grant_) {
            // Auto-renew at TTL/3 intervals
            renewalThread_ = std::jthread([this, resource, owner, ttl](std::stop_token st) {
                while (!st.stop_requested()) {
                    std::this_thread::sleep_for(ttl / 3);
                    if (st.stop_requested()) break;
                    manager_.tryAcquire(resource, owner, ttl);  // Extend
                }
            });
        }
    }
    
    ~DistributedLockGuard() {
        renewalThread_.request_stop();
        if (grant_) manager_.release(grant_->resourceId, grant_->ownerId);
    }
    
    bool acquired() const { return grant_.has_value(); }
    uint64_t fencingToken() const { return grant_ ? grant_->fencingToken : 0; }
};

// Usage with fencing token:
void transferFunds(Account& from, Account& to, Money amount) {
    DistributedLockGuard lock(lockManager, "transfer:" + from.id, myNodeId, 30s);
    if (!lock.acquired()) throw LockException("Failed to acquire lock");
    
    // Pass fencing token to storage ? storage rejects stale writes
    from.debit(amount, lock.fencingToken());
    to.credit(amount, lock.fencingToken());
}
```

### Explanation:
- **Fencing tokens** solve the fundamental GC-pause/network-delay problem with distributed locks
- **TTL + auto-renewal** prevents deadlocks from crashed clients while keeping lock alive for healthy ones
- **Redlock (Redis)** acquires on N/2+1 nodes ? our design is single-node but teaches the principles
- **Martin Kleppmann's critique**: "Distributed locks are only safe with fencing tokens" ? this design implements that
- **Follow-ups**: Multi-resource locking (deadlock ordering), lock hierarchies, wait-die/wound-wait schemes

---

## Q12: Design an Event-Driven Architecture with backpressure handling.

### Answer:

```cpp
#include <queue>
#include <functional>
#include <variant>
#include <memory>
#include <thread>
#include <condition_variable>

// Type-safe event system with compile-time dispatch
struct OrderPlaced { std::string orderId; double amount; };
struct OrderCancelled { std::string orderId; std::string reason; };
struct PaymentReceived { std::string orderId; double amount; };
struct InventoryUpdated { std::string sku; int newQuantity; };

using Event = std::variant<OrderPlaced, OrderCancelled, PaymentReceived, InventoryUpdated>;

// Backpressure strategies
enum class BackpressureStrategy {
    BLOCK,       // Block producer until consumer catches up
    DROP_OLDEST, // Drop oldest events when full
    DROP_NEWEST, // Reject new events when full
    SAMPLE,      // Accept every Nth event when overloaded
};

template<BackpressureStrategy Strategy = BackpressureStrategy::BLOCK>
class EventBus {
    struct Subscription {
        std::string subscriberId;
        std::function<void(const Event&)> handler;
        std::unique_ptr<BoundedQueue> queue;  // Per-subscriber backpressure
    };
    
    std::vector<Subscription> subscribers_;
    std::mutex mutex_;
    
    // Bounded queue with backpressure
    class BoundedQueue {
        std::deque<Event> queue_;
        size_t maxSize_;
        std::mutex mutex_;
        std::condition_variable notFull_;
        std::condition_variable notEmpty_;
        std::atomic<size_t> dropped_{0};
        
    public:
        explicit BoundedQueue(size_t maxSize) : maxSize_(maxSize) {}
        
        bool enqueue(Event event) {
            std::unique_lock lock(mutex_);
            
            if constexpr (Strategy == BackpressureStrategy::BLOCK) {
                notFull_.wait(lock, [this] { return queue_.size() < maxSize_; });
                queue_.push_back(std::move(event));
                notEmpty_.notify_one();
                return true;
            }
            else if constexpr (Strategy == BackpressureStrategy::DROP_OLDEST) {
                if (queue_.size() >= maxSize_) {
                    queue_.pop_front();
                    dropped_++;
                }
                queue_.push_back(std::move(event));
                notEmpty_.notify_one();
                return true;
            }
            else if constexpr (Strategy == BackpressureStrategy::DROP_NEWEST) {
                if (queue_.size() >= maxSize_) {
                    dropped_++;
                    return false;  // Rejected
                }
                queue_.push_back(std::move(event));
                notEmpty_.notify_one();
                return true;
            }
        }
        
        std::optional<Event> dequeue(std::chrono::milliseconds timeout) {
            std::unique_lock lock(mutex_);
            if (!notEmpty_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
                return std::nullopt;
            }
            Event event = std::move(queue_.front());
            queue_.pop_front();
            notFull_.notify_one();
            return event;
        }
        
        size_t droppedCount() const { return dropped_.load(); }
        size_t size() const { return queue_.size(); }
        double fillRatio() const { return static_cast<double>(queue_.size()) / maxSize_; }
    };
    
public:
    // Subscribe with per-subscriber queue depth
    void subscribe(const std::string& id, std::function<void(const Event&)> handler, 
                   size_t queueDepth = 1000) {
        std::lock_guard lock(mutex_);
        subscribers_.push_back({id, std::move(handler), std::make_unique<BoundedQueue>(queueDepth)});
    }
    
    // Publish to all subscribers (fan-out)
    void publish(Event event) {
        std::lock_guard lock(mutex_);
        for (auto& sub : subscribers_) {
            sub.queue->enqueue(event);  // Each subscriber gets its own copy
        }
    }
    
    // Health monitoring
    struct BusHealth {
        size_t totalSubscribers;
        size_t overloadedSubscribers;  // >80% full
        size_t totalDropped;
    };
    
    BusHealth health() const {
        BusHealth h{subscribers_.size(), 0, 0};
        for (auto& sub : subscribers_) {
            if (sub.queue->fillRatio() > 0.8) h.overloadedSubscribers++;
            h.totalDropped += sub.queue->droppedCount();
        }
        return h;
    }
};

// Event handler with automatic type dispatch
class OrderProcessor {
public:
    void handle(const Event& event) {
        std::visit([this](const auto& e) { process(e); }, event);
    }
    
private:
    void process(const OrderPlaced& e) {
        // Reserve inventory, initiate payment
    }
    void process(const PaymentReceived& e) {
        // Fulfill order, update status
    }
    void process(const OrderCancelled& e) {
        // Release inventory, refund payment
    }
    void process(const InventoryUpdated& e) {
        // Check if any pending orders can now be fulfilled
    }
};
```

### Explanation:
- **Per-subscriber queues** prevent a slow consumer from blocking fast consumers
- **Backpressure strategies** are a critical design decision: BLOCK for correctness (financial), DROP for telemetry/metrics
- **Monitoring fill ratios** provides early warning before drops occur
- **std::variant events** give compile-time type safety with std::visit dispatch
- **Follow-ups**: Event sourcing (replay), CQRS, dead letter queues, exactly-once delivery, event schemas/versioning
- **Real systems**: Kafka (DROP_OLDEST with offset), RabbitMQ (BLOCK with flow control), Disruptor (ring buffer)

---

# ?? INTERVIEWER GUIDE ? Set 4 Scoring & Evaluation

---

## LLD Interview Expectations by Level

| Level | What We Expect |
|-------|----------------|
| **Mid** | Correct class design, handles basic requirements, clean code |
| **Senior** | SOLID principles applied, thread-safety considered, extensible design |
| **Staff** | Discusses trade-offs (CAP, latency vs consistency), handles edge cases proactively, designs for testability |
| **Principal** | Drives requirements clarification, identifies non-functional requirements, proposes monitoring/observability |

## ?? Green Flags for LLD

```
? Starts by clarifying requirements and scope (asks "Is this single-server or distributed?")
? Identifies design patterns naturally (Strategy for pricing, Observer for notifications)
? Considers thread-safety from the beginning (not as an afterthought)
? Uses enum classes, strong types, and RAII ? not int flags and raw pointers
? Designs interfaces first, implementation second (Dependency Inversion)
? Mentions testing strategy ("I'd mock this interface for unit tests")
? Handles error cases gracefully (std::optional, std::expected)
? Discusses how the design scales (what breaks at 10x? 100x?)
```

## ?? Red Flags for LLD

```
? Jumps to coding without discussing design approach
? God class that does everything (ParkingLotGodManager)
? No interfaces ? everything is concrete with no extensibility
? Ignores concurrency in multi-user systems
? Uses raw pointers for ownership
? No separation between data model and business logic
? Can't explain why they chose one approach over another
? Designs break when requirements change slightly
```

## Top LLD Problems Asked at Top Companies (2024-2026)

| Problem | Company Frequency | Key Design Challenge |
|---------|-------------------|---------------------|
| Parking Lot | Very High (Amazon, Google) | Strategy, Observer, thread-safety |
| Chess Game | High (Microsoft, Meta) | Polymorphism, move validation, state machine |
| Elevator System | High (Amazon, Uber) | Scheduling algorithms, state machine |
| Rate Limiter | Very High (All FAANG) | Token bucket, sliding window, distributed |
| URL Shortener (LLD) | High (Google, Twitter) | Encoding, collision handling, analytics |
| Pub/Sub System | High (Bloomberg, Google) | Thread-safe queues, backpressure |
| Task Scheduler | Medium (Citadel, Jane Street) | Priority queue, DAG execution |
| File System | Medium (Google, Apple) | Composite pattern, path resolution |
| Hotel Booking | Medium (Amazon, Booking) | Double-booking prevention, transactions |

---
