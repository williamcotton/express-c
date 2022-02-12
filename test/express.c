#include "../src/express.h"
#include "tape.h"
#include "test-helpers.h"
#include <Block.h>
#include <stdlib.h>
#include <string.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"

void expressTests(tape_t *t) {
  /* Mock system call failures */
#ifdef __linux__
  void expressMockSystemCalls(tape_t * t);
  expressMockSystemCalls(t);
#endif

  /* Fuzz testing */
  void expressFuzz(tape_t * t);
  expressFuzz(t);

  /* HTTP status codes */
  void statusMessageTests(tape_t * t);
  statusMessageTests(t);

  /* Helper functions */
  t->test("matchEmbeddedFile", ^(tape_t *t) {
    unsigned char demo_public_app_css[] = {0x68};
    // unsigned int demo_public_app_css_len = 1;
    unsigned char *embeddedFilesData[] = {demo_public_app_css};
    int embeddedFilesLengths[] = {1};
    char *embeddedFilesNames[] = {"demo_public_app_css"};
    const int embeddedFilesCount = 1;
    embedded_files_data_t embeddedFiles = {
        embeddedFilesData, embeddedFilesLengths, embeddedFilesNames,
        embeddedFilesCount};
    t->strEqual("matching file",
                matchEmbeddedFile("demo/public/app.css", embeddedFiles), "h");

    const char *bogus = matchEmbeddedFile("bogus", embeddedFiles);
    t->ok("bogus file", bogus == NULL);
  });

  t->test("writePid", ^(tape_t *t) {
    char *pidFile = "test.pid";
    writePid(pidFile);
    t->ok("writePid", readPid(pidFile) > 0);
    unlink(pidFile);
  });

  /* Express */
  t->test("GET", ^(tape_t *t) {
    t->strEqual("root", curlGet("/"), "Hello World!");
    t->strEqual("basic route", curlGet("/test"), "Testing, testing!");
    t->strEqual(
        "route params", curlGet("/one/123/two/345/567.jpg"),
        "<h1>Params</h1><p>One: 123</p><p>Two: 345</p><p>Three: 567</p>");
    t->strEqual("send status", curlGet("/status"), "I'm a teapot");
    t->strEqual("query string", curlGet("/qs\?value1=123\\&value2=34%205"),
                "<h1>Query String</h1><p>Value 1: 123</p><p>Value 2: 34 5</p>");
    t->strEqual("send file", curlGet("/file"), "hello, world!\n");
  });

  t->test("POST", ^(tape_t *t) {
    t->strEqual("form data",
                curlPost("/post/form123", "param1=12%2B3&param2=3+4%205"),
                "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
  });

  t->test("PUT", ^(tape_t *t) {
    t->strEqual("put form data",
                curlPut("/put/form123", "param1=12%2B3&param2=3+4%205"),
                "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    t->strEqual(
        "body method",
        curlPost("/put/form123", "param1=12%2B3&param2=3+4%205&_method=put"),
        "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
  });

  t->test("PATCH", ^(tape_t *t) {
    t->strEqual("patch form data",
                curlPatch("/patch/form123", "param1=12%2B3&param2=3+4%205"),
                "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    t->strEqual("body method",
                curlPost("/patch/form123",
                         "param1=12%2B3&param2=3+4%205&_method=patch"),
                "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
  });

  t->test("DELETE", ^(tape_t *t) {
    t->strEqual("delete form data", curlDelete("/delete/form123"),
                "<h1>Delete</h1><p>ID: form123</p>");
    t->strEqual("body method", curlPost("/delete/form123", "_method=delete"),
                "<h1>Delete</h1><p>ID: form123</p>");
  });

  t->test("Middleware", ^(tape_t *t) {
    t->strEqual("static file middleware", curlGet("/test/test2.txt"),
                "this is a test!!!");
    t->strEqual("custom request middleware", curlGet("/m"), "super test");
    t->strEqual("custom request middleware",
                curlGet("/base/params/bloop/m-isolated"),
                "No super-nested-router");
    t->strEqual("custom request middleware", curlGet("/base/nested/m-isolated"),
                "No super-params-router");
    t->strEqual("custom request middleware", curlGet("/base/m-isolated"),
                "No super-nested-router");
    t->strEqual("custom request middleware", curlGet("/m-isolated"),
                "No super-nested-router");
  });

  t->test("File", ^(tape_t *t) {
    t->strEqual("file", curlGet("/file"), "hello, world!\n");
    char error[1024];
    sprintf(error, errorHTML, "Cannot GET /test/test3.txt");
    t->strEqual("file not found", curlGet("/test/test3.txt"), error);
    t->strEqual("download", curlGet("/download"), "hello, world!\n");
    char *missingFile = curlGet("/download_missing");
    t->ok("download missing", missingFile != NULL);
    free(missingFile);
  });

  t->test("Session", ^(tape_t *t) {
    t->strEqual("session set", curlPost("/session", "param1=session-data"),
                "ok");
    t->strEqual("session get", curlGet("/session"), "session-data");
  });

  t->test("Header", ^(tape_t *t) {
    t->strEqual(
        "headers", curlGetHeaders("/headers"),
        "<h1>Headers</h1><p>Host: one.two.three.test.com</p><p>Accept: "
        "*/*</p><p>3 Subdomains: one two three</p><p>3 IPs: 1.1.1.1 2.2.2.2 "
        "3.3.3.3</p>");
    t->strEqual("set header", curlGet("/set_header"), "test1");
  });

  t->test("Cookies", ^(tape_t *t) {
    t->strEqual("set cookie", curlGet("/set_cookie\?session=123\\&user=test"),
                "ok");
    t->strEqual("get cookie", curlGet("/get_cookie"),
                "session: 123 - user: test");
    t->strEqual("clear cookie", curlGet("/clear_cookie"), "ok");
    t->strEqual("get cleared cookie", curlGet("/get_cookie"),
                "session: 123 - user: (null)");
  });

  t->test("All", ^(tape_t *t) {
    t->strEqual("get", curlGet("/all"), "all");
    t->strEqual("post", curlPost("/all", "param1=all"), "all");
    t->strEqual("put", curlPut("/all", "param1=all"), "all");
    t->strEqual("patch", curlPatch("/all", "param1=all"), "all");
    t->strEqual("delete", curlDelete("/all"), "all");
  });

  t->test("Redirect", ^(tape_t *t) {
    t->strEqual("redirect", curlGet("/redirect"), "Redirecting to /redirected");
    t->strEqual("redirect back", curlGet("/redirect/back"),
                "Redirecting to back");
  });

  t->test("Error", ^(tape_t *t) {
    char error[1024];
    sprintf(error, errorHTML, "Cannot GET /error");
    t->strEqual("error", curlGet("/error"), error);
    t->strEqual("custom error handler", curlGet("/base/error"), "fubar");
  });

  t->test("Router", ^(tape_t *t) {
    t->strEqual("root", curlGet("/base"), "Hello Router!");
    t->strEqual("basic route", curlGet("/base/test"), "Testing Router!");
    t->strEqual("route params", curlGet("/base/one/123/two/345/567.jpg"),
                "<h1>Base Params</h1><p>One: 123</p><p>Two: 345</p><p>Three: "
                "567</p>");
    t->strEqual("custom request middleware", curlGet("/base/m"),
                "super-router test");

    t->test("Nested router", ^(tape_t *t) {
      t->strEqual("root", curlGet("/base/nested"), "Hello Nested Router!");
      t->strEqual("basic route", curlGet("/base/nested/test"),
                  "Testing Nested Router!");
      t->strEqual("route params",
                  curlGet("/base/nested/one/123/two/345/567.jpg"),
                  "<h1>Nested Params</h1><p>One: 123</p><p>Two: "
                  "345</p><p>Three: 567</p>");
      t->strEqual("custom request middleware", curlGet("/base/nested/m"),
                  "super-nested-router test");
    });

    t->test("Params router", ^(tape_t *t) {
      t->strEqual("basic route", curlGet("/base/params/blip/test"),
                  "Testing Nested blip Router!");
      t->strEqual("route params",
                  curlGet("/base/params/blip/one/123/two/345/567.jpg"),
                  "<h1>Nested Params blip</h1><p>One: 123</p><p>Two: "
                  "345</p><p>Three: 567</p>");
      t->strEqual("root", curlGet("/base/params/blip"),
                  "Hello Params blip Router!");
      t->strEqual("custom request middleware", curlGet("/base/params/blip/m"),
                  "super-params-router test blip");
      t->strEqual("param value", curlGet("/base/params/hip/param_value"),
                  "hip");
    });

    t->test("Root router", ^(tape_t *t) {
      t->strEqual("root route", curlGet("/root"), "Hello Root Router!");
    });
  });
}
#pragma clang diagnostic pop
