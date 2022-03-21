#include "note.h"

resource_t *NoteResource(model_t *model, resource_store_t *resourceStore) {
  resource_t *Note = CreateResource("notes", model, resourceStore);

  Note->attribute("title", "string", NULL);
  Note->attribute("date", "date", NULL);

  Note->belongsTo("employees", NULL);

  return Note;
}
