#include <model/model.h>

typedef model_t Meeting_t;
typedef model_instance_t meeting_t;
typedef model_instance_collection_t meeting_collection_t;

model_t *MeetingModel(memory_manager_t *memoryManager, pg_t *pg,
                      model_store_t *modelStore);
