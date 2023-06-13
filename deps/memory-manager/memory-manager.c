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
#include <sys/mman.h>
#include <unistd.h>

unsigned int roundTo8(unsigned int value) { return (value + 7) & ~7; }

void *mmMalloc(memory_manager_t *memoryManager, size_t size) {
  void *ptr = memoryManager->freePtr;
  memoryManager->freePtr += roundTo8(size);
  if (memoryManager->freePtr > memoryManager->endPtr) {
    return NULL;
  }
  memset(ptr, 0, size);
  return ptr;
}

void *mmRealloc(memory_manager_t *memoryManager, void *ptr, size_t size) {
  void *newPtr = mmMalloc(memoryManager, size);
  if (newPtr) {
    memmove(newPtr, ptr, size);
  }
  return newPtr;
}

void *mmBlockCopy(memory_manager_t *memoryManager, void *block) {
  if (!block) {
    return NULL;
  }
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
    if (memoryManager->cleanupHandlers[i]) {
      memoryManager->cleanupHandlers[i]();
    }
  }

  for (int i = 0; i < memoryManager->blockCopyCount; i++) {
    Block_release(memoryManager->blockCopies[i].ptr);
  }

  munmap(memoryManager->startPtr, HEAP_SIZE);

  free(memoryManager->blockCopies);
  free(memoryManager->cleanupHandlers);

  dispatch_async(dispatch_get_main_queue(), ^() {
    free(memoryManager);
  });
}

memory_manager_t *createMemoryManager() {
  memory_manager_t *memoryManager = malloc(sizeof(memory_manager_t));

  memoryManager->freePtr = mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memoryManager->startPtr = memoryManager->freePtr;
  memoryManager->endPtr = memoryManager->startPtr + HEAP_SIZE;

  memoryManager->blockCopyCount = 0;
  memoryManager->maxBlockCopyCount = 8192;
  memoryManager->cleanupHandlersCount = 0;
  memoryManager->maxCleanupHandlersCount = 1024;

  memoryManager->blockCopies = malloc(sizeof(memory_manager_block_copy_t) *
                                      memoryManager->maxBlockCopyCount);
  memoryManager->cleanupHandlers =
      malloc(sizeof(memoryManagerCleanupHandler) *
             memoryManager->maxCleanupHandlersCount);

  return memoryManager;
}
