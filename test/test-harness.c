#include "test-harness.h"
#include "../src/express.h"
#include <Block.h>
#include <stdlib.h>

app_t *testApp();

test_harness_t *testHarnessFactory() {
  __block app_t *app = testApp();
  int port = 3032;

  test_harness_t *testHarness = malloc(sizeof(test_harness_t));

  testHarness->teardown = Block_copy(^{
    shutdownAndFreeApp(app);
  });

  testHarness->setup = Block_copy(^(void (^callback)()) {
    app->listen(port, ^{
      callback();
    });
  });

  return testHarness;
}
