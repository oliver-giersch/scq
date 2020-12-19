#ifndef SCQ2_HPP
#define SCQ2_HPP

#include <atomic>
#include <array>

#include "scq2_fwd.hpp"

namespace scq::cas2 {
template<typename T, std::size_t O>
bounded_queue_t<T, O>::bounded_queue_t(pointer first) {
  if (first == nullptr) {
    throw std::invalid_argument("elem must not be null");
  }

  const auto idx = cache_remap(N);
  this->m_tail.store(N + 1, relaxed);
  this->m_array[idx].tag.store(N | ENQUEUE_BIT, relaxed);
  this->m_array[idx].ptr.store(first, relaxed);
  this->reset_threshold(relaxed);
}

template<typename T, std::size_t O>
template<bool finalize>
bool bounded_queue_t<T, O>::try_enqueue(
    pointer elem,
    bool ignore_empty,
    bool ignore_full
) {
  if (elem == nullptr) {
    throw std::invalid_argument("`elem` must not be null");
  }

  if (!ignore_full) {
    // check if the queue is empty
    const auto tail = this->m_tail.load(acquire);
    if (tail >= this->m_head.load(acquire) + N) {
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
    auto& slot = this->m_array[cache_remap(tail)];
    // read the pair at the (remapped) buffer index
    auto pair = pair_t{
        slot.tag.load(relaxed),
        slot.ptr.load(relaxed)
    };

    while (true) {
      // calculate cycle of the read tuple value
      const auto cycle = cycle_t{ pair.tag & ~(N - 1) };
      if (
          cycle < tail_cycle
          && (
              pair.tag == cycle.val
              || (
                  pair.tag == (cycle.val | DEQUEUE_BIT)
                  && this->m_head.load(acquire) <= tail
              )
          )
      ) {
        const auto desired = pair_t{ tail_cycle.val | ENQUEUE_BIT, elem };
        if (!slot.compare_exchange_weak(pair, desired, acq_rel, acquire)) {
          continue;
        }

        if (!ignore_empty && this->m_threshold.load(relaxed) != THRESHOLD) {
          this->reset_threshold(release);
        }

        return true;
      }

      if (!ignore_full) {
        if (tail + 1 >= this->m_head.load(relaxed) + N) {
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
bool bounded_queue_t<T, O>::try_dequeue(pointer& result, bool ignore_empty) noexcept {
  if (!ignore_empty && this->m_threshold.load(acquire) < 0) {
    return false;
  }

  while (true) {
    const auto head = this->m_head.fetch_add(1, acq_rel);
    const auto head_cycle = cycle_t{ head & ~(N - 1) };

    auto& slot = this->m_array[cache_remap(head)];
    auto tag = slot.tag.load(acquire);

    cycle_t tag_cycle;
    std::uintmax_t tag_new;

    do {
      tag_cycle = cycle_t{ tag & ~(N - 1) };
      if (tag_cycle.val == head_cycle.val) {
        auto pair = slot.fetch_and(pair_t{ ~ENQUEUE_BIT, nullptr }, acq_rel);
        result = pair.ptr;
        return true;
      }

      if ((tag & ~DEQUEUE_BIT) != tag_cycle.val) {
        tag_new = tag | DEQUEUE_BIT;
        if (tag == tag_new) {
          break;
        }
      } else {
        tag_new = head_cycle.val | (tag & DEQUEUE_BIT);
      }
    } while (
        tag_cycle < head_cycle
        && !slot.tag.compare_exchange_weak(tag, tag_new, acq_rel, acquire)
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

template<typename T, std::size_t O>
void bounded_queue_t<T, O>::reset_threshold(std::memory_order order) {
  this->m_threshold.store(THRESHOLD, order);
}

template<typename T, std::size_t O>
void bounded_queue_t<T, O>::catchup(std::uintmax_t tail, std::uintmax_t head) noexcept {
  while (!this->m_tail.compare_exchange_weak(tail, head, acq_rel, acquire)) {
    head = this->m_head.load(acquire);
    tail = this->m_tail.load(acquire);
    if (cycle_t{ tail } >= cycle_t{ head }) {
      break;
    }
  }
}
}

#endif /* SCQ2_HPP */
