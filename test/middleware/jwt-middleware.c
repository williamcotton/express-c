#include "../src/express.h"
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void jwtMiddlewareTests(tape_t *t) {
  t->test("jansson jwt middleware", ^(tape_t *t) {
    t->strEqual("sign", t->get("/jwt/sign")->split(".")->first(),
                "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9");

    t->strEqual("verify", t->get("/jwt/verify")->split(".")->first(),
                "{\"hello\":\"world\"}");
  });
}
#pragma clang diagnostic pop
