#include "../src/express.h"
#include <Block.h>
#include <stdlib.h>
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"

void expressTests(tape_t *t) {
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
    char *fileData = matchEmbeddedFile("demo/public/app.css", embeddedFiles);
    t->strEqual("matching file", t->string(fileData), "h");
    free(fileData);
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
    t->strEqual("root", t->get("/"), "Hello World!");
    t->ok("root contains", t->get("/")->contains("Hello"));
    t->strEqual("basic route", t->get("/test"), "Testing, testing!");
    t->strEqual(
        "route params", t->get("/one/123/two/345/567.jpg"),
        "<h1>Params</h1><p>One: 123</p><p>Two: 345</p><p>Three: 567</p>");
    t->strEqual("send status", t->get("/status"), "I'm a teapot");
    t->strEqual("query string", t->get("/qs\?value1=123\\&value2=34%205"),
                "<h1>Query String</h1><p>Value 1: 123</p><p>Value 2: 34 5</p>");
    t->strEqual("send file", t->get("/file"), "hello, world!\n");
  });

  t->test("POST", ^(tape_t *t) {
    t->strEqual("form data",
                t->post("/post/form123", "param1=12%2B3&param2=3+4%205"),
                "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
  });

  t->test("PUT", ^(tape_t *t) {
    t->strEqual("put form data",
                t->put("/put/form123", "param1=12%2B3&param2=3+4%205"),
                "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    t->strEqual(
        "body method",
        t->post("/put/form123", "param1=12%2B3&param2=3+4%205&_method=put"),
        "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
  });

  t->test("PATCH", ^(tape_t *t) {
    t->strEqual("patch form data",
                t->patch("/patch/form123", "param1=12%2B3&param2=3+4%205"),
                "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    t->strEqual(
        "body method",
        t->post("/patch/form123", "param1=12%2B3&param2=3+4%205&_method=patch"),
        "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
  });

  t->test("DELETE", ^(tape_t *t) {
    t->strEqual("delete form data", t->delete ("/delete/form123"),
                "<h1>Delete</h1><p>ID: form123</p>");
    t->strEqual("body method", t->post("/delete/form123", "_method=delete"),
                "<h1>Delete</h1><p>ID: form123</p>");
  });

  t->test("Middleware", ^(tape_t *t) {
    t->strEqual("static file middleware", t->get("/test/files/test2.txt"),
                "this is a test!!!");
    t->strEqual("custom request middleware", t->get("/m"), "super test");
    t->strEqual("custom request middleware",
                t->get("/base/params/bloop/m-isolated"),
                "No super-nested-router");
    t->strEqual("custom request middleware", t->get("/base/nested/m-isolated"),
                "No super-params-router");
    t->strEqual("custom request middleware", t->get("/base/m-isolated"),
                "No super-nested-router");
    t->strEqual("custom request middleware", t->get("/m-isolated"),
                "No super-nested-router");
  });

  t->test("File", ^(tape_t *t) {
    t->strEqual("file", t->get("/file"), "hello, world!\n");
    char error[1024];
    sprintf(error, errorHTML, "Cannot GET /test/test3.txt");
    t->strEqual("file not found", t->get("/test/test3.txt"), error);
    t->strEqual("download", t->get("/download"), "hello, world!\n");
    // char *missingFile = t->get("/download_missing");
    t->ok("download missing", t->get("/download_missing")->value != NULL);
    // free(missingFile);
  });

  t->test("Session", ^(tape_t *t) {
    t->strEqual("session set", t->post("/session", "param1=session-data"),
                "ok");
    t->strEqual("session get", t->get("/session"), "session-data");
  });

  t->test("Header", ^(tape_t *t) {
    t->strEqual(
        "headers", t->getHeaders("/headers"),
        "<h1>Headers</h1><p>Host: one.two.three.test.com</p><p>Accept: "
        "*/*</p><p>3 Subdomains: one two three</p><p>3 IPs: 1.1.1.1 2.2.2.2 "
        "3.3.3.3</p>");
    t->strEqual("set header", t->get("/set_header"), "test1");
  });

  t->test("Cookies", ^(tape_t *t) {
    t->strEqual("set cookie", t->get("/set_cookie\?session=123\\&user=test"),
                "ok");
    t->strEqual("get cookie", t->get("/get_cookie"),
                "session: 123 - user: test");
    t->strEqual("clear cookie", t->get("/clear_cookie"), "ok");
    t->strEqual("get cleared cookie", t->get("/get_cookie"),
                "session: 123 - user: (null)");
  });

  t->test("All", ^(tape_t *t) {
    t->strEqual("get", t->get("/all"), "all");
    t->strEqual("post", t->post("/all", "param1=all"), "all");
    t->strEqual("put", t->put("/all", "param1=all"), "all");
    t->strEqual("patch", t->patch("/all", "param1=all"), "all");
    t->strEqual("delete", t->delete ("/all"), "all");
  });

  t->test("Redirect", ^(tape_t *t) {
    t->strEqual("redirect", t->get("/redirect"), "Redirecting to /redirected");
    t->strEqual("redirect back", t->get("/redirect/back"),
                "Redirecting to back");
  });

  t->test("Error", ^(tape_t *t) {
    char error[1024];
    sprintf(error, errorHTML, "Cannot GET /error");
    t->strEqual("error", t->get("/error"), error);
    t->strEqual("custom error handler", t->get("/base/error"), "fubar");
  });

  t->test("Router", ^(tape_t *t) {
    t->strEqual("root", t->get("/base"), "Hello Router!");
    t->strEqual("basic route", t->get("/base/test"), "Testing Router!");
    t->strEqual(
        "route params", t->get("/base/one/123/two/345/567.jpg"),
        "<h1>Base Params</h1><p>One: 123</p><p>Two: 345</p><p>Three: 567</p>");
    t->strEqual("custom request middleware", t->get("/base/m"),
                "super-router test");

    t->test("Nested router", ^(tape_t *t) {
      t->strEqual("root", t->get("/base/nested"), "Hello Nested Router!");
      t->strEqual("basic route", t->get("/base/nested/test"),
                  "Testing Nested Router!");
      t->strEqual("route params",
                  t->get("/base/nested/one/123/two/345/567.jpg"),
                  "<h1>Nested Params</h1><p>One: 123</p><p>Two: "
                  "345</p><p>Three: 567</p>");
      t->strEqual("custom request middleware", t->get("/base/nested/m"),
                  "super-nested-router test");
    });

    t->test("Params router", ^(tape_t *t) {
      t->strEqual("basic route", t->get("/base/params/blip/test"),
                  "Testing Nested blip Router!");
      t->strEqual("route params",
                  t->get("/base/params/blip/one/123/two/345/567.jpg"),
                  "<h1>Nested Params blip</h1><p>One: 123</p><p>Two: "
                  "345</p><p>Three: 567</p>");
      t->strEqual("root", t->get("/base/params/blip"),
                  "Hello Params blip Router!");
      t->strEqual("custom request middleware", t->get("/base/params/blip/m"),
                  "super-params-router test blip");
      t->strEqual("param value", t->get("/base/params/hip/param_value"), "hip");
    });

    t->test("Root router", ^(tape_t *t) {
      t->strEqual("root route", t->get("/root"), "Hello Root Router!");
    });
  });

  t->test("Trash", ^(tape_t *t) {
    t->strEqual("take out the trash", t->get("/trash"), "trash");
  });

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

/* Middleware */
#ifdef __linux__
  void postgresMiddlewareTests(tape_t * t);
  postgresMiddlewareTests(t);
#endif
  void mustacheMiddlewareTests(tape_t * t);
  mustacheMiddlewareTests(t);
  void cookieSessionMiddlewareTests(tape_t * t);
  cookieSessionMiddlewareTests(t);

  /* Strings */
  void stringTests(tape_t * t);
  stringTests(t);
}
#pragma clang diagnostic pop
