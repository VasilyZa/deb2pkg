/**
 * @file executor.h
 * @brief 外部命令执行器 — 通过 fork+exec 调用系统命令
 *
 * 所有外部命令调用都通过此模块，保证：
 * 1. 不依赖 shell（使用 fork + execvp 而非 system/popen）
 * 2. 保留原始文件权限（不使用 shell 重定向）
 * 3. 支持超时控制和工作目录设置
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <utility>

namespace deb2pkg {

/**
 * @brief 外部命令执行器
 *
 * 通过 fork() + execvp() 直接执行外部命令，不经过 shell。
 * 支持：捕获 stdout/stderr、设置工作目录、超时控制。
 */
class Executor {
public:
    /**
     * @brief 执行单个命令并捕获输出
     * @param args 命令及参数列表，第一个元素为命令名
     * @param cwd 工作目录（默认为空，使用当前目录）
     * @param timeout_seconds 超时时间（秒），默认 60 秒
     * @return pair{是否成功, stdout 输出字符串}
     */
    static std::pair<bool, std::string> RunCommand(
        const std::vector<std::string>& args,
        const std::filesystem::path& cwd = {},
        int timeout_seconds = 60);

    /**
     * @brief 执行命令并返回按行切分的输出
     * @param args 命令及参数列表
     * @param cwd 工作目录
     * @param timeout_seconds 超时时间
     * @return pair{是否成功, stdout 按行切分的 vector}
     */
    static std::pair<bool, std::vector<std::string>> RunCommandLines(
        const std::vector<std::string>& args,
        const std::filesystem::path& cwd = {},
        int timeout_seconds = 60);

    /**
     * @brief 检查命令是否可在 PATH 中找到
     * @param command 命令名（如 "pkgfile"）
     * @return true 表示命令可用
     */
    static bool IsCommandAvailable(const std::string& command);

    /**
     * @brief 在指定目录中执行命令（兼容接口）
     * @param command 命令字符串（用于日志，不经过 shell 解析）
     * @param cwd 工作目录
     * @return 是否成功
     */
    static bool ExecuteInDir(const std::string& command,
                             const std::filesystem::path& cwd);
};

} // namespace deb2pkg
