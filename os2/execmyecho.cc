#include "helpers.hh"

int main() {
    const char* args[] = {
        "./myecho", // argv[0] is the string used to execute the program
        "Hello!",
        "Myecho should print these",
        "arguments.",
        nullptr
    };
    fprintf(stderr, "[execmyecho] About to exec myecho in pid %d\n", getpid());

    int r = execv("./myecho", (char**) args);

    fprintf(stderr, "[execmyecho] Finished execing myecho in pid %d;"
            "status %d\n", getpid(), r);
}
