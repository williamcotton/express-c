#include <express.h>
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void sqliteMiddlewareTests(tape_t *t) {
  t->test("sqlite middleware", ^(tape_t *t) {
    t->strEqual("sqlite->exec()", t->get("/sqlite/exec"), "test123");
  });
}
#pragma clang diagnostic pop
