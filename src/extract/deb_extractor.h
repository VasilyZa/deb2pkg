/**
 * @file deb_extractor.h
 * @brief Debian 软件包 (.deb) 解压器
 *
 * .deb 文件是 ar 归档，包含三个成员：
 * 1. debian-binary — 版本号文件（如 "2.0\n"）
 * 2. control.tar.xz — 元数据（含 control 文件、preinst/postinst 脚本等）
 * 3. data.tar.xz — 实际安装的数据文件
 *
 * 解压流程：
 * 1. 用 ar 命令或 libarchive 提取 ar 归档成员
 * 2. 用 libarchive 解压 control.tar.* 获取 control 文件内容
 * 3. 用 libarchive 解压 data.tar.* 获取全部数据文件
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>

namespace deb2pkg {

/**
 * @brief deb 解压结果
 */
struct DebExtractResult {
    std::filesystem::path extract_dir;       ///< data.tar 解压目录
    std::vector<char> control_content;       ///< control 文件原始内容
};

/**
 * @brief Deb 包解压器
 *
 * 从 .deb 的 ar 归档中提取 control 和 data 两个压缩包。
 * 优先使用 libarchive 直接读取，回退到 ar 命令。
 */
class DebExtractor {
public:
    /**
     * @brief 解压 deb 包到工作目录
     * @param deb_path .deb 文件路径
     * @param work_dir 工作目录（将在此目录下创建子目录）
     * @return 解压结果（含 data 目录和 control 内容），失败返回 nullopt
     */
    static std::optional<DebExtractResult> Extract(
        const std::filesystem::path& deb_path,
        const std::filesystem::path& work_dir);

private:
    /**
     * @brief 使用 libarchive 直接提取（首选方案）
     *
     * libarchive 原生支持 ar 格式，可以直接遍历 ar 成员，
     * 对 control.tar.* 和 data.tar.* 自动解压。
     */
    static std::optional<DebExtractResult> ExtractWithLibarchive(
        const std::filesystem::path& deb_path,
        const std::filesystem::path& work_dir);

    /**
     * @brief 使用 ar 命令提取（回退方案）
     *
     * 当 libarchive 方式失败时，使用系统 ar 命令：
     *   ar x package.deb
     * 然后用 libarchive 解压 control.tar.* 和 data.tar.*。
     */
    static std::optional<DebExtractResult> ExtractWithAr(
        const std::filesystem::path& deb_path,
        const std::filesystem::path& work_dir);
};

} // namespace deb2pkg
