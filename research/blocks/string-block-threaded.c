#include <string/string.h>

#define LOOPS 1000000

int main() {
  dispatch_queue_t queue =
      dispatch_queue_create("reverse", DISPATCH_QUEUE_CONCURRENT);

  printf("string block pool!\n");

  size_t iterations = LOOPS;

  for (int i = 0; i < iterations; i++) {
    dispatch_async(queue, ^{
      string_t *test = string("test");
      test->reverse();
      test->free();
    });
  };

  dispatch_sync(queue, ^{
    printf("done!\n");
  });
}
