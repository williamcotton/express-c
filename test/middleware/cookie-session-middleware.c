#include "../src/express.h"
#include "../tape.h"
#include "../test-helpers.h"
#include <string.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void cookieSessionMiddlewareTests(tape_t *t) {
  t->test("cookie session middleware", ^(tape_t *t) {
    t->strEqual("set", curlGet("/cookie-session/set"), "ok");
    t->strEqual("get", curlGet("/cookie-session/get"), "{\"title\":\"test\"}");
  });
}
#pragma clang diagnostic pop