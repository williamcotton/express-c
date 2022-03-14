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

typedef struct query_t {
  char *sql;
  const char *paramValues[100];
  int paramValueCount;
  const char *selectConditions[100];
  int selectConditionsCount;
  char *whereConditions[100];
  int whereConditionsCount;
  char *orderConditions[100];
  int orderConditionsCount;
  char *limitCondition;
  char *offsetCondition;
  char *havingConditions[100];
  void *includesArray[100];
  int includesCount;
  int havingConditionsCount;
  char *joinsConditions;
  char *groupConditions;
  int distinctCondition;
  struct query_t * (^select)(const char *);
  struct query_t * (^where)(const char *, ...);
  struct query_t * (^whereIn)(const char *, int in, const char **, int);
  struct query_t * (^order)(const char *, char *);
  struct query_t * (^limit)(int);
  struct query_t * (^offset)(int);
  struct query_t * (^group)(const char *);
  struct query_t * (^having)(const char *, ...);
  struct query_t * (^joins)(const char *);
  struct query_t * (^distinct)();
  struct query_t * (^includes)(const char *);
  char * (^toSql)();
  int (^count)();
  void * (^find)(char *);
  void * (^all)();
} query_t;

typedef query_t * (^getPostgresQueryBlock)(const char *);

typedef struct pg_t {
  PGconn *connection;
  int used;
  PGresult * (^exec)(const char *, ...);
  PGresult * (^execParams)(const char *, int, const Oid *, const char *const *,
                           const int *, const int *, int);
  query_t * (^query)(const char *);
  void (^close)();
  void (^free)();
} pg_t;

typedef struct postgres_connection_t {
  const char *uri;
  PGconn *connection;
  pg_t **pool;
  dispatch_semaphore_t semaphore;
  dispatch_queue_t queue;
  int poolSize;
  void (^free)();
} postgres_connection_t;

int pgParamCount(const char *query);
postgres_connection_t *initPostgressConnection(const char *pgUri, int poolSize);
middlewareHandler postgresMiddlewareFactory(postgres_connection_t *postgres);
getPostgresQueryBlock getPostgresQuery(memory_manager_t *memoryManager,
                                       pg_t *pg);
pg_t *initPg(const char *pgUri);

#endif // POSTGRES_MIDDLEWARE_H
