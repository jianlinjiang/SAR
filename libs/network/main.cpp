#include "request.hpp"
#include <iostream>
using namespace network::server;

int test(const int a, const int b ) {
  return a + b;
}

int main() {
  std::cout << sizeof(request) <<std::endl;
  std::cout << REQUEST_MAX_CONTENT_LENGTH << std::endl;
  int a = 1;
  int b = 1;
  std::cout << test(a, b) << std::endl;
  return 0;
}