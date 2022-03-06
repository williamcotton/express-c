#include "../src/express.h"
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void janssonJasonapiMiddlewareTests(tape_t *t) {
  t->test("jansson jasonapi middleware", ^(tape_t *t) {
    t->strEqual("render", t->get("/jansson-jsonapi"), "ok");
  });
}
#pragma clang diagnostic pop
