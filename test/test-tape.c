#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

#include <stdlib.h>
#include <stdio.h>
#include "tape.h"

int main()
{
  tape_t t = tape();

  return t.test("ok test", ^(UNUSED tape_t *t) {
    t->ok("2 == 2", 2 == 2);
    t->ok("1 == 2", 1 == 2);

    t->test("strEqual test", ^(tape_t *t) {
      t->strEqual("foo", "foo", "foo");
      t->strEqual("foobar", "foo", "bar");
    });
  });
}

#pragma clang diagnostic pop
