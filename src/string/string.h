#include "../express.h"

typedef struct malloc_t {
  void *ptr;
} malloc_t;

struct string_t;
struct string_collection_t;
struct string_t *string(const char *str);
struct string_collection_t *stringCollection(size_t size,
                                             struct string_t **arr);
typedef void (^eachCallback)(struct string_t *string);
typedef void (^eachWithIndexCallback)(struct string_t *string, int index);
typedef void * (^reducerCallback)(void *accumulator, struct string_t *string);
typedef void * (^mapCallback)(struct string_t *string);

typedef struct string_collection_t {
  size_t size;
  struct string_t **arr;
  int mallocCount;
  malloc_t mallocs[1024];
  void * (^malloc)(size_t size);
  int blockCopyCount;
  malloc_t blockCopies[1024];
  void * (^blockCopy)(void *);
  void (^each)(eachCallback);
  void (^eachWithIndex)(eachWithIndexCallback);
  void * (^reduce)(void *accumulator, reducerCallback);
  void ** (^map)(mapCallback);
  void (^reverse)(void);
  void (^sort)(void);
  void (^free)(void);
  int (^indexOf)(const char *str);
  struct string_t * (^join)(const char *delim);
} string_collection_t;

typedef struct string_t {
  char *str;
  size_t size;
  int blockCopyCount;
  malloc_t blockCopies[1024];
  void * (^blockCopy)(void *);
  void (^print)(void);
  struct string_t * (^concat)(const char *str);
  struct string_t * (^upcase)(void);
  struct string_t * (^downcase)(void);
  struct string_t * (^capitalize)(void);
  struct string_t * (^reverse)(void);
  struct string_t * (^trim)(void);
  string_collection_t * (^split)(const char *delim);
  struct string_t * (^replace)(const char *str1, const char *str2);
  struct string_t * (^chomp)(void);
  struct string_t * (^slice)(size_t start, size_t length);
  struct string_t * (^delete)(const char *str);
  int (^indexOf)(const char *str);
  int (^lastIndexOf)(const char *str);
  int (^eql)(const char *str);
  int (^to_i)(void);
  void (^free)(void);
} string_t;