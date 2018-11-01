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

int test_ydb_open_close()
{
	printf("\n\n=== %s ===\n", __func__);
	ydb *block1, *block2, *block3;
	block1 = ydb_open("/path/to/datablock1");
	block2 = ydb_open("/path/to/datablock2");
	block3 = ydb_open("/path/to/datablock3");

	ydb_close(block3);
	ydb_close(block2);
	ydb_close(block1);
	printf("\n");
	return 0;
}

ydb_res update_hook(FILE *ydb_fp, ynode *cur, void *user)
{
	fprintf(ydb_fp, 
		"system:\n"
		" hostname: my-pc\n"
		);
	return YDB_OK;
}

int test_ydb_read_write()
{
	int num;
	ydb_res res = YDB_OK;
	printf("\n\n=== %s ===\n", __func__);
	ydb *datablock;
	datablock = ydb_open("/path/to/data");

	// inserting one value
	res = ydb_write(datablock, "VALUE");
	if (res)
		goto _done;
	char value[128] = {0};
	num = ydb_read(datablock, "%s\n", value);
	printf("num %d, value=%s\n", num, value);

	// insert a list.
	res = ydb_write(datablock,
	"- list-entry1\n"
	"- list-entry2\n"
	"- list-entry3\n");
	if (res)
		goto _done;
	char entry[3][64];
	num = ydb_read(datablock, 
		"- %s\n"
		"- %s\n"
		"- %s\n", 
		entry[0],
		entry[1],
		entry[2]);

	printf("num %d, list-entry=%s\n", num, entry[2]);

	res = ydb_write(datablock,
					"system:\n"
					" hostname: %s\n"
					" fan-speed: %d\n"
					" fan-enable: %s\n"
					" os: linux\n",
					"my-machine",
					100,
					"True");
	if (res)
		goto _done;

	ydb_iter *iter = NULL;
	ydb_dump(ydb_get("/path/to/data/system/hostname", &iter), stdout);
	printf("ydb_iter = %s\n", ydb_value(iter));
	int pathlen = 0;
	char *path = ydb_path(datablock, iter, &pathlen);
	printf("ydb_path=%s\n", path);
	if (path)
		free(path);
	
	ydb_delete(datablock, "system: {fan-enable: , }");

	ydb_read_hook_add(datablock, "/system/hostname", update_hook, NULL);

	int speed = 0;
	char hostname[128] = {
		0,
	};
	num = ydb_read(datablock,
				   "system:\n"
				   " hostname: %s\n"
				   " fan-speed: %d\n",
				   hostname, &speed);
	printf("num=%d hostname=%s, fan-speed=%d\n", num, hostname, speed);
	if (num < 0)
		goto _done;
	
	ydb_path_write(datablock, "system/temporature=%d", 60);
	ydb_path_write(datablock, "system/running=%s", "2 hours");

	char *temp = ydb_path_read(datablock, "system/temporature");
	printf("temporature=%d\n", atoi(temp));

	ydb_path_delete(datablock, "system/os");

	ynode_dump(ydb_root(datablock), 0, YDB_LEVEL_MAX);
_done:
	ydb_close(datablock);
	printf("\n");
	return res;
}

#define TEST_FUNC(func)                    \
	do                                     \
	{                                      \
		if (func())                        \
		{                                  \
			printf("%s failed.\n", #func); \
			return -1;                     \
		}                                  \
	} while (0)

int main(int argc, char *argv[])
{
	ydb_log_severity = YDB_LOG_DEBUG;
	TEST_FUNC(test_ydb_open_close);
	TEST_FUNC(test_ydb_read_write);
	return 0;
}
