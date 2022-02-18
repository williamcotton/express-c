#include <string/string.h>

#include "../src/express.h"
#include "../tape.h"
#include "../test-helpers.h"
#include <math.h>
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

      t->ok("print", 1);
      s->print();

      s->upcase();
      t->ok("upcase", strcmp(s->str, "HELLO WORLD!") == 0);

      s->downcase();
      t->ok("downcase", strcmp(s->str, "hello world!") == 0);

      s->capitalize();
      t->ok("capitalize", strcmp(s->str, "Hello world!") == 0);

      s->reverse();
      t->ok("reverse", strcmp(s->str, "!dlrow olleH") == 0);

      s->concat("  ");
      t->ok("concat end space", strcmp(s->str, "!dlrow olleH  ") == 0);
      s->trim();
      t->ok("trim end", strcmp(s->str, "!dlrow olleH") == 0);
      s->concat("  ")->reverse();
      t->ok("concat start space", strcmp(s->str, "  Hello world!") == 0);
      s->trim();
      t->ok("trim start", strcmp(s->str, "Hello world!") == 0);
      s->concat("  ")->reverse()->concat("  ")->reverse();
      t->ok("concat start and end space",
            strcmp(s->str, "  Hello world!  ") == 0);
      s->trim();
      t->ok("trim start and end", strcmp(s->str, "Hello world!") == 0);

      s->free();
    });

    t->test("toInt", ^(tape_t *t) {
      string_t *ten = string("10");
      t->ok("int", ten->toInt()->value == 10);
      t->ok("int no error", ten->toInt()->error == 0);
      ten->free();

      string_t *negativeTen = string("-10");
      t->ok("negative int", negativeTen->toInt()->value == -10);
      t->ok("negative int no error", negativeTen->toInt()->error == 0);
      negativeTen->free();

      string_t *tenDecimal = string("10.0");
      t->ok("decimal", tenDecimal->toInt()->value == 10);
      t->ok("decimal error",
            tenDecimal->toInt()->error == NUMBER_ERROR_ADDITIONAL_CHARACTERS);
      tenDecimal->free();

      string_t *tooBig = string("10000000000000000000000000000000000000");
      t->ok("too big", tooBig->toInt()->value == 0);
      t->ok("too big error", tooBig->toInt()->error == NUMBER_ERROR_OVERFLOW);
      tooBig->free();

      string_t *tooSmall = string("-10000000000000000000000000000000000000");
      t->ok("too small", (unsigned long long)tooSmall->toInt()->value == 0);
      t->ok("too small error",
            tooSmall->toInt()->error == NUMBER_ERROR_UNDERFLOW);
      tooSmall->free();

      string_t *notANumber = string("not a number");
      t->ok("not a number", notANumber->toInt()->value == 0);
      t->ok("not a number error",
            notANumber->toInt()->error == NUMBER_ERROR_NO_DIGITS);
      notANumber->free();

      string_t *empty = string("");
      t->ok("empty", empty->toInt()->value == 0);
      t->ok("not a number error",
            empty->toInt()->error == NUMBER_ERROR_NO_DIGITS);
      empty->free();
    });

    t->test("toDecimal", ^(tape_t *t) {
      string_t *tenPointOne = string("10.1");
      t->ok("decimal", tenPointOne->toDecimal()->value == 10.1);
      t->ok("decimal no error", tenPointOne->toDecimal()->error == 0);
      tenPointOne->free();

      string_t *negativeTenPointOne = string("-10.6");
      negativeTenPointOne->print();
      t->ok("negative decimal",
            negativeTenPointOne->toDecimal()->value == -10.6);
      t->ok("negative decimal no error",
            negativeTenPointOne->toDecimal()->error == 0);
      negativeTenPointOne->free();

      string_t *tenInteger = string("10");
      t->ok("integer", tenInteger->toDecimal()->value == 10.0);
      t->ok("integer error", tenInteger->toDecimal()->error == 0);
      tenInteger->free();

      string_t *tenDecimalExcess = string("10abc");
      t->ok("integer", tenDecimalExcess->toDecimal()->value == 10.0);
      t->ok("integer error", tenDecimalExcess->toDecimal()->error ==
                                 NUMBER_ERROR_ADDITIONAL_CHARACTERS);
      tenDecimalExcess->free();

      string_t *tooBig = string("1e5000L");
      t->ok("too big", tooBig->toDecimal()->value == 0);
      t->ok("too big error",
            tooBig->toDecimal()->error == NUMBER_ERROR_OVERFLOW);
      tooBig->free();

      string_t *tooSmall = string("-1e5000L");
      t->ok("too small", tooSmall->toDecimal()->value == 0);
      t->ok("too small error",
            tooSmall->toDecimal()->error == NUMBER_ERROR_UNDERFLOW);
      tooSmall->free();

      string_t *notANumber = string("not a number");
      t->ok("not a number", notANumber->toDecimal()->value == 0);
      t->ok("not a number error",
            notANumber->toDecimal()->error == NUMBER_ERROR_NO_DIGITS);
      notANumber->free();

      string_t *empty = string("");
      t->ok("empty", empty->toDecimal()->value == 0);
      t->ok("not a number error",
            empty->toDecimal()->error == NUMBER_ERROR_NO_DIGITS);
      empty->free();
    });

    t->test("string collection", ^(tape_t *t) {
      string_t *s2 = string("one,two,three");
      string_collection_t *c = s2->split(",");
      s2->free();
      __block int i = 0;
      c->each(^(string_t *string) {
        t->ok("split and each", strcmp(string->str, (i == 0)   ? "one"
                                                    : (i == 1) ? "two"
                                                               : "three") == 0);
        i++;
      });

      c->eachWithIndex(^(string_t *string, int j) {
        t->ok("eachWithIndex", strcmp(string->str, (j == 0)   ? "one"
                                                   : (j == 1) ? "two"
                                                              : "three") == 0);
      });

      t->ok("indexOf zero", c->indexOf("zero") == -1);
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