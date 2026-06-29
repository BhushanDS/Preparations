#include "kvstore/store.hpp"
#include <benchmark/benchmark.h>
#include <filesystem>
#include <random>

using namespace kvstore;

class StoreFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State&) override {
        testDir_ = std::filesystem::temp_directory_path() / "kvstore_bench";
        std::filesystem::remove_all(testDir_);
        
        StoreConfig config{
            .dataDir = testDir_,
            .enablePersistence = false,  // Benchmark without I/O
        };
        store_ = std::make_unique<KVStore>(config);
        store_->open();
    }
    
    void TearDown(benchmark::State&) override {
        store_->close();
        store_.reset();
        std::filesystem::remove_all(testDir_);
    }
    
    std::filesystem::path testDir_;
    std::unique_ptr<KVStore> store_;
};

BENCHMARK_DEFINE_F(StoreFixture, SetSequential)(benchmark::State& state) {
    int i = 0;
    for (auto _ : state) {
        store_->set("key" + std::to_string(i++), "value");
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(StoreFixture, SetSequential)->Threads(1)->Threads(4)->Threads(8);

BENCHMARK_DEFINE_F(StoreFixture, GetExisting)(benchmark::State& state) {
    // Pre-populate
    for (int i = 0; i < 100000; ++i) {
        store_->set("key" + std::to_string(i), "value" + std::to_string(i));
    }
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 99999);
    
    for (auto _ : state) {
        auto key = "key" + std::to_string(dist(rng));
        benchmark::DoNotOptimize(store_->get(key));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(StoreFixture, GetExisting)->Threads(1)->Threads(4)->Threads(8);

BENCHMARK_DEFINE_F(StoreFixture, GetMiss)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(store_->get("nonexistent"));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(StoreFixture, GetMiss);

BENCHMARK_DEFINE_F(StoreFixture, MixedWorkload)(benchmark::State& state) {
    // 80% reads, 20% writes (typical cache workload)
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> opDist(0, 99);
    std::uniform_int_distribution<int> keyDist(0, 9999);
    
    // Pre-populate
    for (int i = 0; i < 10000; ++i) {
        store_->set("key" + std::to_string(i), "value");
    }
    
    for (auto _ : state) {
        auto key = "key" + std::to_string(keyDist(rng));
        if (opDist(rng) < 80) {
            benchmark::DoNotOptimize(store_->get(key));
        } else {
            store_->set(key, "newvalue");
        }
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(StoreFixture, MixedWorkload)->Threads(1)->Threads(4)->Threads(8);

BENCHMARK_MAIN();
