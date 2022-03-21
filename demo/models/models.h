#include <model/model.h>

model_t *DepartmentModel(memory_manager_t *memoryManager, pg_t *pg,
                         model_store_t *modelStore);
model_t *EmployeeModel(memory_manager_t *memoryManager, pg_t *pg,
                       model_store_t *modelStore);
model_t *MilestoneModel(memory_manager_t *memoryManager, pg_t *pg,
                        model_store_t *modelStore);
model_t *NoteModel(memory_manager_t *memoryManager, pg_t *pg,
                   model_store_t *modelStore);
model_t *PositionModel(memory_manager_t *memoryManager, pg_t *pg,
                       model_store_t *modelStore);
model_t *TaskModel(memory_manager_t *memoryManager, pg_t *pg,
                   model_store_t *modelStore);
model_t *TeamMembershipModel(memory_manager_t *memoryManager, pg_t *pg,
                             model_store_t *modelStore);
model_t *TeamModel(memory_manager_t *memoryManager, pg_t *pg,
                   model_store_t *modelStore);
