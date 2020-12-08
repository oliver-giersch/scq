#ifndef SCQ_HPP
#define SCQ_HPP

#include <atomic>
#include <array>

#include "detail.hpp"

namespace scq {
template <typename T, std::size_t O = 15>
class ring {
  using pointer = T*;
public:
  /**
   * Attempts to enqueue an element in the ring buffer's tail position.
   *
   * @tparam finalize defaults to false, if true full buffers are finalized,
   *   thereby preventing all further enqueue attempts
   *
   * @param elem the element to be enqueued
   * @param ignore_empty TODO
   * @param ignore_full if true, the function returns false if the ring is full,
   *   otherwise, the function becomes blocking and loops until an element can
   *   be enqueued
   * @return true upon success, false otherwise
   */
  template <bool finalize = false>
  bool try_enqueue(
      pointer elem,
      bool ignore_empty = false,
      bool ignore_full = false
  ) noexcept {
    if (!ignore_full) {
      // check if the queue is empty
      const auto tail = this->m_tail.load(ACQUIRE);
      if (tail >= N + this->m_head.load(ACQUIRE)) {
        return false;
      }
    }

    while (true) {
      // increment tail index
      const auto tail = this->m_tail.fetch_add(1, ACQ_REL);
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
      auto pair = this->m_array[tail_idx].load(ACQUIRE);

      while (true) {
        // calculate cycle of the read tuple value
        const auto enq_cycle = cycle_t{ pair.tag & ~(N - 1) };
        if (
            enq_cycle < tail_cycle &&
            (pair.tag == enq_cycle.val ||
             (pair.tag == (enq_cycle.val | uint64_t{ 0x2 }) &&
             this->m_head.load(ACQUIRE) <= tail))
        ) {
          const auto desired = pair_t{ tail_cycle.val | uint64_t{ 0x1 }, elem };
          if (!this->m_array[tail_idx].compare_exchange_weak(pair, desired, ACQ_REL, ACQUIRE)) {
            continue;
          }

          if (!ignore_empty && this->m_threshold.load(SEQ_CST) != THRESHOLD) {
            this->m_threshold.store(THRESHOLD, RELEASE);
          }

          return true;
        }

        this->m_threshold.store(THRESHOLD, RELEASE);

        if (!ignore_full) {
          if (tail + 1 >= N + this->m_head.load(SEQ_CST)) {
            if constexpr (finalize) {
              this->m_tail.fetch_or(FINALIZE_BIT, RELEASE);
            }

            return false;
          }
        }

        break;
      }
    }
  }

  bool try_dequeue(pointer& result, bool non_empty = false) noexcept {
    if (!non_empty && this->m_threshold.load(SEQ_CST) < 0) {
      return false;
    }

    while (true) {
      const auto head = this->m_head.fetch_add(1, ACQ_REL);
      const auto head_cycle = cycle_t{ head & ~(N - 1) };
      const auto head_idx = cache_remap(head);
      auto tag = this->m_array[head_idx].tag.load(ACQUIRE);

      cycle_t enq_cycle;
      uint64_t tag_new;

      do {
        enq_cycle = cycle_t{ tag & ~(N - 1) };
        if (enq_cycle.val == head_cycle.val) {
          auto pair = this->m_array[head_idx].fetch_and(pair_t { ~uint64_t{ 0x1 }, nullptr }, ACQ_REL);
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
          !this->m_array[head_idx].tag.compare_exchange_weak(tag, tag_new, ACQ_REL, ACQUIRE)
      );

      if (!non_empty) {
        const auto tail = cycle_t{ this->m_tail.load(ACQUIRE) };
        if (tail <= cycle_t { head + 1 }) {
          this->catchup(tail.val, head + 1);
          this->m_threshold.fetch_sub(1, ACQ_REL);
          return false;
        }

        if (this->m_threshold.fetch_sub(1, ACQ_REL) <= 0) {
          return false;
        }
      }
    }
  }

private:
  static constexpr std::size_t N = std::size_t{ 1 } << (O + 1);
  static constexpr std::size_t RING_MIN_PTR = 3;
  static constexpr int64_t     THRESHOLD = 2 * int64_t{ N } - 1;

  static constexpr uint64_t    FINALIZE_BIT = uint64_t{ 1 } << uint64_t{ 63 };

  using atomic_pair_t = detail::atomic_pair_t<T>;
  using cycle_t       = detail::cycle_t;
  using pair_t        = detail::pair_t<T>;
  using pair_array_t  = std::array<atomic_pair_t, N>;

  static constexpr auto ACQUIRE = std::memory_order_acquire;
  static constexpr auto RELEASE = std::memory_order_release;
  static constexpr auto ACQ_REL = std::memory_order_acq_rel;
  static constexpr auto SEQ_CST = std::memory_order_seq_cst;

  static constexpr size_t cache_remap(uint64_t idx) {
    return ((idx & (N - 1)) >> (O + 1 - RING_MIN_PTR)) | ((idx << RING_MIN_PTR) & (N - 1));
  }

  void catchup(uint64_t tail, uint64_t head) {
    while (!this->m_tail.compare_exchange_weak(tail, head, ACQ_REL, ACQUIRE)) {
      head = this->m_head.load(SEQ_CST);
      tail = this->m_tail.load(SEQ_CST);
      if (cycle_t { tail } >= cycle_t { head }) {
        break;
      }
    }
  }

  alignas(128) std::atomic<uint64_t> m_head{ N };
  alignas(128) std::atomic<int64_t>  m_threshold{ -1 };
  alignas(128) std::atomic<uint64_t> m_tail{ N };
  alignas(128) pair_array_t          m_array{ };
public:
  static constexpr std::size_t CAPACITY = N;
};
}

#endif /* SCQ_HPP */
