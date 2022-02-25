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

static getPostgresQueryBlock getPostgresQuery(request_t *req, pg_t *pg) {
  return req->blockCopy(^(const char *tableName) {
    query_t *query = req->malloc(sizeof(query_t));

    query->paramValueCount = 0;
    query->whereConditionsCount = 0;
    query->selectCondition = "*";
    query->limitCondition = "";
    query->offsetCondition = "";
    query->orderConditionsCount = 0;

    query->select = req->blockCopy(^(const char *select) {
      query->selectCondition = select;
      return query;
    });

    query->where = req->blockCopy(^(const char *conditions, ...) {
      int nParams = paramCount(conditions);

      char varNum[10];
      sprintf(varNum, "$%d", query->whereConditionsCount + 1);

      string_t *whereConditions = string(conditions);
      char *sequentialConditions =
          strdup(whereConditions->replace("$", varNum)->value);
      whereConditions->free();

      query->whereConditions[query->whereConditionsCount++] =
          sequentialConditions;

      va_list args;
      va_start(args, conditions);

      for (int j = 0; j < nParams; j++) {
        query->paramValues[query->paramValueCount++] =
            va_arg(args, const char *);
      }

      va_end(args);

      return query;
    });

    query->limit = req->blockCopy(^(int limit) {
      char *limitStr = req->malloc(sizeof(char) * 10);
      sprintf(limitStr, "%d", limit);
      query->limitCondition = limitStr;
      return query;
    });

    query->offset = req->blockCopy(^(int offset) {
      char *offsetStr = req->malloc(sizeof(char) * 10);
      sprintf(offsetStr, "%d", offset);
      query->offsetCondition = offsetStr;
      return query;
    });

    query->order = req->blockCopy(^(const char *orderCondition) {
      query->orderConditions[query->orderConditionsCount++] =
          (char *)orderCondition;
      return query;
    });

    query->all = req->blockCopy(^() {
      // SELECT
      char *select =
          req->malloc(strlen(query->selectCondition) + strlen("SELECT ") + 1);
      sprintf(select, "SELECT %s", query->selectCondition);

      // FROM
      char *from = req->malloc(strlen(tableName) + strlen("FROM ") + 1);
      sprintf(from, "FROM %s", tableName);

      // WHERE
      char *where = NULL;
      for (int i = 0; i < query->whereConditionsCount; i++) {
        if (where == NULL) {
          where = req->malloc(strlen(query->whereConditions[i]) +
                              strlen(" WHERE ") + 1);
          sprintf(where, "WHERE %s", query->whereConditions[i]);
        } else {
          char *tmp = req->malloc(strlen(where) + strlen(" AND ") +
                                  strlen(query->whereConditions[i]) + 1);
          sprintf(tmp, "%s AND %s", where, query->whereConditions[i]);
          where = tmp;
        }
      }
      if (where == NULL)
        where = "";

      // LIMIT
      char *limit = NULL;
      if (strlen(query->limitCondition) > 0) {
        limit =
            req->malloc(strlen(query->limitCondition) + strlen(" LIMIT ") + 1);
        sprintf(limit, "LIMIT %s", query->limitCondition);
      }
      if (limit == NULL)
        limit = "";

      // OFFSET
      char *offset = NULL;
      if (strlen(query->offsetCondition) > 0) {
        offset = req->malloc(strlen(query->offsetCondition) +
                             strlen(" OFFSET ") + 1);
        sprintf(offset, "OFFSET %s", query->offsetCondition);
      }
      if (offset == NULL)
        offset = "";

      // ORDER BY
      char *order = NULL;
      for (int i = 0; i < query->orderConditionsCount; i++) {
        if (order == NULL) {
          order = req->malloc(strlen(query->orderConditions[i]) +
                              strlen(" ORDER BY ") + 1);
          sprintf(order, "ORDER BY %s", query->orderConditions[i]);
        } else {
          char *tmp = req->malloc(strlen(order) + strlen(" , ") +
                                  strlen(query->orderConditions[i]) + 1);
          sprintf(tmp, "%s , %s", order, query->orderConditions[i]);
          order = tmp;
        }
      }
      if (order == NULL)
        order = "";

      char *queryString =
          req->malloc(strlen(select) + strlen(from) + strlen(where) +
                      strlen(limit) + strlen(offset) + strlen(order) + 6);
      sprintf(queryString, "%s %s %s %s %s %s", select, from, where, limit,
              offset, order);

      for (int i = 0; i < query->whereConditionsCount; i++) {
        free(query->whereConditions[i]);
      }

      return pg->execParams(queryString, query->paramValueCount, NULL,
                            query->paramValues, NULL, NULL, 0);
    });

    query->find = req->blockCopy(^(char *id) {
      query->where("id = $", id);
      return query->all();
    });

    query->count = req->blockCopy(^() {
      query->selectCondition = "count(*)";
      PGresult *pgres = query->all();
      if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
        log_err("%s", PQresultErrorMessage(pgres));
        return -1;
      }
      int count = atoi(PQgetvalue(pgres, 0, 0));
      PQclear(pgres);
      return count;
    });

    return query;
  });
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

  postgres->free = Block_copy(^() {
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
    dispatch_async(dispatch_get_main_queue(), ^() {
      Block_release(postgres->free);
      free(postgres);
    });
  });

  return postgres;
error:
  return NULL;
}

middlewareHandler postgresMiddlewareFactory(postgres_connection_t *postgres) {
  /* Test connection */
  postgres->connection = PQconnectdb(postgres->uri);
  check(PQstatus(postgres->connection) == CONNECTION_OK,
        "Failed to connect to postgres: %s (%s)",
        PQerrorMessage(postgres->connection), postgres->uri);

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

    pg->query = getPostgresQuery(req, pg);

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

  return Block_copy(^(UNUSED request_t *req, UNUSED response_t *res,
                      void (^next)(), void (^cleanup)(cleanupHandler)) {
    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
        // PQfinish(postgres->connection);
    }));
    next();
  });
}
