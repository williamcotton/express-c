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

#include "postgresMiddleware.h"
#include <Block.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <stdlib.h>

#define POOL_SIZE 30

middlewareHandler postgresMiddlewareFactory(const char *pgUri) {
  /* Test connection */
  PGconn *connection = PQconnectdb(pgUri);
  check(PQstatus(connection) == CONNECTION_OK,
        "Failed to connect to postgres: %s (%s)", PQerrorMessage(connection),
        pgUri);
  PQfinish(connection);

  /* Create pool */
  pg_t **connectionPool = malloc(sizeof(pg_t *) * POOL_SIZE);
  dispatch_semaphore_t connectionPoolSemaphore =
      dispatch_semaphore_create(POOL_SIZE);
  for (int i = 0; i < POOL_SIZE; i++) {
    connectionPool[i] = malloc(sizeof(pg_t));
    connectionPool[i]->connection = PQconnectdb(pgUri);
    connectionPool[i]->used = 0;
    connectionPool[i]->exec = Block_copy(^(const char *sql) {
      PGresult *pgres = PQexec(connectionPool[i]->connection, sql);
      return pgres;
    });
    connectionPool[i]->close = Block_copy(^{
      PQfinish(connectionPool[i]->connection);
    });
  }

  /* Create sync queue for mutex */
  dispatch_queue_t postgresQueue = dispatch_queue_create("postgresQueue", NULL);

  /* Create middleware, getting a postgress connection at the beginning of every
   * request and releasing it after the request has finished */
  return Block_copy(^(UNUSED request_t *req, UNUSED response_t *res,
                      void (^next)(), void (^cleanup)(cleanupHandler)) {
    /* Wait for a connection */
    dispatch_semaphore_wait(connectionPoolSemaphore, DISPATCH_TIME_FOREVER);

    __block pg_t *postgres;

    /* Get connection */
    dispatch_sync(postgresQueue, ^{
      for (int i = 0; i < POOL_SIZE; i++) {
        if (connectionPool[i]->used == 0) {
          connectionPool[i]->used = 1;
          postgres = connectionPool[i];
          break;
        }
      }
    });

    req->mSet("pg", postgres);

    cleanup(Block_copy(^(UNUSED request_t *finishedReq) {
      /* Release connection */
      dispatch_sync(postgresQueue, ^{
        postgres->used = 0;
      });

      /* Signal a release */
      dispatch_semaphore_signal(connectionPoolSemaphore);
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
