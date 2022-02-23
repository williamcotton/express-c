#include <string/string.h>

#include "../src/express.h"
#include <math.h>
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void stringTests(tape_t *t) {
  t->test("string", ^(tape_t *t) {
    t->test("single string", ^(tape_t *t) {
      string_t *s = string("Hello");
      t->ok("new string", s->eql("Hello"));

      s->concat(" World");
      t->ok("concat", s->eql("Hello World"));

      s->concat("!");
      t->ok("concat again", s->eql("Hello World!"));

      t->ok("print", 1);
      s->print();

      s->upcase();
      t->ok("upcase", s->eql("HELLO WORLD!"));

      s->downcase();
      t->ok("downcase", s->eql("hello world!"));

      s->capitalize();
      t->ok("capitalize", s->eql("Hello world!"));

      s->reverse();
      t->ok("reverse", s->eql("!dlrow olleH"));

      s->concat("  ");
      t->ok("concat end space", s->eql("!dlrow olleH  "));
      s->trim();
      t->ok("trim end", s->eql("!dlrow olleH"));
      s->concat("  ")->reverse();
      t->ok("concat start space", s->eql("  Hello world!"));
      s->trim();
      t->ok("trim start", s->eql("Hello world!"));
      s->concat("  ")->reverse()->concat("  ")->reverse();
      t->ok("concat start and end space", s->eql("  Hello world!  "));
      s->trim();
      t->ok("trim start and end", s->eql("Hello world!"));

      t->ok("chomp", s->concat("\n")->chomp()->eql("Hello world!"));

      t->ok("indexOf", s->indexOf("llo") == 2);
      t->ok("indexOf", s->indexOf("bleep") == -1);
      t->ok("lastIndexOf", s->lastIndexOf("l") == 9);
      t->ok("lastIndexOf", s->lastIndexOf("#") == -1);
      t->ok("contains", s->contains("Hello"));
      t->ok("contains", !s->contains("bleep"));

      s->free();
    });

    t->test("toInt", ^(tape_t *t) {
      string_t *ten = string("10");
      t->ok("int", ten->toInt().value == 10);
      t->ok("int no error", ten->toInt().error == 0);
      ten->free();

      string_t *negativeTen = string("-10");
      t->ok("negative int", negativeTen->toInt().value == -10);
      t->ok("negative int no error", negativeTen->toInt().error == 0);
      negativeTen->free();

      string_t *tenDecimal = string("10.0");
      t->ok("decimal", tenDecimal->toInt().value == 10);
      t->ok("decimal error",
            tenDecimal->toInt().error == NUMBER_ERROR_ADDITIONAL_CHARACTERS);
      tenDecimal->free();

      string_t *tooBig = string("10000000000000000000000000000000000000");
      t->ok("too big", tooBig->toInt().value == 0);
      t->ok("too big error", tooBig->toInt().error == NUMBER_ERROR_OVERFLOW);
      tooBig->free();

      string_t *tooSmall = string("-10000000000000000000000000000000000000");
      t->ok("too small", (unsigned long long)tooSmall->toInt().value == 0);
      t->ok("too small error",
            tooSmall->toInt().error == NUMBER_ERROR_UNDERFLOW);
      tooSmall->free();

      string_t *notANumber = string("not a number");
      t->ok("not a number", notANumber->toInt().value == 0);
      t->ok("not a number error",
            notANumber->toInt().error == NUMBER_ERROR_NO_DIGITS);
      notANumber->free();

      string_t *empty = string("");
      t->ok("empty", empty->toInt().value == 0);
      t->ok("not a number error",
            empty->toInt().error == NUMBER_ERROR_NO_DIGITS);
      empty->free();
    });

    t->test("toDecimal", ^(tape_t *t) {
      string_t *tenPointOne = string("10.1");
      t->ok("decimal", tenPointOne->toDecimal().value == 10.1L);
      t->ok("decimal no error", tenPointOne->toDecimal().error == 0);
      tenPointOne->free();

      string_t *negativeTenPointOne = string("-10.6");
      t->ok("negative decimal",
            negativeTenPointOne->toDecimal().value == -10.6L);
      t->ok("negative decimal no error",
            negativeTenPointOne->toDecimal().error == 0);
      negativeTenPointOne->free();

      string_t *tenInteger = string("10");
      t->ok("integer", tenInteger->toDecimal().value == 10.0L);
      t->ok("integer error", tenInteger->toDecimal().error == 0);
      tenInteger->free();

      string_t *tenDecimalExcess = string("10abc");
      t->ok("integer", tenDecimalExcess->toDecimal().value == 10.0L);
      t->ok("integer error", tenDecimalExcess->toDecimal().error ==
                                 NUMBER_ERROR_ADDITIONAL_CHARACTERS);
      tenDecimalExcess->free();

      string_t *tooBig = string("1e5000L");
      t->ok("too big", tooBig->toDecimal().value == 0);
      t->ok("too big error",
            tooBig->toDecimal().error == NUMBER_ERROR_OVERFLOW);
      tooBig->free();

      string_t *tooSmall = string("-1e5000L");
      t->ok("too small", tooSmall->toDecimal().value == 0);
      t->ok("too small error",
            tooSmall->toDecimal().error == NUMBER_ERROR_UNDERFLOW);
      tooSmall->free();

      string_t *notANumber = string("not a number");
      t->ok("not a number", notANumber->toDecimal().value == 0);
      t->ok("not a number error",
            notANumber->toDecimal().error == NUMBER_ERROR_NO_DIGITS);
      notANumber->free();

      string_t *empty = string("");
      t->ok("empty", empty->toDecimal().value == 0);
      t->ok("not a number error",
            empty->toDecimal().error == NUMBER_ERROR_NO_DIGITS);
      empty->free();
    });

    t->test("string collection", ^(tape_t *t) {
      string_t *s2 = string("one,two,three");
      string_collection_t *c = s2->split(",");
      s2->free();
      __block int i = 0;
      c->each(^(string_t *string) {
        t->ok("split and each", string->eql((i == 0)   ? "one"
                                            : (i == 1) ? "two"
                                                       : "three"));
        i++;
      });

      c->eachWithIndex(^(string_t *string, int j) {
        t->ok("eachWithIndex", string->eql((j == 0)   ? "one"
                                           : (j == 1) ? "two"
                                                      : "three"));
      });

      t->ok("indexOf zero", c->indexOf("zero") == -1);
      t->ok("indexOf one", c->indexOf("one") == 0);
      t->ok("indexOf two", c->indexOf("two") == 1);
      t->ok("indexOf three", c->indexOf("three") == 2);

      c->reverse();
      i = 0;
      c->each(^(string_t *string) {
        t->ok("reverse", string->eql((i == 0)   ? "three"
                                     : (i == 1) ? "two"
                                                : "one"));
        i++;
      });

      string_t *s3 = c->join("-");
      t->ok("join", s3->eql("three-two-one"));

      s3->replace("three", "four");
      t->ok("replace", s3->eql("four-two-one"));

      s3->replace("-", "=");
      t->ok("replace all", s3->eql("four=two=one"));

      string_t *s4 = s3->slice(5, 3);
      t->ok("slice", s4->eql("two"));
      string_t *s4a = s3->slice(15, 4);
      t->ok("slice over", s4a->eql(""));
      string_t *s4b = s3->slice(4, 15);
      t->ok("slice over", s4b->eql("=two=one"));
      s3->free();
      s4a->free();
      s4b->free();
      s4->free();

      string_t *s5 = string("");
      c->reduce((void *)s5, ^(void *accumulator, string_t *str) {
        string_t *acc = (string_t *)accumulator;
        acc->concat(str->reverse()->value)->concat("+");
        return (void *)acc;
      });
      t->ok("reduce", s5->eql("eerht+owt+eno+"));
      s5->free();

      string_t **sa = (string_t **)c->map(^(string_t *str) {
        return (void *)str->concat("!!!")->reverse()->concat("!!!");
      });

      for (size_t i = 0; i < c->size; i++) {
        t->ok("map", sa[i]->eql((i == 0)   ? "!!!three!!!"
                                : (i == 1) ? "!!!two!!!"
                                           : "!!!one!!!"));
      }

      c->free();

      string_t *unsortedString = string("car,tree,boar");
      string_collection_t *unsorted = unsortedString->split(",");
      string_t *sortedString = unsorted->sort()->join(",");
      t->ok("sort", sortedString->eql("boar,car,tree"));
      unsorted->free();
      unsortedString->free();
      sortedString->free();

      string_t *s6 = string("one,two,three,four,five,six");
      string_collection_t *c2 = s6->split(",");
      t->strEqual("first", c2->first(), "one");
      t->strEqual("second", c2->second(), "two");
      t->strEqual("third", c2->third(), "three");
      t->strEqual("fourth", c2->fourth(), "four");
      t->strEqual("fifth", c2->fifth(), "five");
      t->strEqual("last", c2->last(), "six");
      s6->free();
      c2->free();
    });

    t->test("regex collection", ^(tape_t *t) {
      string_t *s = string("/:one/:two/:three/");
      string_collection_t *c = s->matchGroup("(:[a-z]+)");
      t->ok("match", c->size == 3);
      c->eachWithIndex(^(string_t *string, int j) {
        t->ok("eachWithIndex", string->eql((j == 0)   ? ":one"
                                           : (j == 1) ? ":two"
                                                      : ":three"));
      });
      c->free();
      s->free();
    });
  });
}
#pragma clang diagnostic pop

void test() {}