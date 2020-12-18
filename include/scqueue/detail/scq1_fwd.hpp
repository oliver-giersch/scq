#ifndef SCQ1_FWD_HPP
#define SCQ1_FWD_HPP

#include <atomic>
#include <array>
#include <cstdint>
#include <limits>

#include "scqueue/detail/detail.hpp"

namespace scq::cas1 {
template <std::size_t O = 15>
class bounded_index_queue_t {
  static_assert(O >= 2, "order must be greater than 2");
  /** constructor argument type */
  struct queue_init_t {
    std::size_t deq_count, enq_count;
    [[nodiscard]] bool is_empty() const {
      return this->deq_count == 0 && this->enq_count == 0;
    }
  };
  /** size and bit constants */
  static constexpr auto HALF       = std::size_t{ 1 } << O;
  static constexpr auto N          = 2 * HALF;
  static constexpr auto THRESHOLD  = 3 * std::intptr_t{ N } - 1;
  static constexpr auto EMPTY_SLOT = std::numeric_limits<std::uintptr_t>::max();
  static constexpr auto FINALIZE   =
      std::uintptr_t{ 1 } << (std::numeric_limits<std::uintptr_t>::digits - 1);
  /** type aliases */
  using cycle_t = scq::detail::cycle_t;
  using slot_array_t = std::array<std::atomic_uintptr_t, N>;
  /** memory ordering constants */
  static constexpr auto relaxed = std::memory_order_relaxed;
  static constexpr auto acquire = std::memory_order_acquire;
  static constexpr auto release = std::memory_order_release;
  static constexpr auto acq_rel = std::memory_order_acq_rel;

  static constexpr std::size_t cache_remap(std::size_t idx) noexcept {
    return ((idx % N) >> (O - 3)) | ((idx << 4) % N);
  }

  void catchup(std::uintptr_t tail, std::uintptr_t head) noexcept;

  alignas(128) std::atomic_uintptr_t m_head;
  alignas(128) std::atomic_uintptr_t m_tail;
  alignas(128) std::atomic_intptr_t  m_threshold;
  alignas(128) slot_array_t          m_slots{ };

public:
  /** queue capacity */
  static constexpr auto CAPACITY = HALF;
  /** default init arguments */
  static constexpr auto EMPTY  = queue_init_t{ 0, 0 };
  static constexpr auto FILLED = queue_init_t{ 0, HALF };

  /** constructor */
  explicit bounded_index_queue_t(queue_init_t init);
  ~bounded_index_queue_t() = default;

  /** Attempts to enqueue the given index at the queue's back. */
  template <bool finalize = false>
  bool try_enqueue(std::size_t idx, bool ignore_empty = false);
  /** Attempts to dequeue the index at the queue's front. */
  bool try_dequeue(std::size_t& idx, bool ignore_empty = false) noexcept;
  /** Finalizes the queue, closing it for further enqueues. */
  void finalize_queue() noexcept;
  /** Resets the threshold value. */
  void reset_threshold(std::memory_order order) noexcept;
};
}

#endif /* SCQ1_FWD_HPP */
