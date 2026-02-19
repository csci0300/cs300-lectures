#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 16

int main() {
    FILE* fp = fopen("test.txt", "r");
    if (fp == NULL) {
	perror("Failed to open file");
	exit(1);
    }
    printf("Opened fd %d\n", fileno(fp));

    while(1) {
	char buffer[BUFFER_SIZE];
	memset(&buffer, 0, BUFFER_SIZE);

	int bytes_read = fread(buffer, BUFFER_SIZE, 1, fp);

	printf("fread(%d) => %2d  buffer:  %s\n",
	       BUFFER_SIZE, bytes_read, buffer);
	if (bytes_read <= 0) {
	    break;
	}
    }

    return 0;
}
