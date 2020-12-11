#ifndef SCQ_HPP
#define SCQ_HPP

#include <atomic>
#include <array>

#include "scq_fwd.hpp"

namespace scq {
template<typename T, std::size_t O>
ring_t<T, O>::ring_t(pointer first) {
  if (first == nullptr) {
    throw std::invalid_argument("elem must not be null");
  }

  const auto idx = cache_remap(N);
  this->m_tail.store(N + 1, relaxed);
  this->m_array[idx].tag.store(N | uint64_t{ 0x1 }, relaxed);
  this->m_array[idx].ptr.store(first, relaxed);
  this->reset_threshold(relaxed);
}

template<typename T, std::size_t O>
template<bool finalize>
bool ring_t<T, O>::try_enqueue(pointer elem, bool ignore_empty, bool ignore_full) {
  if (elem == nullptr) {
    throw std::invalid_argument("elem must not be null");
  }

  if (!ignore_full) {
    // check if the queue is empty
    const auto tail = this->m_tail.load(acquire);
    if (tail >= N + this->m_head.load(acquire)) {
      return false;
    }
  }

  while (true) {
    // increment tail index
    const auto tail = this->m_tail.fetch_add(1, acq_rel);
    if constexpr (finalize) {
      // if the ring is finalized, return false
      if ((tail & FINALIZE_BIT) == FINALIZE_BIT) {
        return false;
      }
    }

    // calculate cycle for tail index
    const auto tail_cycle = cycle_t{ tail & ~(N - 1) };
    // calculate remapped index for avoiding false sharing
    const auto tail_idx = cache_remap(tail);
    // read the pair at the (remapped) buffer index
    auto pair = pair_t{
        this->m_array[tail_idx].tag.load(acquire),
        this->m_array[tail_idx].ptr.load(acquire)
    };

    while (true) {
      // calculate cycle of the read tuple value
      const auto cycle = cycle_t{ pair.tag & ~(N - 1) };
      if (
          cycle < tail_cycle
          && (
              pair.tag == cycle.val
              || (
                  pair.tag == (cycle.val | uint64_t{ 0x2 })
                  && this->m_head.load(acquire) <= tail
              )
          )
      ) {
        const auto desired = pair_t{ tail_cycle.val | uint64_t{ 0x1 }, elem };
        if (!this->m_array[tail_idx].compare_exchange_weak(pair, desired, acq_rel, acquire)) {
          continue;
        }

        if (!ignore_empty && this->m_threshold.load(relaxed) != THRESHOLD) {
          this->reset_threshold(release);
        }

        return true;
      }

      this->reset_threshold(seq_cst);

      if (!ignore_full) {
        if (tail + 1 >= N + this->m_head.load(relaxed)) {
          if constexpr (finalize) {
            this->m_tail.fetch_or(FINALIZE_BIT, release);
          }

          return false;
        }
      }

      break;
    }
  }
}

template<typename T, std::size_t O>
bool ring_t<T, O>::try_dequeue(pointer& result, bool non_empty) noexcept {
  if (!non_empty && this->m_threshold.load(acquire) < 0) {
    return false;
  }

  while (true) {
    const auto head = this->m_head.fetch_add(1, acq_rel);
    const auto head_cycle = cycle_t{ head & ~(N - 1) };
    const auto head_idx = cache_remap(head);
    auto tag = this->m_array[head_idx].tag.load(acquire);

    cycle_t enq_cycle;
    uint64_t tag_new;

    do {
      enq_cycle = cycle_t{ tag & ~(N - 1) };
      if (enq_cycle.val == head_cycle.val) {
        auto pair = this->m_array[head_idx].fetch_and(pair_t{ ~uint64_t{ 0x1 }, nullptr }, acq_rel);
        result = pair.ptr;
        return true;
      }

      if ((tag & ~uint64_t{ 0x2 }) != enq_cycle.val) {
        tag_new = tag | uint64_t{ 0x2 };
        if (tag == tag_new) {
          break;
        }
      } else {
        tag_new = head_cycle.val | (tag & 0x2);
      }
    } while (
        enq_cycle < head_cycle &&
        !this->m_array[head_idx].tag.compare_exchange_weak(tag, tag_new, acq_rel, acquire)
        );

    if (!non_empty) {
      const auto tail = cycle_t{ this->m_tail.load(acquire) };
      if (tail <= cycle_t{ head + 1 }) {
        this->catchup(tail.val, head + 1);
        this->m_threshold.fetch_sub(1, acq_rel);
        return false;
      }

      if (this->m_threshold.fetch_sub(1, acq_rel) <= 0) {
        return false;
      }
    }
  }
}

template<typename T, std::size_t O>
void ring_t<T, O>::reset_threshold(std::memory_order order) {
  this->m_threshold.store(THRESHOLD, order);
}

template<typename T, std::size_t O>
void ring_t<T, O>::catchup(uint64_t tail, uint64_t head) noexcept {
  while (!this->m_tail.compare_exchange_weak(tail, head, acq_rel, acquire)) {
    head = this->m_head.load(acquire);
    tail = this->m_tail.load(acquire);
    if (cycle_t{ tail } >= cycle_t{ head }) {
      break;
    }
  }
}
}

#endif /* SCQ_HPP */
