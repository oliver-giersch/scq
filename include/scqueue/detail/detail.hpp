#ifndef SCQ_DETAIL_HPP
#define SCQ_DETAIL_HPP

#include <atomic>
#include <compare>

#include <iostream>

namespace scq::detail {
struct cycle_t {
  std::uintmax_t val;
};

constexpr auto operator<=>(const cycle_t& lhs, const cycle_t& rhs) {
  return static_cast<std::intmax_t>(lhs.val) - static_cast<std::intmax_t>(rhs.val) <=> 0;
}

template <typename T>
struct pair_t {
  using pointer = T*;

  std::uintmax_t tag;
  pointer        ptr;
};

template <typename T>
struct alignas(16) atomic_pair_t {
  using pointer = T*;

  std::atomic<std::uintmax_t> tag{ 0 };
  std::atomic<pointer>        ptr{ nullptr };

  bool compare_exchange_weak(
      pair_t<T>& expected,
      pair_t<T>  desired,
      std::memory_order success,
      std::memory_order failure
  ) {
    (void) success;
    (void) failure;
    uint8_t res;
    asm volatile(
      "lock cmpxchg16b %0"
      : "+m"(*this), "=@ccz"(res), "+a"(expected.tag), "+d"(expected.ptr) // input ops
      : "b"(desired.tag), "c"(desired.ptr)                                // output ops
      : "memory"                                                          // clobbers
    );

    return res != 0;
  }

  pair_t<T> fetch_and(pair_t<T> pair, std::memory_order order) {
    auto curr = pair_t {
      this->tag.load(std::memory_order_relaxed),
      this->ptr.load(std::memory_order_relaxed)
    };

    while (true) {
      const auto next = pair_t {
          curr.tag & pair.tag,
          reinterpret_cast<pointer>(
              reinterpret_cast<std::uintptr_t>(curr.ptr)
              & reinterpret_cast<std::uintptr_t>(pair.ptr)
          )
      };

      if (this->compare_exchange_weak(curr, next, order, std::memory_order_relaxed)) {
        return curr;
      }
    }
  }
};
}

#endif /* SCQ_DETAIL_HPP */
