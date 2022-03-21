#include "models.h"

model_t *NoteModel(memory_manager_t *memoryManager, pg_t *pg,
                   model_store_t *modelStore) {
  model_t *Note = CreateModel("notes", memoryManager, pg, modelStore);

  Note->attribute("notable_id", "integer", NULL);
  Note->attribute("notable_type", "varchar(255)", NULL);
  Note->attribute("body", "text", NULL);
  Note->attribute("created_at", "timestamp", NULL);
  Note->attribute("updated_at", "timestamp", NULL);

  // Note->belongsTo("notable", "notable_id", "notable_type", NULL);

  return Note;
}
