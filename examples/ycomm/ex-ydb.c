#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ydb.h"

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

int test_ydb_open_close()
{
	printf("\n\n=== %s ===\n", __func__);
	ydb *block1, *block2, *block3;
	block1 = ydb_open("/path/to/datablock1");
	ynode_printf(ydb_top(block1), 1, YDB_LEVEL_MAX);
	block2 = ydb_open("/path/to/datablock2");
	ynode_printf(ydb_top(block2), 1, YDB_LEVEL_MAX);
	block3 = ydb_open("/path/to/datablock3");
	ynode_printf(ydb_top(block3), 1, YDB_LEVEL_MAX);
	ydb_close(block3);
	ydb_close(block2);
	ydb_close(block1);
	printf("\n");
	return 0;
}

int test_ydb_read_write()
{
	printf("\n\n=== %s ===\n", __func__);
	ydb *block1, *block2, *block3;
	block1 = ydb_open("/path/to/datablock1");
	ydb_write(block1, "system: {temporature: 100c}");
	ynode_printf(ydb_top(block1), 1, YDB_LEVEL_MAX);
	ydb_close(block1);
	printf("\n");
	return 0;
}

#define TEST_FUNC(func) \
	do { if(func()) {printf("%s failed.\n", #func); return -1;} } while(0)

int main(int argc, char *argv[])
{
	ydb_log_severity = YDB_LOG_DBG;
	TEST_FUNC(test_ydb_open_close);
	TEST_FUNC(test_ydb_read_write);
	return 0;
}
