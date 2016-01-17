/* Example search application. Error handling omitted for brevity! */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "volubile.h"
#include "src/lib/mini.h"

static const char *lexicon_path = "./example_lexicon.dat";

static struct mini *load_lexicon(void)
{
   FILE *fp = fopen(lexicon_path, "rb");
   struct mini *lexicon;
   
   mn_load_file(&lexicon, fp);
   fclose(fp);
   return lexicon;
}

static void print_word(void *arg, const char *word, size_t len)
{
   (void)arg;
   (void)len;

   puts(word);
}

int main(int argc, char **argv)
{
   struct mini *lexicon;
   struct vb_query query = VB_QUERY_INIT; /* Set reasonable default values. */
   
   if (argc != 2 && argc != 4) {
      fprintf(stderr, "Usage: %s <word> [<last_pos> <last_weight>]\n", *argv);
      return EXIT_FAILURE;
   }

   /* Set the word to search for. */
   query.query = argv[1];
   query.len = strlen(argv[1]);
   
   query.page_size = 5;
   
   /* Fill pagination informations. */
   if (argc > 2) {
      query.pagination.last_pos = atoll(argv[2]);
      query.pagination.last_weight = atoll(argv[3]);
   }
   
   /* Load our lexicon and search it. print_word() will be invoked for each
    * word that matches the query.
    */
   lexicon = load_lexicon();
   vb_match(lexicon, &query, print_word, NULL);

   /* Show pagination informations so that the user can request to see the next
    * results page.
    */
   if (!query.pagination.last_page) {
      printf("=> [%"PRIu32" %"PRId32"]\n",
             query.pagination.last_pos,
             query.pagination.last_weight);
   }

   mn_free(lexicon);
}
