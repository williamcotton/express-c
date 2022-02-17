#include <string/string.h>

#include "../src/express.h"
#include "../tape.h"
#include "../test-helpers.h"
#include <string.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void stringTests(tape_t *t) {
  t->test("string", ^(tape_t *t) {
    t->test("single string", ^(tape_t *t) {
      string_t *s = string("Hello");
      t->ok("new string", strcmp(s->str, "Hello") == 0);

      s->concat(" World");
      t->ok("concat", strcmp(s->str, "Hello World") == 0);

      s->concat("!");
      t->ok("concat again", strcmp(s->str, "Hello World!") == 0);

      s->upcase();
      t->ok("upcase", strcmp(s->str, "HELLO WORLD!") == 0);

      s->downcase();
      t->ok("downcase", strcmp(s->str, "hello world!") == 0);

      s->capitalize();
      t->ok("capitalize", strcmp(s->str, "Hello world!") == 0);

      s->reverse();
      t->ok("reverse", strcmp(s->str, "!dlrow olleH") == 0);

      s->concat(" ");
      t->ok("concat space", strcmp(s->str, "!dlrow olleH ") == 0);
      s->trim();
      t->ok("trim", strcmp(s->str, "!dlrow olleH") == 0);

      s->free();
    });

    t->test("string collection", ^(tape_t *t) {
      string_t *s2 = string("one,two,three");
      string_collection_t *c = s2->split(",");
      s2->free();
      __block int i = 0;
      c->each(^(string_t *string) {
        t->ok("split", strcmp(string->str, (i == 0)   ? "one"
                                           : (i == 1) ? "two"
                                                      : "three") == 0);
        i++;
      });

      t->ok("indexOf one", c->indexOf("one") == 0);
      t->ok("indexOf two", c->indexOf("two") == 1);
      t->ok("indexOf three", c->indexOf("three") == 2);

      c->reverse();
      i = 0;
      c->each(^(string_t *string) {
        t->ok("reverse", strcmp(string->str, (i == 0)   ? "three"
                                             : (i == 1) ? "two"
                                                        : "one") == 0);
        i++;
      });

      string_t *s3 = c->join("-");
      t->ok("join", strcmp(s3->str, "three-two-one") == 0);

      s3->replace("three", "four");
      t->ok("replace", strcmp(s3->str, "four-two-one") == 0);

      s3->replace("-", "=");
      t->ok("replace all", strcmp(s3->str, "four=two=one") == 0);

      string_t *s4 = s3->slice(5, 3);
      s3->free();
      t->ok("slice", strcmp(s4->str, "two") == 0);
      s4->free();

      string_t *s5 = string("");
      c->reduce((void *)s5, ^(void *accumulator, string_t *str) {
        string_t *acc = (string_t *)accumulator;
        acc->concat(str->reverse()->str)->concat("+");
        return (void *)acc;
      });
      t->ok("reduce", strcmp(s5->str, "eerht+owt+eno+") == 0);
      s5->free();

      string_t **sa = (string_t **)c->map(^(string_t *str) {
        return (void *)str->concat("!!!")->reverse()->concat("!!!");
      });

      for (size_t i = 0; i < c->size; i++) {
        t->ok("map", strcmp(sa[i]->str, (i == 0)   ? "!!!three!!!"
                                        : (i == 1) ? "!!!two!!!"
                                                   : "!!!one!!!") == 0);
      }

      c->free();
    });
  });
}
#pragma clang diagnostic pop

void test() {}