#include "./models/models.h"
#include "./test/faker.h"
#include <stdio.h>

int main() {
  char *DATABASE_URL = getenv("DATABASE_URL");

  const char *databaseUrl =
      DATABASE_URL ? DATABASE_URL : "postgresql://localhost/express-demo";

  memory_manager_t *memoryManager = createMemoryManager();

  database_pool_t *db = createPostgresPool(databaseUrl, 10);

  pg_t *pg = initPg(databaseUrl);
  pg->query = getPostgresDBQuery(memoryManager, db);

  model_store_t *modelStore = createModelStore(memoryManager);

  model_t *Team = TeamModel(memoryManager, pg, modelStore);
  model_t *Employee = EmployeeModel(memoryManager, pg, modelStore);

  faker_t *faker = createFaker();

  for (int i = 0; i < 10; i++) {
    model_instance_t *employee = Employee->new ();
    employee->set("first_name", faker->firstName());
    employee->set("last_name", faker->lastName());
    employee->set("age", faker->randomInteger(18, 65));
    employee->save();
  }

  mmFree(memoryManager);

  db->free();
};
