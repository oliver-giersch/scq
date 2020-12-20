#ifndef SCQD_FWD_HPP
#define SCQD_FWD_HPP

#include <atomic>
#include <array>

#include "scqueue/detail/detail.hpp"
#include <scqueue/detail/scq1_fwd.hpp>

namespace scq::d {
template <typename T, std::size_t O = 16, bool finalize = false>
class bounded_queue_t {
public:
  using pointer = T*;
  static constexpr auto CAPACITY = std::size_t{ 1 } << O;
private:
  template <bool _finalize>
  using index_queue_t = ::scq::cas1::bounded_index_queue_t<O, _finalize>;
  using slot_array_t  = std::array<pointer, CAPACITY>;
  /** The queue for storing the indices of enqueued pointers. */
  index_queue_t<finalize> m_aq;
  /** The array storing the actual pointers. */
  slot_array_t  m_slots{};
  /** The queue for storing all available indices. */
  index_queue_t<false> m_fq;

public:
  /** constructors */
  bounded_queue_t() noexcept;
  explicit bounded_queue_t(pointer first);
  /** destructor */
  ~bounded_queue_t() = default;

  /** Attempts to enqueue an element at the end of the queue. */
  bool try_enqueue(pointer elem, bool ignore_empty = false);
  /** Attempts to dequeue an element from the start of the queue. */
  bool try_dequeue(pointer& result, bool ignore_empty = false);
  void reset_threshold(std::memory_order order);
};
}

#endif /* SCQD_FWD_HPP */
