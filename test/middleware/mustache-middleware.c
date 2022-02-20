#include "../src/express.h"
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void mustacheMiddlewareTests(tape_t *t) {
  t->test("mustache middleware", ^(tape_t *t) {
    t->strEqual("render", curlGet("/mustache"), "<h1>test</h1>\n<p>test</p>\n");
  });
}
#pragma clang diagnostic pop
