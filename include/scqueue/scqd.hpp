#ifndef SCQD_HPP
#define SCQD_HPP

#include "scqueue/scqd_fwd.hpp"
#include "scqueue/detail/scq1.hpp"

namespace scq::d {
template <typename T, std::size_t O, bool finalize>
bounded_queue_t<T, O, finalize>::bounded_queue_t() noexcept :
  m_aq{ index_queue_t<finalize>::EMPTY },
  m_fq{ index_queue_t<false>::FILLED } {}

template <typename T, std::size_t O, bool finalize>
bounded_queue_t<T, O, finalize>::bounded_queue_t(pointer first) :
  m_aq{{ 0, 1 }}, m_fq{{ 1, CAPACITY }}
{
  if (first == nullptr) [[unlikely]] {
    throw std::invalid_argument("pointer `first` must not be null");
  }

  this->m_slots[0] = first;
}

template <typename T, std::size_t O, bool finalize>
bool bounded_queue_t<T, O, finalize>::try_enqueue(pointer elem, bool ignore_empty) {
  std::size_t enqueue_idx;
  if (!this->m_fq.try_dequeue(enqueue_idx, ignore_empty)) {
    if constexpr (finalize) {
      this->m_aq.finalize_queue();
    }

    return false;
  }

  this->m_slots[enqueue_idx] = elem;

  const auto res = this->m_aq.try_enqueue(enqueue_idx);
  if constexpr (finalize) {
    if (!res) {
      (void) this->m_fq.try_enqueue(enqueue_idx);
      return false;
    }
  }

  return true;
}

template <typename T, std::size_t O, bool finalize>
bool bounded_queue_t<T, O, finalize>::try_dequeue(pointer& result, bool ignore_empty) {
  std::uintmax_t dequeue_idx;
  if (!this->m_aq.try_dequeue(dequeue_idx)) {
    return false;
  }

  result = this->m_slots[dequeue_idx];

  (void) this->m_fq.try_enqueue(dequeue_idx, ignore_empty);
  return true;
}

template <typename T, std::size_t O, bool finalize>
void bounded_queue_t<T, O, finalize>::reset_threshold(std::memory_order order) {
  this->m_aq.reset_threshold(order);
}
}

#endif /* SCQD_HPP */
