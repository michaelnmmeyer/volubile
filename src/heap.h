#ifndef VB_HEAP_H
#define VB_HEAP_H

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#define VB_HEAP_INIT(array, max_elems) {.data = array, .max = max_elems}

#define VB_HEAP_DECLARE(NAME, T, COMPAR)                                       \
struct NAME {                                                                  \
   T *data;                                                                    \
   size_t size;                                                                \
   size_t max;                                                                 \
};                                                                             \
                                                                               \
static void NAME##_push(struct NAME *heap, T item)                             \
{                                                                              \
   assert(heap->max > 0);                                                      \
   T *restrict data = heap->data;                                              \
   const size_t max = heap->max;                                               \
                                                                               \
   if (heap->size < max) {                                                     \
      size_t cur = heap->size;                                                 \
      while (cur) {                                                            \
         const size_t par = (cur - 1) >> 1;                                    \
         if (COMPAR(data[par], item) >= 0)                                     \
            break;                                                             \
         data[cur] = data[par];                                                \
         cur = par;                                                            \
      }                                                                        \
      data[cur] = item;                                                        \
      heap->size++;                                                            \
   } else if (COMPAR(item, data[0]) < 0) {                                     \
      size_t cur = 0;                                                          \
      const size_t lim = max >> 1;                                             \
      while (cur < lim) {                                                      \
         size_t child = (cur << 1) + 1;                                        \
         if (child < max - 1 && COMPAR(data[child], data[child + 1]) < 0)      \
            child++;                                                           \
         if (COMPAR(item, data[child]) >= 0)                                   \
            break;                                                             \
         data[cur] = data[child];                                              \
         cur = child;                                                          \
      }                                                                        \
      data[cur] = item;                                                        \
   }                                                                           \
}                                                                              \
                                                                               \
static int NAME##_qsort_cmp_(const void *a, const void *b)                     \
{                                                                              \
   return COMPAR(*(const T *)a, *(const T *)b);                                \
}                                                                              \
static void NAME##_finish(struct NAME *heap)                                   \
{                                                                              \
   qsort(heap->data, heap->size, sizeof *heap->data, NAME##_qsort_cmp_);       \
}

#endif
