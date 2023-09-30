#include <string_utils.h>
#include <unity.h>
#include <cstdio>

void setUp() {}
void tearDown() {}

void test_isSubstring() {
  const char* a = "abcdef";
  const char* b = "1234abcd6789abcdefJJKKLL";
  TEST_ASSERT_TRUE(isSubstring(a, b));
}

void test_indexOf() {
  const char* str = "hello world";

  TEST_ASSERT_EQUAL(indexOf(str, 'w'), 6);
  TEST_ASSERT_EQUAL(indexOf(str, 'o', 4), 4);
  TEST_ASSERT_EQUAL(indexOf(str, 'o', 5), 7);
}

void test_getStringSplit() {
  const char* str = "SEM:C5:0:1671003832";
  size_t startIdx, length;
  getStringSplit(str, ':', 1, &startIdx, &length);
  printf("%d %d\n", startIdx, length);
  TEST_ASSERT_EQUAL(str[startIdx], 'C');
  TEST_ASSERT_EQUAL(str[startIdx + length - 1], '5');

  getStringSplit(str, ':', 2, &startIdx, &length);
  TEST_ASSERT_EQUAL(str[startIdx], '0');
  TEST_ASSERT_EQUAL(length, 1);

  getStringSplit(str, ':', 3, &startIdx, &length);
  char substr[length + 1];
  strncpy(substr, str + startIdx, sizeof(substr));
  TEST_ASSERT_TRUE(strcmp(substr, "1671003832") == 0);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_indexOf);
  RUN_TEST(test_isSubstring);
  RUN_TEST(test_getStringSplit);

  UNITY_END();
}