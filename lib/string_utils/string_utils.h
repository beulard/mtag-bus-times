#include <cstring>

int indexOf(const char *str, char c, size_t from = 0) {
  for (size_t i = from; str[i] != '\0'; ++i) {
    if (str[i] == c) {
      return i;
    }
  }
  return -1;
}

// True if a is a substring of b
bool isSubstring(const char *a, const char *b) {
  // Previous occurrence of the first char of a
  size_t lastIndexOf = 0;
  size_t a_len = strlen(a);
  size_t b_len = strlen(b);
  while (lastIndexOf < b_len) {
    char c = a[0];
    int start = indexOf(b, c, lastIndexOf);
    if (start == -1) // No occurence of this char in b
      return false;
    bool notFound = false;
    for (size_t i = 1; i < a_len && !notFound; ++i) {
      if (b[start + i] != a[i]) {
        lastIndexOf = start + i;
        notFound = true;
      }
    }
    if (!notFound) {
      // Found !
      return true;
    }
  }
  return false;
}
