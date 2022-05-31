#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LOOPS 1000000

int main() {
  printf("string no block!\n");
  for (int i = 0; i < LOOPS; i++) {
    char *test = "test";
    size_t len = strlen(test);
    char *new_str = malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
      new_str[len - i - 1] = test[i];
    }
    new_str[len] = '\0';
    // printf("%s\n", new_str);
    free(new_str);
  }
}
