# Set 2: Data Structures & Algorithms (C++ Focus)
## Senior-Level Interview Questions (10 Years Experience)

---

## Q1: Implement an LRU Cache with O(1) get and put operations.

### Answer:
```cpp
#include <unordered_map>
#include <list>

class LRUCache {
    int capacity_;
    std::list<std::pair<int, int>> items_;  // {key, value} | front = most recent
    std::unordered_map<int, std::list<std::pair<int, int>>::iterator> cache_;

public:
    LRUCache(int capacity) : capacity_(capacity) {}

    int get(int key) {
        auto it = cache_.find(key);
        if (it == cache_.end()) return -1;

        // Move to front (most recently used)
        items_.splice(items_.begin(), items_, it->second);
        return it->second->second;
    }

    void put(int key, int value) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            it->second->second = value;
            items_.splice(items_.begin(), items_, it->second);
            return;
        }

        if (static_cast<int>(cache_.size()) >= capacity_) {
            // Evict LRU (back of list)
            int evictKey = items_.back().first;
            items_.pop_back();
            cache_.erase(evictKey);
        }

        items_.emplace_front(key, value);
        cache_[key] = items_.begin();
    }
};
```

### Explanation:
- **Data structures**: Doubly-linked list (`std::list`) + hash map (`std::unordered_map`)
- **Why `std::list`?** O(1) splice/move operations; iterators remain valid after modifications
- **Why not `std::vector`?** Moving an element to the front is O(n) in a vector
- **`splice`** is the key -> it transfers nodes between positions in O(1) without copies
- **Real-world use**: Caching geometry computations in CAD, texture cache in gaming, market data cache in finance

**Time Complexity**: O(1) for both `get` and `put`
**Space Complexity**: O(capacity)

---

## Q2: Implement a thread-safe lock-free queue using atomics.

### Answer:
```cpp
#include <atomic>
#include <memory>

template<typename T>
class LockFreeQueue {
    struct Node {
        std::shared_ptr<T> data;
        std::atomic<Node*> next{nullptr};
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

public:
    LockFreeQueue() {
        Node* sentinel = new Node();
        head_.store(sentinel);
        tail_.store(sentinel);
    }

    ~LockFreeQueue() {
        while (Node* old_head = head_.load()) {
            head_.store(old_head->next.load());
            delete old_head;
        }
    }

    void push(T value) {
        auto new_data = std::make_shared<T>(std::move(value));
        Node* new_node = new Node();
        new_node->data = new_data;

        Node* old_tail;
        while (true) {
            old_tail = tail_.load(std::memory_order_acquire);
            Node* next = old_tail->next.load(std::memory_order_acquire);

            if (old_tail == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (old_tail->next.compare_exchange_weak(next, new_node,
                            std::memory_order_release, std::memory_order_relaxed)) {
                        break;
                    }
                } else {
                    tail_.compare_exchange_weak(old_tail, next,
                        std::memory_order_release, std::memory_order_relaxed);
                }
            }
        }
        tail_.compare_exchange_weak(old_tail, new_node,
            std::memory_order_release, std::memory_order_relaxed);
    }

    std::shared_ptr<T> pop() {
        while (true) {
            Node* old_head = head_.load(std::memory_order_acquire);
            Node* old_tail = tail_.load(std::memory_order_acquire);
            Node* next = old_head->next.load(std::memory_order_acquire);

            if (old_head == head_.load(std::memory_order_acquire)) {
                if (old_head == old_tail) {
                    if (next == nullptr) return nullptr;  // Empty
                    tail_.compare_exchange_weak(old_tail, next,
                        std::memory_order_release, std::memory_order_relaxed);
                } else {
                    auto result = next->data;
                    if (head_.compare_exchange_weak(old_head, next,
                            std::memory_order_release, std::memory_order_relaxed)) {
                        delete old_head;
                        return result;
                    }
                }
            }
        }
    }
};
```

### Explanation:
- **Michael & Scott Queue** ? classic lock-free algorithm
- Uses **CAS (compare-and-swap)** via `compare_exchange_weak`
- **Sentinel node** separates head/tail contention
- **Memory ordering**: `acquire` on loads, `release` on stores -> avoids full sequential consistency overhead
- **Use case**: High-frequency trading order queues, game engine message passing, real-time CAD update streams

---

## Q3: Given a stream of stock prices, find the maximum profit with at most K transactions.

### Answer:
```cpp
#include <vector>
#include <algorithm>

int maxProfit(int k, const std::vector<int>& prices) {
    int n = prices.size();
    if (n <= 1 || k == 0) return 0;

    // Optimization: if k >= n/2, unlimited transactions
    if (k >= n / 2) {
        int profit = 0;
        for (int i = 1; i < n; ++i)
            profit += std::max(0, prices[i] - prices[i - 1]);
        return profit;
    }

    // dp[t][d] = max profit using at most t transactions on day d
    // Optimized to O(k) space
    std::vector<int> buy(k + 1, INT_MIN);   // Max profit after buying on transaction t
    std::vector<int> sell(k + 1, 0);         // Max profit after selling on transaction t

    for (int price : prices) {
        for (int t = 1; t <= k; ++t) {
            buy[t] = std::max(buy[t], sell[t - 1] - price);
            sell[t] = std::max(sell[t], buy[t] + price);
        }
    }

    return sell[k];
}
```

### Explanation:
- **State machine DP**: At each step, you're either holding stock (bought) or not (sold)
- `buy[t]` = best balance after buying in transaction `t`
- `sell[t]` = best balance after completing transaction `t`
- **Optimization**: When `k >= n/2`, it's equivalent to unlimited transactions -> greedy approach
- **Time**: O(n × k), **Space**: O(k)
- **Finance domain relevance**: Directly applicable to backtesting trading strategies

**Alternative: Space-optimized with just 2 arrays when k is small:**
```cpp
// For k=1 (single transaction): O(n) time, O(1) space
int maxProfitSingle(const std::vector<int>& prices) {
    int minPrice = INT_MAX, maxProfit = 0;
    for (int price : prices) {
        minPrice = std::min(minPrice, price);
        maxProfit = std::max(maxProfit, price - minPrice);
    }
    return maxProfit;
}
```

**Follow-up variations:**
| Variant | Key Difference | Time |
|---------|---------------|------|
| Best Time I (k=1) | Track min price, max diff | O(n) |
| Best Time II (k=|) | Sum all positive differences | O(n) |
| Best Time III (k=2) | Two passes or DP with 4 states | O(n) |
| Best Time IV (k=any) | Full DP as shown above | O(nk) |
| With cooldown | Add COOLDOWN state after sell | O(n) |
| With transaction fee | Subtract fee from each sell | O(n) |

---

## Q4: Implement a Trie with insert, search, and prefix-based autocomplete.

### Answer:
```cpp
#include <string>
#include <vector>
#include <array>
#include <memory>

class Trie {
    struct Node {
        std::array<std::unique_ptr<Node>, 26> children;
        bool isEnd = false;
        int frequency = 0;  // For ranking suggestions
    };

    std::unique_ptr<Node> root_;

    void collectWords(const Node* node, std::string& prefix,
                      std::vector<std::pair<std::string, int>>& results) const {
        if (!node) return;
        if (node->isEnd) {
            results.emplace_back(prefix, node->frequency);
        }
        for (int i = 0; i < 26; ++i) {
            if (node->children[i]) {
                prefix.push_back('a' + i);
                collectWords(node->children[i].get(), prefix, results);
                prefix.pop_back();
            }
        }
    }

public:
    Trie() : root_(std::make_unique<Node>()) {}

    void insert(const std::string& word) {
        Node* curr = root_.get();
        for (char c : word) {
            int idx = c - 'a';
            if (!curr->children[idx])
                curr->children[idx] = std::make_unique<Node>();
            curr = curr->children[idx].get();
        }
        curr->isEnd = true;
        curr->frequency++;
    }

    bool search(const std::string& word) const {
        const Node* curr = root_.get();
        for (char c : word) {
            int idx = c - 'a';
            if (!curr->children[idx]) return false;
            curr = curr->children[idx].get();
        }
        return curr->isEnd;
    }

    std::vector<std::string> autocomplete(const std::string& prefix, int topK = 5) const {
        const Node* curr = root_.get();
        for (char c : prefix) {
            int idx = c - 'a';
            if (!curr->children[idx]) return {};
            curr = curr->children[idx].get();
        }

        std::vector<std::pair<std::string, int>> candidates;
        std::string mutablePrefix = prefix;
        collectWords(curr, mutablePrefix, candidates);

        // Sort by frequency descending
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        std::vector<std::string> result;
        for (int i = 0; i < std::min(topK, static_cast<int>(candidates.size())); ++i)
            result.push_back(candidates[i].first);
        return result;
    }
};
```

### Explanation:
- **Time Complexity**: Insert/Search O(L) where L = word length; Autocomplete O(N) where N = all words with prefix
- **Space**: O(ALPHABET_SIZE × total characters) -> can be optimized with compressed tries
- **Uses**: Command autocomplete in CAD tools, symbol lookup in IDEs, search suggestion in finance terminals
- **Interview follow-up**: Discuss compressed/radix tries, ternary search trees, or using `std::unordered_map` for sparse alphabets

---

## Q5: Find the shortest path in a weighted graph with negative edges (but no negative cycles).

### Answer:
```cpp
#include <vector>
#include <limits>

struct Edge {
    int from, to, weight;
};

// Bellman-Ford Algorithm
std::vector<int> shortestPath(int vertices, const std::vector<Edge>& edges, int source) {
    std::vector<int> dist(vertices, std::numeric_limits<int>::max());
    dist[source] = 0;

    // Relax all edges V-1 times
    for (int i = 0; i < vertices - 1; ++i) {
        bool updated = false;
        for (const auto& [from, to, weight] : edges) {
            if (dist[from] != std::numeric_limits<int>::max() &&
                dist[from] + weight < dist[to]) {
                dist[to] = dist[from] + weight;
                updated = true;
            }
        }
        if (!updated) break;  // Early termination
    }

    // Check for negative cycles
    for (const auto& [from, to, weight] : edges) {
        if (dist[from] != std::numeric_limits<int>::max() &&
            dist[from] + weight < dist[to]) {
            throw std::runtime_error("Graph contains a negative-weight cycle");
        }
    }

    return dist;
}
```

### Explanation:
- **Why not Dijkstra?** Dijkstra fails with negative edges (greedy assumption breaks)
- **Bellman-Ford**: Relaxes all edges V-1 times. Correct even with negative edges
- **Time**: O(V × E), **Space**: O(V)
- **Early termination**: If no update in an iteration, algorithm has converged
- **Finance use case**: Arbitrage detection -> negative cycle in currency exchange graph means profit opportunity

**Dijkstra vs Bellman-Ford vs SPFA -> When to use which:**
| Algorithm | Negative Edges | Negative Cycle Detection | Time | Best For |
|-----------|---------------|--------------------------|------|----------|
| Dijkstra | No | No | O((V+E) log V) | Non-negative weighted graphs |
| Bellman-Ford | Yes | Yes | O(V × E) | Negative edges, small graphs |
| SPFA (Shortest Path Faster Algorithm) | Yes | Yes | O(V × E) worst, O(E) avg | Sparse graphs with negative edges |
| Floyd-Warshall | Yes | Yes | O(V³) | All-pairs shortest paths |

**SPFA -> Faster Bellman-Ford alternative:**
```cpp
std::vector<int> spfa(int vertices, const std::vector<std::vector<std::pair<int,int>>>& adj, int source) {
    std::vector<int> dist(vertices, INT_MAX);
    std::vector<bool> inQueue(vertices, false);
    std::vector<int> count(vertices, 0);  // For negative cycle detection
    std::queue<int> q;
    
    dist[source] = 0;
    q.push(source);
    inQueue[source] = true;
    
    while (!q.empty()) {
        int u = q.front(); q.pop();
        inQueue[u] = false;
        
        for (auto [v, w] : adj[u]) {
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                if (!inQueue[v]) {
                    q.push(v);
                    inQueue[v] = true;
                    if (++count[v] >= vertices) 
                        throw std::runtime_error("Negative cycle detected");
                }
            }
        }
    }
    return dist;
}
```

---

## Q6: Implement a Segment Tree with lazy propagation for range updates and queries.

### Answer:
```cpp
#include <vector>

class SegmentTree {
    int n_;
    std::vector<long long> tree_, lazy_;

    void build(const std::vector<int>& arr, int node, int start, int end) {
        if (start == end) {
            tree_[node] = arr[start];
            return;
        }
        int mid = (start + end) / 2;
        build(arr, 2 * node, start, mid);
        build(arr, 2 * node + 1, mid + 1, end);
        tree_[node] = tree_[2 * node] + tree_[2 * node + 1];
    }

    void pushDown(int node, int start, int end) {
        if (lazy_[node] != 0) {
            int mid = (start + end) / 2;
            tree_[2 * node] += lazy_[node] * (mid - start + 1);
            tree_[2 * node + 1] += lazy_[node] * (end - mid);
            lazy_[2 * node] += lazy_[node];
            lazy_[2 * node + 1] += lazy_[node];
            lazy_[node] = 0;
        }
    }

    void updateRange(int node, int start, int end, int l, int r, long long val) {
        if (r < start || end < l) return;
        if (l <= start && end <= r) {
            tree_[node] += val * (end - start + 1);
            lazy_[node] += val;
            return;
        }
        pushDown(node, start, end);
        int mid = (start + end) / 2;
        updateRange(2 * node, start, mid, l, r, val);
        updateRange(2 * node + 1, mid + 1, end, l, r, val);
        tree_[node] = tree_[2 * node] + tree_[2 * node + 1];
    }

    long long queryRange(int node, int start, int end, int l, int r) {
        if (r < start || end < l) return 0;
        if (l <= start && end <= r) return tree_[node];
        pushDown(node, start, end);
        int mid = (start + end) / 2;
        return queryRange(2 * node, start, mid, l, r) +
               queryRange(2 * node + 1, mid + 1, end, l, r);
    }

public:
    SegmentTree(const std::vector<int>& arr) : n_(arr.size()),
        tree_(4 * n_, 0), lazy_(4 * n_, 0) {
        build(arr, 1, 0, n_ - 1);
    }

    void update(int l, int r, long long val) {
        updateRange(1, 0, n_ - 1, l, r, val);
    }

    long long query(int l, int r) {
        return queryRange(1, 0, n_ - 1, l, r);
    }
};

// Usage:
// std::vector<int> arr = {1, 3, 5, 7, 9, 11};
// SegmentTree st(arr);
// st.query(1, 3);        // Sum of arr[1..3] = 15
// st.update(1, 3, 10);   // Add 10 to arr[1..3]
// st.query(1, 3);        // Now 45
```

### Explanation:
- **Lazy propagation**: Defers updates to children until they're needed -> O(log n) range updates
- **Without lazy propagation**: Range update is O(n log n)
- **Applications**: Financial rolling aggregates, CAD spatial queries, game damage calculations over ranges
- **Time**: O(log n) per query and update, **Space**: O(4n)

---

## Q7: Solve the "Meeting Rooms" problem -> find the minimum number of conference rooms needed.

### Answer:
```cpp
#include <vector>
#include <queue>
#include <algorithm>

int minMeetingRooms(std::vector<std::pair<int, int>>& intervals) {
    if (intervals.empty()) return 0;

    // Sort by start time
    std::sort(intervals.begin(), intervals.end());

    // Min-heap of end times (earliest ending meeting on top)
    std::priority_queue<int, std::vector<int>, std::greater<>> minHeap;
    minHeap.push(intervals[0].second);

    for (int i = 1; i < static_cast<int>(intervals.size()); ++i) {
        // If earliest ending meeting has ended, reuse that room
        if (intervals[i].first >= minHeap.top()) {
            minHeap.pop();
        }
        minHeap.push(intervals[i].second);
    }

    return minHeap.size();
}

// Alternative: Sweep line approach
int minMeetingRoomsSweep(std::vector<std::pair<int, int>>& intervals) {
    std::vector<std::pair<int, int>> events;
    for (const auto& [start, end] : intervals) {
        events.emplace_back(start, 1);   // Meeting starts
        events.emplace_back(end, -1);    // Meeting ends
    }
    std::sort(events.begin(), events.end());

    int rooms = 0, maxRooms = 0;
    for (const auto& [time, delta] : events) {
        rooms += delta;
        maxRooms = std::max(maxRooms, rooms);
    }
    return maxRooms;
}
```

### Explanation:
- **Heap approach**: Maintain a heap of room end times. If current meeting starts after earliest end, reuse; otherwise, allocate new room. O(n log n)
- **Sweep line approach**: Convert intervals to events (+1 for start, -1 for end). Track concurrent meetings. O(n log n)
- **Both approaches are expected at senior level** ? discuss trade-offs

---

## Q8: Implement a Graph with DFS, BFS, and detect cycles in directed and undirected graphs.

### Answer:
```cpp
#include <vector>
#include <queue>
#include <stack>

class Graph {
    int V_;
    std::vector<std::vector<int>> adj_;

public:
    Graph(int V) : V_(V), adj_(V) {}

    void addEdge(int u, int v, bool directed = true) {
        adj_[u].push_back(v);
        if (!directed) adj_[v].push_back(u);
    }

    // BFS | level-order, shortest path in unweighted
    std::vector<int> bfs(int start) {
        std::vector<int> result;
        std::vector<bool> visited(V_, false);
        std::queue<int> q;
        visited[start] = true;
        q.push(start);

        while (!q.empty()) {
            int node = q.front(); q.pop();
            result.push_back(node);
            for (int neighbor : adj_[node]) {
                if (!visited[neighbor]) {
                    visited[neighbor] = true;
                    q.push(neighbor);
                }
            }
        }
        return result;
    }

    // DFS | iterative (preferred in interviews to avoid stack overflow)
    std::vector<int> dfs(int start) {
        std::vector<int> result;
        std::vector<bool> visited(V_, false);
        std::stack<int> s;
        s.push(start);

        while (!s.empty()) {
            int node = s.top(); s.pop();
            if (visited[node]) continue;
            visited[node] = true;
            result.push_back(node);
            for (auto it = adj_[node].rbegin(); it != adj_[node].rend(); ++it)
                if (!visited[*it]) s.push(*it);
        }
        return result;
    }

    // Cycle detection in directed graph (DFS with coloring)
    bool hasCycleDirected() {
        enum Color { WHITE, GRAY, BLACK };
        std::vector<Color> color(V_, WHITE);

        std::function<bool(int)> dfs = [&](int u) -> bool {
            color[u] = GRAY;  // Being processed
            for (int v : adj_[u]) {
                if (color[v] == GRAY) return true;  // Back edge = cycle
                if (color[v] == WHITE && dfs(v)) return true;
            }
            color[u] = BLACK;  // Done
            return false;
        };

        for (int i = 0; i < V_; ++i)
            if (color[i] == WHITE && dfs(i)) return true;
        return false;
    }

    // Cycle detection in undirected graph (Union-Find)
    bool hasCycleUndirected() {
        std::vector<int> parent(V_), rank(V_, 0);
        std::iota(parent.begin(), parent.end(), 0);

        std::function<int(int)> find = [&](int x) -> int {
            if (parent[x] != x) parent[x] = find(parent[x]);
            return parent[x];
        };

        auto unite = [&](int x, int y) -> bool {
            int px = find(x), py = find(y);
            if (px == py) return false;  // Cycle!
            if (rank[px] < rank[py]) std::swap(px, py);
            parent[py] = px;
            if (rank[px] == rank[py]) rank[px]++;
            return true;
        };

        for (int u = 0; u < V_; ++u)
            for (int v : adj_[u])
                if (u < v && !unite(u, v)) return true;
        return false;
    }
};
```

### Explanation:
- **Directed cycle detection**: 3-color DFS. GRAY node encountered = back edge = cycle
- **Undirected cycle detection**: Union-Find with path compression + rank -> O(|(n)) per operation
- **Senior-level expectation**: Know both approaches and when each applies
- **CAD use case**: Dependency graphs for constraint solving; Gaming: scene graph cycle detection

---

## Q9: Implement a custom hash map from scratch.

### Answer:
```cpp
#include <vector>
#include <list>
#include <functional>
#include <stdexcept>

template<typename K, typename V, typename Hash = std::hash<K>>
class HashMap {
    struct Entry {
        K key;
        V value;
    };

    std::vector<std::list<Entry>> buckets_;
    size_t size_ = 0;
    float maxLoadFactor_ = 0.75f;
    Hash hasher_;

    size_t getBucket(const K& key) const {
        return hasher_(key) % buckets_.size();
    }

    void rehash() {
        auto oldBuckets = std::move(buckets_);
        buckets_.resize(oldBuckets.size() * 2);
        size_ = 0;

        for (auto& chain : oldBuckets)
            for (auto& entry : chain)
                insert(std::move(entry.key), std::move(entry.value));
    }

public:
    HashMap(size_t initialCapacity = 16) : buckets_(initialCapacity) {}

    void insert(K key, V value) {
        if (static_cast<float>(size_ + 1) / buckets_.size() > maxLoadFactor_)
            rehash();

        size_t idx = getBucket(key);
        for (auto& entry : buckets_[idx]) {
            if (entry.key == key) {
                entry.value = std::move(value);
                return;
            }
        }
        buckets_[idx].push_back({std::move(key), std::move(value)});
        ++size_;
    }

    V& get(const K& key) {
        size_t idx = getBucket(key);
        for (auto& entry : buckets_[idx])
            if (entry.key == key) return entry.value;
        throw std::out_of_range("Key not found");
    }

    bool remove(const K& key) {
        size_t idx = getBucket(key);
        auto& chain = buckets_[idx];
        for (auto it = chain.begin(); it != chain.end(); ++it) {
            if (it->key == key) {
                chain.erase(it);
                --size_;
                return true;
            }
        }
        return false;
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
};
```

### Explanation:
- **Chaining with `std::list`** ? handles collisions via linked list per bucket
- **Load factor 0.75** ? standard threshold (same as Java's HashMap)
- **Rehashing doubles capacity** and reinserts all elements -> amortized O(1) insert
- **Interview follow-ups**: Open addressing vs. chaining, Robin Hood hashing, custom hash functions for domain types (CAD point coordinates, financial instrument IDs)

**Open Addressing Alternative (Linear Probing):**
```cpp
template<typename K, typename V>
class OpenAddressMap {
    enum State { EMPTY, OCCUPIED, DELETED };
    struct Slot { K key; V value; State state = EMPTY; };
    std::vector<Slot> table_;
    size_t size_ = 0;

    size_t probe(const K& key) const {
        size_t idx = std::hash<K>{}(key) % table_.size();
        while (table_[idx].state == OCCUPIED && table_[idx].key != key) {
            idx = (idx + 1) % table_.size();  // Linear probing
        }
        return idx;
    }

public:
    OpenAddressMap(size_t cap = 16) : table_(cap) {}

    void insert(K key, V value) {
        if (size_ * 2 >= table_.size()) rehash();  // Keep load < 0.5
        size_t idx = probe(key);
        if (table_[idx].state != OCCUPIED) ++size_;
        table_[idx] = {std::move(key), std::move(value), OCCUPIED};
    }

    V* find(const K& key) {
        size_t idx = probe(key);
        return (table_[idx].state == OCCUPIED) ? &table_[idx].value : nullptr;
    }
};
```

**Chaining vs Open Addressing trade-offs:**
| Aspect | Chaining | Open Addressing |
|--------|----------|----------------|
| Cache performance | Poor (pointer chasing) | Excellent (contiguous) |
| Load factor | Can exceed 1.0 | Must stay < ~0.7 |
| Deletion | Simple (unlink node) | Complex (tombstones) |
| Memory overhead | Per-node allocation | Wasted slots |
| Implementation | Simpler | More complex |
| Best for | Large values, high load | Small values, cache-sensitive |

**Robin Hood Hashing (advanced):** Elements that are "rich" (close to home bucket) give up their slot to "poor" elements (far from home). This equalizes probe distances, reducing worst-case lookup from O(n) to O(log log n).

---

## Q10: Solve "Word Ladder" ? find shortest transformation sequence from beginWord to endWord.

### Answer:
```cpp
#include <string>
#include <vector>
#include <queue>
#include <unordered_set>
#include <unordered_map>

int wordLadder(const std::string& beginWord, const std::string& endWord,
               const std::vector<std::string>& wordList) {
    std::unordered_set<std::string> wordSet(wordList.begin(), wordList.end());
    if (wordSet.find(endWord) == wordSet.end()) return 0;

    // Bidirectional BFS for optimization
    std::unordered_set<std::string> frontSet{beginWord};
    std::unordered_set<std::string> backSet{endWord};
    std::unordered_set<std::string> visited;
    int steps = 1;

    while (!frontSet.empty() && !backSet.empty()) {
        // Always expand the smaller frontier
        if (frontSet.size() > backSet.size())
            std::swap(frontSet, backSet);

        std::unordered_set<std::string> nextFront;
        for (const auto& word : frontSet) {
            std::string temp = word;
            for (int i = 0; i < static_cast<int>(temp.size()); ++i) {
                char original = temp[i];
                for (char c = 'a'; c <= 'z'; ++c) {
                    if (c == original) continue;
                    temp[i] = c;

                    if (backSet.count(temp)) return steps + 1;

                    if (wordSet.count(temp) && !visited.count(temp)) {
                        nextFront.insert(temp);
                        visited.insert(temp);
                    }
                }
                temp[i] = original;
            }
        }
        frontSet = std::move(nextFront);
        ++steps;
    }
    return 0;
}
```

### Explanation:
- **Bidirectional BFS** reduces search space from O(b^d) to O(b^(d/2)) where b = branching factor, d = depth
- **Key optimization**: Always expand the smaller frontier
- **Time**: O(M² × N) where M = word length, N = word list size
- **Senior insight**: Regular BFS is acceptable, but bidirectional BFS shows depth of understanding
- **Follow-up**: Return the actual path, not just length -> use parent tracking

**Path Reconstruction (returning the actual transformation sequence):**
```cpp
std::vector<std::string> findLadderPath(
    const std::string& beginWord, const std::string& endWord,
    const std::vector<std::string>& wordList) 
{
    std::unordered_set<std::string> wordSet(wordList.begin(), wordList.end());
    if (!wordSet.count(endWord)) return {};

    std::queue<std::vector<std::string>> q;
    q.push({beginWord});
    std::unordered_set<std::string> visited{beginWord};

    while (!q.empty()) {
        int levelSize = q.size();
        std::unordered_set<std::string> levelVisited;

        for (int i = 0; i < levelSize; ++i) {
            auto path = q.front(); q.pop();
            std::string word = path.back();

            for (int j = 0; j < (int)word.size(); ++j) {
                std::string temp = word;
                for (char c = 'a'; c <= 'z'; ++c) {
                    temp[j] = c;
                    if (temp == endWord) {
                        path.push_back(endWord);
                        return path;  // First path found = shortest
                    }
                    if (wordSet.count(temp) && !visited.count(temp)) {
                        auto newPath = path;
                        newPath.push_back(temp);
                        q.push(newPath);
                        levelVisited.insert(temp);
                    }
                }
            }
        }
        visited.insert(levelVisited.begin(), levelVisited.end());
    }
    return {};
}
// Example: "hit" | "hot" | "dot" | "dog" | "cog"
```

**Complexity Summary for All Q2 Algorithms:**
| Problem | Algorithm | Time | Space | Key Insight |
|---------|-----------|------|-------|-------------|
| LRU Cache | List + HashMap | O(1) get/put | O(n) | splice is O(1) |
| Lock-free Queue | CAS (M&S) | O(1) amortized | O(n) | Sentinel separates contention |
| K Transactions | State Machine DP | O(nk) | O(k) | buy/sell state machine |
| Trie | Prefix tree | O(L) insert/search | O(|L) | 26-ary tree, frequency ranking |
| Shortest Path | Bellman-Ford/SPFA | O(VE)/O(E) avg | O(V) | Relaxation, negative cycle check |
| Segment Tree | Lazy propagation | O(log n) | O(4n) | Defer updates to children |
| Meeting Rooms | Sweep line/Heap | O(n log n) | O(n) | Event-based counting |
| Graph Cycles | DFS 3-color/UF | O(V+E) | O(V) | GRAY = back edge |
| Hash Map | Chaining/Open addr | O(1) avg | O(n) | Load factor < 0.75 |
| Word Ladder | Bidirectional BFS | O(M²×N) | O(M×N) | Expand smaller frontier |

---

# ENHANCED SECTION: Principal Engineer / Architect Level DSA

> *These questions test algorithmic thinking at the system design intersection -> where DSA meets real-world constraints. Expected at Google L6+, Meta E6+, Bloomberg, Citadel.*

---

## Q11: Implement a Monotonic Stack -> solve "Next Greater Element" and "Largest Rectangle in Histogram."

### Answer:
```cpp
// Next Greater Element: For each element, find the next larger element to its right
std::vector<int> nextGreaterElement(const std::vector<int>& nums) {
    int n = nums.size();
    std::vector<int> result(n, -1);
    std::stack<int> stk; // Stack of indices, monotonically decreasing values

    for (int i = 0; i < n; ++i) {
        while (!stk.empty() && nums[stk.top()] < nums[i]) {
            result[stk.top()] = nums[i];
            stk.pop();
        }
        stk.push(i);
    }
    return result;
}

// Largest Rectangle in Histogram: O(n)
int largestRectangleArea(const std::vector<int>& heights) {
    std::stack<int> stk;
    int maxArea = 0, n = heights.size();

    for (int i = 0; i <= n; ++i) {
        int h = (i == n) ? 0 : heights[i];
        while (!stk.empty() && heights[stk.top()] > h) {
            int height = heights[stk.top()]; stk.pop();
            int width = stk.empty() ? i : (i - stk.top() - 1);
            maxArea = std::max(maxArea, height * width);
        }
        stk.push(i);
    }
    return maxArea;
}
```

### Explanation:
- **Monotonic stack** maintains invariant: elements always increasing (or decreasing)
- Each element pushed/popped exactly once -> **O(n) amortized**
- **Real-world use**: Stock span calculation (finance), temperature prediction, histogram-based image analysis
- **Pattern recognition**: When you need "next greater/smaller" or "range bounded by current element" ? think monotonic stack

---

## Q12: Implement Sliding Window Maximum using a Monotonic Deque.

### Answer:
```cpp
std::vector<int> maxSlidingWindow(const std::vector<int>& nums, int k) {
    std::deque<int> dq; // Indices of elements in decreasing order
    std::vector<int> result;

    for (int i = 0; i < static_cast<int>(nums.size()); ++i) {
        // Remove elements outside window
        while (!dq.empty() && dq.front() <= i - k)
            dq.pop_front();

        // Maintain decreasing invariant
        while (!dq.empty() && nums[dq.back()] <= nums[i])
            dq.pop_back();

        dq.push_back(i);

        if (i >= k - 1)
            result.push_back(nums[dq.front()]); // Front is always the max
    }
    return result;
}
```

### Explanation:
- O(n) overall -> each element enters and leaves the deque at most once
- **Finance use case**: Rolling maximum P&L in a time window, peak detection in market data
- **HFT**: Efficiently tracking best bid/offer in sliding time windows
- **iCluster analogy**: Tracking maximum replication lag across sliding checkpoint windows

---

## Q13: Implement Topological Sort (Kahn's BFS and DFS). When do you need it in real systems?

### Answer:
```cpp
// Kahn's Algorithm (BFS-based | produces lexicographically smallest order with min-heap)
std::vector<int> topologicalSort(int V, const std::vector<std::vector<int>>& adj) {
    std::vector<int> inDegree(V, 0);
    for (int u = 0; u < V; ++u)
        for (int v : adj[u]) ++inDegree[v];

    std::queue<int> q;
    for (int i = 0; i < V; ++i)
        if (inDegree[i] == 0) q.push(i);

    std::vector<int> order;
    while (!q.empty()) {
        int u = q.front(); q.pop();
        order.push_back(u);
        for (int v : adj[u])
            if (--inDegree[v] == 0) q.push(v);
    }

    if (static_cast<int>(order.size()) != V)
        throw std::runtime_error("Cycle detected ? no topological order");
    return order;
}
```

### Explanation:
- **Build systems**: CMake, Bazel, Make -> compile dependencies in topological order
- **iCluster**: Object dependency resolution -> base physical files before logical files, libraries before programs
- **Database migrations**: Schema changes must be applied in dependency order
- **Package managers**: apt, npm -> install dependencies before dependents
- **Senior insight**: Cycle detection is the bonus -> if topo sort doesn't include all nodes, there's a cycle

---

## Q14: Implement an Interval Tree for overlapping range queries.

### Answer:
```cpp
struct Interval { int low, high; };

struct ITNode {
    Interval interval;
    int maxHigh; // Max high value in subtree
    std::unique_ptr<ITNode> left, right;
};

class IntervalTree {
    std::unique_ptr<ITNode> root_;

    void insert(std::unique_ptr<ITNode>& node, Interval i) {
        if (!node) {
            node = std::make_unique<ITNode>();
            node->interval = i;
            node->maxHigh = i.high;
            return;
        }
        node->maxHigh = std::max(node->maxHigh, i.high);
        if (i.low <= node->interval.low)
            insert(node->left, i);
        else
            insert(node->right, i);
    }

    bool overlaps(const Interval& a, const Interval& b) const {
        return a.low <= b.high && b.low <= a.high;
    }

    void queryAll(const ITNode* node, Interval q,
                  std::vector<Interval>& results) const {
        if (!node) return;
        if (overlaps(node->interval, q)) results.push_back(node->interval);
        if (node->left && node->left->maxHigh >= q.low)
            queryAll(node->left.get(), q, results);
        if (node->right && node->right->interval.low <= q.high)
            queryAll(node->right.get(), q, results);
    }

public:
    void insert(Interval i) { insert(root_, i); }

    std::vector<Interval> query(Interval q) const {
        std::vector<Interval> results;
        queryAll(root_.get(), q, results);
        return results;
    }
};
```

### Explanation:
- **O(log n + k)** for finding all k overlapping intervals
- **CAD**: Spatial queries, finding all geometry intersecting a bounding box
- **Finance**: Finding all active trades/orders within a time range
- **iCluster**: Checking overlapping replication windows, journal position range queries
- **Gaming**: Collision detection -> which objects overlap a given region

---

## Q15: Explain the Union-Find (Disjoint Set Union) with Path Compression and Union by Rank.

### Answer:
```cpp
class DSU {
    std::vector<int> parent_, rank_;
public:
    DSU(int n) : parent_(n), rank_(n, 0) {
        std::iota(parent_.begin(), parent_.end(), 0);
    }

    int find(int x) {
        if (parent_[x] != x)
            parent_[x] = find(parent_[x]); // Path compression
        return parent_[x];
    }

    bool unite(int x, int y) {
        int px = find(x), py = find(y);
        if (px == py) return false; // Already connected
        if (rank_[px] < rank_[py]) std::swap(px, py);
        parent_[py] = px; // Union by rank
        if (rank_[px] == rank_[py]) ++rank_[px];
        return true;
    }

    bool connected(int x, int y) { return find(x) == find(y); }
};
```

### Explanation:
- **Nearly O(1)** amortized per operation (inverse Ackermann -> practically constant)
- **Kruskal's MST**: Use DSU to check if adding an edge creates a cycle
- **Network connectivity**: "Are nodes A and B in the same cluster?" ? constant time after setup
- **iCluster use case**: Determining if nodes in a cluster can still communicate (network partition detection)
- **Social networks**: Friend-of-friend connected component detection

---
