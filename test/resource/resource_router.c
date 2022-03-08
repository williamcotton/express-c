#include "../model/employee.h"
#include "../model/meeting.h"
#include "../model/note.h"
#include "../model/team.h"
#include "employee.h"
#include "meeting.h"
#include "note.h"
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

    Team_t *TeamM = TeamModel(req->memoryManager);
    Employee_t *EmployeeM = EmployeeModel(req->memoryManager);
    Meeting_t *MeetingM = MeetingModel(req->memoryManager);
    Note_t *NoteM = NoteModel(req->memoryManager);

    TeamM->setPg(pg);
    EmployeeM->setPg(pg);
    MeetingM->setPg(pg);
    NoteM->setPg(pg);

    resource_t *Team = TeamResource(TeamM);
    resource_t *Employee = EmployeeResource(EmployeeM);
    resource_t *Meeting = MeetingResource(MeetingM);
    resource_t *Note = NoteResource(NoteM);

    req->mSet("Team", Team);
    req->mSet("Employee", Employee);
    req->mSet("Meeting", Meeting);
    req->mSet("Note", Note);

    cleanup(Block_copy(^(UNUSED request_t *finishedReq){

    }));

    next();
  });

  router->get("/teams", ^(request_t *req, UNUSED response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Team = req->m("Team");
    resource_instance_collection_t *teams = Team->all(jsonapi->params);

    jsonapi->send(teams->toJSONAPI());
  });

  router->get("/teams/:id", ^(request_t *req, UNUSED response_t *res) {
    UNUSED jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Team = req->m("Team");
    resource_instance_t *team = Team->find(jsonapi->params, req->params("id"));

    jsonapi->send(team->toJSONAPI());
  });

  // TODO: router->post("/team", ^(request_t *req, UNUSED response_t *res) {
  // TODO: router->patch("/team", ^(request_t *req, UNUSED response_t *res) {
  // TODO: router->delete("/team", ^(request_t *req, UNUSED response_t *res) {

  router->get("/meetings", ^(request_t *req, UNUSED response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Meeting = req->m("Meeting");
    resource_instance_collection_t *meetings = Meeting->all(jsonapi->params);

    jsonapi->send(meetings->toJSONAPI());
  });

  router->get("/notes", ^(request_t *req, UNUSED response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Note = req->m("Note");
    resource_instance_collection_t *notes = Note->all(jsonapi->params);

    jsonapi->send(notes->toJSONAPI());
  });

  router->cleanup(Block_copy(^{
    postgres->free();
  }));

  return router;
}