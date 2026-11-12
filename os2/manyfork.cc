#include "helpers.hh"

int main() {
    int nprocesses = 10000;

    for (int i = 0; i != nprocesses; ++i) {
        pid_t p1 = fork();
        assert(p1 >= 0);
        if (p1 == 0) {
            exit(0);
        }
    }

    printf("Forked %d processes\n", nprocesses);
}
