#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

#include "tape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
  tape_t *t = tape();

  return t->test("ok test", ^(tape_t *t) {
    t->ok("2 == 2", 2 == 2);
    t->ok("1 == 2", 1 == 2);

    t->test("strEqual test", ^(tape_t *t) {
      t->strEqual("foo", strdup("foo"), "foo");
      t->strEqual("foobar", strdup("foo"), "bar");
    });
  });
}

#pragma clang diagnostic pop
