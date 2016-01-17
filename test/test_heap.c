#include <time.h>
#include <stdlib.h>

#undef NDEBUG
#include "../src/heap.h"

static int intcmp(const int a, const int b)
{
   return a < b ? -1 : a > b;
}
VB_HEAP_DECLARE(heap, int, intcmp)

static int intpcmp(const void *a, const void *b)
{
   return intcmp(*(const int *)a, *(const int *)b);
}

static void test_heap(void)
{
   const size_t nums_size = rand() % 3000 + 1;
   int *nums = malloc(nums_size * sizeof *nums);
   for (size_t i = 0; i < nums_size; i++)
      nums[i] = rand();

   const size_t heap_size = rand() % 1000 + 1;
   int *heap_data = malloc(heap_size * sizeof *nums);
   struct heap heap = VB_HEAP_INIT(heap_data, heap_size);

   for (size_t i = 0; i < nums_size; i++)
      heap_push(&heap, nums[i]);
   heap_finish(&heap);

   qsort(nums, nums_size, sizeof *nums, intpcmp);

   for (size_t i = 0; i < heap.size; i++)
      assert(heap.data[i] == nums[i]);

   free(nums);
   free(heap_data);
}

int main(void)
{
   srand(time(NULL));
   test_heap();
}
