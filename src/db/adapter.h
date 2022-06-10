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

#ifndef DB_ADAPTER_H
#define DB_ADAPTER_H

#include <express.h>
#include <libpq-fe.h>

typedef struct query_result_t {
  char *sql;
  char *result;
  int resultCount;
} query_result_t;

typedef struct database_connection_t {
  struct database_connection_t *next;
  void *connection;
} database_connection_t;

typedef struct database_pool_t {
  const char *uri;
  int size;
  database_connection_t *head;
  dispatch_semaphore_t semaphore;
  dispatch_queue_t queue;
  PGresult * (^exec)(const char *, ...);
  PGresult * (^execParams)(const char *, int, const Oid *, const char *const *,
                           const int *, const int *, int);
  void (^free)();
  database_connection_t * (^borrow)();
  void (^release)(database_connection_t *);
} database_pool_t;

database_pool_t *createPostgresPool(const char *uri, int size);

#endif // DB_ADAPTER_H
