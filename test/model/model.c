#include "employee.h"
#include "team.h"
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"

static void setupTest(pg_t *pg) {
  PQclear(pg->exec("DROP TABLE IF EXISTS teams"));
  PQclear(pg->exec("CREATE TABLE teams (id serial PRIMARY KEY, name text)"));
  PQclear(pg->exec("INSERT INTO teams (name) VALUES ('design')"));
  PQclear(pg->exec("INSERT INTO teams (name) VALUES ('product')"));
  PQclear(pg->exec("INSERT INTO teams (name) VALUES ('engineering')"));

  PQclear(pg->exec("DROP TABLE IF EXISTS employees"));
  PQclear(
      pg->exec("CREATE TABLE employees (id serial PRIMARY KEY, team_id int, "
               "name text, email text)"));
  PQclear(pg->exec("INSERT INTO employees (team_id, name, email) VALUES (2, "
                   "'Steve', 'steve@email.com')"));
};

void modelTests(tape_t *t, request_t *req, pg_t *pg) {
  setupTest(pg);
  Team_t *Team = TeamModel(req, pg);
  Employee_t *Employee = EmployeeModel(req, pg);

  t->test("model", ^(tape_t *t) {
    t->test("find", ^(tape_t *t) {
      team_t *team = Team->find("1");

      string_t *nameValue = string(team->get("name"));
      t->strEqual("name", nameValue, "design");

      nameValue->free();
    });

    t->test("related", ^(tape_t *t) {
      team_t *team = Team->find("2");
      employee_collection_t *employees = team->r("employees");

      t->ok("employees count", employees->size == 1);

      employee_t *employee = employees->at(0);

      string_t *employeeName = string(employee->get("name"));
      t->strEqual("employee name", employeeName, "Steve");

      string_t *employeeEmail = string(employee->get("email"));
      t->strEqual("employee name", employeeEmail, "steve@email.com");

      employeeName->free();
      employeeEmail->free();
    });

    t->test("validations", ^(tape_t *t) {
      team_t *team = Team->new ();

      t->test("without name", ^(tape_t *t) {
        team->validate();

        t->ok("invalid", !team->isValid());
        t->ok("one error", team->errors->count == 1);

        string_t *nameAttribute = string(team->errors->attributes[0]);
        t->strEqual("name error", nameAttribute, "name");

        string_t *nameError = string(team->errors->messages[0]);
        t->strEqual("name error present", nameError, "is required");

        nameAttribute->free();
        nameError->free();
      });

      t->test("with short name", ^(tape_t *t) {
        team->set("name", "te");
        team->validate();

        t->ok("invalid", !team->isValid());
        t->ok("one error", team->errors->count == 1);

        string_t *nameAttribute = string(team->errors->attributes[0]);
        t->strEqual("name error", nameAttribute, "name");

        string_t *nameError = string(team->errors->messages[0]);
        t->strEqual("name error length", nameError,
                    "must be at least 3 characters long");

        nameAttribute->free();
        nameError->free();
      });

      t->test("with name", ^(tape_t *t) {
        team->set("name", "team");
        team->validate();

        t->ok("valid", team->isValid());
        t->ok("no errors", team->errors->count == 0);
      });
    });
  });
}

#pragma clang diagnostic pop
