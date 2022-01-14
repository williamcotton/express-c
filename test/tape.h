#define UNUSED __attribute__((unused))

typedef struct tape_t
{
  char *name;
  int count;
  int failed;
  int (^ok)(char *, int);
  int (^strEqual)(char *, char *, char *);
  int (^test)(char *, void (^)(struct tape_t *));
} tape_t;

tape_t tape();
