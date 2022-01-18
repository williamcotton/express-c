#ifndef TODO_MODEL_H
#define TODO_MODEL_H

#include <cJSON/cJSON.h>
#include "../../src/express.h"

typedef int (^filterCallback)(void *item);
typedef void (^eachCallback)(void *item);

typedef struct collection_t
{
  void (^each)(eachCallback);
} collection_t;

typedef struct todo_t
{
  int id;
  char *title;
  int completed;
  cJSON * (^toJSON)();
} todo_t;

typedef struct todo_store_t
{
  cJSON *store;
  int count;
  todo_t * (^new)(char *);
  todo_t * (^create)(todo_t *todo);
  todo_t * (^find)(int id);
  void (^update)(todo_t *todo);
  void (^delete)(int id);
  collection_t * (^all)();
  collection_t * (^filter)(filterCallback);
} todo_store_t;

middlewareHandler todoStoreMiddleware();

#endif // !TODO_MODEL_H
