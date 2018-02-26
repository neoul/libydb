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
	for (count=0; count < (sizeof(example)/sizeof(char *)) ; count++)
	{
		char *key = ystrdup(example[count]);
		printf("key address %p\n", key);
	}
	
	ystrprint();

	for (count=0; count < (sizeof(example)/sizeof(char *)) ; count++)
	{
		yfree(example[count]);
	}
	printf("TEST\n");
	yalloc_destroy();
	return 0;
}