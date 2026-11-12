#include "helpers.hh"

int main(int argc, char* argv[]) {
    fprintf(stderr, "[myecho] Myecho running in pid %d\n", getpid());
    for (int i = 0; i != argc; ++i) {
        fprintf(stderr, "[myecho] Arg %d: \"%s\"\n", i, argv[i]);
    }
}
