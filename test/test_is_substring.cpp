#include <string_utils.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_isSubstring() {
  const char *a = "abcdef";
  const char *b = "1234abcd6789abcdefJJKKLL";
  TEST_ASSERT_TRUE(isSubstring(a, b));
}

void test_indexOf() {
  const char *str = "hello world";

  TEST_ASSERT_EQUAL(indexOf(str, 'w'), 6);
  TEST_ASSERT_EQUAL(indexOf(str, 'o', 4), 4);
  TEST_ASSERT_EQUAL(indexOf(str, 'o', 5), 7);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_indexOf);
  RUN_TEST(test_isSubstring);

  UNITY_END();
}