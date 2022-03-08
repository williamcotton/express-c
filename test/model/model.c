#include "employee.h"
#include "island.h"
#include "team.h"
#include <tape/tape.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"

void setupTest(pg_t *pg) {
  PQclear(pg->exec("DROP TABLE IF EXISTS teams"));
  PQclear(pg->exec("CREATE TABLE teams (id SERIAL PRIMARY KEY, name TEXT)"));
  PQclear(pg->exec("INSERT INTO teams (name) VALUES ('design')"));
  PQclear(pg->exec("INSERT INTO teams (name) VALUES ('product')"));
  PQclear(pg->exec("INSERT INTO teams (name) VALUES ('engineering')"));

  PQclear(pg->exec("DROP TABLE IF EXISTS employees"));
  PQclear(pg->exec(
      "CREATE TABLE employees (id SERIAL PRIMARY KEY, team_id INTEGER, "
      "name TEXT, email TEXT)"));
  PQclear(pg->exec("INSERT INTO employees (team_id, name, email) VALUES (2, "
                   "'Alice', 'alice@email.com')"));
  PQclear(pg->exec("INSERT INTO employees (team_id, name, email) VALUES (2, "
                   "'Bob', 'bob@email.com')"));

  PQclear(pg->exec("DROP TABLE IF EXISTS meetings"));
  PQclear(
      pg->exec("CREATE TABLE meetings (id serial PRIMARY KEY, team_id INTEGER, "
               "max_size INTEGER, date DATE, timestamp TIMESTAMP WITH TIME "
               "ZONE, max_temp REAL, budget DECIMAL, open BOOLEAN)"));
  PQclear(pg->exec("INSERT INTO meetings (team_id, max_size, date, timestamp, "
                   "max_temp, budget, open) VALUES (3, "
                   "10, '2018-06-18', '2018-06-18 05:00:00-06', 72.295618, "
                   "85000.25, true)"));
  PQclear(pg->exec("INSERT INTO meetings (team_id, max_size, date, timestamp, "
                   "max_temp, budget, open) VALUES (3, "
                   "5, '2021-03-08', '2021-03-08 10:00:00-06', 71.3235838, "
                   "45000.20, false)"));

  PQclear(pg->exec("DROP TABLE IF EXISTS notes"));
  PQclear(pg->exec(
      "CREATE TABLE notes (id serial PRIMARY KEY, employee_id INTEGER, "
      "title TEXT, date DATE)"));
  PQclear(pg->exec("INSERT INTO notes (employee_id, title, date) VALUES (1, "
                   "'a', '2022-03-08')"));
  PQclear(pg->exec("INSERT INTO notes (employee_id, title, date) VALUES (1, "
                   "'b', '2022-03-08')"));
  PQclear(pg->exec("INSERT INTO notes (employee_id, title, date) VALUES (1, "
                   "'c', '2022-03-08')"));
  PQclear(pg->exec("INSERT INTO notes (employee_id, title, date) VALUES (1, "
                   "'a', '2022-03-09')"));
  PQclear(pg->exec("INSERT INTO notes (employee_id, title, date) VALUES (1, "
                   "'b', '2022-03-09')"));
  PQclear(pg->exec("INSERT INTO notes (employee_id, title, date) VALUES (1, "
                   "'c', '2022-03-09')"));
};

void modelTests(tape_t *t, const char *databaseUrl) {
  pg_t *pg = initPg(databaseUrl);
  memory_manager_t *memoryManager = createMemoryManager();
  pg->query = getPostgresQuery(memoryManager, pg);

  setupTest(pg);

  Team_t *Team = TeamModel(memoryManager);
  Team->setPg(pg);
  Employee_t *Employee = EmployeeModel(memoryManager);
  Employee->setPg(pg);
  Island_t *Island = IslandModel(memoryManager);
  Island->setPg(pg);

  t->test("model", ^(tape_t *t) {
    t->test("find existing", ^(tape_t *t) {
      team_t *team = Team->find("1");

      string_t *nameValue = string(team->get("name"));
      t->strEqual("name", nameValue, "design");

      nameValue->free();
    });

    t->test("find non-existing", ^(tape_t *t) {
      team_t *team = Team->find("10");

      t->ok("null", team == NULL);
    });

    t->test("get and set", ^(tape_t *t) {
      team_t *team = Team->find("1");

      char *bogus = team->get("bogus");
      t->ok("bogus is null", bogus == NULL);

      team->set("name", "development");

      string_t *nameValue = string(team->get("name"));
      t->strEqual("name updated", nameValue, "development");

      team->set("bogus", "bogus");
      bogus = team->get("bogus");
      t->ok("bogus is null", bogus == NULL);

      nameValue->free();
    });

    t->test("all", ^(tape_t *t) {
      team_collection_t *teams =
          Team->query()->where("name = $", "design")->all();

      t->ok("has a result", teams->size == 1);

      string_t *nameValue = string(teams->at(0)->get("name"));
      t->strEqual("name", nameValue, "design");

      nameValue->free();
    });

    t->test("all no results", ^(tape_t *t) {
      team_collection_t *teams =
          Team->query()->where("name = $", "bogus")->all();

      t->ok("empty", teams->size == 0);
    });

    t->test("hasMany", ^(tape_t *t) {
      team_t *team = Team->find("2");
      employee_collection_t *employees = team->r("employees");

      t->ok("employees count", employees->size == 2);

      employee_t *employeeAlice = employees->at(0);

      string_t *employeeAliceName = string(employeeAlice->get("name"));
      t->strEqual("Alice's name", employeeAliceName, "Alice");

      string_t *employeeAliceEmail = string(employeeAlice->get("email"));
      t->strEqual("Alice's email", employeeAliceEmail, "alice@email.com");

      employee_t *employeeBob = employees->at(1);

      string_t *employeeBobName = string(employeeBob->get("name"));
      t->strEqual("Bob's name", employeeBobName, "Bob");

      string_t *employeeBobEmail = string(employeeBob->get("email"));
      t->strEqual("Bob's email", employeeBobEmail, "bob@email.com");

      employee_t *employeeBogus = employees->at(2);
      t->ok("bogus", employeeBogus == NULL);

      employeeAliceName->free();
      employeeAliceEmail->free();
      employeeBobName->free();
      employeeBobEmail->free();
    });

    t->test("hasOne", ^(tape_t *t){
                // TODO: test hasOne
            });

    t->test("belongsTo", ^(tape_t *t) {
      employee_t *employee = Employee->find("1");
      team_collection_t *teams = employee->r("teams");

      team_t *team = teams->at(0);

      string_t *teamName = string(team->get("name"));
      t->strEqual("team name", teamName, "product");

      teamName->free();
    });

    t->test("incorrect relations", ^(tape_t *t) {
      employee_t *employee = Employee->find("1");
      t->ok("null", employee->r("bogus") == NULL);

      t->ok("null", employee->r("islands") == NULL);
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

    t->test("save", ^(tape_t *t) {
      t->test("existing record", ^(tape_t *t) {
        team_t *employee = Employee->find("2");
        employee->set("name", "Robert");
        employee->set("email", "robert@email.com");
        int res = employee->save();

        t->ok("updated", res == 1);

        team_t *employeeAgain = Employee->find("2");

        string_t *nameValue = string(employeeAgain->get("name"));
        t->strEqual("updated name", nameValue, "Robert");
        string_t *emailValue = string(employeeAgain->get("email"));
        t->strEqual("updated email", emailValue, "robert@email.com");

        nameValue->free();
        emailValue->free();
      });

      t->test("new record", ^(tape_t *t) {
        team_t *employee = Employee->new ();
        employee->set("name", "Charlie");
        employee->set("email", "charlie@email.com");
        employee->set("team_id", "1");
        int res = employee->save();

        t->ok("created", res == 1);

        team_t *employeeAgain = Employee->find(employee->id);

        string_t *nameValue = string(employeeAgain->get("name"));
        t->strEqual("created name", nameValue, "Charlie");
        string_t *emailValue = string(employeeAgain->get("email"));
        t->strEqual("created email", emailValue, "charlie@email.com");

        nameValue->free();
        emailValue->free();
      });

      t->test("invalid new record", ^(tape_t *t) {
        team_t *employee = Employee->new ();
        employee->set("name", "Charlie");
        employee->set("email", "charlie@email.com");
        int res = employee->save();

        t->ok("created", res == 0);

        string_t *teamAttribute = string(employee->errors->attributes[0]);
        t->strEqual("team_id error", teamAttribute, "team_id");

        string_t *teamError = string(employee->errors->messages[0]);
        t->strEqual("team_id error present", teamError, "is required");

        teamAttribute->free();
        teamError->free();
      });
    });

    t->test("destroy", ^(tape_t *t) {
      team_t *employee = Employee->find("3");
      int res = employee->destroy();

      t->ok("destroyed", res == 1);

      team_t *employeeAgain = Employee->find("3");
      t->ok("null", employeeAgain == NULL);
    });

    t->test("callbacks", ^(tape_t *t){
                // TODO: test callbacks
            });

    t->test("collection", ^(tape_t *t) {
      employee_collection_t *employees = Employee->all();

      t->test("each", ^(tape_t *t) {
        __block int i = 0;
        employees->each(^(employee_t *employee) {
          if (i == 0) {
            string_t *name = string(employee->get("name"));

            t->strEqual("first name", name, "Alice");

            name->free();
          } else if (i == 1) {
            string_t *name = string(employee->get("name"));

            t->strEqual("second name", name, "Robert");

            name->free();
          } else {
            t->ok("too many", false);
          }
          i++;
        });
      });

      t->test("eachWithIndex", ^(tape_t *t) {
        employees->eachWithIndex(^(employee_t *employee, int i) {
          if (i == 0) {
            string_t *name = string(employee->get("name"));

            t->strEqual("first name", name, "Alice");

            name->free();
          } else if (i == 1) {
            string_t *name = string(employee->get("name"));

            t->strEqual("second name", name, "Robert");

            name->free();
          } else {
            t->ok("too many", false);
          }
        });
      });

      t->test("map", ^(tape_t *t) {
        char **names = (char **)employees->map(^(employee_t *employee) {
          return (void *)employee->get("name");
        });

        string_t *firstName = string(names[0]);
        string_t *secondName = string(names[1]);

        t->strEqual("first name", firstName, "Alice");
        t->strEqual("second name", secondName, "Robert");

        firstName->free();
        secondName->free();
      });

      t->test("reduce", ^(tape_t *t) {
        string_t *names = string("");

        employees->reduce((void *)names, ^(void *accum, employee_t *e) {
          string_t *acc = (string_t *)accum;
          acc->concat(e->get("name"))->concat("+");
          return (void *)acc;
        });

        t->strEqual("names", names, "Alice+Robert+");

        names->free();
      });

      t->test("filter", ^(tape_t *t) {
        team_t *employee = Employee->new ();
        employee->set("name", "Charlie");
        employee->set("email", "charlie@email.com");
        employee->set("team_id", "1");
        employee->save();

        employee_collection_t *employees = Employee->all();
        employee_collection_t *filtered =
            employees->filter(^(employee_t *employee) {
              return strlen(employee->get("name")) > 5;
            });

        string_t *firstName = string(filtered->at(0)->get("name"));
        string_t *secondName = string(filtered->at(1)->get("name"));

        t->strEqual("first name", firstName, "Robert");
        t->strEqual("second name", secondName, "Charlie");

        firstName->free();
        secondName->free();
      });

      t->test("find", ^(tape_t *t) {
        employee_collection_t *employees = Employee->all();
        employee_t *employee = employees->find(^(employee_t *employee) {
          return strcmp(employee->get("team_id"), "1") == 0;
        });

        string_t *name = string(employee->get("name"));

        t->strEqual("name", name, "Charlie");

        name->free();
      });
    });
  });

  memoryManager->free();
  pg->free();
}

#pragma clang diagnostic pop
