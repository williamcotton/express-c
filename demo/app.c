#include <stdlib.h>
#include <stdio.h>
#include <hash/hash.h>
#include <Block.h>
#include <string.h>
#include <dirent.h>
#include <cJSON/cJSON.h>
#include <mustach/mustach-cjson.h>
#include <dotenv-c/dotenv.h>
#include "../src/express.h"

#ifndef PORT
#define PORT 3000
#endif // !PORT

typedef struct todo_t
{
  int id;
  char *title;
  int completed;
  cJSON * (^toJSON)();
} todo_t;

typedef struct todo_store_t
{
  hash_t *store;
  int count;
  todo_t * (^new)(char *);
  todo_t * (^create)(todo_t *todo);
  todo_t * (^find)(int id);
  void (^update)(int id, todo_t *todo);
  void (^delete)(int id);
} todo_store_t;

middlewareHandler cJSONMustache(char *viewsPath)
{
  char * (^mustacheErrorMessage)(int) = ^(int result) {
    switch (result)
    {
    case MUSTACH_ERROR_SYSTEM:
      return "System error";
    case MUSTACH_ERROR_UNEXPECTED_END:
      return "Unexpected end of template";
    case MUSTACH_ERROR_EMPTY_TAG:
      return "Empty tag";
    case MUSTACH_ERROR_TAG_TOO_LONG:
      return "Tag too long";
    case MUSTACH_ERROR_BAD_SEPARATORS:
      return "Bad separators";
    case MUSTACH_ERROR_TOO_DEEP:
      return "Too deep";
    case MUSTACH_ERROR_CLOSING:
      return "Closing tag without opening tag";
    case MUSTACH_ERROR_BAD_UNESCAPE_TAG:
      return "Bad unescape tag";
    case MUSTACH_ERROR_INVALID_ITF:
      return "Invalid item or partial";
    case MUSTACH_ERROR_ITEM_NOT_FOUND:
      return "Item not found";
    case MUSTACH_ERROR_PARTIAL_NOT_FOUND:
      return "Partial not found";
    default:
      return "Unknown error";
    }
  };

  char * (^loadTemplate)(char *) = ^(char *templateFile) {
    char *template = NULL;
    size_t length;
    char *templatePath = malloc(strlen(viewsPath) + strlen(templateFile) + 3);
    sprintf(templatePath, "%s/%s", viewsPath, (char *)templateFile);
    FILE *templateFd = fopen(templatePath, "r");
    if (templateFd)
    {
      fseek(templateFd, 0, SEEK_END);
      length = ftell(templateFd);
      fseek(templateFd, 0, SEEK_SET);
      template = malloc(length + 1);
      fread(template, 1, length, templateFd);
      fclose(templateFd);
      template[length] = '\0';
    }
    return template;
  };

  void (^loadPartials)(cJSON *data, char *templateFile) = ^(UNUSED cJSON *data, char *templateFile) {
    UNUSED struct dirent *de;
    DIR *dr = opendir(viewsPath);
    while ((de = readdir(dr)) != NULL)
    {
      if (strstr(de->d_name, ".mustache") && strcmp(de->d_name, templateFile))
      {
        char *partial = loadTemplate(de->d_name);
        char *partialName = strdup(de->d_name);
        char *partialNameSplit = strtok(partialName, ".");
        cJSON_AddStringToObject(data, partialNameSplit, partial);
        free(partial);
      }
    }
    closedir(dr);
  };

  return Block_copy(^(UNUSED request_t *req, response_t *res, void (^next)()) {
    res->render = ^(void *templateFile, void *data) {
      cJSON *json = data;
      char *template = loadTemplate(templateFile);
      if (template)
      {
        size_t length;
        char *renderedTemplate;
        loadPartials(json, (char *)templateFile);
        int result = mustach_cJSON_mem(template, 0, json, 0, &renderedTemplate, &length);
        if (result == 0)
        {
          res->send(renderedTemplate);
        }
        else
        {
          res->status = 500;
          res->sendf("Error rendering mustache template: %s", mustacheErrorMessage(result));
        }
      }
      else
      {
        res->status = 500;
        res->send("Template file not found");
      }
    };
    next();
  });
}

middlewareHandler todoStoreMiddleware()
{
  return Block_copy(^(request_t *req, UNUSED response_t *res, void (^next)()) {
    todo_store_t *todoStore = req->session->get("todoStore");

    if (todoStore == NULL)
    {
      todoStore = malloc(sizeof(todo_store_t));
      todoStore->store = hash_new();
      todoStore->count = 0;

      todoStore->new = Block_copy(^(char *title) {
        __block todo_t *newTodo = malloc(sizeof(todo_t));
        newTodo->title = title;
        newTodo->completed = 0;
        newTodo->toJSON = Block_copy(^() {
          cJSON *json = cJSON_CreateObject();
          cJSON_AddNumberToObject(json, "id", newTodo->id);
          cJSON_AddStringToObject(json, "title", newTodo->title);
          newTodo->completed ? cJSON_AddTrueToObject(json, "completed") : cJSON_AddFalseToObject(json, "completed");
          return json;
        });
        return newTodo;
      });

      todoStore->create = Block_copy(^(todo_t *todo) {
        todo->id = todoStore->count++;
        char *key = malloc(sizeof(int));
        sprintf(key, "%d", todo->id);
        hash_set(todoStore->store, key, todo);
        return todo;
      });

      todoStore->update = Block_copy(^(int id, todo_t *todo) {
        char key[10];
        sprintf(key, "%d", id);
        hash_set(todoStore->store, key, todo);
      });

      todoStore->delete = Block_copy(^(int id) {
        char key[10];
        sprintf(key, "%d", id);
        hash_del(todoStore->store, key);
      });

      todoStore->find = Block_copy(^(int id) {
        char key[10];
        sprintf(key, "%d", id);
        return (todo_t *)hash_get(todoStore->store, key);
      });

      req->session->set("todoStore", todoStore);
    }

    req->mSet("todoStore", todoStore);

    next();
  });
}

int main()
{
  env_load(".", false);

  app_t app = express();
  int port = getenv("PORT") ? atoi(getenv("PORT")) : PORT;

  app.use(expressStatic("demo/public"));
  app.use(memSessionMiddlewareFactory());
  app.use(todoStoreMiddleware());
  app.use(cJSONMustache("demo/views"));

  app.get("/", ^(request_t *req, response_t *res) {
    cJSON *json = cJSON_CreateObject();

    char *filter = req->query("filter") ? req->query("filter") : "all";
    cJSON_AddStringToObject(json, "filterAll", strcmp(filter, "all") == 0 ? "selected" : "");
    cJSON_AddStringToObject(json, "filterActive", strcmp(filter, "active") == 0 ? "selected" : "");
    cJSON_AddStringToObject(json, "filterCompleted", strcmp(filter, "completed") == 0 ? "selected" : "");

    int completedCount = 0;
    int total = 0;

    cJSON *todos = cJSON_AddArrayToObject(json, "todos");

    todo_store_t *todoStore = req->m("todoStore");
    hash_each(todoStore->store, {
      todo_t *todo = (todo_t *)val;
      total++;
      if (todo->completed)
        completedCount++;
      if (strcmp(filter, "all") == 0 ||
          (strcmp(filter, "active") == 0 && !todo->completed) ||
          (strcmp(filter, "completed") == 0 && todo->completed))
      {
        cJSON_AddItemToArray(todos, todo->toJSON());
      }
    });

    int uncompletedCount = total - completedCount;

    cJSON_AddNumberToObject(json, "uncompletedCount", uncompletedCount);
    cJSON_AddNumberToObject(json, "completedCount", completedCount);
    cJSON_AddNumberToObject(json, "totalCount", total);

    total == completedCount ? cJSON_AddTrueToObject(json, "allCompleted") : cJSON_AddFalseToObject(json, "allCompleted");
    completedCount > 0 ? cJSON_AddTrueToObject(json, "hasCompleted") : cJSON_AddFalseToObject(json, "hasCompleted");
    total == 0 ? cJSON_AddTrueToObject(json, "noTodos") : cJSON_AddFalseToObject(json, "noTodos");

    res->render("index.mustache", json);
  });

  app.post("/todo", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    todo_t *newTodo = todoStore->new (req->body("title"));
    todoStore->create(newTodo);
    res->redirect("back");
  });

  app.put("/todo/:id", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    int id = atoi(req->params("id"));
    todo_t *todo = todoStore->find(id);
    if (todo != NULL)
    {
      todo->completed = !todo->completed;
      todoStore->update(id, todo);
    }
    res->redirect("back");
  });

  app.delete("/todo/:id", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    int id = atoi(req->params("id"));
    todoStore->delete (id);
    res->redirect("back");
  });

  app.post("/todo_clear_all_completed", ^(UNUSED request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    hash_each(todoStore->store, {
      todo_t *todo = (todo_t *)val;
      if (todo->completed)
      {
        todoStore->delete (todo->id);
      }
    });
    res->redirect("back");
  });

  app.listen(port, ^{
    printf("Todo app listening at http://localhost:%d\n", port);
  });

  return 0;
}
