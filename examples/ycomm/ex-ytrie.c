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
	// // Fixed length of key
	// // CREATE
	// printf("CREATE\n");
	// ytrie *trie = ytrie_create();

	// // INSERT
	// printf("INSERT\n");
	// int count;
	// for(count=0; count<10; count++)
	// {
	// 	struct user_data *data = user_data_new();
	// 	data->key = count;
	// 	snprintf(data->value, 16, "VALUE%d", (count*count));
	// 	ytrie_insert(trie, (const void *) data, YDATA_KEY_SIZE, data);
	// }

	// // SIZE
	// printf("[SIZE] %lu\n", ytrie_size(trie));

	// // TRAVERSE
	// ytrie_traverse(trie, traverse, "****");

	// // SEARCH
	// struct user_data query;
	// struct user_data *qres;
	// memset(&query, 0x0, sizeof(struct user_data));
	// query.key = 5;
	// qres = ytrie_search(trie, &query, YDATA_KEY_SIZE);
	// if(qres)
	// 	printf("[SEARCH]('%u')=%u,%s\n", query.key, qres->key, qres->value);
	// else
	// 	printf("[SEARCH] -- failed\n");

	// // DELETE
	// printf("DELETE\n");
	// for(count=0; count<10; count++)
	// {
	// 	unsigned int key = count;
	// 	void *deleted = ytrie_delete(trie, (const void *) &key, YDATA_KEY_SIZE);
	// 	user_data_free((struct user_data *)deleted);
	// }

	// // Variable length of key
	// // INSERT
	// printf("INSERT\n");
	// for(count=10; count<20; count++)
	// {
	// 	struct user_data *data = user_data_new();
	// 	data->key = count;
	// 	snprintf(data->value, 16, "VALUE%d", (count*count));
	// 	ytrie_insert(trie, (const void *) data->value, sizeof(data->value), data);
	// }

	// // SIZE
	// printf("[SIZE] %lu\n", ytrie_size(trie));

	// // TRAVERSE
	// printf("TRAVERSE with prefix\n");
	// sprintf(query.value, "VALUE1");
	// ytrie_traverse_matched(trie, query.value, strlen(query.value), traverse, "Callback_data");

	// // TRAVERSE_IN_RANGE
	// ytrie_iter* range = ytrie_iter_create(trie, query.value, strlen(query.value));
	// for(; ytrie_iter_done(range); range=ytrie_iter_next(range))
	// {
	// 	struct user_data *udata = ytrie_iter_get_data(range);
    // 	printf("[TRAVERSE_IN_RANGE] %u %s\n", udata->key, udata->value);
	// }
	// ytrie_iter_delete(range);

	// // DESTROY
	// printf("DESTROY\n");
	// ytrie_destroy_custom(trie, (user_free) user_data_free);


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
				printf("duplicated %s\n", old);
				free(old);
			}
			else
			{
				printf("inserted %s\n", copy);
			}
		}

      	
		// SIZE
		printf("[SIZE] %lu\n", ytrie_size(trie));

		// // TRAVERSE
		// ytrie_traverse(trie, traverse2, "****");

		// SEARCH
		// printf("SEARCH: au\n");
		// printf("SEARCH: ytrie_search(au, 2)\n");
		// char *searchvalue = ytrie_search(trie, "auto", strlen("auto"));
		// if(searchvalue)
		// 	printf("[SEARCH] search value: %s\n", searchvalue);
		// else
		// 	printf("[SEARCH] -- failed\n");

		// TRAVERSE
		// printf("SEARCH: ytrie_traverse_matched(au, 2)\n");
		// ytrie_traverse_prefix_match(trie, "au", strlen("au"), traverse_prefix, "Callback_data");
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
			printf("search : %s (%ld)\n", item[i], strlen(item[i]));
			searchvalue = ytrie_best_match(trie, item[i], strlen(item[i]), &matchlen);
			if(searchvalue)
				printf("  [SEARCH] search value: %s, match_len %d\n", searchvalue, matchlen);
			else
				printf("  [SEARCH] -- failed\n");
		}
		ytrie_destroy_custom(trie, free);
		fclose(fp);
	}
	return 0;
}