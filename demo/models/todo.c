#include <stdlib.h>
#include <stdio.h>
#include <Block.h>
#include <cJSON/cJSON.h>
#include "todo.h"

typedef cJSON * (^toJSON)();

static toJSON todoToJSON(todo_t *todo)
{
  return Block_copy(^(void) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "id", todo->id);
    cJSON_AddStringToObject(json, "title", todo->title);
    todo->completed ? cJSON_AddTrueToObject(json, "completed") : cJSON_AddFalseToObject(json, "completed");
    return json;
  });
}

static todo_t *buildTodoFromJson(cJSON *json)
{
  todo_t *todo = malloc(sizeof(todo_t));
  todo->id = cJSON_GetObjectItem(json, "id")->valueint;
  todo->title = cJSON_GetObjectItem(json, "title")->valuestring;
  todo->completed = cJSON_GetObjectItem(json, "completed")->valueint;
  todo->toJSON = todoToJSON(todo);
  return todo;
}

middlewareHandler todoStoreMiddleware()
{
  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)(), void (^cleanup)(cleanupHandler)) {
    todo_store_t *todoStore = malloc(sizeof(todo_store_t));
    cJSON *todoStoreJson = req->session->get("todoStore");

    if (todoStoreJson == NULL)
    {
      todoStore->store = cJSON_CreateArray();
      todoStore->count = 0;
    }
    else
    {
      todoStore->store = todoStoreJson;
      int maxId = 0;
      cJSON *item;
      cJSON_ArrayForEach(item, todoStore->store)
      {
        int id = cJSON_GetObjectItem(item, "id")->valueint;
        if (id > maxId)
        {
          maxId = id;
        }
      }
      todoStore->count = maxId + 1;
    }

    todoStore->new = Block_copy(^(char *title) {
      __block todo_t *newTodo = malloc(sizeof(todo_t));
      newTodo->id = todoStore->count;
      newTodo->title = title;
      newTodo->completed = 0;
      newTodo->toJSON = todoToJSON(newTodo);
      return newTodo;
    });

    todoStore->create = Block_copy(^(todo_t *todo) {
      todo->id = todoStore->count++;
      cJSON *todoJson = todo->toJSON();
      cJSON_AddItemToArray(todoStore->store, todoJson);
      req->session->set("todoStore", todoStore->store);
      return todo;
    });

    todoStore->update = Block_copy(^(todo_t *todo) {
      cJSON *json = todo->toJSON();
      cJSON *item = NULL;
      int i = 0;
      cJSON_ArrayForEach(item, todoStore->store)
      {
        if (cJSON_GetObjectItem(item, "id")->valueint == todo->id)
        {
          cJSON_ReplaceItemInArray(todoStore->store, i, json);
          break;
        }
        i++;
      }
      req->session->set("todoStore", todoStore->store);
    });

    todoStore->delete = Block_copy(^(int id) {
      cJSON *item = NULL;
      int i = 0;
      cJSON_ArrayForEach(item, todoStore->store)
      {
        if (cJSON_GetObjectItem(item, "id")->valueint == id)
        {
          cJSON_DeleteItemFromArray(todoStore->store, i);
          break;
        }
        i++;
      }
      req->session->set("todoStore", todoStore->store);
    });

    todoStore->find = Block_copy(^(int id) {
      cJSON *item = NULL;
      cJSON_ArrayForEach(item, todoStore->store)
      {
        if (cJSON_GetObjectItem(item, "id")->valueint == id)
        {

          break;
        }
      }
      if (item == NULL)
      {
        return (todo_t *)NULL;
      }
      todo_t *todo = buildTodoFromJson(item);
      return todo;
    });

    todoStore->all = Block_copy(^() {
      collection_t *collection = malloc(sizeof(collection_t));
      collection->each = Block_copy(^(eachCallback callback) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, todoStore->store)
        {
          todo_t *todo = buildTodoFromJson(item);
          callback(todo);
        }
      });
      return collection;
    });

    todoStore->filter = Block_copy(^(UNUSED filterCallback fCb) {
      collection_t *collection = malloc(sizeof(collection_t));
      collection->each = Block_copy(^(UNUSED eachCallback eCb) {
        cJSON *item = NULL;
        int totalTodos = cJSON_GetArraySize(todoStore->store);
        todo_t *filteredTodos[totalTodos];
        int filteredTodosCount = 0;
        cJSON_ArrayForEach(item, todoStore->store)
        {
          todo_t *todo = buildTodoFromJson(item);
          if (fCb(todo))
          {
            filteredTodos[filteredTodosCount++] = todo;
          }
        }
        for (int i = 0; i < filteredTodosCount; i++)
        {
          eCb(filteredTodos[i]);
        }
      });
      return collection;
    });

    req->session->set("todoStore", todoStore->store);

    req->mSet("todoStore", todoStore);

    cleanup(Block_copy(^() {
      printf("cleanup todoStoreMiddleware\n");
      // Block_release(todoStore->filter);
      // Block_release(todoStore->new);
      // Block_release(todoStore->update);
      // Block_release(todoStore->all);
      // Block_release(todoStore->delete);
      // Block_release(todoStore->create);
      // Block_release(todoStore->find);
      // free(todoStore);
    }));

    next();
  });
}