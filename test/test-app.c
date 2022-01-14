#include <stdlib.h>
#include <stdio.h>
#include <Block.h>
#include <hash/hash.h>
#include "../src/express.h"
#include "test-harnass.h"
#include "tape.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

static char *errorHTML = "<!DOCTYPE html>\n"
                         "<html lang=\"en\">\n"
                         "<head>\n"
                         "<meta charset=\"utf-8\">\n"
                         "<title>Error</title>\n"
                         "</head>\n"
                         "<body>\n"
                         "<pre>Cannot GET %s</pre>\n"
                         "</body>\n"
                         "</html>\n";

void runTests()
{
  tape_t t = tape();

  int testStatus = t.test("express", ^(UNUSED tape_t *t) {
    clearState();

    t->test("GET", ^(tape_t *t) {
      t->strEqual("root", curlGet("/"), "Hello World!");
      t->strEqual("basic route", curlGet("/test"), "Testing, testing!");
      t->strEqual("query string", curlGet("/qs\?value1=123\\&value2=34%205"), "<h1>Query String</h1><p>Value 1: 123</p><p>Value 2: 34 5</p>");
      t->strEqual("route params", curlGet("/one/123/two/345/567.jpg"), "<h1>Params</h1><p>One: 123</p><p>Two: 345</p><p>Three: 567</p>");
      t->strEqual("send file", curlGet("/file"), "hello, world!\n");
    });

    t->test("POST", ^(tape_t *t) {
      t->strEqual("form data", curlPost("/post/form123", "param1=12%2B3&param2=3+4%205"), "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    });

    t->test("PUT", ^(tape_t *t) {
      t->strEqual("put form data", curlPut("/put/form123", "param1=12%2B3&param2=3+4%205"), "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    });

    t->test("PATCH", ^(tape_t *t) {
      t->strEqual("patch form data", curlPatch("/patch/form123", "param1=12%2B3&param2=3+4%205"), "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
    });

    t->test("DELETE", ^(tape_t *t) {
      t->strEqual("delete form data", curlDelete("/delete/form123"), "<h1>Delete</h1><p>ID: form123</p>");
    });

    t->test("Middleware", ^(tape_t *t) {
      t->strEqual("static file middleware", curlGet("/test/test2.txt"), "this is a test!!!");
      t->strEqual("custom request middleware", curlGet("/m"), "super test");
    });

    t->test("File", ^(tape_t *t) {
      t->strEqual("file", curlGet("/file"), "hello, world!\n");
      char *error = malloc(1024);
      sprintf(error, errorHTML, "/test/test3.txt");
      t->strEqual("file not found", curlGet("/test/test3.txt"), error);
    });

    t->test("Session", ^(tape_t *t) {
      t->strEqual("session set", curlPost("/session", "param1=session-data"), "ok");
      t->strEqual("session get", curlGet("/session"), "session-data");
    });

    t->test("Header", ^(tape_t *t) {
      t->strEqual("headers", curlGet("/headers"), "<h1>Headers</h1><p>Host: 127.0.0.1:3032</p><p>Accept: */*</p>");
      t->strEqual("set header", curlGet("/set_header"), "ok");
    });

    t->test("Cookies", ^(tape_t *t) {
      t->strEqual("set cookie", curlGet("/set_cookie\?session=123\\&user=test"), "ok");
      t->strEqual("get cookie", curlGet("/get_cookie"), "session: 123 - user: test");
    });

    t->test("Redirect", ^(tape_t *t) {
      t->strEqual("redirect", curlGet("/redirect"), "Redirecting to /redirected");
      t->strEqual("redirect back", curlGet("/redirect/back"), "Redirecting to back");
    });

    t->test("Error", ^(tape_t *t) {
      char *error = malloc(1024);
      sprintf(error, errorHTML, "/error");
      t->strEqual("error", curlGet("/error"), error);
    });
  });

  exit(testStatus);
}

#pragma clang diagnostic pop

int main()
{
  app_t app = express();
  int port = 3032;

  app.use(expressStatic("test"));
  app.use(memSessionMiddlewareFactory());

  typedef struct super_t
  {
    char *uuid;
  } super_t;

  app.use(^(request_t *req, UNUSED response_t *res, void (^next)()) {
    super_t *super = malloc(sizeof(super_t));
    super->uuid = "super test";
    req->mSet("super", super);
    next();
  });

  app.get("/", ^(UNUSED request_t *req, response_t *res) {
    res->send("Hello World!");
  });

  app.get("/test", ^(UNUSED request_t *req, response_t *res) {
    res->status = 201;
    res->send("Testing, testing!");
  });

  app.get("/qs", ^(request_t *req, response_t *res) {
    res->send("<h1>Query String</h1><p>Value 1: %s</p><p>Value 2: %s</p>", req->query("value1"), req->query("value2"));
  });

  app.get("/headers", ^(request_t *req, response_t *res) {
    res->send("<h1>Headers</h1><p>Host: %s</p><p>Accept: %s</p>", req->get("Host"), req->get("Accept"));
  });

  app.get("/file", ^(UNUSED request_t *req, response_t *res) {
    res->sendFile("./test/test.txt");
  });

  app.get("/one/:one/two/:two/:three.jpg", ^(request_t *req, response_t *res) {
    res->send("<h1>Params</h1><p>One: %s</p><p>Two: %s</p><p>Three: %s</p>", req->params("one"), req->params("two"), req->params("three"));
  });

  app.get("/form", ^(UNUSED request_t *req, response_t *res) {
    res->send("<form method=\"POST\" action=\"/post/new\">"
              "  <input type=\"text\" name=\"param1\">"
              "  <input type=\"text\" name=\"param2\">"
              "  <input type=\"submit\" value=\"Submit\">"
              "</form>");
  });

  app.post("/post/:form", ^(request_t *req, response_t *res) {
    res->status = 201;
    res->send("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", req->body("param1"), req->body("param2"));
  });

  app.post("/session", ^(request_t *req, response_t *res) {
    req->session->set("param1", req->body("param1"));
    res->send("ok");
  });

  app.put("/put/:form", ^(UNUSED request_t *req, response_t *res) {
    res->status = 201;
    res->send("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", req->body("param1"), req->body("param2"));
  });

  app.patch("/patch/:form", ^(UNUSED request_t *req, response_t *res) {
    res->status = 201;
    res->send("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", req->body("param1"), req->body("param2"));
  });

  app.delete("/delete/:id", ^(UNUSED request_t *req, response_t *res) {
    res->send("<h1>Delete</h1><p>ID: %s</p>", req->params("id"));
  });

  app.get("/session", ^(request_t *req, response_t *res) {
    res->send(req->session->get("param1"));
  });

  app.get("/m", ^(request_t *req, response_t *res) {
    super_t *super = req->m("super");
    res->send(super->uuid);
  });

  app.get("/set_header", ^(UNUSED request_t *req, response_t *res) {
    res->set("X-Test-1", "test1");
    res->set("X-Test-2", "test2");
    res->send("ok");
  });

  app.get("/set_cookie", ^(UNUSED request_t *req, response_t *res) {
    res->cookie("session", req->query("session"));
    res->cookie("user", req->query("user"));
    res->send("ok");
  });

  app.get("/get_cookie", ^(UNUSED request_t *req, response_t *res) {
    res->send("session: %s - user: %s", req->cookie("session"), req->cookie("user"));
  });

  app.get("/redirect", ^(UNUSED request_t *req, response_t *res) {
    res->redirect("/redirected");
  });

  app.get("/redirect/back", ^(UNUSED request_t *req, response_t *res) {
    res->redirect("back");
  });

  app.listen(port, ^{
    runTests();
  });

  return 0;
}
