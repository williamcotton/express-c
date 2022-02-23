#include "../src/express.h"
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void jwtMiddlewareTests(tape_t *t) {
  t->test("jansson jwt middleware", ^(tape_t *t) {
    string_t *response = t->get("/jwt/sign");
    string_collection_t *tokens = response->split(".");
    t->strEqual("sign", tokens->first(),
                "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9");
    tokens->free();

    t->strEqual("verify", t->get("/jwt/verify"), "{\"hello\":\"world\"}");
  });
}
#pragma clang diagnostic pop
