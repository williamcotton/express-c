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

  router->use(janssonJsonapiMiddleware());

  router->use(^(request_t *req, UNUSED response_t *res, void (^next)(),
                void (^cleanup)(cleanupHandler)) {
    pg_t *pg = req->m("pg");

    Team_t *TeamM = TeamModel(req->memoryManager, pg);
    Employee_t *EmployeeM = EmployeeModel(req->memoryManager, pg);

    resource_t *Team = TeamResource(req, TeamM);
    resource_t *Employee = EmployeeResource(req, EmployeeM);

    req->mSet("Team", Team);
    req->mSet("Employee", Employee);

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){
        //
    }));

    next();
  });

  router->get("/teams", ^(UNUSED request_t *req, response_t *res) {
    UNUSED jsonapi_t *jsonapi = req->m("jsonapi");

    res->send("ok");
  });

  router->get("/teams/1", ^(UNUSED request_t *req, response_t *res) {
    UNUSED jsonapi_t *jsonapi = req->m("jsonapi");

    res->send("ok");
  });

  return router;
}