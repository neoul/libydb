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

int test_ynode_attach_detach()
{
	int i;
	char *item[] = {
		"mtu\n", "100",
		"type", "mgmt",
		"admin", "enabled",
		"name", "ge1\ngo\x07"
	};
	printf("\n\n=== %s ===\n", __func__);
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
	
	ynode_dump(root, 0);
	printf("\n\n");

	ynode_dump(root, -1);
	printf("\n\n");


	char buf[300];
	printf("\n\n=== ynode_snprintf ===\n");
	ynode_snprintf(buf, 300, root, 0);
	printf("%s", buf);
	printf("\n\n");

	printf("\n\n=== ynode_fprintf ===\n");
	ynode_fprintf(stdout, root, 0);
	printf("\n\n");

	printf("\n\n=== ynode_write ===\n");
	ynode_write(STDOUT_FILENO, root, 0);
	printf("\n\n");

	printf("\n\n=== ynode_printf ===\n");
	ynode_printf(root, 0);
	printf("\n\n");

	printf("\n\n=== ynode_search ===\n");
	ynode *found = ynode_search(root, "/my-list/5");
	ynode_printf(found, 0);
	printf("\n\n");

	ynode_detach(mylist);
	ynode_free(mylist);

	ynode_detach(root);
	ynode_free(root);
	return 0;
}

int test_ynode_fscanf(char *fname)
{
	FILE *fp = fopen(fname, "r");
	if(!fp)
	{
		printf("fopen failed\n");
		return -1;
	}
	printf("\n\n=== %s ===\n", __func__);
	ynode *top = ynode_fscanf(fp);
	ynode_dump(top, 0);
	ynode_free(top);

	fclose(fp);
	return 0;
}

int test_ynode_scanf()
{
	ynode *node = NULL;
	printf("\n\n=== %s ===\n", __func__);
	node = ynode_scanf();
	ynode_dump(node, 0);
	ynode_free(node);
	return 0;
}

int test_ynode_search_and_iterate(char *fname)
{
	FILE *fp = fopen(fname, "r");
	if(!fp)
	{
		printf("fopen failed\n");
		return -1;
	}
	printf("\n\n=== %s ===\n", __func__);
	ynode *top = ynode_fscanf(fp);
	ynode_printf(top, 0);

	top = ynode_down(top);
	top = ynode_next(top);
	ynode_printf(top, 0);

	top = ynode_prev(top);
	top = ynode_down(top);
	top = ynode_search(top, "name");
	ynode_printf(top, 0);
	top = ynode_top(top);
	ynode_free(top);
	fclose(fp);
	return 0;
}

int test_ynode_sscanf()
{
	char *buf = "abc";
	ynode *node = NULL;
	printf("\n\n=== %s ===\n", __func__);
	node = ynode_sscanf(buf, strlen(buf));
	ynode_dump(node, 0);
	ynode_free(node);
	return 0;
}

int main(int argc, char *argv[])
{
	ydb_log_severity = YDB_LOG_DBG;
	if(test_ydb_new_free())
	{
		printf("test_ydb_new_free() failed.\n");
	}
	
	if(test_ynode_attach_detach())
	{
		printf("test_ynode_attach_detach() failed.\n");
	}

	if(test_ynode_fscanf("test.yaml"))
	{
		printf("test_ynode_fscanf() failed.\n");
	}

	if(test_ynode_sscanf())
	{
		printf("test_ynode_sscanf() failed.\n");
	}
	
	if(test_ynode_search_and_iterate("ynode-input.yaml"))
	{
		printf("test_ynode_search_and_iterate() failed.\n");
	}
	return 0;
}