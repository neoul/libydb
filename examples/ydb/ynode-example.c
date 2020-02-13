#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ylog.h"
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


int test_ynode_scanf_from_fp(char *fname)
{
	FILE *fp = fopen(fname, "r");
	if(!fp)
	{
		printf("fopen failed\n");
		return -1;
	}
	printf("\n\n=== %s (%s) ===\n", __func__, fname);
	ynode *top = NULL;
	ynode_scanf_from_fp(fp, &top);
	ynode_dump(top, 0, YDB_LEVEL_MAX);
	
	ynode_remove(ynode_top(top));

	fclose(fp);
	return 0;
}

int test_ynode_scanf()
{
	ynode *node = NULL;
	printf("\n\n=== %s ===\n", __func__);
	ynode_scanf(&node);
	ynode_dump(node, 0, 0);
	ynode_remove(node);
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
	ynode_scanf_from_fp(fp, &top);
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
	ynode_remove(top);
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
	ynode_scanf_from_fp(fp, &top);
	ynode_printf(top, 0, 5);
	printf("==\n\n");

	top = ynode_down(top);
	top = ynode_down(top);
	path = ynode_path(top, YDB_LEVEL_MAX, NULL);
	printf("path=%s\n", path);
	free(path);

	top = ynode_up(top);
	top = ynode_next(top);
	top = ynode_down(top);
	top = ynode_down(top);

	path = ynode_path_and_val(top, YDB_LEVEL_MAX, NULL);
	printf("path=%s\n", path);
	free(path);

	path = ynode_path_and_val(top, 0, NULL);
	printf("path=%s\n", path);
	free(path);
	
	top = ynode_top(top);
	ynode_remove(top);
	fclose(fp);
	return 0;
}

int test_ynode_scanf_from_buf()
{
	char *buf = "abc";
	ynode *node = NULL;
	printf("\n\n=== %s ===\n", __func__);
	ynode_scanf_from_buf(buf, strlen(buf), 0, &node);
	ynode_dump(node, 0, 0);
	ynode_remove(node);
	return 0;
}

int test_ynode_crud()
{
	// ylog_severity = YLOG_DEBUG;
	printf("\n\n=== %s ===\n", __func__);
	ynode *node, *clone;
	ynode *a, *b, *c;
	ynode *top = NULL;
	ynode_log *log = NULL;
	ynode_scanf_from_buf(example_yaml, strlen(example_yaml), 0, &top);
	printf("== top ==\n");
	ynode_printf(top, 1, YDB_LEVEL_MAX);
	log = ynode_log_open(top, stdout);
	printf("== ynode_create ==\n");
	node = ynode_create(YNODE_TYPE_VAL, NULL, "create", "first", top, log);

	node = ynode_next(node);
	printf("== ynode_copy (%s) ==\n", ynode_key(node));
	clone = ynode_copy(node);
	ynode_printf(clone, 0, 5);
	ynode_remove(clone);

	printf("== a ==\n");
	a = ynode_search(top, "system");
	ynode_printf(a, 0, 3);

	printf("== b ==\n");
	ynode_scanf_from_buf(example_yaml2, strlen(example_yaml2), 0, &b);
	ynode_printf(b, 0, 3);

	printf("== ynode_merge (b to a) ==\n");
	c = ynode_merge(a, b, log);
	ynode_printf(c, -2, 5);	
	
	printf("== ynode_create (b) ==\n");
	ynode_create(YNODE_TYPE_VAL, NULL, "io", "100", ynode_down(b), NULL);
	ynode_create(YNODE_TYPE_VAL, NULL, "cpu", "x86", ynode_down(b), NULL);
	ynode_printf(b, -2, 5);

	// only copy the existent ynode's data
	printf("== ynode_merge_new (b to a) ==\n");	
	c = ynode_merge_new(a, b);
	ynode_printf(c, -2, 5);
	ynode_remove(c);


	ynode_remove(b);

	printf("== top ==\n");
	ynode_printf(top, 1, YDB_LEVEL_MAX);
	ynode_delete(top, log);
	ynode_log_close(log, NULL, NULL);
	return 0;
}

void ynode_hooker(char op, ynode *base, ynode *cur, ynode *new, void *user)
{
	char *path;
	printf("%s (op=%c)\n", __func__, op);
	path = ynode_path_and_val(base, YDB_LEVEL_MAX, NULL);
	if (path)
	{
		printf(" base: %s\n", path);
		free(path);
	}
	path = ynode_path_and_val(cur, YDB_LEVEL_MAX, NULL);
	if (path)
	{
		printf(" cur: %s\n", path);
		free(path);
	}
	path = ynode_path_and_val(new, YDB_LEVEL_MAX, NULL);
	if (path)
	{
		printf(" new: %s\n", path);
		free(path);
	}
}

void ynode_hooker_suppressed(char op, ynode *base, void *user)
{
	char *path;
	printf("%s\n", __func__);
	path = ynode_path_and_val(base, YDB_LEVEL_MAX, NULL);
	if (path)
	{
		printf(" base: %s\n", path);
		free(path);
	}
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
	ynode_scanf_from_buf(sample, strlen(sample), 0, &top);
	ynode_dump(top, 1, YDB_LEVEL_MAX);

	yhook_register(ynode_search(top, "/1"), YNODE_SUPPRESS_HOOK, (yhook_func) ynode_hooker_suppressed, 0, NULL);
	yhook_register(ynode_search(top, "/2"), YNODE_SUPPRESS_HOOK, (yhook_func) ynode_hooker_suppressed, 0, NULL);


	// move to 1-2 node
	top = ynode_search(top, "1/1-2");
	yhook_register(top, 0x0, (yhook_func) ynode_hooker, 0, NULL);
	

	printf("== ynode_create to check yhook ==\n");
	ynode_create(YNODE_TYPE_VAL, NULL, "1-2-4", "v13", top, NULL);
	// yhook_unregister(top);

	printf("== top ==\n");
	top = ynode_top(top);
	ynode_printf(top, 1, YDB_LEVEL_MAX);

	ynode_write(&top, 
		"3:\n"
		" 3-1: v14\n"
		"2:\n"
		" 2-1:\n"
		"  2-1-4: v14\n"
		" 2-3:\n"
		"  2-3-1: v14\n"
		" 2-4: v14\n"
		"1:\n"
		" 1-2:\n"
		"  1-2-5: check-hook\n"
		);
	ynode_printf(top, 1, YDB_LEVEL_MAX);
	ynode_erase(&top, 
		"3:\n"
		" 3-1: v14\n"
		);
	ynode_printf(top, 1, YDB_LEVEL_MAX);

	char s2_2_3[32];
	ydb_retrieve(top, 
		"2:\n"
		" 2-2:\n"
		"  2-2-3: %s\n", s2_2_3
		);
	printf("2-2-3: %s\n", s2_2_3);
	ynode_delete(top, NULL);
	return 0;
}

int main(int argc, char *argv[])
{
	// if(test_ynode_scanf_from_fp("ynode-value.yaml"))
	// {
	// 	printf("test_ynode_scanf_from_fp() failed.\n");
	// }
	
	// if(test_ynode_scanf_from_fp("ynode-list.yaml"))
	// {
	// 	printf("test_ynode_scanf_from_fp() failed.\n");
	// }

	// if(test_ynode_scanf_from_fp("ynode-dict.yaml"))
	// {
	// 	printf("test_ynode_scanf_from_fp() failed.\n");
	// }

	// // if(test_ynode_scanf_from_buf())
	// // {
	// // 	printf("test_ynode_scanf_from_buf() failed.\n");
	// // }
	
	// if(test_ynode_search_and_iterate("ynode-input.yaml"))
	// {
	// 	printf("test_ynode_search_and_iterate() failed.\n");
	// }

	// if(test_ynode_path("ynode-input.yaml"))
	// {
	// 	printf("test_ynode_path() failed.\n");
	// }

	// ylog_severity = YLOG_DEBUG;
	// if(test_ynode_crud())
	// {
	// 	printf("test_ynode_crud() failed.\n");
	// }

	// ylog_severity = YLOG_INFO;
	if(test_yhook())
	{
		printf("test_yhook() failed.\n");
	}
	return 0;
}
