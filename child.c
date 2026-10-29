#include <unistd.h>

static int is_vowel(char c) {
    return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y' ||
            c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y');
}

static int write_all(int fd, const char *buf, size_t len) {
    for (size_t off = 0; off < len; ) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n <= 0) {
            write(STDERR_FILENO, "write error\n", 12);
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

int main(void) {
    char buf[4096];
    ssize_t n;

    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        size_t j = 0;
        for (ssize_t i = 0; i < n; i++) {
            if (!is_vowel(buf[i])) buf[j++] = buf[i];
        }
        if (write_all(STDOUT_FILENO, buf, j) < 0) return 1;
    }
    if (n < 0) {
        write(STDERR_FILENO, "read error\n", 11);
        return 1;
    }
    return 0;
}