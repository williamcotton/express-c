#include "../../models/models.h"
#include "../../resources/resources.h"
#include <express.h>
#include <jansson.h>
#include <middleware/jansson-jsonapi-middleware.h>
#include <middleware/resource-middleware.h>
#include <stdlib.h>

router_t *resourceRouter(const char *pgUri, int poolSize) {
  router_t *router = expressRouter();

  database_pool_t *db = createPostgresPool(pgUri, poolSize);

  /* jsonapi middleware */
  router->use(janssonJsonapiMiddleware("/api/v1"));

  /* resource middleware */
  resource_library_t *resourceLibrary =
      initResourceLibrary(router->memoryManager, db);
  resourceLibrary->add("Department", DepartmentModel, DepartmentResource);
  resourceLibrary->add("Employee", EmployeeModel, EmployeeResource);
  resourceLibrary->add("Milestone", MilestoneModel, MilestoneResource);
  resourceLibrary->add("Note", NoteModel, NoteResource);
  resourceLibrary->add("Position", PositionModel, PositionResource);
  resourceLibrary->add("Task", TaskModel, TaskResource);
  resourceLibrary->add("TeamMembership", TeamMembershipModel,
                       TeamMembershipResource);
  resourceLibrary->add("Team", TeamModel, TeamResource);

  router->use(resourceMiddleware(resourceLibrary, db));

  router->get("departments", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Department = req->m("Department");
    resource_instance_collection_t *departments =
        Department->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(departments));
  });

  router->get("departments/:id", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Department = req->m("Department");
    resource_instance_t *department =
        Department->find(jsonapi->params, req->params("id"));

    check(department, "Department not found");

    res->s("jsonapi", toJSONAPI(department));
  error:
    res->send("404"); // TODO: jsonapi error
  });

  router->get("/employees", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Employee = req->m("Employee");
    resource_instance_collection_t *employees = Employee->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(employees));
  });

  router->get("/employees/:id", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Employee = req->m("Employee");
    resource_instance_t *employee =
        Employee->find(jsonapi->params, req->params("id"));

    check(employee, "Employee not found");

    res->s("jsonapi", toJSONAPI(employee));
  error:
    res->send("404"); // TODO: jsonapi error
  });

  router->get("/milestones", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Milestone = req->m("Milestone");
    resource_instance_collection_t *milestones =
        Milestone->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(milestones));
  });

  router->get("/milestones/:id", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Milestone = req->m("Milestone");
    resource_instance_t *milestone =
        Milestone->find(jsonapi->params, req->params("id"));

    check(milestone, "Milestone not found");

    res->s("jsonapi", toJSONAPI(milestone));
  error:
    res->send("404"); // TODO: jsonapi error
  });

  router->get("/notes", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Note = req->m("Note");
    resource_instance_collection_t *notes = Note->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(notes));
  });

  router->get("/notes/:id", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Note = req->m("Note");
    resource_instance_t *note = Note->find(jsonapi->params, req->params("id"));

    check(note, "Note not found");

    res->s("jsonapi", toJSONAPI(note));
  error:
    res->send("404"); // TODO: jsonapi error
  });

  router->get("/positions", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Position = req->m("Position");
    resource_instance_collection_t *positions = Position->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(positions));
  });

  router->get("/positions/:id", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Position = req->m("Position");
    resource_instance_t *position =
        Position->find(jsonapi->params, req->params("id"));

    check(position, "Position not found");

    res->s("jsonapi", toJSONAPI(position));
  error:
    res->send("404"); // TODO: jsonapi error
  });

  router->get("/tasks", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Task = req->m("Task");
    resource_instance_collection_t *tasks = Task->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(tasks));
  });

  router->get("/tasks/:id", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *Task = req->m("Task");
    resource_instance_t *task = Task->find(jsonapi->params, req->params("id"));

    check(task, "Task not found");

    res->s("jsonapi", toJSONAPI(task));
  error:
    res->send("404"); // TODO: jsonapi error
  });

  router->get("/team-memberships", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *TeamMembership = req->m("TeamMembership");
    resource_instance_collection_t *teamMemberships =
        TeamMembership->all(jsonapi->params);

    res->s("jsonapi", toJSONAPI(teamMemberships));
  });

  router->get("/team-memberships/:id", ^(request_t *req, response_t *res) {
    jsonapi_t *jsonapi = req->m("jsonapi");

    resource_t *TeamMembership = req->m("TeamMembership");
    resource_instance_t *teamMembership =
        TeamMembership->find(jsonapi->params, req->params("id"));

    check(teamMembership, "TeamMembership not found");

    res->s("jsonapi", toJSONAPI(teamMembership));
  error:
    res->send("404"); // TODO: jsonapi error
  });

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

  router->cleanup(Block_copy(^{
    debug("cleanup");
    db->free();
    // postgres->free();
  }));

  return router;
}
