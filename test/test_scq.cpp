#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#include "scqueue/scqd.hpp"
#include "scqueue/scq2.hpp"

template <typename Q>
int test_queue();

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    throw std::invalid_argument("too few arguments");
  }

  const auto queue = std::string_view{ argv[1] };
  if (queue == "scq2") {
    return test_queue<scq::cas2::bounded_queue_t<int, 16>>();
  } else if (queue == "scqd") {
    return test_queue<scq::d::bounded_queue_t<int, 16>>();
  }

  throw std::invalid_argument("invalid queue argument");
}

template <typename Q>
int test_queue() {
  const auto thread_count = 8;
  const auto count = 8192;

  std::vector<std::vector<int>> thread_elements{};
  thread_elements.reserve(thread_count);

  for (auto t = 0; t < thread_count; ++t) {
    thread_elements.emplace_back();
    thread_elements[t].reserve(count);

    for (auto i = 0; i < count; ++i) {
      thread_elements[t].push_back(i);
    }
  }

  std::vector<std::thread> threads{ };
  threads.reserve(thread_count * 2);

  std::atomic_bool start{ false };
  std::atomic_uint64_t sum{ 0 };

  Q queue{};
  static_assert(Q::CAPACITY == thread_count * count, "not enough capacity");

  for (auto thread = 0; thread < thread_count; ++thread) {
    // producer thread
    threads.emplace_back([&, thread] {
      while (!start.load());

      for (auto op = 0; op < count; ++op) {
        queue.try_enqueue(&thread_elements[thread][op]);
      }
    });

    // consumer thread
    threads.emplace_back([&] {
      uint64_t thread_sum = 0;
      uint64_t deq_count = 0;

      while (!start.load()) {}

      while (deq_count < count) {
        int* deq = nullptr;
        const auto res = queue.try_dequeue(deq);
        if (res) {
          thread_sum += *deq;
          deq_count += 1;
        }
      }

      sum.fetch_add(thread_sum);
    });
  }

  start.store(true);

  for (auto& thread : threads) {
    thread.join();
  }

  int* deq = nullptr;
  if (queue.try_dequeue(deq)) {
    std::cerr << "queue not empty after count * threads dequeue operations" << std::endl;
    return 1;
  }

  const auto res = sum.load();
  const auto expected = thread_count * (count * (count - 1) / 2);
  if (res != expected) {
    std::cerr << "incorrect element sum, got " << sum << ", expected " << expected << std::endl;
    return 1;
  }

  std::cout << "test successful (sum = " << res << ")" << std::endl;
  return 0;
}
