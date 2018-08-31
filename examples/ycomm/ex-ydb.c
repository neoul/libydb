#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ydb.h"

int test_ydb_new_free()
{
	ynode *node;
	node = ynode_new(YNODE_TYPE_VAL, "hello");
	if(!node)
		return -1;
	ynode_free(node);
	node = ynode_new(YNODE_TYPE_LIST, NULL);
	if(!node)
		return -1;
	ynode_free(node);
	node = ynode_new(YNODE_TYPE_DICT, NULL);
	if(!node)
		return -1;
	ynode_free(node);
	return 0;
}

int test_ydb_push_pop()
{
	int i;
	char *item[] = {
		"mtu", "100",
		"type", "mgmt",
		"admin", "enabled",
		"name", "ge1"
	};
	ynode *root = ynode_new(YNODE_TYPE_DICT, NULL);

	for(i=0; i < (sizeof(item)/sizeof(char *)); i+=2)
	{
		ynode *node;
		node = ynode_new(YNODE_TYPE_VAL, item[i+1]);
		if(ynode_attach(node, root, item[i])) {
			printf("ynode_attach() failed\n");
			return -1;
		}
	}
	ynode *mylist = ynode_new(YNODE_TYPE_LIST, NULL);
	ynode_attach(mylist, root, "my-list");
	for(i=0; i < (sizeof(item)/sizeof(char *)); i+=1)
	{
		ynode *node;
		node = ynode_new(YNODE_TYPE_VAL, item[i]);
		if(ynode_attach(node, mylist, NULL)) {
			printf("ynode_attach() failed\n");
			return -1;
		}
	}
	
	ynode_dump(root, -1);
	printf("\n\n");

	char buf[300];
	ynode_snprintf(buf, 300, mylist, 0);
	printf("%s", buf);
	printf("\n\n");
	ynode_fprintf(stdout, mylist, 0);
	printf("\n\n");
	ynode_write(STDOUT_FILENO, mylist, 0);
	printf("\n\n");
	ynode_printf(mylist, 0);
	printf("\n\n");

	ynode_dump_debug(root, 0);

	ynode_detach(mylist);
	ynode_free(mylist);



	ynode_detach(root);
	ynode_free(root);
	return 0;
}

int main(int argc, char *argv[])
{
	if(test_ydb_new_free())
	{
		printf("test_ydb_new_free() failed.\n");
	}
	if(test_ydb_push_pop())
	{
		printf("test_ydb_push_pop() failed.\n");
	}
	return 0;
}