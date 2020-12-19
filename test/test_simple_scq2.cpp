#include <iostream>
#include <stdexcept>

#include "scqueue/scq2.hpp"

using bounded_queue_t = scq::cas2::bounded_queue_t<int, 3>;

int test_with_first();
int test_capacity();
int test_finalize();

int main() {
  test_with_first();
  test_capacity();
  test_finalize();
}

int test_with_first() {
  int elem = 5;
  auto with_first = bounded_queue_t{ &elem };

  int second = 6;
  if (!with_first.try_enqueue(&second)) {
    throw std::runtime_error("enqueue failed on non-full queue");
  }

  int* dequeued;
  if (!with_first.try_dequeue(dequeued)) {
    throw std::runtime_error("failed to dequeue element");
  }

  if (*dequeued != 5) {
    throw std::runtime_error("dequeued wrong element");
  }

  if (!with_first.try_dequeue(dequeued)) {
    throw std::runtime_error("failed to dequeue element");
  }

  if (*dequeued != 6) {
    throw std::runtime_error("dequeued wrong element");
  }

  if (with_first.try_dequeue(dequeued)) {
    throw std::runtime_error("queue should be empty");
  }

  return 0;
}

int test_capacity() {
  auto queue = bounded_queue_t{ };
  int elem = 1;
  static_assert(bounded_queue_t::CAPACITY == 8);

  for (auto i = 0; i < bounded_queue_t::CAPACITY; ++i) {
    if (!queue.try_enqueue(&elem)) {
      throw std::runtime_error("enqueue failed on non-full queue");
    }
  }

  if (queue.try_enqueue(&elem)) {
    throw std::runtime_error("enqueue should have failed on full queue");
  }

  int* res;
  for (auto i = 0; i < bounded_queue_t::CAPACITY; ++i) {
    if (!queue.try_dequeue(res)) {
      throw std::runtime_error("dequeue failed on non-empty queue");
    }

    if (*res != 1) {
      throw std::runtime_error("dequeued wrong element");
    }
  }

  if (queue.try_dequeue(res)) {
    throw std::runtime_error("dequeued should have failed on empty queue");
  }

  return 0;
}

int test_finalize() {
  auto queue = bounded_queue_t{ };
  int elem = 1;
  static_assert(bounded_queue_t::CAPACITY == 8);

  for (auto i = 0; i < bounded_queue_t::CAPACITY; ++i) {
    if (!queue.template try_enqueue<true>(&elem)) {
      throw std::runtime_error("enqueue failed on non-full queue");
    }
  }

  // finalizes the queue
  if (queue.template try_enqueue<true>(&elem)) {
    throw std::runtime_error("enqueue should have failed on full queue");
  }

  int* res;
  for (auto i = 0; i < bounded_queue_t::CAPACITY; ++i) {
    if (!queue.try_dequeue(res)) {
      throw std::runtime_error("dequeue failed on non-empty queue");
    }

    if (*res != 1) {
      throw std::runtime_error("dequeued wrong element");
    }
  }

  if (queue.template try_enqueue<true>(&elem)) {
    throw std::runtime_error("enqueue should have failed on finalized queue");
  }

  if (queue.try_dequeue(res)) {
    throw std::runtime_error("dequeued should have failed on empty queue");
  }

  return 0;
}
