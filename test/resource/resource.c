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

  Team_t *TeamM = TeamModel(memoryManager, pg);
  Employee_t *EmployeeM = EmployeeModel(memoryManager, pg);

  UNUSED resource_t *Team = TeamResource(req, TeamM);
  UNUSED resource_t *Employee = EmployeeResource(req, EmployeeM);

  string_collection_t *headers = stringCollection(0, NULL);
  string_t *contentType = string("Content-Type: application/vnd.api+json");
  headers->push(contentType);

  t->test("resource", ^(tape_t *t) {
    t->ok("true", true);

    t->test("all", ^(tape_t *t) {
      t->strEqual("with jsonapi header",
                  t->fetch("/api/v1/teams", "GET", headers, NULL), "ok");
    });

    t->test("find", ^(tape_t *t) {
      t->strEqual("with jsonapi header",
                  t->fetch("/api/v1/teams/1", "GET", headers, NULL), "ok");
    });
  });

  headers->free();
  memoryManager->free();
  pg->free();
}

#pragma clang diagnostic pop
