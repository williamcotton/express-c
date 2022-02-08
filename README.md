# express-c

Fast, unopinionated, minimalist web framework for C

![Test](https://github.com/williamcotton/express-c/actions/workflows/test.yml/badge.svg)

## Development Status

Fast and loose! This is alpha software and is incomplete. It's not ready for production as it is most likely unstable and insecure.

### Contributing

Please [open an issue](https://github.com/williamcotton/express-c/issues/new) or [open a pull request](https://github.com/williamcotton/express-c/compare) if you have any suggestions or bug reports. Thanks!

## Features

- User-friendly API using blocks modeled on [expressjs](https://expressjs.com/)
- Uses libdispatch for concurrency
- Request-based memory management for middleware

## Basic Usage

```c
#include "../src/express.h"
#include <dotenv-c/dotenv.h>
#include <stdio.h>
#include <stdlib.h>

char *styles = "<link rel='stylesheet' href='/demo/public/app.css'>";

char *createTodoForm =
    "<form method='POST' action='/todo/create'>"
    "  <p><label>Title: <input type='text' name='title'></label></p>"
    "  <p><label>Body: <input type='text' name='body'></label></p>"
    "  </p><input type='submit' value='Create'></p>"
    "</form>";

int main() {
  app_t app = express();

  /* Load .env file */
  env_load(".", false);
  char *PORT = getenv("PORT");
  int port = PORT ? atoi(PORT) : 3000;

  /* Load static files middleware */
  embedded_files_data_t embeddedFiles = {0};
  char *staticFilesPath = cwdFullPath("demo/public");
  app.use(expressStatic("demo/public", staticFilesPath, embeddedFiles));

  /* Route handlers */
  app.get("/", ^(UNUSED request_t *req, response_t *res) {
    res->sendf("%s<h1>Todo App</h1><p>%s</p>", styles, createTodoForm);
  });

  app.post("/todo/create", ^(request_t *req, response_t *res) {
    res->status = 201;
    res->sendf("%s<h1>%s</h1><p>%s</p>", styles, req->body("title"), req->body("body"));
  });

  /* Start server */
  app.listen(port, ^{
    printf("express-c app listening at http://localhost:%d\n", port);
  });
}
```

Take a look at the [TodoMVC demo](https://github.com/williamcotton/express-c/tree/master/demo) for a more complete example that includes [Mustache templates middleware](https://github.com/williamcotton/express-c/tree/master/deps/cJSONMustacheMiddleware), [cookie-based sessions middleware](https://github.com/williamcotton/express-c/tree/master/deps/cJSONCookieSessionMiddleware), [thread-safe postgres middleware](https://github.com/williamcotton/express-c/tree/master/deps/postgresMiddleware), and more.

### Request-based Memory Management

Features `req.malloc` and `req.blockCopy` to allocate memory which is automatically freed when the request is done.

Middleware is given a `cleanup` callback which is also called at completion of request.

Apps and routers are also given `cleanup` callback stacks which are handled during `app.closeServer()`.

Take a look at the [Todo store](https://github.com/williamcotton/express-c/tree/master/demo/models#request-based-memory-management) for an example.

## Installation

### OS X

```
$ brew install llvm clib fswatch dbmate
```

### Linux

The primary requirements are `clang`, `libbsd`, `libblocksruntime` and `libdispatch`.

The easiest way is to open this in a GitHub Codespace.

Otherwise see the [Dockerfile](https://github.com/williamcotton/express-c/blob/master/Dockerfile) for instructions on how to build the dependencies.

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

### Memory Leaks

To test for memory leaks:

```
$ make test-leaks
```

#### OS X

Uses the native `leaks` command line application. Build `make build-test-trace` to enable tracing using the Instruments app.

#### Linux

Uses `valgrind` with `--tool=memcheck` and `--leak-check=full` options.

### Linting

Linting using `clang-tidy` with `-warnings-as-errors=*`:

```
$ make test-lint
```

### Code Formatting

Formatting using `clang-format --dry-run --Werror`:

```
$ make test-format
```

### Watch

To recompile and rerun the tests when a file changes:

```
$ make test-watch
```

### Continuous Integration

There is a [GitHub Actions](https://github.com/williamcotton/express-c/actions) workflow for continuous integration. It builds and runs the a number of tests on both Ubuntu and OS X.

- `make format`
- `make lint`
- `make test-leaks`
- `make test`

## Dependencies

Uses [clib](https://github.com/clibs/clib) for dependency management:

- [Isty001/dotenv-c](https://github.com/Isty001/dotenv-c)

And:

- [h2o/picohttpparser](https://github.com/h2o/picohttpparser)
- [trumpowen/MegaMimes](https://github.com/trumpowen/MegaMimes)
