#include <model/model.h>

typedef model_t Employee_t;
typedef model_instance_t employee_t;
typedef model_instance_collection_t employee_collection_t;

model_t *EmployeeModel(request_t *req, pg_t *pg);
