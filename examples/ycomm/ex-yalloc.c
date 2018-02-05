#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ytree.h"
#include "yalloc.h"


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
	ytree mpool = ytree_create(NULL, NULL);
	// FILE *fp = fopen("alphago.txt", "r");
	// char buf[4094];
	// while(fgets(buf, sizeof(buf), fp) != NULL)
	// {
	// 	char *pch = strtok (buf," ,.-\n\t'\"");
	// 	while (pch != NULL)
	// 	{
	// 		char *tmp = ystrdup(pch);
	// 		pch = strtok (NULL," ,.-\n\t'\"");
	// 		//ytree_insert(mpool, tmp);
	// 	}
	// }
	// fclose(fp);
	
	for (count=0; count < (sizeof(example)/sizeof(char *)) ; count++)
	{
		char *key = ystrdup(example[count]);
		ytree_insert(mpool, key);
	}
	
	ystrprint();
	ytree_destroy_custom(mpool, yfree);
	yalloc_destroy();
	return 0;
}