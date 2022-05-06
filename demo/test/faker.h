#include <string/string.h>

typedef struct faker_t {
  string_collection_t *firstNames;
  string_collection_t *lastNames;
  char * (^firstName)(void);
  char * (^lastName)(void);
  char * (^randomInteger)(int min, int max);
} faker_t;

faker_t *createFaker();