#include "../model/employee.h"
#include "../model/team.h"
#include "employee.h"
#include "team.h"
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"

void setupTest(pg_t *pg);

void resourceTests(tape_t *t, const char *databaseUrl) {
  pg_t *pg = initPg(databaseUrl);
  memory_manager_t *memoryManager = createMemoryManager();
  pg->query = getPostgresQuery(memoryManager, pg);
  request_t *req = memoryManager->malloc(sizeof(request_t));
  req->memoryManager = memoryManager;

  setupTest(pg);

  string_collection_t *headers = stringCollection(0, NULL);
  string_t *contentType = string("Content-Type: application/vnd.api+json");
  headers->push(contentType);
  __block string_t *res;

  t->test("resource", ^(tape_t *t) {
    t->test("all", ^(tape_t *t) {
      t->strEqual(
          "no filters", t->fetch("/api/v1/teams", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad request",
          t->fetch("/api/v1/teams?sdfsdfsd=sdfsd", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "sort name asc",
          t->fetch("/api/v1/teams?sort=name", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"3\", "
          "\"attributes\": {\"name\": \"engineering\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"2\", \"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "bad sort request",
          t->fetch("/api/v1/teams?sort=sdfsd", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "sort id desc",
          t->fetch("/api/v1/teams?sort=-id", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"3\", \"attributes\": "
          "{\"name\": \"engineering\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"1\", \"attributes\": {\"name\": \"design\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}], \"meta\": {}}");

      res = t->fetch("/api/v1/notes?sort=-title,date", "GET", headers, NULL);
      // TODO: this works as expected but don't hard code the response!
      string_t *sortedIds = string("3,6,2,5,1,4");
      t->strEqual("sort title desc, date asc", sortedIds, "3,6,2,5,1,4");
      sortedIds->free();

      t->strEqual(
          "bad sort request",
          t->fetch("/api/v1/teams?sort=id,-sdfsd", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad sort request",
          t->fetch("/api/v1/teams?sort=[]sd[f]sdf]", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad sort request",
          t->fetch("/api/v1/teams?sort[gorp]=ssdf", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad sort request",
          t->fetch("/api/v1/teams?sort=[fubar][gorp]=", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "page size 1 and page number 2",
          t->fetch("/api/v1/teams?page[size]=1&page[number]=2", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
          "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "bad page request",
          t->fetch("/api/v1/teams?page[size]=1", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad page request",
          t->fetch("/api/v1/teams?page[number]=1", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad page request",
          t->fetch("/api/v1/teams?page[sdfsdf]", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad page request",
          t->fetch("/api/v1/teams?page=1", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "filter name eq",
          t->fetch("/api/v1/teams?filter[name][eq]=Product", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
          "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "bad filter request",
          t->fetch("/api/v1/teams?filter[name]=Product", "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad filter request",
          t->fetch("/api/v1/teams?filter[name][sdfsd]=Product", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad filter request",
          t->fetch("/api/v1/teams?filter[sdfsdf][sdfsd]=Product", "GET",
                   headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad filter request",
          t->fetch("/api/v1/teams?filter[sdfsdf][sdfsd][sdfsdf]=Product", "GET",
                   headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad filter request",
          t->fetch("/api/v1/teams?filter[sdfsdf][sdfsd]=[Product]", "GET",
                   headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad filter request",
          t->fetch("/api/v1/teams?filter[sdfsdf][sdfsd]=[sdf][sdfsd],Product",
                   "GET", headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "bad filter request",
          t->fetch("/api/v1/teams?filter[sdfsdf]sdfsd]=Product", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->strEqual(
          "filter name not eq",
          t->fetch("/api/v1/teams?filter[name][not_eq]=Product", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"3\", "
          "\"attributes\": {\"name\": \"engineering\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "filter name eql",
          t->fetch("/api/v1/teams?filter[name][eql]=product", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
          "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "filter name not eql",
          t->fetch("/api/v1/teams?filter[name][not_eql]=product", "GET",
                   headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"3\", "
          "\"attributes\": {\"name\": \"engineering\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "filter name eql two things",
          t->fetch("/api/v1/teams?filter[name][eql]=design,engineering", "GET",
                   headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"3\", "
          "\"attributes\": {\"name\": \"engineering\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "filter name match",
          t->fetch("/api/v1/teams?filter[name][match]=rod", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
          "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "filter name not match",
          t->fetch("/api/v1/teams?filter[name][not_match]=rod", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"3\", "
          "\"attributes\": {\"name\": \"engineering\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "filter name prefix",
          t->fetch("/api/v1/teams?filter[name][prefix]=prod", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
          "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "filter name not prefix",
          t->fetch("/api/v1/teams?filter[name][not_prefix]=prod", "GET",
                   headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"3\", "
          "\"attributes\": {\"name\": \"engineering\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "filter name suffix",
          t->fetch("/api/v1/teams?filter[name][suffix]=duct", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
          "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}], \"meta\": {}}");

      t->strEqual(
          "filter name not suffix",
          t->fetch("/api/v1/teams?filter[name][not_suffix]=duct", "GET",
                   headers, NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"3\", "
          "\"attributes\": {\"name\": \"engineering\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}], \"meta\": {}}");

      res = t->fetch("/api/v1/meetings?filter[max_size][eq]=5", "GET", headers,
                     NULL);
      t->ok("filter max_size eq",
            !res->contains("\"id\": \"1\"") && res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[max_size][not_eq]=5", "GET",
                     headers, NULL);
      t->ok("filter max_size eq",
            res->contains("\"id\": \"1\"") && !res->contains("\"id\": \"2\""));

      t->strEqual("bad filter request",
                  t->fetch("/api/v1/meetings?filter[max_size][not_eq]=sdfsdf",
                           "GET", headers, NULL),
                  "{\"data\": [], \"meta\": {}}");

      t->strEqual("bad filter request",
                  t->fetch("/api/v1/meetings?filter[max_size][eq]=sdfsdf",
                           "GET", headers, NULL),
                  "{\"data\": [], \"meta\": {}}");

      res = t->fetch("/api/v1/meetings?filter[date][eq]=2018-06-18", "GET",
                     headers, NULL);
      t->ok("filter date eq",
            res->contains("\"id\": \"1\"") && !res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[date][not_eq]=2018-06-18", "GET",
                     headers, NULL);
      t->ok("filter date not eq",
            !res->contains("\"id\": \"1\"") && res->contains("\"id\": \"2\""));

      t->strEqual("bad filter request",
                  t->fetch("/api/v1/meetings?filter[date][eq]=sdfsdf", "GET",
                           headers, NULL),
                  "{\"data\": [], \"meta\": {}}");

      t->strEqual("bad filter request",
                  t->fetch("/api/v1/meetings?filter[date][not_eq]=sdfsdf",
                           "GET", headers, NULL),
                  "{\"data\": [], \"meta\": {}}");

      t->strEqual("bad filter request",
                  t->fetch("/api/v1/meetings?filter[date][not_eq]=", "GET",
                           headers, NULL),
                  "{\"data\": [], \"meta\": {}}");

      res = t->fetch(
          "/api/v1/meetings?filter[timestamp][eq]=2018-06-18T05:00:00-06:00",
          "GET", headers, NULL);
      t->ok("filter timestamp eq",
            res->contains("\"id\": \"1\"") && !res->contains("\"id\": \"2\""));

      res = t->fetch(
          "/api/v1/"
          "meetings?filter[timestamp][not_eq]=2018-06-18T05:00:00-06:00",
          "GET", headers, NULL);
      t->ok("filter timestamp not eq",
            !res->contains("\"id\": \"1\"") && res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[max_temp][eq]=72.295618", "GET",
                     headers, NULL);
      t->ok("filter max_temp eq",
            res->contains("\"id\": \"1\"") && !res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[max_temp][not_eq]=72.295618",
                     "GET", headers, NULL);
      t->ok("filter max_temp not eq",
            !res->contains("\"id\": \"1\"") && res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[budget][eq]=85000.25", "GET",
                     headers, NULL);
      t->ok("filter budget eq",
            res->contains("\"id\": \"1\"") && !res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[budget][not_eq]=85000.25", "GET",
                     headers, NULL);
      t->ok("filter budget not eq",
            !res->contains("\"id\": \"1\"") && res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[open][eq]=true", "GET", headers,
                     NULL);
      t->ok("filter open eq",
            res->contains("\"id\": \"1\"") && !res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[open][not_eq]=true", "GET",
                     headers, NULL);
      t->ok("filter open not eq",
            !res->contains("\"id\": \"1\"") && res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[max_size][gt]=5", "GET", headers,
                     NULL);
      t->ok("filter max_size gt",
            res->contains("\"id\": \"1\"") && !res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[max_size][gte]=5", "GET", headers,
                     NULL);
      t->ok("filter max_size gte",
            res->contains("\"id\": \"1\"") && res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[max_size][lt]=5", "GET", headers,
                     NULL);
      t->ok("filter max_size lt",
            !res->contains("\"id\": \"1\"") && !res->contains("\"id\": \"2\""));

      res = t->fetch("/api/v1/meetings?filter[max_size][lte]=5", "GET", headers,
                     NULL);
      t->ok("filter max_size lte",
            !res->contains("\"id\": \"1\"") && res->contains("\"id\": \"2\""));

      t->ok("bad filter request",
            t->fetch("/api/v1/meetings?filter=sdfsdf", "GET", headers, NULL)
                    ->size > 0);

      t->strEqual(
          "fields",
          t->fetch("/api/v1/meetings?fields[meetings]=max_size,open", "GET",
                   headers, NULL),
          "{\"data\": [{\"type\": \"meetings\", \"id\": \"10\", "
          "\"attributes\": {\"max_size\": 10, \"open\": true}, "
          "\"relationships\": {\"teams\": {\"meta\": {\"included\": false}}}}, "
          "{\"type\": \"meetings\", \"id\": \"5\", \"attributes\": "
          "{\"max_size\": 5, \"open\": false}, \"relationships\": {\"teams\": "
          "{\"meta\": {\"included\": false}}}}], \"meta\": {}}");

      t->strEqual("bad fields request",
                  t->fetch("/api/v1/meetings?fields[meetings]=sdfsdf", "GET",
                           headers, NULL),
                  "{\"data\": [], \"meta\": {}}");

      t->strEqual(
          "bad fields request",
          t->fetch("/api/v1/teams?fields[dsfsdfhsdf]=sdfsdf", "GET", headers,
                   NULL),
          "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
          "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
          "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
          "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"2\", "
          "\"attributes\": {\"name\": \"product\"}, \"relationships\": "
          "{\"employees\": {\"meta\": {\"included\": false}}, \"meetings\": "
          "{\"meta\": {\"included\": false}}}}, {\"type\": \"teams\", \"id\": "
          "\"3\", \"attributes\": {\"name\": \"engineering\"}, "
          "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
          "false}}, \"meetings\": {\"meta\": {\"included\": false}}}}], "
          "\"meta\": {}}");

      t->ok("bad fields request",
            t->fetch("/api/v1/meetings?fields=sdfsdf", "GET", headers, NULL)
                    ->size > 0);

      t->ok("bad fields request",
            t->fetch("/api/v1/meetings?fields=[meetings][sdfsdf]=123", "GET",
                     headers, NULL)
                    ->size > 0);

      t->test("find", ^(tape_t *t) {
        t->strEqual(
            "id 1", t->fetch("/api/v1/teams/1", "GET", headers, NULL),
            "{\"data\": {\"type\": \"teams\", \"id\": \"1\", \"attributes\": "
            "{\"name\": \"design\"}, \"relationships\": {\"employees\": "
            "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
            "{\"included\": false}}}}, \"meta\": {}}");

        t->ok("bad find request",
              t->fetch("/api/v1/teams/23423", "GET", headers, NULL)->size > 0);

        t->ok("bad find request",
              t->fetch("/api/v1/teams/sdfsdkj", "GET", headers, NULL)->size >
                  0);
      });

      t->test("include", ^(tape_t *t) {
        t->test("find", ^(tape_t *t) {
          t->strEqual(
              "single related resources",
              t->fetch("/api/v1/teams/2?include=employees", "GET", headers,
                       NULL),
              "{\"data\": {\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
              "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
              "{\"data\": [{\"id\": \"1\", \"type\": \"employees\"}, {\"id\": "
              "\"2\", \"type\": \"employees\"}]}, \"meetings\": {\"meta\": "
              "{\"included\": false}}}}, \"meta\": {}, \"included\": "
              "[{\"type\": \"employees\", \"id\": \"1\", \"attributes\": "
              "{\"name\": \"Alice\", \"email\": \"alice@email.com\"}, "
              "\"relationships\": {\"teams\": {\"meta\": {\"included\": "
              "false}}, \"notes\": {\"meta\": {\"included\": false}}}}, "
              "{\"type\": \"employees\", \"id\": \"2\", \"attributes\": "
              "{\"name\": \"Bob\", \"email\": \"bob@email.com\"}, "
              "\"relationships\": {\"teams\": {\"meta\": {\"included\": "
              "false}}, \"notes\": {\"meta\": {\"included\": false}}}}]}");

          t->strEqual(
              "multiple related resources",
              t->fetch("/api/v1/teams/"
                       "2?include=employees,meetings&fields[meetings]=max_size,"
                       "max_temp",
                       "GET", headers, NULL),
              "{\"data\": {\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
              "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
              "{\"data\": [{\"id\": \"1\", \"type\": \"employees\"}, {\"id\": "
              "\"2\", \"type\": \"employees\"}]}, \"meetings\": {\"data\": "
              "[{\"id\": \"2\", \"type\": \"meetings\"}]}}}, \"meta\": {}, "
              "\"included\": [{\"type\": \"employees\", \"id\": \"1\", "
              "\"attributes\": {\"name\": \"Alice\", \"email\": "
              "\"alice@email.com\"}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}, \"notes\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"employees\", \"id\": \"2\", "
              "\"attributes\": {\"name\": \"Bob\", \"email\": "
              "\"bob@email.com\"}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}, \"notes\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"meetings\", \"id\": \"2\", "
              "\"attributes\": {\"max_size\": 5, \"max_temp\": "
              "71.323586000000006}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}}}]}");

          t->strEqual(
              "bad include request",
              t->fetch("/api/v1/teams/2?include=sdfdsfd", "GET", headers, NULL),
              "{\"data\": {\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
              "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
              "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
              "{\"included\": false}}}}, \"meta\": {}}");

          t->strEqual(
              "multiple nested related resources",
              t->fetch("/api/v1/teams/"
                       "2?include=employees.notes,meetings&fields[meetings]="
                       "max_size,"
                       "max_temp",
                       "GET", headers, NULL),
              "{\"data\": {\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
              "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
              "{\"data\": [{\"id\": \"1\", \"type\": \"employees\"}, {\"id\": "
              "\"2\", \"type\": \"employees\"}]}, \"meetings\": {\"data\": "
              "[{\"id\": \"2\", \"type\": \"meetings\"}]}}}, \"meta\": {}, "
              "\"included\": [{\"type\": \"notes\", \"id\": \"1\", "
              "\"attributes\": {\"title\": \"a\", \"date\": \"2022-03-08\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"notes\", \"id\": \"2\", \"attributes\": "
              "{\"title\": \"b\", \"date\": \"2022-03-08\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"notes\", \"id\": \"3\", \"attributes\": "
              "{\"title\": \"c\", \"date\": \"2022-03-08\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"notes\", \"id\": \"4\", \"attributes\": "
              "{\"title\": \"a\", \"date\": \"2022-03-09\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"notes\", \"id\": \"5\", \"attributes\": "
              "{\"title\": \"b\", \"date\": \"2022-03-09\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"notes\", \"id\": \"6\", \"attributes\": "
              "{\"title\": \"c\", \"date\": \"2022-03-09\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"employees\", \"id\": \"1\", "
              "\"attributes\": {\"name\": \"Alice\", \"email\": "
              "\"alice@email.com\"}, \"relationships\": {\"notes\": {\"data\": "
              "[{\"id\": \"1\", \"type\": \"notes\"}, {\"id\": \"3\", "
              "\"type\": \"notes\"}, {\"id\": \"5\", \"type\": \"notes\"}]}, "
              "\"teams\": {\"meta\": {\"included\": false}}}}, {\"type\": "
              "\"employees\", \"id\": \"2\", \"attributes\": {\"name\": "
              "\"Bob\", \"email\": \"bob@email.com\"}, \"relationships\": "
              "{\"notes\": {\"data\": [{\"id\": \"2\", \"type\": \"notes\"}, "
              "{\"id\": \"4\", \"type\": \"notes\"}, {\"id\": \"6\", \"type\": "
              "\"notes\"}]}, \"teams\": {\"meta\": {\"included\": false}}}}, "
              "{\"type\": \"meetings\", \"id\": \"2\", \"attributes\": "
              "{\"max_size\": 5, \"max_temp\": 71.323586000000006}, "
              "\"relationships\": {\"teams\": {\"meta\": {\"included\": "
              "false}}}}]}");
        });

        t->strEqual(
            "bad include request",
            t->fetch("/api/v1/teams/2?include=employees.sdfsdf", "GET", headers,
                     NULL),
            "{\"data\": {\"type\": \"teams\", \"id\": \"2\", \"attributes\": "
            "{\"name\": \"product\"}, \"relationships\": {\"employees\": "
            "{\"data\": [{\"id\": \"1\", \"type\": \"employees\"}, {\"id\": "
            "\"2\", \"type\": \"employees\"}]}, \"meetings\": {\"meta\": "
            "{\"included\": false}}}}, \"meta\": {}, \"included\": [{\"type\": "
            "\"employees\", \"id\": \"1\", \"attributes\": {\"name\": "
            "\"Alice\", \"email\": \"alice@email.com\"}, \"relationships\": "
            "{\"teams\": {\"meta\": {\"included\": false}}, \"notes\": "
            "{\"meta\": {\"included\": false}}}}, {\"type\": \"employees\", "
            "\"id\": \"2\", \"attributes\": {\"name\": \"Bob\", \"email\": "
            "\"bob@email.com\"}, \"relationships\": {\"teams\": {\"meta\": "
            "{\"included\": false}}, \"notes\": {\"meta\": {\"included\": "
            "false}}}}]}");

        t->test("all", ^(tape_t *t) {
          t->strEqual(
              "single related resources",
              t->fetch("/api/v1/teams?include=employees", "GET", headers, NULL),
              "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", "
              "\"attributes\": {\"name\": \"design\"}, \"relationships\": "
              "{\"employees\": {\"meta\": {\"included\": false}}, "
              "\"meetings\": {\"meta\": {\"included\": false}}}}, {\"type\": "
              "\"teams\", \"id\": \"2\", \"attributes\": {\"name\": "
              "\"product\"}, \"relationships\": {\"employees\": {\"data\": "
              "[{\"id\": \"1\", \"type\": \"employees\"}, {\"id\": \"2\", "
              "\"type\": \"employees\"}]}, \"meetings\": {\"meta\": "
              "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"3\", "
              "\"attributes\": {\"name\": \"engineering\"}, \"relationships\": "
              "{\"employees\": {\"meta\": {\"included\": false}}, "
              "\"meetings\": {\"meta\": {\"included\": false}}}}], \"meta\": "
              "{}, \"included\": [{\"type\": \"employees\", \"id\": \"1\", "
              "\"attributes\": {\"name\": \"Alice\", \"email\": "
              "\"alice@email.com\"}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}, \"notes\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"employees\", \"id\": \"2\", "
              "\"attributes\": {\"name\": \"Bob\", \"email\": "
              "\"bob@email.com\"}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}, \"notes\": {\"meta\": {\"included\": "
              "false}}}}]}");

          t->strEqual(
              "multiple related resources",
              t->fetch("/api/v1/"
                       "teams?include=employees,meetings&fields[meetings]=max_"
                       "size,max_temp",
                       "GET", headers, NULL),
              "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", "
              "\"attributes\": {\"name\": \"design\"}, \"relationships\": "
              "{\"employees\": {\"meta\": {\"included\": false}}, "
              "\"meetings\": {\"meta\": {\"included\": false}}}}, {\"type\": "
              "\"teams\", \"id\": \"2\", \"attributes\": {\"name\": "
              "\"product\"}, \"relationships\": {\"employees\": {\"data\": "
              "[{\"id\": \"1\", \"type\": \"employees\"}, {\"id\": \"2\", "
              "\"type\": \"employees\"}]}, \"meetings\": {\"data\": [{\"id\": "
              "\"2\", \"type\": \"meetings\"}]}}}, {\"type\": \"teams\", "
              "\"id\": \"3\", \"attributes\": {\"name\": \"engineering\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}, \"meetings\": {\"data\": [{\"id\": \"1\", \"type\": "
              "\"meetings\"}]}}}], \"meta\": {}, \"included\": [{\"type\": "
              "\"employees\", \"id\": \"1\", \"attributes\": {\"name\": "
              "\"Alice\", \"email\": \"alice@email.com\"}, \"relationships\": "
              "{\"teams\": {\"meta\": {\"included\": false}}, \"notes\": "
              "{\"meta\": {\"included\": false}}}}, {\"type\": \"employees\", "
              "\"id\": \"2\", \"attributes\": {\"name\": \"Bob\", \"email\": "
              "\"bob@email.com\"}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}, \"notes\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"meetings\", \"id\": \"1\", "
              "\"attributes\": {\"max_size\": 10, \"max_temp\": "
              "72.295615999999995}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}}}, {\"type\": \"meetings\", \"id\": "
              "\"2\", \"attributes\": {\"max_size\": 5, \"max_temp\": "
              "71.323586000000006}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}}}]}");

          t->strEqual(
              "bad include request",
              t->fetch("/api/v1/teams?include=sdfdsfd", "GET", headers, NULL),
              "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", "
              "\"attributes\": {\"name\": \"design\"}, \"relationships\": "
              "{\"employees\": {\"meta\": {\"included\": false}}, "
              "\"meetings\": {\"meta\": {\"included\": false}}}}, {\"type\": "
              "\"teams\", \"id\": \"2\", \"attributes\": {\"name\": "
              "\"product\"}, \"relationships\": {\"employees\": {\"meta\": "
              "{\"included\": false}}, \"meetings\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"teams\", \"id\": \"3\", \"attributes\": "
              "{\"name\": \"engineering\"}, \"relationships\": {\"employees\": "
              "{\"meta\": {\"included\": false}}, \"meetings\": {\"meta\": "
              "{\"included\": false}}}}], \"meta\": {}}");

          t->strEqual(
              "multiple nested related resources",
              t->fetch(
                  "/api/v1/"
                  "teams?include=employees.notes,meetings&fields[meetings]=max_"
                  "size,max_temp",
                  "GET", headers, NULL),
              "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", "
              "\"attributes\": {\"name\": \"design\"}, \"relationships\": "
              "{\"employees\": {\"meta\": {\"included\": false}}, "
              "\"meetings\": {\"meta\": {\"included\": false}}}}, {\"type\": "
              "\"teams\", \"id\": \"2\", \"attributes\": {\"name\": "
              "\"product\"}, \"relationships\": {\"employees\": {\"data\": "
              "[{\"id\": \"1\", \"type\": \"employees\"}, {\"id\": \"2\", "
              "\"type\": \"employees\"}]}, \"meetings\": {\"data\": [{\"id\": "
              "\"2\", \"type\": \"meetings\"}]}}}, {\"type\": \"teams\", "
              "\"id\": \"3\", \"attributes\": {\"name\": \"engineering\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}, \"meetings\": {\"data\": [{\"id\": \"1\", \"type\": "
              "\"meetings\"}]}}}], \"meta\": {}, \"included\": [{\"type\": "
              "\"employees\", \"id\": \"1\", \"attributes\": {\"name\": "
              "\"Alice\", \"email\": \"alice@email.com\"}, \"relationships\": "
              "{\"notes\": {\"data\": [{\"id\": \"1\", \"type\": \"notes\"}, "
              "{\"id\": \"3\", \"type\": \"notes\"}, {\"id\": \"5\", \"type\": "
              "\"notes\"}]}, \"teams\": {\"meta\": {\"included\": false}}}}, "
              "{\"type\": \"employees\", \"id\": \"2\", \"attributes\": "
              "{\"name\": \"Bob\", \"email\": \"bob@email.com\"}, "
              "\"relationships\": {\"notes\": {\"data\": [{\"id\": \"2\", "
              "\"type\": \"notes\"}, {\"id\": \"4\", \"type\": \"notes\"}, "
              "{\"id\": \"6\", \"type\": \"notes\"}]}, \"teams\": {\"meta\": "
              "{\"included\": false}}}}, {\"type\": \"notes\", \"id\": \"1\", "
              "\"attributes\": {\"title\": \"a\", \"date\": \"2022-03-08\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"notes\", \"id\": \"2\", \"attributes\": "
              "{\"title\": \"b\", \"date\": \"2022-03-08\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"notes\", \"id\": \"3\", \"attributes\": "
              "{\"title\": \"c\", \"date\": \"2022-03-08\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"notes\", \"id\": \"4\", \"attributes\": "
              "{\"title\": \"a\", \"date\": \"2022-03-09\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"notes\", \"id\": \"5\", \"attributes\": "
              "{\"title\": \"b\", \"date\": \"2022-03-09\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"notes\", \"id\": \"6\", \"attributes\": "
              "{\"title\": \"c\", \"date\": \"2022-03-09\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"meetings\", \"id\": \"1\", "
              "\"attributes\": {\"max_size\": 10, \"max_temp\": "
              "72.295615999999995}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}}}, {\"type\": \"meetings\", \"id\": "
              "\"2\", \"attributes\": {\"max_size\": 5, \"max_temp\": "
              "71.323586000000006}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}}}]}");

          t->strEqual(
              "multiple nested related resources bad request",
              t->fetch("/api/v1/"
                       "teams?include=employees.sdfsdfsdf,meetings&fields["
                       "meetings]=max_"
                       "size,max_temp",
                       "GET", headers, NULL),
              "{\"data\": [{\"type\": \"teams\", \"id\": \"1\", "
              "\"attributes\": {\"name\": \"design\"}, \"relationships\": "
              "{\"employees\": {\"meta\": {\"included\": false}}, "
              "\"meetings\": {\"meta\": {\"included\": false}}}}, {\"type\": "
              "\"teams\", \"id\": \"2\", \"attributes\": {\"name\": "
              "\"product\"}, \"relationships\": {\"employees\": {\"data\": "
              "[{\"id\": \"1\", \"type\": \"employees\"}, {\"id\": \"2\", "
              "\"type\": \"employees\"}]}, \"meetings\": {\"data\": [{\"id\": "
              "\"2\", \"type\": \"meetings\"}]}}}, {\"type\": \"teams\", "
              "\"id\": \"3\", \"attributes\": {\"name\": \"engineering\"}, "
              "\"relationships\": {\"employees\": {\"meta\": {\"included\": "
              "false}}, \"meetings\": {\"data\": [{\"id\": \"1\", \"type\": "
              "\"meetings\"}]}}}], \"meta\": {}, \"included\": [{\"type\": "
              "\"employees\", \"id\": \"1\", \"attributes\": {\"name\": "
              "\"Alice\", \"email\": \"alice@email.com\"}, \"relationships\": "
              "{\"teams\": {\"meta\": {\"included\": false}}, \"notes\": "
              "{\"meta\": {\"included\": false}}}}, {\"type\": \"employees\", "
              "\"id\": \"2\", \"attributes\": {\"name\": \"Bob\", \"email\": "
              "\"bob@email.com\"}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}, \"notes\": {\"meta\": {\"included\": "
              "false}}}}, {\"type\": \"meetings\", \"id\": \"1\", "
              "\"attributes\": {\"max_size\": 10, \"max_temp\": "
              "72.295615999999995}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}}}, {\"type\": \"meetings\", \"id\": "
              "\"2\", \"attributes\": {\"max_size\": 5, \"max_temp\": "
              "71.323586000000006}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}}}]}");

          t->strEqual(
              "multiple deep nested related resources",
              t->fetch("/api/v1/"
                       "notes?include=employees.teams.meetings&"
                       "fields[meetings]=max_"
                       "size,max_temp",
                       "GET", headers, NULL),
              "{\"data\": [{\"type\": \"notes\", \"id\": \"1\", "
              "\"attributes\": {\"title\": \"a\", \"date\": \"2022-03-08\"}, "
              "\"relationships\": {\"employees\": {\"data\": [{\"id\": \"1\", "
              "\"type\": \"employees\"}]}}}, {\"type\": \"notes\", \"id\": "
              "\"2\", \"attributes\": {\"title\": \"b\", \"date\": "
              "\"2022-03-08\"}, \"relationships\": {\"employees\": {\"data\": "
              "[{\"id\": \"2\", \"type\": \"employees\"}]}}}, {\"type\": "
              "\"notes\", \"id\": \"3\", \"attributes\": {\"title\": \"c\", "
              "\"date\": \"2022-03-08\"}, \"relationships\": {\"employees\": "
              "{\"data\": [{\"id\": \"1\", \"type\": \"employees\"}]}}}, "
              "{\"type\": \"notes\", \"id\": \"4\", \"attributes\": "
              "{\"title\": \"a\", \"date\": \"2022-03-09\"}, "
              "\"relationships\": {\"employees\": {\"data\": [{\"id\": \"2\", "
              "\"type\": \"employees\"}]}}}, {\"type\": \"notes\", \"id\": "
              "\"5\", \"attributes\": {\"title\": \"b\", \"date\": "
              "\"2022-03-09\"}, \"relationships\": {\"employees\": {\"data\": "
              "[{\"id\": \"1\", \"type\": \"employees\"}]}}}, {\"type\": "
              "\"notes\", \"id\": \"6\", \"attributes\": {\"title\": \"c\", "
              "\"date\": \"2022-03-09\"}, \"relationships\": {\"employees\": "
              "{\"data\": [{\"id\": \"2\", \"type\": \"employees\"}]}}}], "
              "\"meta\": {}, \"included\": [{\"type\": \"employees\", \"id\": "
              "\"1\", \"attributes\": {\"name\": \"Alice\", \"email\": "
              "\"alice@email.com\"}, \"relationships\": {\"teams\": {\"data\": "
              "[{\"id\": \"2\", \"type\": \"teams\"}]}, \"notes\": {\"meta\": "
              "{\"included\": false}}}}, {\"type\": \"employees\", \"id\": "
              "\"2\", \"attributes\": {\"name\": \"Bob\", \"email\": "
              "\"bob@email.com\"}, \"relationships\": {\"teams\": {\"data\": "
              "[{\"id\": \"2\", \"type\": \"teams\"}]}, \"notes\": {\"meta\": "
              "{\"included\": false}}}}, {\"type\": \"teams\", \"id\": \"1\", "
              "\"attributes\": {\"name\": \"design\"}, \"relationships\": "
              "{\"meetings\": {\"meta\": {\"included\": false}}, "
              "\"employees\": {\"meta\": {\"included\": false}}}}, {\"type\": "
              "\"teams\", \"id\": \"2\", \"attributes\": {\"name\": "
              "\"product\"}, \"relationships\": {\"meetings\": {\"data\": "
              "[{\"id\": \"2\", \"type\": \"meetings\"}]}, \"employees\": "
              "{\"meta\": {\"included\": false}}}}, {\"type\": \"meetings\", "
              "\"id\": \"2\", \"attributes\": {\"max_size\": 5, \"max_temp\": "
              "71.323586000000006}, \"relationships\": {\"teams\": {\"meta\": "
              "{\"included\": false}}}}]}");
        });
      });
    });
  });

  headers->free();
  memoryManager->free();
  pg->free();
}

#pragma clang diagnostic pop
