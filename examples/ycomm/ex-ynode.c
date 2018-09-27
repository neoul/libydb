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
	printf("\n\n=== %s (%s) ===\n", __func__, fname);
	ynode *top = NULL;
	ynode_fscanf(fp, &top);
	ynode_dump(top, 0, YDB_LEVEL_MAX);
	
	ynode_delete(ynode_top(top));

	fclose(fp);
	return 0;
}

int test_ynode_scanf()
{
	ynode *node = NULL;
	printf("\n\n=== %s ===\n", __func__);
	ynode_scanf(&node);
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
	ynode *top = NULL;
	ynode_fscanf(fp, &top);
	ynode_dump(top, 0, YDB_LEVEL_MAX);

	printf("ynode_down()\n");
	printf("==\n");
	top = ynode_down(top);
	ynode_printf(top, 0, 0);

	printf("ynode_next()\n");
	printf("==\n");
	top = ynode_next(top);
	ynode_printf(top, 0, 0);

	printf("ynode_prev()\n");
	printf("==\n");
	top = ynode_prev(top);
	ynode_printf(top, 0, 0);

	printf("ynode_down()\n");
	printf("==\n");
	top = ynode_down(top);
	ynode_printf(top, 0, 0);

	printf("ynode_search(%s)\n", "name");
	printf("==\n");
	top = ynode_search(top, "name");
	ynode_printf(top, 0, 0);

	printf("ynode_delete()\n");
	printf("==\n");
	top = ynode_top(top);
	ynode_delete(top);
	fclose(fp);
	printf("==\n");
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
	ynode *top = NULL;
	ynode_fscanf(fp, &top);
	ynode_printf(top, 0, 5);
	printf("==\n\n");

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
	ynode_sscanf(buf, strlen(buf), &node);
	ynode_dump(node, 0, 0);
	ynode_delete(node);
	return 0;
}

int test_ynode_crud()
{
	printf("\n\n=== %s ===\n", __func__);
	
	ynode *node, *clone;
	ynode *a, *b, *c;
	ynode *top = NULL;
	ynode_sscanf(example_yaml, strlen(example_yaml), &top);
	printf("== top ==\n");
	ynode_printf(top, 0, 6);
	
	printf("== ynode_create ==\n");
	node = ynode_create(YNODE_TYPE_VAL, "create", "first", top);
	ynode_printf(top, 0, 1);

	printf("== node ==\n");
	node = ynode_next(node);
	ynode_printf(node, 0, 2);

	printf("== ynode_copy (node) ==\n");
	clone = ynode_copy(node);
	ynode_printf(clone, 0, 5);
	ynode_delete(clone);

	printf("== a ==\n");
	a = ynode_search(top, "system");
	ynode_printf(a, 0, 3);

	printf("== b ==\n");
	ynode_sscanf(example_yaml2, strlen(example_yaml2), &b);
	ynode_printf(b, 0, 3);

	printf("== ynode_merge (b to a) ==\n");
	c = ynode_merge(a, b);
	ynode_printf(c, -2, 5);
	
	ynode_create(YNODE_TYPE_VAL, "io", "100", ynode_down(b));
	ynode_create(YNODE_TYPE_VAL, "cpu", "x86", ynode_down(b));

	printf("== b ==\n");
	ynode_printf(b, -2, 5);

	// only copy the existent ynode's data
	printf("== ynode_merge_new (b to a) ==\n");	
	c = ynode_merge_new(a, b);
	ynode_printf(c, -2, 5);
	ynode_delete(c);


	ynode_delete(b);

	printf("== top ==\n");
	ynode_printf(top, 0, 10);
	ynode_delete(top);
	return 0;
}

void ynode_hooker(yhook_op_type op, ynode *cur, ynode *new, void *user)
{
	printf("== %s: %s ==\n", __func__, yhook_op_str[op]);
	if (op == YHOOK_OP_CREATE || op == YHOOK_OP_REPLACE)
		ynode_dump(new, 0, 0);
	else
		ynode_dump(cur, 0, 0);
}

int test_yhook()
{
	char *sample = 
		"1:\n"
		"  1-1:\n"
		"   1-1-1: v1\n"
		"   1-1-2: v2\n"
		"   1-1-3: v3\n"
		"  1-2:\n"
		"   1-2-1: v4\n"
		"   1-2-2: v5\n"
		"   1-2-3: v6\n"
		"2:\n"
		"  2-1:\n"
		"   2-1-1: v7\n"
		"   2-1-2: v8\n"
		"   2-1-3: v9\n"
		"  2-2:\n"
		"   2-2-1: v10\n"
		"   2-2-2: v11\n"
		"   2-2-3: v12\n";

	printf("\n\n=== %s ===\n", __func__);
	
	ynode *top = NULL;
	ynode_sscanf(sample, strlen(sample), &top);
	ynode_dump(top, 1, YDB_LEVEL_MAX);

	// move to 1-2 node
	top = ynode_search(top, "1/1-2");
	ynode_dump(top, 0, 0);
	yhook_register(top, YNODE_LEAF_FIRST, ynode_hooker, NULL);

	printf("== ynode_create to check yhook ==\n");
	ynode_create(YNODE_TYPE_VAL, "1-2-4", "v13", top);
	// yhook_unregister(top);

	printf("== top ==\n");
	top = ynode_top(top);
	ynode_printf(top, 1, YDB_LEVEL_MAX);
	ynode_delete(top);
	return 0;
}

int main(int argc, char *argv[])
{
	if(test_ynode_fscanf("ynode-value.yaml"))
	{
		printf("test_ynode_fscanf() failed.\n");
	}
	
	if(test_ynode_fscanf("ynode-list.yaml"))
	{
		printf("test_ynode_fscanf() failed.\n");
	}

	if(test_ynode_fscanf("ynode-dict.yaml"))
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

	if(test_ynode_crud())
	{
		printf("test_ynode_crud() failed.\n");
	}

	ydb_log_severity = YDB_LOG_DBG;
	if(test_yhook())
	{
		printf("test_yhook() failed.\n");
	}
	return 0;
}
