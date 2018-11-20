#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ytrie.h"

struct user_data
{
	unsigned int key;
	char value[16];
};

#define YDATA_KEY_SIZE  (4)


struct user_data *user_data_new()
{
	void *y = malloc(sizeof(struct user_data));
	if(y)
		memset(y, 0x0, sizeof(struct user_data));
	return y;
}

void user_data_free(struct user_data *y)
{
	if(y)
		free(y);
}

int traverse(void *data, const void *key, int key_len, void *value)
{
    static int cnt = 0;
	struct user_data *udata = value;
    printf("[TRAVERSE] %u %s\n", udata->key, udata->value);
    cnt++;
    return 0;
}

int traverse2(void *data, const void *key, int key_len, void *value)
{
    static int cnt = 0;
	cnt++;
    printf("[TRAVERSE] %u key=%s\n", cnt, (char *)key);
    return 0;
}

int traverse_prefix(void *data, const void *key, int key_len, void *value)
{
    static int cnt = 0;
    printf("[TRAVERSE] key=%s, len=%d\n", (char *)key, key_len);
    cnt++;
    return 0;
}

int main(int argc, char *argv[])
{
	if(argc > 1)
	{
		char *searchvalue;
		char buf[512], *line;
		FILE *fp = fopen(argv[1], "r");
		if(!fp)
		{
			printf("[EXIT] no argument\n");
			return -1;
		}
		
		printf("CREATE\n");
		ytrie *trie = ytrie_create();

		while((line = fgets(buf, 512, fp)) != NULL)
		{
			char *copy = strdup(line);
			char *newline = strchr(copy, '\n');
			if(newline)
				*newline=0;
			
			char *old = ytrie_insert(trie, copy, strlen(copy), copy);
			if(old)
			{
				// printf("duplicated %s\n", old);
				free(old);
			}
			else
			{
				printf("inserted %s\n", copy);
			}
		}
      	
		// SIZE
		printf("[SIZE] %lu\n", ytrie_size(trie));

		int i;
		int matchlen = 0;
		char *item[] = {
			"auto",
			"auto-",
			"auto-b",
			"aut",
			"auto-ne",
			"auto-negotiation-xx",
			"ge",
			"ge1",
			"ge11",
			"ge111",
			"geX",
			"ge24x"
		};
		for(i=0; i < (sizeof(item)/sizeof(char *)); i++)
		{
			printf("best match : %s (%ld)\n", item[i], strlen(item[i]));
			searchvalue = ytrie_best_match(trie, item[i], strlen(item[i]), &matchlen);
			if(searchvalue)
				printf("  [SEARCH] best match value: %s, match_len %d\n", searchvalue, matchlen);
			else
				printf("  [SEARCH] -- failed\n");
		}
		for(i=0; i < (sizeof(item)/sizeof(char *)); i++)
		{
			printf("search : %s (%ld)\n", item[i], strlen(item[i]));
			searchvalue = ytrie_search(trie, item[i], strlen(item[i]));
			if(searchvalue)
				printf("  [SEARCH] search value: %s\n", searchvalue);
			else
				printf("  [SEARCH] -- failed\n");
		}
		ylist *search_range = ytrie_search_range(trie, "ge", strlen("ge"));
		while(!ylist_empty(search_range))
		{
			printf(" + %s\n", (char *) ylist_pop_front(search_range));
		}
		ylist_destroy(search_range);
		ytrie_destroy_custom(trie, free);
		fclose(fp);
	}
	return 0;
}