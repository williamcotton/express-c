/*
  Copyright (c) 2022 William Cotton

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"

#include "tape.h"
#include <Block.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (^testHandler)(char *name, void (^block)(tape_t *));
testHandler testHandlerFactory(tape_t *root, int level) {
  return Block_copy(^(char *name, void (^block)(tape_t *)) {
    printf("\n%s\n", name);
    __block tape_t *t = malloc(sizeof(tape_t));
    t->name = name;
    t->ok = ^(char *okName, int condition) {
      root->count++;
      if (condition) {
        printf("\033[32m✓ %s\n\033[0m", okName);
        return 1;
      } else {
        printf("\033[31m✗ %s\n\033[0m", okName);
        root->failed++;
        return 0;
      }
    };
    t->strEqual = ^(char *name, char *str1, char *str2) {
      int result = strcmp(str1, str2) == 0;
      t->ok(name, result);
      if (!result) {
        printf("\nExpected: \033[32m\n\n%s\n\n\033[0m", str2);
        printf("Received: \n\n\033[31m%s\033[0m\n", str1);
      }
      free(str1);
      return result;
    };

    t->test = testHandlerFactory(root, level + 1);
    block(t);
    if (level == 0) {
      if (root->failed == 0)
        printf("\033[32m\n%d tests passed\n\n\033[0m", root->count);
      else
        printf("\033[31m\n%d tests, %d failed\n\n\033[0m", root->count,
               root->failed);
    }
    Block_release(t->test);
    free(t);
    return root->failed > 0;
  });
}

tape_t *tape() {
  tape_t *tape = malloc(sizeof(tape_t));
  tape->count = 0;
  tape->failed = 0;

  tape->test = testHandlerFactory(tape, 0);

  tape->ok = ^(UNUSED char *name, UNUSED int okBool) {
    return 1;
  };

  return tape;
}

#pragma clang diagnostic pop
