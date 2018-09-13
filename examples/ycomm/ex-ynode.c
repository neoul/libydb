#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ydb.h"
#include "ynode.h"

char *example_yaml = 
"system:\n"
" monitor:\n"
" stats:\n"
"  rx-cnt: !!int 2000\n"
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

char *example_yaml2 = 
"monitor:\n"
" mem: 10\n"
" cpu: amd64\n";


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
	
	ynode_delete(ynode_top(top));

	fclose(fp);
	return 0;
}

int test_ynode_scanf()
{
	ynode *node = NULL;
	printf("\n\n=== %s ===\n", __func__);
	node = ynode_scanf();
	ynode_dump(node, 0, 0);
	ynode_delete(node);
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
	ynode_delete(top);
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
	ynode_delete(top);
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
	ynode_delete(node);
	return 0;
}

int test_ynode_crud()
{
	printf("\n\n=== %s ===\n", __func__);
	
	ynode *node, *clone;
	ynode *a, *b, *c;
	ynode *top = ynode_sscanf(example_yaml, strlen(example_yaml));
	printf("== top ==\n");
	ynode_printf(top, 0, 6);
	
	printf("== ynode_create ==\n");
	node = ynode_create(top, YNODE_TYPE_VAL, "create", "first");
	ynode_printf(top, 0, 1);

	printf("== node ==\n");
	node = ynode_next(node);
	ynode_printf(node, 0, 2);

	printf("== ynode_clone (node) ==\n");
	clone = ynode_clone(node);
	ynode_printf(clone, 0, 2);
	ynode_delete(clone);

	printf("== ynode_clone (node) ==\n");
	clone = ynode_copy(node);
	ynode_printf(clone, 0, 5);
	ynode_delete(clone);

	printf("== a ==\n");
	a = ynode_search(top, "system");
	ynode_printf(a, 0, 3);
	printf("== b ==\n");
	b = ynode_sscanf(example_yaml2, strlen(example_yaml2));
	ynode_printf(b, 0, 3);

	printf("== ynode_merge (b to a) ==\n");
	c = ynode_merge(a, b);
	ynode_printf(c, -2, 5);
	
	ynode_create(ynode_down(b), YNODE_TYPE_VAL, "io", "100");
	ynode_create(ynode_down(b), YNODE_TYPE_VAL, "cpu", "x86");

	printf("== b ==\n");
	ynode_printf(b, -2, 5);

	// only copy the existent ynode's data
	printf("== ynode_merge_new (b to a) ==\n");	
	c = ynode_merge_new(a, b);
	ynode_printf(c, -2, 5);
	ynode_delete(c);

	// only copy the existent ynode's data
	printf("== ynode_replace (b to a) ==\n");
	c = ynode_replace(a, b);
	ynode_printf(c, -2, 5);

	ynode_delete(b);

	printf("== top ==\n");
	ynode_printf(top, 0, 10);
	ynode_delete(top);
	return 0;
}

int main(int argc, char *argv[])
{
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

	ydb_log_severity = YDB_LOG_DBG;
	if(test_ynode_crud())
	{
		printf("test_ynode_crud() failed.\n");
	}
	return 0;
}
