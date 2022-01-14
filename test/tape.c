#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Block.h>
#include "tape.h"

typedef int (^testHandler)(char *name, void (^block)(tape_t *));
testHandler testHandlerFactory(tape_t *parent, int level)
{
  return Block_copy(^(char *name, void (^block)(tape_t *)) {
    printf("\n%s\n", name);
    __block tape_t *t = malloc(sizeof(tape_t));
    t->name = name;
    t->ok = ^(char *okName, int condition) {
      parent->count++;
      if (condition)
      {
        printf("\033[32m✓ %s\n\033[0m", okName);
        return 1;
      }
      else
      {
        printf("\033[31m✗ %s\n\033[0m", okName);
        parent->failed++;
        return 0;
      }
    };
    t->strEqual = ^(char *name, char *str1, char *str2) {
      int result = strcmp(str1, str2) == 0;
      t->ok(name, result);
      if (!result)
      {
        printf("\nExpected: \033[32m\n\n%s\n\n\033[0m", str2);
        printf("Received: \n\n\033[31m%s\033[0m", str1);
      }
      return result;
    };

    t->test = testHandlerFactory(parent, level + 1);
    block(t);
    if (level == 0)
    {
      if (parent->failed == 0)
      {
        printf("\033[32m\n%d tests passed\n\n\033[0m", parent->count);
      }
      else
      {
        printf("\033[31m\n%d tests, %d failed\n\n\033[0m", parent->count, parent->failed);
      }
    }
    return parent->failed > 0;
  });
}

tape_t tape()
{
  __block tape_t tape = {
      .name = "tape",
      .count = 0,
      .failed = 0,
  };

  tape.test = testHandlerFactory(&tape, 0);

  tape.ok = ^(UNUSED char *name, UNUSED int okBool) {
    return 1;
  };

  return tape;
}

#pragma clang diagnostic pop
