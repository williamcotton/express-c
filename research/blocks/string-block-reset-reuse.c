#include <string/string.h>

#define LOOPS 1000000

int main() {
  printf("string block reset reuse!\n");
  string_t *test = string("test");
  for (int i = 0; i < LOOPS; i++) {
    free(test->value);
    test->value = strdup("test");
    test->size = strlen(test->value);
    test->reverse();
  }
  test->free();
}
