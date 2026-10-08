#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 4

int main() {
    int fd = open("test.txt", O_RDONLY | O_SYNC);
    if (fd < 0) { 
	perror("Failed to open file");
	exit(1);
    }
    //printf("Opened fd %d\n", fd);
   
    while(1) {
	char buffer[BUFFER_SIZE];        // Make a buffer to store part of file
	memset(&buffer, 0, BUFFER_SIZE); // Sets it to zero

	int bytes_read = read(fd, buffer, BUFFER_SIZE);
	// printf("read(%d, &buffer, %d) => %d, buffer: %s\n",
	//        fd, BUFFER_SIZE, bytes_read, buffer);
 
	if (bytes_read <= 0) { 
	    break;
	}
    }

    return 0;
}
