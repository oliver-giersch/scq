#ifndef SCQ_FWD_HPP
#define SCQ_FWD_HPP

#include <atomic>
#include <array>
#include <limits>

#include "scqueue/detail/detail.hpp"

namespace scq::cas2 {
template <typename T, std::size_t O = 16>
class bounded_queue_t {
  /** size and bit constants */
  static constexpr auto N           = std::size_t{ 1 } << O;
  static constexpr auto ENQUEUE_BIT = std::uintmax_t{ 0b01 };
  static constexpr auto DEQUEUE_BIT = std::uintmax_t{ 0b10 };
  static constexpr auto THRESHOLD   = 2 * std::intmax_t{ N } - 1;
  static constexpr auto FINALIZE    =
      std::uintmax_t{ 1 } << (std::numeric_limits<std::uintmax_t>::digits - 1);
  /** type aliases */
  using atomic_pair_t = detail::atomic_pair_t<T>;
  using cycle_t       = detail::cycle_t;
  using pair_t        = detail::pair_t<T>;
  using pair_array_t  = std::array<atomic_pair_t, N>;
  /** memory ordering constants */
  static constexpr auto relaxed = std::memory_order_relaxed;
  static constexpr auto acquire = std::memory_order_acquire;
  static constexpr auto release = std::memory_order_release;
  static constexpr auto acq_rel = std::memory_order_acq_rel;

  /** Remaps idx to spread consecutive access around in order to avoid false sharing. */
  static constexpr auto cache_remap(std::size_t idx) noexcept {
    return ((idx % N) >> (O - 3)) | ((idx << 3) % N);
  }

  void catchup(std::uintmax_t tail, std::uintmax_t head) noexcept;

  alignas(128) std::atomic_uintmax_t m_head{ N };
  alignas(128) std::atomic_uintmax_t m_tail{ N };
  alignas(128) std::atomic_intmax_t  m_threshold{ -1 };
  alignas(128) pair_array_t          m_array{ };

public:
  /** queue capacity */
  static constexpr auto CAPACITY = N;
  using pointer = T*;

  /** constructor */
  bounded_queue_t() noexcept = default;
  explicit bounded_queue_t(pointer first);

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
  bool try_enqueue(
      pointer elem,
      bool ignore_empty = false,
      bool ignore_full = false
  );

  /**
   * Attempts to dequeue an element from the ring buffer's head position.
   *
   * @param result the pointer where the dequeued element is written into on
   * success
   * @param ignore_empty if true, the procedure will ignore the possibility of
   * the queue ever being empty
   *
   * @return true upon success, false otherwise
   */
  bool try_dequeue(pointer& result, bool ignore_empty = false) noexcept;

  /** Resets the threshold. */
  void reset_threshold(std::memory_order order) noexcept;
};
}

#endif /* SCQ_FWD_HPP */
