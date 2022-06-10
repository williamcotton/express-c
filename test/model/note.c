#include "note.h"

model_t *NoteModel(memory_manager_t *memoryManager, database_pool_t *db,
                   model_store_t *modelStore) {
  model_t *Note = CreateModel("notes", memoryManager, db, modelStore);

  Note->attribute("title", "text", NULL);
  Note->attribute("body", "text", NULL);
  Note->attribute("date", "date", NULL);

  Note->belongsTo("employees", "employee_id", NULL);

  return Note;
}
