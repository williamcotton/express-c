#include <stdlib.h>
#include <stdio.h>
#include <hash/hash.h>
#include <Block.h>
#include <string.h>
#include <cJSON/cJSON.h>
#include <mustach/mustach-cjson.h>
#include <dirent.h>
#include "../src/express.h"

typedef struct todo_t
{
  int id;
  char *title;
  int completed;
  cJSON * (^toJSON)(struct todo_t *);
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
  char * (^loadTemplate)(char *) = ^(char *templateFile) {
    char *template = NULL;
    size_t length;
    char *templatePath = malloc(strlen(viewsPath) + strlen(templateFile) + 3);
    sprintf(templatePath, "%s/%s", viewsPath, (char *)templateFile);
    printf("templatePath: %s\n", templatePath);
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
        mustach_cJSON_mem(template, 0, json, 0, &renderedTemplate, &length);
        res->send(renderedTemplate);
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
        todo_t *newTodo = malloc(sizeof(todo_t));
        newTodo->title = title;
        newTodo->completed = 0;
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
  app_t app = express();
  int port = 3000;

  app.use(expressStatic("demo/public"));
  app.use(memSessionMiddlewareFactory());
  app.use(todoStoreMiddleware());
  app.use(cJSONMustache("demo/views"));

  app.get("/", ^(UNUSED request_t *req, response_t *res) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "completedCount", 1);

    cJSON *todos = cJSON_AddArrayToObject(json, "todos");

    cJSON *todo1 = cJSON_CreateObject();
    cJSON_AddStringToObject(todo1, "title", "Taste Javascript");
    cJSON_AddStringToObject(todo1, "id", "0");
    cJSON_AddTrueToObject(todo1, "completed");
    cJSON_AddItemToArray(todos, todo1);

    cJSON *todo2 = cJSON_CreateObject();
    cJSON_AddStringToObject(todo2, "title", "Buy a unicorn");
    cJSON_AddStringToObject(todo2, "id", "1");
    cJSON_AddFalseToObject(todo2, "completed");
    cJSON_AddItemToArray(todos, todo2);

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

  app.listen(port, ^{
    printf("Todo app listening at http://localhost:%d\n", port);
  });

  return 0;
}
