#include "adapter.h"

int pgParamCount(const char *query);

database_pool_t *databasePoolCreate(const char *uri, int size) {
  database_pool_t *pool = malloc(sizeof(database_pool_t));
  pool->uri = uri;
  pool->size = size;
  pool->head = NULL;
  pool->semaphore = dispatch_semaphore_create(size);
  pool->queue = dispatch_queue_create(pool->uri, NULL);
  return pool;
}

database_connection_t *createPostgresConnection(const char *uri) {
  database_connection_t *connection = malloc(sizeof(database_connection_t));
  connection->next = NULL;
  connection->connection = PQconnectdb(uri);
  return connection;
}

database_pool_t *createPostgresPool(const char *uri, int size) {
  database_pool_t *pool = databasePoolCreate(uri, size);
  database_connection_t *connection = createPostgresConnection(uri);
  database_connection_t *c = connection;
  for (int i = 0; i < size; i++) {
    c->next = createPostgresConnection(uri);
    c = c->next;
  }
  pool->head = connection;
  pool->exec = Block_copy(^(const char *sql, ...) {
    int nParams = pgParamCount(sql);
    va_list args;
    va_start(args, sql);

    const char **paramValues =
        nParams == 0 ? NULL : malloc(sizeof(char *) * nParams);
    for (int i = 0; i < nParams; i++) {
      paramValues[i] = va_arg(args, const char *);
      if (i > 1024) {
        log_err("Too many parameters");
        return (PGresult *)NULL;
      }
    }
    va_end(args);

    __block database_connection_t *current = NULL;
    dispatch_semaphore_wait(pool->semaphore, DISPATCH_TIME_FOREVER);
    dispatch_sync(pool->queue, ^{
      current = pool->head;
      pool->head = current->next;
    });

    PGresult *pgres = PQexecParams(current->connection, sql, nParams, NULL,
                                   paramValues, NULL, NULL, 0);
    free(paramValues);

    dispatch_sync(pool->queue, ^{
      current->next = pool->head;
      pool->head = current;
    });
    dispatch_semaphore_signal(pool->semaphore);

    return pgres;
  });

  pool->execParams =
      Block_copy(^(const char *sql, int nParams, const Oid *paramTypes,
                   const char *const *paramValues, const int *paramLengths,
                   const int *paramFormats, int resultFormat) {
        __block database_connection_t *current = NULL;
        dispatch_semaphore_wait(pool->semaphore, DISPATCH_TIME_FOREVER);
        dispatch_sync(pool->queue, ^{
          current = pool->head;
          pool->head = current->next;
        });

        PGresult *pgres =
            PQexecParams(current->connection, sql, nParams, paramTypes,
                         paramValues, paramLengths, paramFormats, resultFormat);

        dispatch_sync(pool->queue, ^{
          current->next = pool->head;
          pool->head = current;
        });
        dispatch_semaphore_signal(pool->semaphore);
        return pgres;
      });

  return pool;
}
