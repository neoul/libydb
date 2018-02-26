#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yalloc.h"
#include "ylist.h"

char *example[] = {
	"abs",
	"sss",
	"dfe",
	"asfe",
	"23425f",
	"fdvcc",
	"fess",
	"",
};


int main(void) {
	int count;
	ylist *list = ylist_create();

	for (count=0; count < (sizeof(example)/sizeof(char *)) ; count++)
	{
		char *key = ystrdup(example[count]);
		ylist_push_back(list, key);
	}
	
	ystrprint();

	char *key = ylist_pop_front(list);
	while(key)
	{
		yfree(key);
		key = ylist_pop_front(list);
	}
	ylist_destroy(list);
	yalloc_destroy();
	return 0;
}