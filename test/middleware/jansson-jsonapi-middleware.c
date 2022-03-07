#include "../src/express.h"
#include <string.h>
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

void janssonJasonapiMiddlewareTests(tape_t *t) {
  t->test("jansson jsonapi middleware", ^(tape_t *t) {
    t->test("get", ^(tape_t *t) {
      string_collection_t *headers = stringCollection(0, NULL);
      string_t *contentType = string("Content-Type: application/vnd.api+json");
      headers->push(contentType);

      t->strEqual("with jsonapi header",
                  t->fetch("/jansson-jsonapi", "GET", headers, NULL), "ok");

      t->strEqual("without jsonapi header", t->get("/jansson-jsonapi"),
                  "not ok");

      headers->free();
    });

    t->test("query", ^(tape_t *t) {
      string_collection_t *headers = stringCollection(0, NULL);
      string_t *contentType = string("Content-Type: application/vnd.api+json");
      headers->push(contentType);

      string_t *response =
          t->fetch("/jansson-jsonapi/"
                   "query?filter[id][not_eq]=1,2,3&filter[a]=false&sort=foo,"
                   "books.title,-books.pages&stats[total]=count,sum&fields["
                   "people]=name,age&filter[name]=%22Jane%22",
                   "GET", headers, NULL);

      t->strEqual("with jsonapi header", response,
                  "{\"filter\": {\"id\": {\"not_eq\": [\"1\", \"2\", \"3\"]}, "
                  "\"a\": "
                  "[\"false\"], \"name\": [\"\\\"Jane\\\"\"]}, \"sort\": "
                  "[\"foo\", "
                  "\"books.title\", \"-books.pages\"], \"stats\": {\"total\": "
                  "[\"count\", \"sum\"]}, \"fields\": {\"people\": [\"name\", "
                  "\"age\"]}}");

      t->strEqual("without jsonapi header", t->get("/jansson-jsonapi/query"),
                  "not ok");

      headers->free();
    });

    t->test("post", ^(tape_t *t) {
      string_collection_t *headers = stringCollection(0, NULL);
      string_t *contentType = string("Content-Type: application/vnd.api+json");
      headers->push(contentType);

      char *json =
          "{\"data\":{\"type\":\"posts\",\"attributes\":{\"title\":\"foo\"}}}"
          "\n";

      t->strEqual("with jsonapi header",
                  t->fetch("/jansson-jsonapi", "POST", headers, json), "posts");

      t->strEqual("without jsonapi header", t->post("/jansson-jsonapi", json),
                  "not ok");

      headers->free();
    });
  });
}
#pragma clang diagnostic pop
