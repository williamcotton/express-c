#include "employee.h"
#include <model/model.h>

typedef model_t Team_t;
typedef model_instance_t team_t;
typedef model_instance_collection_t team_collection_t;

Team_t *TeamModel(request_t *req, pg_t *pg);