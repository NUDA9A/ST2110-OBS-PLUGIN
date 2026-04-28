#include <cassert>
#include <cstdint>

#include <st2110/rtp.hpp>

static void test_basic_distance() {
  assert(st2110::seq_distance(100, 101) == 1);
  assert(st2110::seq_distance(101, 100) == -1);
  assert(st2110::seq_distance(500, 500) == 0);
}

static void test_wrap_distance() {
  assert(st2110::seq_distance(65535, 0) == 1);
  assert(st2110::seq_distance(0, 65535) == -1);

  assert(st2110::seq_distance(65534, 1) == 3);
  assert(st2110::seq_distance(1, 65534) == -3);
}

static void test_seq_less_basic() {
  assert(st2110::seq_less(100, 101));
  assert(!st2110::seq_less(101, 100));
  assert(!st2110::seq_less(500, 500));
}

static void test_seq_less_wrap() {
  assert(st2110::seq_less(65535, 0));
  assert(st2110::seq_less(65534, 1));

  assert(!st2110::seq_less(0, 65535));
  assert(!st2110::seq_less(1, 65534));
}

int main() {
  test_basic_distance();
  test_wrap_distance();
  test_seq_less_basic();
  test_seq_less_wrap();
  return 0;
}