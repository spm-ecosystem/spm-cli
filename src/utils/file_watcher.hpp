#pragma once
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>

#ifdef __linux__
#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>
#include <limits.h>
#endif

namespace veneer {

class FileWatcher {
public:
    inline static void waitChange(const std::string& directoryPath) {
#ifdef __linux__
        int fd = inotify_init();
        if (fd < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return;
        }

        int wd = inotify_add_watch(fd, directoryPath.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE | IN_CLOSE_WRITE);
        if (wd < 0) {
            close(fd);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return;
        }

        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;

        // Block up to 1000ms on kernel events (using 0% CPU while waiting)
        int pollResult = poll(&pfd, 1, 1000);
        if (pollResult > 0 && (pfd.revents & POLLIN)) {
            char buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
            // Consume the events from the buffer
            int readLen = read(fd, buffer, sizeof(buffer));
            (void)readLen; // Suppress unused compiler warnings
        }

        inotify_rm_watch(fd, wd);
        close(fd);
#else
        // Fallback polling loop sleep for non-Linux OS
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
#endif
    }
};

} // namespace veneer
