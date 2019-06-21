// ydb-example1.c

#include <stdio.h>
#include <stdlib.h>
#include <ydb.h>
int main(int argc, char *argv[])
{
    ydb *datablock;
    unsigned char testdata[14] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
	};
    
    char *encode = NULL;
    unsigned char *binary = NULL;
    size_t binarylen = 0;
    
    datablock = ydb_open("system");

    ydb_write(datablock,
              "system:\n"
              " low-data: %s\n",
              (encode = binary_to_base64(testdata, sizeof(testdata), NULL))
              );
    free(encode);

    char base64code[32] = {0};

    ydb_read(datablock,
             "system:\n"
             " low-data: %s\n",
             base64code);
    binary = base64_to_binary(base64code, 0, &binarylen);
    printf("low-data (base64): %s\n", base64code);
    printf("low-data (binary):");
    int i;
	for (i=0; i<binarylen; i++)
		printf(" %d", binary[i]);
	printf("\n");
    ydb_close(datablock);
    return 0;
}