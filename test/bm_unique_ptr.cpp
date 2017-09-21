#include "array"
#include "benchmark/benchmark.h"
#include "memory"
#include "small_pointer/unique_ptr.hpp"

using namespace small_pointer;

template <typename T>
static void std_ptr_create_destroy(benchmark::State &state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(std::unique_ptr<T>(new T()));
  }
}

template <typename T, typename PoolTag>
static void small_ptr_create_destroy(benchmark::State &state) {
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(make_unique<T, char, PoolTag>());
  }
}

template <typename T> static void std_ptr_access(benchmark::State &state) {
  auto p = std::unique_ptr<T>(new T());
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(p.get());
  }
}

template <typename T, typename PoolTag>
static void small_ptr_access(benchmark::State &state) {
  auto p = make_unique<T, char, PoolTag>();
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(p.get());
  }
}

static void empty_loop(benchmark::State &state) {
  while (state.KeepRunning())
    ;
}

BENCHMARK(empty_loop);

BENCHMARK_TEMPLATE(std_ptr_create_destroy, char);
BENCHMARK_TEMPLATE(small_ptr_create_destroy, char, tag::stack_pool<3>);
BENCHMARK_TEMPLATE(small_ptr_create_destroy, char, tag::stack_pool<255>);
// BENCHMARK_TEMPLATE(small_ptr_create_destroy, char, tag::dynamic_pool);

BENCHMARK_TEMPLATE(std_ptr_create_destroy, std::array<char, 256>);
BENCHMARK_TEMPLATE(small_ptr_create_destroy, std::array<char, 256>,
                   tag::stack_pool<3>);
// BENCHMARK_TEMPLATE(small_ptr_create_destroy, std::array<char, 256>,
//                    tag::dynamic_pool);

BENCHMARK_TEMPLATE(std_ptr_access, char);
BENCHMARK_TEMPLATE(small_ptr_access, char, tag::stack_pool<3>);
// BENCHMARK_TEMPLATE(small_ptr_access, char, tag::dynamic_pool);

BENCHMARK_TEMPLATE(std_ptr_access, std::array<char, 256>);
BENCHMARK_TEMPLATE(small_ptr_access, std::array<char, 256>, tag::stack_pool<3>);
// BENCHMARK_TEMPLATE(small_ptr_access, std::array<char, 256>,
// tag::dynamic_pool);

BENCHMARK_MAIN();
