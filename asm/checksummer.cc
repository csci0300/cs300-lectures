#include <cstdio>
#include <cctype>
#include <cstring>
#include <unistd.h>
#include <vector>

// exec_shell(command)
//   Replace the current process with a process executing a shell command.
void exec_shell(const char* command) {
    static const char* args[4];
    args[0] = "/bin/sh";
    args[1] = "-c";
    args[2] = command;
    args[3] = nullptr;
    execv(args[0], (char**) args);
}


// get_unsigned(buf, idx)
//   Return the `idx`th unsigned value stored in the aligned `buf`.
[[gnu::noinline]]
unsigned get_unsigned(const char* buf, unsigned idx) {
    const unsigned* ubuf = (const unsigned*) buf;
    return ubuf[idx];
}


unsigned checksum(const char* str, size_t n) {
    // copy string into local buffer to ensure alignment
    alignas(unsigned) char buf[400];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, str, n);

    // compute a checksum from the buffer
    unsigned max = 0;
    for (size_t i = 0; i < sizeof(buf) / sizeof(unsigned); ++i) {
        max += get_unsigned(buf, i);
    }
    return max;
}


int main(int argc, char* argv[]) {
    if (argc > 1) {
        // print checksums of command line arguments
        for (int i = 1; i < argc; ++i) {
            size_t len = strlen(argv[i]);
            printf("%s: checksum %08x\n", argv[i], checksum(argv[i], len));
        }
    } else {
        // print checksum of standard input
        static char buf[BUFSIZ];
        size_t n = fread(buf, 1, sizeof(buf), stdin);
        printf("<stdin>: checksum %08x\n", checksum(buf, n));
    }
}
