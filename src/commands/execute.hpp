#pragma once
#include <string>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

namespace veneer {

inline int safeExecute(const std::string& binary, const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "[Error] Fork failed\n";
        return -1;
    } else if (pid == 0) {
        // Child process
        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(binary.c_str()));
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execvp(binary.c_str(), c_args.data());
        // If execvp returns, it failed
        std::cerr << "[Error] execvp failed to run " << binary << "\n";
        _exit(127);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -1;
    }
}

} // namespace veneer
