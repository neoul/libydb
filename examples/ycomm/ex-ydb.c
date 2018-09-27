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
	block2 = ydb_open("/path/to/datablock2");
	block3 = ydb_open("/path/to/datablock3");

	ydb_close(block3);
	ydb_close(block2);
	ydb_close(block1);
	printf("\n");
	return 0;
}

int test_ydb_read_write()
{
	int num;
	ydb_res res = YDB_OK;
	printf("\n\n=== %s ===\n", __func__);
	ydb *db;
	db = ydb_open("/path/to/data");

	// inserting one value
	res = ydb_write(db, "VALUE");
	if (res)
		goto _done;
	char value[128] = {0};
	num = ydb_read(db, "%s\n", value);
	printf("num %d, value=%s\n", num, value);

	// insert a list.
	res = ydb_write(db,
	"- list-entry1\n"
	"- list-entry2\n"
	"- list-entry3\n");
	if (res)
		goto _done;
	char entry[3][64];
	num = ydb_read(db, 
		"- %s\n"
		"- %s\n"
		"- %s\n", 
		entry[0],
		entry[1],
		entry[2]);

	printf("num %d, list-entry=%s\n", num, entry[2]);

	res = ydb_write(db,
					"system:\n"
					" hostname: %s\n"
					" fan-speed: %d\n"
					" fan-enable: %s\n",
					"my-machine",
					100,
					"True");

	if (res)
		goto _done;
	int speed = 0;
	char hostname[128] = {
		0,
	};
	num = ydb_read(db,
				   "system:\n"
				   " hostname: %s\n"
				   " fan-speed: %d\n",
				   hostname, &speed);
	printf("num=%d hostname=%s, fan-speed=%d\n", num, hostname, speed);
	if (num < 0)
		goto _done;
	
	ydb_path_write(db, "system/temporature=%d", 60);
	ydb_path_write(db, "system/running=%s", "2 hours");

	char *temp = ydb_path_read(db, "system/temporature");
	printf("temporature=%d", atoi(temp));
	ynode_dump(ydb_top(db), 0, YDB_LEVEL_MAX);
_done:
	ydb_close(db);
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
	ydb_log_severity = YDB_LOG_DBG;
	TEST_FUNC(test_ydb_open_close);
	TEST_FUNC(test_ydb_read_write);
	return 0;
}
