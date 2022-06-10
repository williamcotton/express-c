#include <model/model.h>

typedef model_t Employee_t;
typedef model_instance_t employee_t;
typedef model_instance_collection_t employee_collection_t;

model_t *EmployeeModel(memory_manager_t *memoryManager, database_pool_t *db,
                       model_store_t *modelStore);
