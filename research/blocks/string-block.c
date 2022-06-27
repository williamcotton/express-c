#include <string/string.h>

#define LOOPS 1000000

int main() {
  printf("string block!\n");
  for (int i = 0; i < LOOPS; i++) {
    string_t *test = string("test");
    test->reverse();
    test->free();
  }
}
