# express-c

Fast, unopinionated, minimalist web framework for C

## Features

- User-friendly API using blocks
- Uses libdispatch for concurrency

## Basic Usage

```c
#include <stdlib.h>
#include <stdio.h>
#include "../src/express.h"

char *styles = "<link rel='stylesheet' href='/demo/public/app.css'>";

char *createTodoForm = "<form method='POST' action='/todo/create'>"
                       "  <p><label>Title: <input type='text' name='title'></label></p>"
                       "  <p><label>Body: <input type='text' name='body'></label></p>"
                       "  </p><input type='submit' value='Create'></p>"
                       "</form>";

int main()
{
  app_t app = express();
  int port = 3000;

  app.use(expressStatic("demo/public"));

  app.get("/", ^(UNUSED request_t *req, response_t *res) {
    res->sendf("%s<h1>Todo App</h1><p>%s</p>", styles, createTodoForm);
  });

  app.post("/todo/create", ^(request_t *req, response_t *res) {
    res->status = 201;
    res->sendf("%s<h1>%s</h1><p>%s</p>", styles, req->body("title"), req->body("body"));
  });

  app.listen(port, ^{
    printf("Todo app listening at http://localhost:%d\n", port);
  });

  return 0;
}
```

See `test/test-app.c` example for a more complete example.

## Installation

### OS X

```
$ brew install llvm clib
```

### Linux

The primary requirements are `clang`, `libbsd`, `libblocksruntime` and `libdispatch`.

The easiest way is to open this in a GitHub Codespace.

Otherwise see the Dockerfile in .devcontainer for full system requirements.

## Demo App

```
$ make app
$ build/app
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

## Testing

Uses a very simple testing harness to launch a test app and run integration tests using system calls to `curl`. There are no unit tests which encourages refactoring.

```
make test
```

## Dependencies

Uses [clib](https://github.com/clibs/clib) for dependency management:

- [clibs/hash](https://github.com/clibs/hash)
- [Isty001/dotenv-c](https://github.com/Isty001/dotenv-c)

And:

- [h2o/picohttpparser](https://github.com/h2o/picohttpparser)
