#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ydb.h"

char *example_yaml = 
"system:\n"
" monitor:\n"
" stats:\n"
"  rx-cnt: 2000\n"
"  tx-cnt: 2010\n"
"  rmon:\n"
"    rx-frame: 1343\n"
"    rx-frame-64: 2343\n"
"    rx-frame-65-127: 233\n"
"    rx-frame-etc: 2\n"
"    tx-frame: 2343\n"
" mgmt:\n"
"interfaces:\n"
"  - eth0\n"
"  - eth1\n"
"  - eth3\n";

int test_ydb_new_free()
{
	ynode *node;
	printf("\n\n=== %s ===\n", __func__);

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
	ynode_printf(root, 0, 3);

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
	ynode_printf(top, 0, 5);

	top = ynode_down(top);
	printf("==\n");
	ynode_printf(top, 0, 5);

	top = ynode_down(top);
	printf("==\n");

	ynode_printf(top, 0, 5);
	printf("==\n");
	ynode_printf(top, 0, 0);
	printf("==\n");
	ynode_printf(top, 0, 1);
	printf("==\n");
	ynode_printf(top, -1, 0);
	printf("==\n");
	ynode_printf(top, -1, 1);
	
	ynode_free(ynode_top(top));

	fclose(fp);
	return 0;
}

int test_ynode_scanf()
{
	ynode *node = NULL;
	printf("\n\n=== %s ===\n", __func__);
	node = ynode_scanf();
	ynode_dump(node, 0, 0);
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
	ynode_printf(top, 0, 0);

	top = ynode_down(top);
	top = ynode_next(top);
	ynode_printf(top, 0, 0);

	top = ynode_prev(top);
	top = ynode_down(top);
	top = ynode_search(top, "name");
	ynode_printf(top, 0, 0);
	top = ynode_top(top);
	ynode_free(top);
	fclose(fp);
	return 0;
}

int test_ynode_path(char *fname)
{
	char *path;
	FILE *fp = fopen(fname, "r");
	if(!fp)
	{
		printf("fopen failed\n");
		return -1;
	}
	printf("\n\n=== %s ===\n", __func__);
	ynode *top = ynode_fscanf(fp);
	ynode_printf(top, 0, 5);
	printf("==\n");

	top = ynode_down(top);
	top = ynode_down(top);
	path = ynode_path(top, YDB_LEVEL_MAX);
	printf("path=%s\n", path);
	free(path);

	top = ynode_up(top);
	top = ynode_next(top);
	top = ynode_down(top);
	top = ynode_down(top);

	path = ynode_path_and_val(top, YDB_LEVEL_MAX);
	printf("path=%s\n", path);
	free(path);

	path = ynode_path_and_val(top, 0);
	printf("path=%s\n", path);
	free(path);
	
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
	ynode_dump(node, 0, 0);
	ynode_free(node);
	return 0;
}

int main(int argc, char *argv[])
{
	// ydb_log_severity = YDB_LOG_DBG;
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

	// if(test_ynode_sscanf())
	// {
	// 	printf("test_ynode_sscanf() failed.\n");
	// }
	
	if(test_ynode_search_and_iterate("ynode-input.yaml"))
	{
		printf("test_ynode_search_and_iterate() failed.\n");
	}

	if(test_ynode_path("ynode-input.yaml"))
	{
		printf("test_ynode_path() failed.\n");
	}
	return 0;
}
