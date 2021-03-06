/*
  Copyright (c) 2022 William Cotton

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include "memory-manager.h"
#include <stdio.h>

void *mmMalloc(memory_manager_t *memoryManager, size_t size) {
  if (memoryManager->mallocCount >= memoryManager->maxMallocCount) {
    memoryManager->maxMallocCount *= 2;
    memoryManager->mallocs =
        realloc(memoryManager->mallocs, sizeof(memory_manager_malloc_t) *
                                            memoryManager->maxMallocCount);
  }
  void *ptr = malloc(size);
  if (ptr == NULL) {
    return NULL;
  }
  memoryManager->mallocs[memoryManager->mallocCount++] =
      (memory_manager_malloc_t){.ptr = ptr};
  return ptr;
}

void *mmRealloc(memory_manager_t *memoryManager, void *ptr, size_t size) {
  void *newPtr = realloc(ptr, size);
  for (int i = 0; i < memoryManager->mallocCount; i++) {
    if (memoryManager->mallocs[i].ptr == ptr) {
      memoryManager->mallocs[i].ptr = newPtr;
      break;
    }
  }
  return newPtr;
}

void *mmBlockCopy(memory_manager_t *memoryManager, void *block) {
  if (memoryManager->blockCopyCount >= memoryManager->maxBlockCopyCount) {
    memoryManager->maxBlockCopyCount *= 2;
    memoryManager->blockCopies = realloc(memoryManager->blockCopies,
                                         sizeof(memory_manager_block_copy_t) *
                                             memoryManager->maxBlockCopyCount);
  }
  void *ptr = Block_copy(block);
  if (ptr == NULL) {
    return NULL;
  }
  memoryManager->blockCopies[memoryManager->blockCopyCount++] =
      (memory_manager_block_copy_t){.ptr = ptr};
  return ptr;
}

void mmCleanup(memory_manager_t *memoryManager,
               memoryManagerCleanupHandler handler) {
  if (memoryManager->cleanupHandlersCount >=
      memoryManager->maxCleanupHandlersCount) {
    memoryManager->maxCleanupHandlersCount *= 2;
    memoryManager->cleanupHandlers =
        realloc(memoryManager->cleanupHandlers,
                sizeof(memoryManagerCleanupHandler) *
                    memoryManager->maxCleanupHandlersCount);
  }
  memoryManager->cleanupHandlers[memoryManager->cleanupHandlersCount++] =
      handler;
}

void mmFree(memory_manager_t *memoryManager) {
  for (int i = 0; i < memoryManager->cleanupHandlersCount; i++) {
    memoryManager->cleanupHandlers[i]();
  }

  for (int i = 0; i < memoryManager->mallocCount; i++) {
    free(memoryManager->mallocs[i].ptr);
  }

  for (int i = 0; i < memoryManager->blockCopyCount; i++) {
    Block_release(memoryManager->blockCopies[i].ptr);
  }

  free(memoryManager->mallocs);
  free(memoryManager->blockCopies);
  free(memoryManager->cleanupHandlers);

  dispatch_async(dispatch_get_main_queue(), ^() {
    free(memoryManager);
  });
}

memory_manager_t *createMemoryManager() {
  memory_manager_t *memoryManager = malloc(sizeof(memory_manager_t));
  memoryManager->mallocCount = 0;
  memoryManager->maxMallocCount = 1024;
  memoryManager->blockCopyCount = 0;
  memoryManager->maxBlockCopyCount = 8192;
  memoryManager->cleanupHandlersCount = 0;
  memoryManager->maxCleanupHandlersCount = 1024;

  memoryManager->mallocs =
      malloc(sizeof(memory_manager_malloc_t) * memoryManager->maxMallocCount);
  memoryManager->blockCopies = malloc(sizeof(memory_manager_block_copy_t) *
                                      memoryManager->maxBlockCopyCount);
  memoryManager->cleanupHandlers =
      malloc(sizeof(memoryManagerCleanupHandler) *
             memoryManager->maxCleanupHandlersCount);

  return memoryManager;
}
