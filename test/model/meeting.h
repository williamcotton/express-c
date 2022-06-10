#include <model/model.h>

typedef model_t Meeting_t;
typedef model_instance_t meeting_t;
typedef model_instance_collection_t meeting_collection_t;

model_t *MeetingModel(memory_manager_t *memoryManager, database_pool_t *db,
                      model_store_t *modelStore);
