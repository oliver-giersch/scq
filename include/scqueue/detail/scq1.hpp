#ifndef SCQ1_HPP
#define SCQ1_HPP

#include "scq1_fwd.hpp"

#include <stdexcept>

using namespace std;

namespace scq::cas1 {
template <std::size_t O>
bounded_index_queue_t<O>::bounded_index_queue_t() noexcept :
  m_head{ 0 }, m_tail{ 0 }, m_threshold{ -1 }
{
  for (auto& slot : this->m_slots) {
    slot.store(EMPTY_SLOT, std::memory_order_relaxed);
  }
}

template <std::size_t O>
bounded_index_queue_t<O>::bounded_index_queue_t(detail::init_t init) noexcept :
  m_head{ 0 }, m_tail( HALF ), m_threshold{ THRESHOLD }
{
  for (auto i = 1; i < HALF; ++i) {
    this->m_slots[cache_remap(i)].store(N + i, relaxed);
  }

  for (auto i = HALF; i < CAPACITY; ++i) {
    this->m_slots[cache_remap(i)].store(EMPTY_SLOT, relaxed);
  }
}

template <std::size_t O>
template <bool finalize>
bool bounded_index_queue_t<O>::try_enqueue(std::size_t idx, bool ignore_empty) {
  if (idx >= CAPACITY) [[unlikely]] {
    throw std::invalid_argument("idx must be >= 0");
  }

  const auto enq_idx = static_cast<std::uintptr_t>(idx) ^ (N - 1);
  while (true) {
    const auto tail = this->m_tail.fetch_add(1, acq_rel);
    if constexpr (finalize) {
      if ((tail & FINALIZE_BIT) == FINALIZE_BIT) [[unlikely]] {
        return false;
      }
    }

    const auto tail_cycle = cycle_t{ (tail << 1) | (2 * N - 1) };
    auto& slot = this->m_slots[cache_remap(tail)];
    auto tag = slot.load(acquire);

    while (true) {
      const auto cycle = cycle_t{ tag | 2 * N - 1 };
      if (
          cycle < tail_cycle
          && (
              tag == cycle.val
              || (
                  (tag == (cycle.val ^ N))
                  && cycle_t{ this->m_head.load(acquire) } <= cycle_t{ tail }
              )
          )
      ) {
        if (!slot.compare_exchange_weak(tag, tail_cycle.val ^ enq_idx, acq_rel, acquire)) {
          continue;
        }

        if (!ignore_empty && (this->m_threshold.load() != THRESHOLD)) {
          this->m_threshold.store(THRESHOLD);
        }

        return true;
      }

      break;
    }
  }
}

template <std::size_t O>
bool bounded_index_queue_t<O>::try_dequeue(std::size_t& idx, bool ignore_empty) noexcept {
  if (!ignore_empty && this->m_threshold.load(acquire) < 0) {
    return false;
  }

  while (true) {
    const auto head = this->m_head.fetch_add(1, acq_rel);
    const auto head_cycle = cycle_t{ (head << 1) | (2 * N - 1) };
    auto& slot = this->m_slots[cache_remap(head)];

    std::uintptr_t entry;
    auto attempt = 0;

    retry:
    entry = slot.load(acquire);
    uintptr_t entry_new;
    cycle_t entry_cycle;

    do {
      entry_cycle = cycle_t{ entry | (2 * N - 1) };
      if (entry_cycle.val == head_cycle.val) {
        slot.fetch_or(N - 1, acq_rel);
        idx = entry % N;
        return true;
      }

      if ((entry | N) != entry_cycle.val) {
        entry_new = entry & ~N;
        if (entry == entry_new) {
          break;
        }
      } else {
        if (++attempt <= 10'000) {
          goto retry;
        }

        entry_new = head_cycle.val ^ ((~entry) & N);
      }
    } while (
        entry_cycle < head_cycle
        && slot.compare_exchange_weak(entry, entry_new, acq_rel, acquire)
    );

    if (!ignore_empty) {
      const auto tail = this->m_tail.load(acquire);
      if (cycle_t{ tail } <= cycle_t{ head + 1 }) {
        this->catchup(tail, head + 1);
        this->m_threshold.fetch_sub(1, acq_rel);
        return false;
      }

      if (this->m_threshold.fetch_sub(1, acq_rel) <= 0) {
        return false;
      }
    }
  }
}

template <std::size_t O>
void bounded_index_queue_t<O>::finalize_queue() noexcept {
  this->m_tail.fetch_or(FINALIZE_BIT, release);
}

template <std::size_t O>
void bounded_index_queue_t<O>::reset_threshold(std::memory_order order) noexcept {
  this->m_threshold.store(THRESHOLD, order);
}

template <std::size_t O>
void bounded_index_queue_t<O>::catchup(uint64_t tail, uint64_t head) noexcept {
  while (!this->m_tail.compare_exchange_weak(tail, head, acq_rel, acquire)) {
    head = this->m_head.load(acquire);
    tail = this->m_tail.load(acquire);

    if (cycle_t{ tail } >= cycle_t{ head }) {
      break;
    }
  }
}
}

#endif /* SCQ1_HPP */
