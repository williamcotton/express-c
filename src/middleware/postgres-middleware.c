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

#include "postgres-middleware.h"
#include <Block.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <stdlib.h>

int paramCount(const char *query) {
  int count = 0;
  for (size_t i = 0; i < strlen(query); i++) {
    if (query[i] == '$') {
      count++;
    }
  }
  return count;
}

postgres_connection_t *initPostgressConnection(const char *pgUri,
                                               int poolSize) {
  postgres_connection_t *postgres = malloc(sizeof(postgres_connection_t));
  check(postgres, "Failed to allocate postgres connection pool");

  postgres->uri = pgUri;

  postgres->poolSize = poolSize;

  postgres->pool = malloc(sizeof(pg_t *) * postgres->poolSize);
  check(postgres->pool, "Failed to allocate postgres connection pool");

  postgres->semaphore = dispatch_semaphore_create(postgres->poolSize);

  postgres->queue = dispatch_queue_create("postgres", NULL);

  for (int i = 0; i < postgres->poolSize; i++) {
    postgres->pool[i] = malloc(sizeof(pg_t));
    postgres->pool[i]->connection = PQconnectdb(postgres->uri);
    postgres->pool[i]->used = 0;

    postgres->pool[i]->exec = Block_copy(^(const char *sql, ...) {
      int nParams = paramCount(sql);
      va_list args;
      va_start(args, sql);

      const char **paramValues = malloc(sizeof(char *) * nParams);
      for (int j = 0; j < nParams; j++) {
        paramValues[j] = va_arg(args, const char *);
      }
      va_end(args);
      PGresult *pgres = PQexecParams(postgres->pool[i]->connection, sql,
                                     nParams, NULL, paramValues, NULL, NULL, 0);
      free(paramValues);
      return pgres;
    });

    postgres->pool[i]->execParams =
        Block_copy(^(const char *sql, int nParams, const Oid *paramTypes,
                     const char *const *paramValues, const int *paramLengths,
                     const int *paramFormats, int resultFormat) {
          PGresult *pgres = PQexecParams(
              postgres->pool[i]->connection, sql, nParams, paramTypes,
              paramValues, paramLengths, paramFormats, resultFormat);
          return pgres;
        });

    postgres->pool[i]->close = Block_copy(^{
      PQfinish(postgres->pool[i]->connection);
    });
  }

  return postgres;
error:
  return NULL;
}

void freePostgresConnection(postgres_connection_t *postgres) {
  for (int i = 0; i < postgres->poolSize; i++) {
    postgres->pool[i]->close();
    Block_release(postgres->pool[i]->exec);
    Block_release(postgres->pool[i]->execParams);
    Block_release(postgres->pool[i]->close);
    free(postgres->pool[i]);
  }
  free(postgres->pool);
  dispatch_release(postgres->semaphore);
  dispatch_release(postgres->queue);
  free(postgres);
}

middlewareHandler postgresMiddlewareFactory(postgres_connection_t *postgres) {
  /* Test connection */
  PGconn *connection = PQconnectdb(postgres->uri);
  check(PQstatus(connection) == CONNECTION_OK,
        "Failed to connect to postgres: %s (%s)", PQerrorMessage(connection),
        postgres->uri);
  PQfinish(connection);

  /* Create middleware, getting a postgress connection at the beginning of every
   * request and releasing it after the request has finished */
  return Block_copy(^(UNUSED request_t *req, UNUSED response_t *res,
                      void (^next)(), void (^cleanup)(cleanupHandler)) {
    /* Wait for a connection */
    dispatch_semaphore_wait(postgres->semaphore, DISPATCH_TIME_FOREVER);

    __block pg_t *pg;

    /* Get connection */
    dispatch_sync(postgres->queue, ^{
      for (int i = 0; i < postgres->poolSize; i++) {
        if (postgres->pool[i]->used == 0) {
          postgres->pool[i]->used = 1;
          pg = postgres->pool[i];
          break;
        }
      }
    });

    req->mSet("pg", pg);

    cleanup(Block_copy(^(UNUSED request_t *finishedReq) {
      /* Release connection */
      dispatch_sync(postgres->queue, ^{
        pg->used = 0;
      });

      /* Signal a release */
      dispatch_semaphore_signal(postgres->semaphore);
    }));

    next();
  });

error:
  PQfinish(connection);
  return ^(UNUSED request_t *req, UNUSED response_t *res, void (^next)(),
           void (^cleanup)(cleanupHandler)) {
    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));
    next();
  };
}
