
#include <memory-manager/memory-manager.h>
#include <string/string.h>

void _Block_use_GC(void *(*alloc)(const unsigned long, const bool isOne,
                                  const bool isObject),
                   void (*setHasRefcount)(const void *, const bool),
                   void (*gc_assign_strong)(void *, void **),
                   void (*gc_assign_weak)(const void *, void *),
                   void (*gc_memmove)(void *, void *, unsigned long));

void _Block_use_GC(void *(*alloc)(size_t, const bool isOne,
                                  const bool isObject),
                   void (*setHasRefcount)(const void *, const bool),
                   void (*gc_assign)(void *, void **),
                   void (*gc_assign_weak)(const void *, void *),
                   void (*gc_memmove)(void *, void *, unsigned long));

static void *_Block_alloc_default(size_t size, const bool initialCountIsOne,
                                  const bool isObject) {
  (void)initialCountIsOne;
  (void)isObject;
  return malloc(size);
}

static void _Block_assign_default(void *value, void **destptr) {
  *destptr = value;
}

static void _Block_setHasRefcount_default(const void *ptr,
                                          const bool hasRefcount) {
  (void)ptr;
  (void)hasRefcount;
}

static void _Block_retain_object_default(const void *ptr) { (void)ptr; }

static void _Block_release_object_default(const void *ptr) { (void)ptr; }

static void _Block_assign_weak_default(const void *ptr, void *dest) {
#if !defined(_WIN32)
  *(long *)dest = (long)ptr;
#else
  *(void **)dest = (void *)ptr;
#endif
}

static void _Block_memmove_default(void *dst, void *src, unsigned long size) {
  memmove(dst, src, (size_t)size);
}

static void _Block_destructInstance_default(const void *aBlock) {
  (void)aBlock;
}

static void *(*_Block_allocator)(size_t, const bool isOne,
                                 const bool isObject) = _Block_alloc_default;
static void (*_Block_deallocator)(const void *) = (void (*)(const void *))free;
static void (*_Block_assign)(void *value,
                             void **destptr) = _Block_assign_default;
static void (*_Block_setHasRefcount)(const void *ptr, const bool hasRefcount) =
    _Block_setHasRefcount_default;
static void (*_Block_retain_object)(const void *ptr) =
    _Block_retain_object_default;
static void (*_Block_release_object)(const void *ptr) =
    _Block_release_object_default;
static void (*_Block_assign_weak)(const void *dest,
                                  void *ptr) = _Block_assign_weak_default;
static void (*_Block_memmove)(void *dest, void *src,
                              unsigned long size) = _Block_memmove_default;
static void (*_Block_destructInstance)(const void *aBlock) =
    _Block_destructInstance_default;

#define HAVE_OBJC 1

static memory_manager_t *memoryManager;

void *allocM(size_t size, const bool isOne, const bool isObject) {
  return mmMalloc(memoryManager, size);
}

// static void *_Block_alloc_default(size_t size, const bool initialCountIsOne,
//                                   const bool isObject) {
//   (void)initialCountIsOne;
//   (void)isObject;
//   mmMalloc(memoryManager, size);
// }

// static void *(*_Block_allocator)(size_t, const bool isOne,
//                                  const bool isObject) = _Block_alloc_default;

#define LOOPS 1000000

int main() {
  memoryManager = createMemoryManager();
  _Block_use_GC(allocM, _Block_setHasRefcount, _Block_assign,
                _Block_assign_weak, _Block_memmove);
  printf("string block!\n");
  for (int i = 0; i < LOOPS; i++) {
    string_t *test = string("test");
    test->reverse();
    test->free();
  }
}
