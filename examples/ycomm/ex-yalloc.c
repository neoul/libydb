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


int main(int argc, char *argv[])
{
	char buf[512];
	char *key, *line;
	FILE *fp = fopen(argv[1], "r");
	if(!fp)
	{
		printf("[EXIT] no argument\n");
		return -1;
	}
	ylist *list = ylist_create();

	while((line = fgets(buf, 512, fp)) != NULL)
	{
		char *newline = strchr(line, '\n');
		if(newline)
			*newline=0;
		key = ystrdup(line);
		ylist_push_back(list, key);
	}

	key = ylist_pop_front(list);
	while(key)
	{
		yfree(key);
		key = ylist_pop_front(list);
	}
	
	ylist_destroy(list);
	// yalloc_destroy();
	return 0;
}