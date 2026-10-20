#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <cstdint>
#include <cstdio>
#include <vector>

constexpr size_t buffer_bytes = 1024 * 6 * sizeof(uint64_t);

static double now_s() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main() {
    int fd = open("/dev/uscope_data", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    std::vector<uint8_t> buf(buffer_bytes);
    double t0 = now_s();

    for (int i = 0; i < 10; i++) {
        struct pollfd pfd{fd, POLLIN, 0};

        printf("[%2d] %8.3f s  before poll\n", i, now_s() - t0);
        poll(&pfd, 1, -1);

        printf("[%2d] %8.3f s  before read\n", i, now_s() - t0);
        read(fd, buf.data(), buf.size());
    }

    close(fd);
    return 0;
}