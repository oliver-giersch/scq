#ifndef SCQ1_FWD_HPP
#define SCQ1_FWD_HPP

#include <atomic>
#include <array>
#include <cstdint>
#include <limits>

#include "scqueue/detail/detail.hpp"

namespace scq::cas1 {
namespace detail {
  enum class init_t { PRE_FILLED };
}

template <std::size_t O = 15>
class bounded_index_queue_t {
  static constexpr auto HALF         = std::size_t{ 1 } << O;
  static constexpr auto N            = 2 * HALF;
  static constexpr auto FINALIZE_BIT = std::uintptr_t{ 1 } << (std::numeric_limits<uintptr_t>::digits - 1);
  static constexpr auto THRESHOLD    = 3 * std::intptr_t{ N } - 1;
  static constexpr auto EMPTY_SLOT   = std::numeric_limits<std::uintptr_t>::max();
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
  static constexpr auto CAPACITY = HALF;

  bounded_index_queue_t() noexcept;
  explicit bounded_index_queue_t(detail::init_t init) noexcept;
  ~bounded_index_queue_t() = default;

  template <bool finalize = false>
  bool try_enqueue(std::size_t idx, bool ignore_empty = false);
  bool try_dequeue(std::size_t& idx, bool ignore_empty = false) noexcept;
  void finalize_queue() noexcept;
  void reset_threshold(std::memory_order order) noexcept;
};
}

#endif /* SCQ1_FWD_HPP */
