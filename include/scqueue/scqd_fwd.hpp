#ifndef SCQD_FWD_HPP
#define SCQD_FWD_HPP

#include <atomic>
#include <array>

#include <scqueue/detail/scq1_fwd.hpp>

namespace scq::d {
template <typename T, std::size_t O = 15>
class bounded_queue_t {
public:
  using pointer = T*;
private:
  static constexpr auto N = std::size_t{ 1 } << O;

  using index_queue_t = ::scq::cas1::bounded_index_queue_t<O>;
  using init_t        = ::scq::cas1::detail::init_t;
  using slot_array_t  = std::array<pointer, N>;

  index_queue_t m_aq;
  slot_array_t  m_slots{};
  index_queue_t m_fq;

public:
  static constexpr auto CAPACITY = N;

  bounded_queue_t() noexcept;
  ~bounded_queue_t() = default;

  /** Attempts to enqueue an element at the end of the queue. */
  template <bool finalize = false>
  bool try_enqueue(pointer elem, bool ignore_empty = false);
  /** Attempts to dequeue an element from the start of the queue. */
  bool try_dequeue(pointer& result, bool ignore_empty = false);
  void reset_threshold(std::memory_order order);
};
}

#endif /* SCQD_FWD_HPP */
