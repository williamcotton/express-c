# Todo Store and Model

A model and store built on top of [cJSON](https://github.com/DaveGamble/cJSON) and the [cookie session middleware](https://github.com/williamcotton/express-c/tree/master/src/middleware/cookie-session-middleware).

```c
#include "todo.h"
#include <Block.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <stdlib.h>

typedef cJSON * (^toJSON)();
```

### Request-based Memory Management

In order to simplify the memory management of the model, we use the `req->blockCopy` function instead of the standard [`Block_copy`](https://clang.llvm.org/docs/BlockLanguageSpec.html#the-copy-and-release-operations) operation.

This allows us to nest the `toJSON()` function deep within the model and avoid an ungainly process of freeing the memory.

```c
static toJSON todoToJSON(request_t *req, todo_t *todo) {
  return req->blockCopy(^(void) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "id", todo->id);
    cJSON_AddStringToObject(json, "title", todo->title);
    todo->completed ? cJSON_AddTrueToObject(json, "completed")
                    : cJSON_AddFalseToObject(json, "completed");
    return json;
  });
}

static todo_t *buildTodoFromJson(request_t *req, todo_t *todo, cJSON *json) {
  todo->id = cJSON_GetObjectItem(json, "id")
                 ? cJSON_GetObjectItem(json, "id")->valueint
                 : 0;
  todo->title = cJSON_GetObjectItem(json, "title")
                    ? cJSON_GetObjectItem(json, "title")->valuestring
                    : NULL;
  todo->completed = cJSON_GetObjectItem(json, "completed")
                        ? cJSON_GetObjectItem(json, "completed")->valueint
                        : 0;
  todo->toJSON = todoToJSON(req, todo);
  return todo;
}
```

### Middleware

On each request we deserialize the cookie and rebuild the store.

```c
middlewareHandler todoStoreMiddleware() {
  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)(),
                      void (^cleanup)(cleanupHandler)) {
    todo_store_t *todoStore = req->malloc(sizeof(todo_store_t));
    cJSON *todoStoreJson = req->session->get("todoStore");

    if (todoStoreJson == NULL) {
      todoStore->store = cJSON_CreateArray();
      todoStore->count = 0;
    } else {
      todoStore->store = todoStoreJson;
      int maxId = 0;
      cJSON *item;
      int totalTodos = cJSON_GetArraySize(todoStore->store);
      if (totalTodos > 0) {
        cJSON_ArrayForEach(item, todoStore->store) {
          if (cJSON_GetObjectItem(item, "id")) {
            int id = cJSON_GetObjectItem(item, "id")->valueint;
            if (id > maxId)
              maxId = id;
          }
        }
      }
      todoStore->count = maxId + 1;
    }
```

### CRUD Operations

In addition to the `req->blockCopy` memory allocation helper there is a `req->malloc` helper that is used to allocate memory for the model for the duration of the request.

```c
    todoStore->new = req->blockCopy(^(char *title) {
      todo_t *newTodo = req->malloc(sizeof(todo_t));
      newTodo->id = todoStore->count;
      newTodo->title = title;
      newTodo->completed = 0;
      newTodo->toJSON = todoToJSON(req, newTodo);
      return newTodo;
    });

    todoStore->create = req->blockCopy(^(todo_t *todo) {
      todo->id = todoStore->count++;
      cJSON *todoJson = todo->toJSON();
      cJSON_AddItemToArray(todoStore->store, todoJson);
      req->session->set("todoStore", todoStore->store);
      return todo;
    });

    todoStore->update = req->blockCopy(^(todo_t *todo) {
      cJSON *json = todo->toJSON();
      cJSON *item = NULL;
      int i = 0;
      int totalTodos = cJSON_GetArraySize(todoStore->store);
      if (totalTodos == 0)
        return;
      cJSON_ArrayForEach(item, todoStore->store) {
        if (cJSON_GetObjectItem(item, "id")->valueint == todo->id) {
          cJSON_ReplaceItemInArray(todoStore->store, i, json);
          break;
        }
        i++;
      }
      req->session->set("todoStore", todoStore->store);
    });

    todoStore->delete = req->blockCopy(^(int id) {
      cJSON *item = NULL;
      int i = 0;
      int totalTodos = cJSON_GetArraySize(todoStore->store);
      if (totalTodos == 0)
        return;
      cJSON_ArrayForEach(item, todoStore->store) {
        if (cJSON_GetObjectItem(item, "id")->valueint == id) {
          cJSON_DeleteItemFromArray(todoStore->store, i);
          break;
        }
        i++;
      }
      req->session->set("todoStore", todoStore->store);
    });

    todoStore->find = req->blockCopy(^(int id) {
      cJSON *item = NULL;
      int totalTodos = cJSON_GetArraySize(todoStore->store);
      if (totalTodos == 0)
        return (todo_t *)NULL;
      cJSON_ArrayForEach(item, todoStore->store) {
        if (cJSON_GetObjectItem(item, "id")->valueint == id)
          break;
      }
      if (item == NULL)
        return (todo_t *)NULL;
      todo_t *todo = req->malloc(sizeof(todo_t));
      todo = buildTodoFromJson(req, todo, item); // leak
      return todo;
    });
```

### Collections

In addition to the basic CRUD operations are a few helper functions for working with a collection of todos.

```
typedef struct collection_t {
  void (^each)(eachCallback);
} collection_t;
```

This very basic implementation of a collection supports an interable `each` function.

```c
    todoStore->all = req->blockCopy(^() {
      collection_t *collection = malloc(sizeof(collection_t));
      collection->each = req->blockCopy(^(eachCallback callback) {
        cJSON *item = NULL;
        int totalTodos = cJSON_GetArraySize(todoStore->store);
        if (totalTodos == 0)
          return;
        cJSON_ArrayForEach(item, todoStore->store) {
          todo_t *todo = req->malloc(sizeof(todo_t));
          todo = buildTodoFromJson(req, todo, item); // leak
          callback(todo);
        }
      });
      return collection;
    });

    todoStore->filter = req->blockCopy(^(UNUSED filterCallback fCb) {
      collection_t *collection = req->malloc(sizeof(collection_t));
      collection->each = req->blockCopy(^(UNUSED eachCallback eCb) {
        cJSON *item = NULL;
        int totalTodos = cJSON_GetArraySize(todoStore->store);
        if (totalTodos == 0)
          return;
        todo_t *filteredTodos[totalTodos];
        int filteredTodosCount = 0;
        cJSON_ArrayForEach(item, todoStore->store) {
          todo_t *todo = req->malloc(sizeof(todo_t));
          todo = buildTodoFromJson(req, todo, item); // leak
          if (fCb(todo)) {
            filteredTodos[filteredTodosCount++] = todo;
          }
        }
        if (filteredTodosCount == 0)
          return;
        for (int i = 0; i < filteredTodosCount; i++) {
          eCb(filteredTodos[i]);
        }
      });
      return collection;
    });

    req->session->set("todoStore", todoStore->store);

    req->mSet("todoStore", todoStore);

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
    }));

    next();
  });
}
```
