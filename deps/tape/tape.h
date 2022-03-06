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

#ifndef TAPE_H
#define TAPE_H

#include <string/string.h>

#define UNUSED __attribute__((unused))

typedef void (^teardown)();
typedef void (^freeHandler)();
typedef void (^setup)(void (^callback)());

typedef struct test_harness_t {
  teardown teardown;
  setup setup;
} test_harness_t;

test_harness_t *testHarnessFactory();

typedef struct tape_t {
  char *name;
  int count;
  int failed;
  string_t * (^string)(char *);
  int (^test)(char *, void (^)(struct tape_t *));
  void (^clearState)();
  void (^randomString)(char *, size_t);
  int (^ok)(char *, int);
  int (^strEqual)(char *, string_t *, char *);
  void (^mockFailOnce)(char *);
  string_t * (^get)(char *);
  string_t * (^post)(char *, char *);
  string_t * (^put)(char *, char *);
  string_t * (^patch)(char *, char *);
  string_t * (^delete)(char *);
  void (^sendData)(char *);
  string_t * (^getHeaders)(char *);
  string_t * (^fetch)(char *path, char *method, string_collection_t *headers,
                      char *json);
  int trashableCount;
  freeHandler trashables[1024];
  void (^trash)(freeHandler);
} tape_t;

tape_t *tape();

#endif // TAPE_H
