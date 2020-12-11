#ifndef SCQ_FWD_HPP
#define SCQ_FWD_HPP

#include <atomic>
#include <array>

#include "detail.hpp"

namespace scq {
template<typename T, std::size_t O = 15>
class ring_t {
  static constexpr std::size_t N = std::size_t{ 1 } << (O + 1);
  static constexpr std::size_t RING_MIN_PTR = 3;
  static constexpr uint64_t    FINALIZE_BIT = uint64_t{ 1 } << uint64_t{ 63 };
  static constexpr int64_t     THRESHOLD = 2 * int64_t{ N } - 1;

  using atomic_pair_t = detail::atomic_pair_t<T>;
  using cycle_t       = detail::cycle_t;
  using pair_t        = detail::pair_t<T>;
  using pair_array_t  = std::array<atomic_pair_t, N>;

  static constexpr auto relaxed = std::memory_order_relaxed;
  static constexpr auto acquire = std::memory_order_acquire;
  static constexpr auto release = std::memory_order_release;
  static constexpr auto acq_rel = std::memory_order_acq_rel;
  static constexpr auto seq_cst = std::memory_order_seq_cst;

  static constexpr size_t cache_remap(uint64_t idx) noexcept {
    return ((idx & (N - 1)) >> (O + 1 - RING_MIN_PTR)) | ((idx << RING_MIN_PTR) & (N - 1));
  }

  void catchup(uint64_t tail, uint64_t head) noexcept;

  alignas(128) std::atomic<uint64_t> m_head{ N };
  alignas(128) std::atomic<int64_t> m_threshold{ -1 };
  alignas(128) std::atomic<uint64_t> m_tail{ N };
  alignas(128) pair_array_t m_array{};

public:
  using pointer = T*;

  ring_t() noexcept = default;
  explicit ring_t(pointer first);

  [[nodiscard]]
  constexpr std::size_t capacity() const noexcept {
    return N;
  }
  /**
   * Attempts to enqueue an element in the ring buffer's tail position.
   *
   * @tparam finalize defaults to false, if true, full buffers are finalized,
   *   thereby preventing all further enqueue attempts
   *
   * @param elem the element to be enqueued, must not be null
   * @param ignore_empty if true, the procedure will not reset the internal
   *   threshold at the appropriate points, which helps dequeue operations to
   *   detect an empty queue and should only be set, if the queue can never
   *   be empty
   * @param ignore_full if true, the procedure will perform any checks if the
   *   queue is already and should only be set, if the queue can never be full
   *
   * @return true upon success, false otherwise
   * @throws `std::invalid_argument` exception, if `elem` is `nullptr`
   */
  template<bool finalize = false>
  bool try_enqueue(pointer elem, bool ignore_empty = false, bool ignore_full = false);
  bool try_dequeue(pointer& result, bool non_empty = false) noexcept;
  void reset_threshold(std::memory_order order);
};
}

#endif /* SCQ_FWD_HPP */
