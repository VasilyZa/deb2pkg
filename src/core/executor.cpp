/**
 * @file executor.cpp
 * @brief Executor 类实现 — 使用 fork+exec 执行外部命令
 *
 * 实现要点：
 * 1. 用 pipe() 捕获子进程 stdout/stderr
 * 2. 用 alarm() 或 select() 实现超时
 * 3. 等待子进程结束并返回状态
 */

#include "executor.h"
#include "logger.h"

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <algorithm>

namespace deb2pkg {

// ---- 辅助：将字符串按行切分 (手动实现，避免 musl 下 std::istringstream 崩溃) ----
static std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
        start = end + 1;
    }
    return lines;
}

std::pair<bool, std::string> Executor::RunCommand(
    const std::vector<std::string>& args,
    const std::filesystem::path& cwd,
    int timeout_seconds)
{
    if (args.empty()) {
        return {false, "命令参数为空"};
    }

    // 构建 C 风格参数数组
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    // 创建管道捕获 stdout
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        Logger::Error("创建管道失败：" + std::string(std::strerror(errno)));
        return {false, "无法创建管道"};
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        Logger::Error("创建子进程失败：" + std::string(std::strerror(errno)));
        return {false, "无法创建子进程"};
    }

    if (pid == 0) {
        // === 子进程 ===
        close(pipefd[0]);  // 关闭读端

        // 将 stdout 和 stderr 重定向到管道写端
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // 设置工作目录
        if (!cwd.empty()) {
            if (chdir(cwd.c_str()) != 0) {
                std::cerr << "无法切换工作目录: " << cwd << std::endl;
                _exit(127);
            }
        }

        // 执行命令（不在 PATH 中搜索的话 execvp 会搜索）
        execvp(argv[0], argv.data());

        // execvp 失败时才走到这里
        std::cerr << "无法执行命令: " << argv[0]
                  << " (" << std::strerror(errno) << ")" << std::endl;
        _exit(127);
    }

    // === 父进程 ===
    close(pipefd[1]);  // 关闭写端

    // 设置读端为非阻塞
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    std::string output;
    char buffer[4096];
    int elapsed = 0;
    int status;
    pid_t result;

    // 超时循环读取
    while (elapsed < timeout_seconds) {
        // 尝试非阻塞读取
        ssize_t n = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            output += buffer;
        }

        // 检查子进程是否结束
        result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            // 子进程已结束，读取剩余数据
            close(pipefd[0]);
            break;
        } else if (result < 0) {
            close(pipefd[0]);
            break;
        }

        // 等待 100ms 再检查
        usleep(100000);
        elapsed++;
    }

    // 超时处理
    if (elapsed >= timeout_seconds) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        close(pipefd[0]);
        Logger::Error("命令执行超时（" + std::to_string(timeout_seconds) + "秒）：" + args[0]);
        return {false, "命令超时"};
    }

    // 判断退出状态
    bool success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);

    return {success, output};
}

std::pair<bool, std::vector<std::string>> Executor::RunCommandLines(
    const std::vector<std::string>& args,
    const std::filesystem::path& cwd,
    int timeout_seconds)
{
    auto result = RunCommand(args, cwd, timeout_seconds);
    if (!result.first) {
        return {false, {}};
    }
    return {true, SplitLines(result.second)};
}

bool Executor::IsCommandAvailable(const std::string& command) {
    // 使用 which 命令检查（轻量，不依赖 which 本身）
    auto result = RunCommand({"/usr/bin/which", command});
    if (result.first && !result.second.empty()) {
        return true;
    }
    // 备用：尝试执行 command --version
    auto result2 = RunCommand({command, "--version"}, {}, 5);
    return result2.first;
}

bool Executor::ExecuteInDir(const std::string& command,
                             const std::filesystem::path& cwd) {
    // 将命令字符串按空格简单分割（手动实现）
    std::vector<std::string> args;
    size_t pos = 0;
    while (pos < command.size()) {
        // 跳过空白
        while (pos < command.size() && (command[pos] == ' ' || command[pos] == '\t')) pos++;
        if (pos >= command.size()) break;
        size_t end = command.find_first_of(" \t", pos);
        if (end == std::string::npos) end = command.size();
        args.push_back(command.substr(pos, end - pos));
        pos = end;
    }

    if (args.empty()) {
        return false;
    }

    auto result = RunCommand(args, cwd);
    return result.first;
}

} // namespace deb2pkg
