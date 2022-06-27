#include <string/string.h>

#define LOOPS 1000000
#define POOL_SIZE 2

typedef struct string_pool_item_t {
  string_t *string;
  struct string_pool_item_t *next;
} string_pool_item_t;

typedef struct string_pool_t {
  string_pool_item_t *head;
  dispatch_semaphore_t semaphore;
  dispatch_queue_t queue;
} string_pool_t;

string_pool_t *string_pool_create() {
  string_pool_t *pool = malloc(sizeof(string_pool_t));
  pool->semaphore = dispatch_semaphore_create(POOL_SIZE);
  pool->queue = dispatch_queue_create("string_pool", NULL);
  string_pool_item_t *item = malloc(sizeof(string_pool_item_t));
  item->string = string("");
  item->next = NULL;
  string_pool_item_t *c = item;
  for (int i = 0; i < POOL_SIZE; i++) {
    c->next = malloc(sizeof(string_pool_item_t));
    c = c->next;
    c->string = string("");
    c->next = NULL;
  }
  pool->head = item;
  return pool;
}

string_pool_item_t *borrow_string_item(string_pool_t *pool, char *str) {
  __block string_pool_item_t *current = NULL;
  dispatch_semaphore_wait(pool->semaphore, DISPATCH_TIME_FOREVER);
  dispatch_sync(pool->queue, ^{
    current = pool->head;
    pool->head = current->next;
  });

  current->string->value = strdup(str);
  current->string->size = strlen(current->string->value);
  return current;
}

void release_string_item(string_pool_t *pool, string_pool_item_t *item) {
  dispatch_sync(pool->queue, ^{
    free(item->string->value);
    item->next = pool->head;
    pool->head = item;
  });
  dispatch_semaphore_signal(pool->semaphore);
};

int main() {
  dispatch_queue_t queue =
      dispatch_queue_create("reverse", DISPATCH_QUEUE_CONCURRENT);

  size_t iterations = LOOPS;
  __block string_pool_t *pool = string_pool_create();

  printf("string block pool!\n");

  for (int i = 0; i < iterations; i++) {
    dispatch_async(queue, ^{
      string_pool_item_t *item = borrow_string_item(pool, "test");
      item->string->reverse();
      release_string_item(pool, item);
    });
  };

  dispatch_sync(queue, ^{
    printf("done!\n");
  });
}
