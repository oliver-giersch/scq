#include <iostream>
#include <stdexcept>

#include "scqueue/scq2.hpp"

using bounded_queue_t = scq::cas2::bounded_queue_t<int, 3>;

int main() {
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
