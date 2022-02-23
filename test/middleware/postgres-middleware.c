#include "../src/express.h"
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void postgresMiddlewareTests(tape_t *t) {
  t->test("postgres middleware", ^(tape_t *t) {
    t->strEqual("pg->exec()", t->get("/pg/exec"), "test123");
    t->strEqual("pg->execParams()", t->get("/pg/execParams/val"), "test-val");
    t->strEqual("pg->exec(...)", t->get("/pg/exec/blip/blop"), "blipblop");

    t->test("pg->query", ^(tape_t *t) {
      t->strEqual("all", t->get("/pg/query/all"), "test123");
      t->strEqual("where", t->get("/pg/query/where"), "test456");
    });
  });
}
#pragma clang diagnostic pop
