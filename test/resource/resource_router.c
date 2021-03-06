#include "../model/employee.h"
#include "../model/island.h"
#include "../model/meeting.h"
#include "../model/note.h"
#include "../model/team.h"
#include "employee.h"
#include "island.h"
#include "meeting.h"
#include "note.h"
#include "team.h"
#include <express.h>
#include <jansson.h>
#include <middleware/jansson-jsonapi-middleware.h>
#include <middleware/resource-middleware.h>
#include <stdlib.h>

router_t *resourceRouter(const char *pgUri, int poolSize) {
  router_t *router = expressRouter();

  database_pool_t *db = createPostgresPool(pgUri, poolSize);

  // model_store_t *modelStore = createModelStore(router->memoryManager);
  // resource_store_t *resourceStore =
  // createResourceStore(router->memoryManager);

  // model_t *islandModel = IslandModel(router->memoryManager, db, modelStore);
  // __block resource_t *Island = IslandResource(islandModel, resourceStore);

  /* jsonapi middleware */
  router->use(janssonJsonapiMiddleware("/api/v1"));

  /* resource middleware */
  resource_library_t *resourceLibrary =
      initResourceLibrary(router->memoryManager, db);
  resourceLibrary->add("Team", TeamModel, TeamResource);
  resourceLibrary->add("Employee", EmployeeModel, EmployeeResource);
  resourceLibrary->add("Meeting", MeetingModel, MeetingResource);
  resourceLibrary->add("Note", NoteModel, NoteResource);
  resourceLibrary->add("Island", IslandModel, IslandResource);
  router->use(resourceMiddleware(resourceLibrary, db));

  router->get("/teams", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Team = req->m("Team");
    resource_instance_collection_t *teams = Team->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(teams));
  });

  router->get("/teams/:id", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Team = req->m("Team");
    resource_instance_t *team = Team->find(jsonapi->params, req->params("id"));

    check(team, "Team not found");

    res->s("jsonapi", toJSONAPI(team));
  error:
    res->send("404"); // TODO: jsonapi error
  });

  // TODO: router->post("/team", ^(request_t *req, UNUSED response_t *res) {
  // TODO: router->patch("/team", ^(request_t *req, UNUSED response_t *res) {
  // TODO: router->delete("/team", ^(request_t *req, UNUSED response_t *res) {

  router->get("/meetings", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Meeting = req->m("Meeting");
    resource_instance_collection_t *meetings = Meeting->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(meetings));
  });

  router->get("/notes", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Note = req->m("Note");
    resource_instance_collection_t *notes = Note->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(notes));
  });

  router->get("/islands", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Island = req->m("Island");

    resource_instance_collection_t *islands = Island->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(islands));
  });

  router->cleanup(Block_copy(^{
    db->free();
  }));

  return router;
}
