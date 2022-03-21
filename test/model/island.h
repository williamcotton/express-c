#include <model/model.h>

typedef model_t Island_t;
typedef model_instance_t island_t;
typedef model_instance_collection_t island_collection_t;

Island_t *IslandModel(memory_manager_t *memoryManager, pg_t *pg,
                      model_store_t *modelStore);
