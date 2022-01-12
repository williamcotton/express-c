#include <stdlib.h>
#include <stdio.h>
#include "../src/express.h"
#include "test-harnass.h"

void runTests()
{
  testEq("root", curlGet("/"), "Hello World!");
  testEq("basic route", curlGet("/test"), "Testing, testing!");
  testEq("query string", curlGet("/qs\?value1=123\\&value2=34%205"), "<h1>Query String</h1><p>Value 1: 123</p><p>Value 2: 34 5</p>");
  testEq("headers", curlGet("/headers"), "<h1>Headers</h1><p>Host: 127.0.0.1:3032</p><p>Accept: */*</p>");
  testEq("route params", curlGet("/one/123/two/345/567.jpg"), "<h1>Params</h1><p>One: 123</p><p>Two: 345</p><p>Three: 567</p>");
  testEq("send file", curlGet("/file"), "hello, world!\n");
  testEq("static file middleware", curlGet("/test/test2.txt"), "this is a test!!!");
  testEq("form data", curlPost("/post/form123", "param1=12%2B3&param2=3+4%205"), "<h1>Form</h1><p>Param 1: 12+3</p><p>Param 2: 3 4 5</p>");
  exit(EXIT_SUCCESS);
}

int main()
{
  app_t app = express();
  int port = 3032;

  app.use(expressStatic("test"));

  app.use(^(UNUSED request_t *req, UNUSED response_t *res, void (^next)()) {
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
    res->sendf("<h1>Query String</h1><p>Value 1: %s</p><p>Value 2: %s</p>", req->query("value1"), req->query("value2"));
  });

  app.get("/headers", ^(request_t *req, response_t *res) {
    res->sendf("<h1>Headers</h1><p>Host: %s</p><p>Accept: %s</p>", req->get("Host"), req->get("Accept"));
  });

  app.get("/file", ^(UNUSED request_t *req, response_t *res) {
    res->sendFile("./test/test.txt");
  });

  app.get("/one/:one/two/:two/:three.jpg", ^(request_t *req, response_t *res) {
    res->sendf("<h1>Params</h1><p>One: %s</p><p>Two: %s</p><p>Three: %s</p>", req->param("one"), req->param("two"), req->param("three"));
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
    res->sendf("<h1>Form</h1><p>Param 1: %s</p><p>Param 2: %s</p>", req->body("param1"), req->body("param2"));
  });

  app.listen(port, ^{
    runTests();
  });

  return 0;
}
