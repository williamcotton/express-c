#include "../src/express.h"
#include "../test-helpers.h"
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void postgresMiddlewareTests(tape_t *t) {
  t->test("postgres middleware", ^(tape_t *t) {
    t->strEqual("pg->exec()", curlGet("/pg/exec"), "test123");
    t->strEqual("pg->execParams()", curlGet("/pg/execParams/val"), "test-val");
    t->strEqual("pg->exec(...)", curlGet("/pg/exec/blip/blop"), "blipblop");
  });
}
#pragma clang diagnostic pop
