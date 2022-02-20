#include "../src/express.h"
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void cookieSessionMiddlewareTests(tape_t *t) {
  t->test("cookie session middleware", ^(tape_t *t) {
    t->strEqual("set", t->get("/cookie-session/set"), "ok");
    t->strEqual("get", t->get("/cookie-session/get"), "{\"title\":\"test\"}");
  });
}
#pragma clang diagnostic pop
