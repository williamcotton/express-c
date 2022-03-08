#include "note.h"

resource_t *NoteResource(model_t *model) {
  resource_t *Note = CreateResource("notes", model);

  Note->attribute("title", "string", NULL);
  Note->attribute("date", "date", NULL);

  Note->belongsTo("employees", "employee_id");

  return Note;
}
