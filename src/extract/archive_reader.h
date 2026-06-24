/**
 * @file archive_reader.h
 * @brief libarchive 封装 — 统一处理各种归档格式的提取
 *
 * 基于 libarchive 库，支持：
 * - .tar 及各压缩变体 (.tar.gz, .tar.xz, .tar.bz2, .tar.zst)
 * - CPIO 归档（RPM payload 内部格式）
 * - RPM 包直接读取（通过 ARCHIVE_FILTER_RPM）
 * - 自动检测压缩和归档格式
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <optional>

// 前向声明，避免在头文件中暴露 libarchive
struct archive;
struct archive_entry;

namespace deb2pkg {

/**
 * @brief libarchive 读取封装
 *
 * RAII 风格管理 archive 对象生命周期，
 * 提供统一的接口提取不同格式的归档文件。
 */
class ArchiveReader {
public:
    ArchiveReader();
    ~ArchiveReader();

    // 禁止拷贝
    ArchiveReader(const ArchiveReader&) = delete;
    ArchiveReader& operator=(const ArchiveReader&) = delete;

    // 允许移动
    ArchiveReader(ArchiveReader&& other) noexcept;
    ArchiveReader& operator=(ArchiveReader&& other) noexcept;

    /**
     * @brief 打开归档文件
     * @param path 归档文件路径
     * @return true 表示成功打开
     *
     * libarchive 会自动检测格式和压缩方式。
     * 支持 tar/cpio/rpm 格式和 gzip/xz/bzip2/zstd 压缩。
     */
    bool Open(const std::filesystem::path& path);

    /**
     * @brief 将归档中所有文件提取到目标目录
     * @param dest_dir 目标目录（必须已存在）
     * @return 成功提取的文件路径列表（相对于 dest_dir）
     */
    std::vector<std::filesystem::path> ExtractAll(
        const std::filesystem::path& dest_dir);

    /**
     * @brief 读取归档中指定名称的条目内容
     * @param entry_name 条目名（如 "control" 或 "./control"）
     * @return 文件内容的字节数组，找不到返回 nullopt
     */
    std::optional<std::vector<char>> ReadEntry(
        const std::string& entry_name);

    /**
     * @brief 关闭归档文件
     */
    void Close();

    /**
     * @brief 检查是否已成功打开归档
     */
    bool IsOpen() const { return opened_; }

private:
    struct archive* archive_;       ///< libarchive 读取器
    struct archive* disk_writer_;   ///< libarchive 磁盘写入器
    bool opened_;

    /**
     * @brief 验证路径安全，防止路径穿越攻击
     * @param entry_path 条目原始路径
     * @param dest_dir 目标根目录
     * @return 安全的绝对路径，如果路径穿越则返回空
     */
    std::filesystem::path SanitizePath(
        const std::filesystem::path& entry_path,
        const std::filesystem::path& dest_dir);
};

} // namespace deb2pkg
