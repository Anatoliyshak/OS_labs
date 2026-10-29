#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

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

static int read_filename(char *buf, size_t max_len) {
    size_t idx = 0;
    char c;
    ssize_t n;
    while ((n = read(STDIN_FILENO, &c, 1)) > 0) {
        if (c == '\n') break;
        if (c == '\r') continue;
        if (idx >= max_len - 1) return -1;
        buf[idx++] = c;
    }
    if (n <= 0 || idx == 0) return -1;
    buf[idx] = '\0';
    return 0;
}

static pid_t spawn_child(int in_fd, int out_fd, int p1[2], int p2[2], int f1, int f2) {
    pid_t pid = fork();
    if (pid == 0) {
        if (dup2(in_fd, STDIN_FILENO) < 0 || dup2(out_fd, STDOUT_FILENO) < 0) {
            write(STDERR_FILENO, "dup2 error\n", 11);
            _exit(1);
        }
        close(p1[0]); close(p1[1]);
        close(p2[0]); close(p2[1]);
        close(f1); close(f2);
        execl("./child", "child", (char *)NULL);
        write(STDERR_FILENO, "exec error\n", 11);
        _exit(1);
    }
    return pid;
}

int main(void) {
    char file1[256], file2[256];
    if (read_filename(file1, sizeof(file1)) < 0 ||
        read_filename(file2, sizeof(file2)) < 0) {
        write(STDERR_FILENO, "file's name error\n", 18);
        return 1;
    }

    int file1_fd = open(file1, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    int file2_fd = open(file2, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (file1_fd < 0 || file2_fd < 0) {
        write(STDERR_FILENO, "file error\n", 11);
        return 1;
    }

    int pipe1[2], pipe2[2];
    if (pipe(pipe1) < 0 || pipe(pipe2) < 0) {
        write(STDERR_FILENO, "pipe error\n", 11);
        return 1;
    }

    pid_t pid1 = spawn_child(pipe1[0], file1_fd, pipe1, pipe2, file1_fd, file2_fd);
    pid_t pid2 = (pid1 > 0) ? spawn_child(pipe2[0], file2_fd, pipe1, pipe2, file1_fd, file2_fd) : -1;
    if (pid1 < 0 || pid2 < 0) {
        write(STDERR_FILENO, "fork error\n", 11);
        return 1;
    }

    close(pipe1[0]);
    close(pipe2[0]);
    close(file1_fd);
    close(file2_fd);

    char buf[4096], short_buf[11];
    size_t len = 0;
    ssize_t bytes;

    while ((bytes = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < bytes; i++) {
            if (buf[i] == '\n') {
                if (len <= 10) {
                    short_buf[len] = '\n';
                    if (write_all(pipe1[1], short_buf, len + 1) < 0) return 1;
                } else if (write_all(pipe2[1], "\n", 1) < 0) {
                    return 1;
                }
                len = 0;
            } else if (len < 10) {
                short_buf[len++] = buf[i];
            } else {
                if (len == 10 && write_all(pipe2[1], short_buf, 10) < 0) return 1;
                len = 11;
                ssize_t start = i;
                while (i < bytes && buf[i] != '\n') i++;
                size_t chunk = (size_t)(i - start + (i < bytes));
                if (write_all(pipe2[1], buf + start, chunk) < 0) return 1;
                if (i < bytes) len = 0;
            }
        }
    }

    if (bytes < 0) {
        write(STDERR_FILENO, "input error\n", 12);
        return 1;
    }
    if (len > 0 && len <= 10 && write_all(pipe1[1], short_buf, len) < 0) {
        return 1;
    }

    close(pipe1[1]);
    close(pipe2[1]);

    int st1 = 0, st2 = 0;
    if (waitpid(pid1, &st1, 0) < 0 || waitpid(pid2, &st2, 0) < 0 ||
        !WIFEXITED(st1) || WEXITSTATUS(st1) != 0 ||
        !WIFEXITED(st2) || WEXITSTATUS(st2) != 0) {
        write(STDERR_FILENO, "child/wait error\n", 17);
        return 1;
    }
    return 0;
}