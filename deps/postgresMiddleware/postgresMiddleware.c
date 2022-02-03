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

middlewareHandler postgresMiddlewareFactory(const char *pgUri) {
  pg_t *postgres = malloc(sizeof(pg_t));

  PGconn *pgConnection = PQconnectdb(pgUri);
  if (PQstatus(pgConnection) != CONNECTION_OK) {
    fprintf(stderr, "%s", PQerrorMessage(pgConnection));
    PQfinish(pgConnection);
  }
  postgres->connection = pgConnection;

  dispatch_queue_t postgresQueue = dispatch_queue_create("postgresQueue", NULL);

  return Block_copy(^(UNUSED request_t *req, UNUSED response_t *res,
                      void (^next)(), void (^cleanup)(cleanupHandler)) {
    postgres->exec = req->blockCopy(^(const char *sql) {
      __block PGresult *pgres;
      dispatch_sync(postgresQueue, ^{
        pgres = PQexec(postgres->connection, sql);
        if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
          fprintf(stderr, "SELECT failed: %s",
                  PQerrorMessage(postgres->connection));
          PQclear(pgres);
          PQfinish(postgres->connection);
        }
      });
      return pgres;
    });

    req->mSet("pg", postgres);

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
        //
    }));
    next();
  });
}
