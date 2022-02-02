# express-c

Fast, unopinionated, minimalist web framework for C

![Test](https://github.com/williamcotton/express-c/actions/workflows/test.yml/badge.svg)

## Development Status

Fast and loose! This is alpha software and is incomplete. It's not ready for production as it is most likely unstable and insecure.

### Contributing

Please [open an issue](https://github.com/williamcotton/express-c/issues/new) or [open a pull request](https://github.com/williamcotton/express-c/compare) if you have any suggestions or bug reports. Thanks!

## Features

- User-friendly API using blocks modeled after [express](https://expressjs.com/)
- Uses libdispatch for concurrency
- Request-based memory management for middleware

## Basic Usage

```c
...

int main()
{
  env_load(".", false);

  app_t app = express();
  int port = getenv("PORT") ? atoi(getenv("PORT")) : 3000;

  char *staticFilesPath = cwdFullPath("demo/public");
  app.use(expressStatic("demo/public", staticFilesPath));
  app.use(cJSONCookieSessionMiddlewareFactory());
  app.use(todoStoreMiddleware());
  app.use(cJSONMustacheMiddleware("demo/views"));

  app.get("/", ^(request_t *req, response_t *res) {
    char *queryFilter = req->query("filter");
    char *filter = queryFilter ? queryFilter : "all";
    todo_store_t *todoStore = req->m("todoStore");

    __block int completedCount = 0;
    __block int total = 0;

    collection_t *todosCollection = todoStore->filter(^(void *item) {
      todo_t *todo = (todo_t *)item;
      total++;
      if (todo->completed)
        completedCount++;
      return strcmp(filter, "all") == 0 ||
             (strcmp(filter, "active") == 0 && !todo->completed) ||
             (strcmp(filter, "completed") == 0 && todo->completed);
    });

    int uncompletedCount = total - completedCount;

    cJSON *json = cJSON_CreateObject();
    cJSON *todos = cJSON_AddArrayToObject(json, "todos");

    cJSON_AddStringToObject(json, "filterAll", strcmp(filter, "all") == 0 ? "selected" : "");
    cJSON_AddStringToObject(json, "filterActive", strcmp(filter, "active") == 0 ? "selected" : "");
    cJSON_AddStringToObject(json, "filterCompleted", strcmp(filter, "completed") == 0 ? "selected" : "");

    todosCollection->each(^(void *item) {
      todo_t *todo = item;
      cJSON_AddItemToArray(todos, todo->toJSON());
    });

    cJSON_AddNumberToObject(json, "uncompletedCount", uncompletedCount);

    uncompletedCount == 1 ? cJSON_AddTrueToObject(json, "hasUncompletedSingular") : cJSON_AddFalseToObject(json, "hasUncompletedSingular");
    completedCount > 0 ? cJSON_AddTrueToObject(json, "hasCompleted") : cJSON_AddFalseToObject(json, "hasCompleted");
    total == 0 ? cJSON_AddTrueToObject(json, "noTodos") : cJSON_AddFalseToObject(json, "noTodos");

    res->render("index", json);

    if (strcmp(filter, "all") != 0)
      free(filter);
  });

  app.post("/todo", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    char *title = req->body("title");

    todo_t *newTodo = todoStore->new (title);
    todoStore->create(newTodo);
    res->redirect("back");

    free(title);
  });

  app.put("/todo/:id", ^(request_t *req, response_t *res) {
    todo_store_t *todoStore = req->m("todoStore");
    char *idString = req->params("id");

    int id = atoi(idString);
    todo_t *todo = todoStore->find(id);
    if (todo != NULL)
    {
      todo->completed = !todo->completed;
      todoStore->update(todo);
    }
    res->redirect("back");

    free(idString);
  });

...

  app.cleanup(^{
    free(staticFilesPath);
  });

  app.listen(port, ^{
    printf("TodoMVC app listening at http://localhost:%d\n", port);
  });

  return 0;
}

```

See the rest of the TodoMVC [demo](https://github.com/williamcotton/express-c/tree/master/demo) for more.

### Request-Based Memory Management

Features `req.malloc` and `req.blockCopy` to allocate memory which is automatically freed when the request is done.

Middleware is given a `cleanup` callback which is also called at completion of request.

Take a look at the [Todo store middleware](https://github.com/williamcotton/express-c/blob/master/demo/models/todo.c) for an example.

## Installation

### OS X

```
$ brew install llvm clib fswatch
```

### Linux

The primary requirements are `clang`, `libbsd`, `libblocksruntime` and `libdispatch`.

The easiest way is to open this in a GitHub Codespace.

Otherwise see the Dockerfile in .devcontainer for full system requirements.

## Demo App

See it running: [https://express-c-demo.herokuapp.com](https://express-c-demo.herokuapp.com).

```
$ make TodoMVC
$ build/TodoMVC
Todo app listening at http://localhost:3000
$ open http://localhost:3000
^C
Closing server...
```

## VS Code Development

Please install the recommended extensions for VS Code:

- [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
- [C/C++ Themes](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-themes)
- [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)
- [CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb)

### Run and Debug

The VS Code `launch.json` and `tasks.json` are set up to run and debug any `.c` file in the `demo/` directory using `make` and the `CodeLLDB` extension.

### Clang Tidy

It is recommended that you set `"C_Cpp.updateChannel": "Insiders",` in your local `settings.json` file. This will give you access to the latest features including support for `clang-tidy`.

Otherwise you can install the [Clang-Tidy](https://marketplace.visualstudio.com/items?itemName=notskm.clang-tidy) extension.

### Make

The build process is configured to add everything in the `deps/` directory following the [clib best practices](https://github.com/clibs/clib/blob/master/BEST_PRACTICE.md).

The Makefile constructs `CFLAGS` from the `clang-tidy` compatible [compile_flags.txt](https://clang.llvm.org/docs/JSONCompilationDatabase.html#alternatives) file.

There are additional platform dependent `CFLAGS` for Linux and Darwin (OS X).

### Watch

To recompile and restart a server app in the `demo/` directory when a file changes:

```
$ make TodoMVC-watch
```

## Testing

Uses a very simple testing harness to launch a test app and run integration tests using system calls to `curl`. There are no unit tests which encourages refactoring.

```
$ make test
```

### Memory Leak Testing

To test for memory leaks:

```
$ make test-leaks
```

#### OS X

Uses the native `leaks` command line application. Build `make build-test-trace` to enable tracing using the Instruments app.

#### Linux

Uses `valgrind` with `--tool=memcheck` and `--leak-check=full` options.

### Watch

To recompile and rerun the tests when a file changes:

```
$ make test-watch
```

## Dependencies

Uses [clib](https://github.com/clibs/clib) for dependency management:

- [Isty001/dotenv-c](https://github.com/Isty001/dotenv-c)

And:

- [h2o/picohttpparser](https://github.com/h2o/picohttpparser)
- [trumpowen/MegaMimes](https://github.com/trumpowen/MegaMimes)
