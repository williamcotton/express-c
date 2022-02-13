#include "../src/express.h"

typedef void (^teardown)();
typedef void (^setup)(void (^callback)());

typedef struct test_harness_t {
  teardown teardown;
  setup setup;
} test_harness_t;

test_harness_t *testHarnessFactory();
