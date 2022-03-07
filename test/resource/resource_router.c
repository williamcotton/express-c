#include "../model/employee.h"
#include "../model/team.h"
#include "employee.h"
#include "team.h"
#include <express.h>
#include <jansson.h>
#include <middleware/jansson-jsonapi-middleware.h>
#include <stdlib.h>

router_t *resourceRouter(const char *pgUri, int poolSize) {
  router_t *router = expressRouter();

  postgres_connection_t *postgres = initPostgressConnection(pgUri, poolSize);

  router->use(postgresMiddlewareFactory(postgres));

  router->use(janssonJsonapiMiddleware("/api/v1"));

  router->use(^(request_t *req, UNUSED response_t *res, void (^next)(),
                void (^cleanup)(cleanupHandler)) {
    pg_t *pg = req->m("pg");

    resource_t *Team = TeamResource(req, TeamModel(req->memoryManager, pg));
    resource_t *Employee =
        EmployeeResource(req, EmployeeModel(req->memoryManager, pg));

    req->mSet("Team", Team);
    req->mSet("Employee", Employee);

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){

    }));

    next();
  });

  router->get("/teams", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Team = req->m("Team");
    UNUSED resource_instance_collection_t *teams = Team->all(jsonapi->params);

    res->send("ok");
  });

  router->get("/teams/1", ^(request_t *req, response_t *res) {
    UNUSED jsonapi_t *jsonapi = req->m("jsonapi");

    res->send("ok");
  });

  router->cleanup(Block_copy(^{
    postgres->free();
  }));

  return router;
}