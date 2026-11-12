#include "helpers.hh"

int main() {
    int pfd[2];
    int r = pipe(pfd); // Make the pipe
    assert(r == 0);

    pid_t pid = fork();
    if (pid != 0) { // Parent
	ssize_t n = write(pfd[1], "hello world", 11);
	printf("[babypipe, parent] Wrote %ld bytes to pipe\n", n);

	int status;
	waitpid(pid, &status, 0);
	printf("[babypipe, parent] Child %d exited with status %d\n", pid, WEXITSTATUS(status));
    } else { // Child
	printf("[babypipe, child] Child %d started\n", getpid());

	char buf[BUFSIZ];	
        int n = read(pfd[0], buf, 11);
	assert(n >= 0);
	buf[n] = 0;

	printf("[babypipe, child] Read %d bytes from pipe:  %s\n", n, buf);
    }


}
