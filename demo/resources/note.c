#include "resources.h"

resource_t *NoteResource(model_t *model) {
  resource_t *Note = CreateResource("notes", model);

  Note->attribute("notable_id", "integer", NULL);
  Note->attribute("notable_type", "string", NULL);
  Note->attribute("body", "string", NULL);
  Note->attribute("created_at", "datetime", NULL);
  Note->attribute("updated_at", "datetime", NULL);

  // Note->belongsTo("notable", "notable_id", "notable_type", NULL);

  return Note;
}
