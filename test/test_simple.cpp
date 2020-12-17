#include <iostream>

#include "scqueue/detail/scq1.hpp"

int main() {
  auto queue = scq::cas1::bounded_index_queue_t<15>{ scq::cas1::detail::init_t{} };

  std::size_t idx;
  auto res = queue.try_dequeue(idx);
  std::cout << (res ? "true" : "false") << std::endl;
  if (res) std::cout << idx << std::endl;

  res = queue.try_dequeue(idx);
  std::cout << (res ? "true" : "false") << std::endl;
  if (res) std::cout << idx << std::endl;

  res = queue.try_dequeue(idx);
  std::cout << (res ? "true" : "false") << std::endl;
  if (res) std::cout << idx << std::endl;

  res = queue.try_dequeue(idx);
  std::cout << (res ? "true" : "false") << std::endl;
  if (res) std::cout << idx << std::endl;

  res = queue.try_dequeue(idx);
  std::cout << (res ? "true" : "false") << std::endl;
  if (res) std::cout << idx << std::endl;
}
