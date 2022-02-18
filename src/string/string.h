#include "../express.h"

#define NUMBER_ERROR_NO_DIGITS 1
#define NUMBER_ERROR_UNDERFLOW 2
#define NUMBER_ERROR_OVERFLOW 3
#define NUMBER_ERROR_BASE_UNSUPPORTED 4
#define NUMBER_ERROR_UNSPECIFIED 5
#define NUMBER_ERROR_ADDITIONAL_CHARACTERS 6

typedef struct malloc_t {
  void *ptr;
} malloc_t;

struct string_t;
struct string_collection_t;
struct number_t;

struct string_t *string(const char *str);
struct string_collection_t *stringCollection(size_t size,
                                             struct string_t **arr);
typedef void (^eachCallback)(struct string_t *string);
typedef void (^eachWithIndexCallback)(struct string_t *string, int index);
typedef void * (^reducerCallback)(void *accumulator, struct string_t *string);
typedef void * (^mapCallback)(struct string_t *string);

typedef struct integer_number_t {
  long long value;
  int error;
} integer_number_t;

typedef struct decimal_number_t {
  long double value;
  int error;
} decimal_number_t;

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
  void (^free)(void);
  int (^indexOf)(const char *str);
  struct string_collection_t * (^reverse)(void);
  struct string_collection_t * (^sort)(void);
  struct string_collection_t * (^push)(struct string_t *string);
  struct string_t * (^join)(const char *delim);
} string_collection_t;

typedef struct string_t {
  char *value;
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
  struct string_t * (^replace)(const char *str1, const char *str2);
  struct string_t * (^chomp)(void);
  struct string_t * (^slice)(size_t start, size_t length);
  struct string_t * (^delete)(const char *str);
  string_collection_t * (^split)(const char *delim);
  string_collection_t * (^matchGroup)(const char *regex);
  int (^indexOf)(const char *str);
  int (^lastIndexOf)(const char *str);
  int (^eql)(const char *str);
  integer_number_t (^toInt)(void);
  decimal_number_t (^toDecimal)(void);
  void (^free)(void);
} string_t;