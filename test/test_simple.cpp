#include <iostream>

#include "scqueue/detail/scq1.hpp"

using scq1 = scq::cas1::bounded_index_queue_t<3>;

int dequeue_expect(bool result, std::size_t& idx);

int test_empty();
int test_one_out();

int main() {
  std::size_t idx;

  /*auto empty = scq1{ 0, 0 };
  if (empty.try_dequeue(idx)) {
    std::cout << idx << " (bad)" << std::endl;
  } else {
    std::cout << "empty (good)" << std::endl;
  }

  empty.try_enqueue(0);
  if (empty.try_dequeue(idx)) {
    std::cout << idx << " (good)" << std::endl;
  } else {
    std::cout << "empty (bad)" << std::endl;
  }*/

  /*auto single = scq1{ 0, 1 };
  single.try_enqueue(1);
  if (single.try_dequeue(idx)) {
    std::cout << idx << " (good)" << std::endl;
  } else {
    std::cout << "empty (bad)" << std::endl;
  }
  if (single.try_dequeue(idx)) {
    std::cout << idx << " (good)" << std::endl;
  } else {
    std::cout << "empty (bad)" << std::endl;
  }*/

  test_empty();
  test_one_out();

}

int dequeue_expect(bool result, std::size_t& idx, std::size_t expect) {
  if (!result) {
    std::cerr << "expected 1, got empty" << std::endl;
    return 1;
  } else if (idx != expect) {
    std::cerr << "expected " << expect << ", got " << idx << std::endl;
    return 1;
  }

  return 0;
}

int test_empty() {
  std::size_t idx;
  auto empty = scq1{ 0, 0 };
  if (empty.try_dequeue(idx)) {
    std::cerr << "queue should be empty" << std::endl;
    return 1;
  }

  empty.try_enqueue(0);
  if (dequeue_expect(empty.try_dequeue(idx), idx, 0)) {
    return 1;
  }

  return 0;
}

int test_one_out() {
  std::size_t idx;
  auto one_out = scq1{ 1, scq1::CAPACITY };
  one_out.try_enqueue(0);

  if (dequeue_expect(one_out.try_dequeue(idx), idx, 1)) {
    return 1;
  }

  if (dequeue_expect(one_out.try_dequeue(idx), idx, 2)) {
    return 1;
  }

  if (dequeue_expect(one_out.try_dequeue(idx), idx, 3)) {
    return 1;
  }

  if (dequeue_expect(one_out.try_dequeue(idx), idx, 4)) {
    return 1;
  }

  if (dequeue_expect(one_out.try_dequeue(idx), idx, 5)) {
    return 1;
  }

  if (dequeue_expect(one_out.try_dequeue(idx), idx, 6)) {
    return 1;
  }

  if (dequeue_expect(one_out.try_dequeue(idx), idx, 7)) {
    return 1;
  }

  if (dequeue_expect(one_out.try_dequeue(idx), idx, 0)) {
    return 1;
  }

  return 0;
}
