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

#ifndef POSTGRES_MIDDLEWARE_H
#define POSTGRES_MIDDLEWARE_H

#include <express.h>
#include <libpq-fe.h>

typedef struct pg_t {
  PGconn *connection;
  int used;
  PGresult * (^exec)(const char *, ...);
  PGresult * (^execParams)(const char *, int, const Oid *, const char *const *,
                           const int *, const int *, int);
  void (^close)();
} pg_t;

typedef struct postgres_connection_t {
  const char *uri;
  pg_t **pool;
  dispatch_semaphore_t semaphore;
  dispatch_queue_t queue;
  int poolSize;
} postgres_connection_t;

postgres_connection_t *initPostgressConnection(const char *pgUri, int poolSize);

void freePostgresConnection(postgres_connection_t *postgres);

middlewareHandler postgresMiddlewareFactory(postgres_connection_t *postgres);

#endif // POSTGRES_MIDDLEWARE_H