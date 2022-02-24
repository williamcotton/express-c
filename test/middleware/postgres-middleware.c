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
      t->strEqual("find", t->get("/pg/query/find"), "test456");
      t->strEqual("all", t->get("/pg/query/all"), "test123");
      t->strEqual("select", t->get("/pg/query/select"), "testcity");
      t->strEqual("where", t->get("/pg/query/where"), "another123");
      t->strEqual("count", t->get("/pg/query/count"), "2");
      t->strEqual("limit", t->get("/pg/query/limit"), "1");
      t->strEqual("offset", t->get("/pg/query/offset"), "another123");
      t->strEqual("order", t->get("/pg/query/order"), "another123");
    });
  });
}
#pragma clang diagnostic pop
