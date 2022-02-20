#include "../src/express.h"
#include "test-helpers.h"
#include <Block.h>
#include <dotenv-c/dotenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <tape/tape.h>

app_t *testApp();

void expressTests(tape_t *t);

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

void runTests(int runAndExit, test_harness_t *testHarness) {
  tape_t *test = tape();

  int testStatus = test->test("express", ^(tape_t *t) {
    clearState();
    expressTests(t);
  });

  Block_release(test->test);
  free(test);

  if (runAndExit) {
    testHarness->teardown();
    exit(testStatus);
  }
}

int main() {
  env_load(".", false);

  int runXTimes = getenv("RUN_X_TIMES") ? atoi(getenv("RUN_X_TIMES")) : 1;
  int sleepTime = getenv("SLEEP_TIME") ? atoi(getenv("SLEEP_TIME")) : 0;

  sleep(sleepTime);

  test_harness_t *testHarness = testHarnessFactory();

  testHarness->setup(^{
    for (int i = 0; i < runXTimes; i++) {
      runTests(runXTimes == 1, testHarness);
    }
    if (runXTimes > 1)
      exit(0);
  });
}
