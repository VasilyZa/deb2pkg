/**
 * @file archive_reader.cpp
 * @brief ArchiveReader 实现 — 基于 libarchive 的归档读取封装
 *
 * 使用 libarchive C API 提供统一的归档提取能力。
 * 支持 tar、cpio、RPM 等格式及 gzip/xz/bzip2/zstd 等压缩。
 */

#include "archive_reader.h"
#include "core/logger.h"

#include <archive.h>
#include <archive_entry.h>
#include <cstring>
#include <fstream>

namespace deb2pkg {

ArchiveReader::ArchiveReader()
    : archive_(nullptr)
    , disk_writer_(nullptr)
    , opened_(false)
{
}

ArchiveReader::~ArchiveReader() {
    Close();
}

ArchiveReader::ArchiveReader(ArchiveReader&& other) noexcept
    : archive_(other.archive_)
    , disk_writer_(other.disk_writer_)
    , opened_(other.opened_)
{
    other.archive_ = nullptr;
    other.disk_writer_ = nullptr;
    other.opened_ = false;
}

ArchiveReader& ArchiveReader::operator=(ArchiveReader&& other) noexcept {
    if (this != &other) {
        Close();
        archive_ = other.archive_;
        disk_writer_ = other.disk_writer_;
        opened_ = other.opened_;
        other.archive_ = nullptr;
        other.disk_writer_ = nullptr;
        other.opened_ = false;
    }
    return *this;
}

bool ArchiveReader::Open(const std::filesystem::path& path) {
    Close();

    // 创建读取器
    archive_ = archive_read_new();
    if (!archive_) {
        Logger::Error("无法创建 libarchive 读取器");
        return false;
    }

    // 支持所有常见格式
    archive_read_support_format_tar(archive_);
    archive_read_support_format_cpio(archive_);
    archive_read_support_format_gnutar(archive_);
    archive_read_support_format_zip(archive_);
    archive_read_support_format_ar(archive_);

    // 支持所有常见压缩格式
    archive_read_support_filter_gzip(archive_);
    archive_read_support_filter_xz(archive_);
    archive_read_support_filter_bzip2(archive_);
    archive_read_support_filter_lzma(archive_);
    archive_read_support_filter_zstd(archive_);
    archive_read_support_filter_compress(archive_);
    archive_read_support_filter_lz4(archive_);
    archive_read_support_filter_lzop(archive_);

    // 打开文件（10240 是建议的块大小）
    int rc = archive_read_open_filename(archive_, path.c_str(), 10240);
    if (rc != ARCHIVE_OK) {
        Logger::Error("无法打开归档文件：" + path.string());
        Logger::Verbose("  libarchive 错误：" + std::string(archive_error_string(archive_)));
        archive_read_free(archive_);
        archive_ = nullptr;
        return false;
    }

    opened_ = true;
    Logger::Verbose("成功打开归档文件：" + path.string());
    return true;
}

std::filesystem::path ArchiveReader::SanitizePath(
    const std::filesystem::path& entry_path,
    const std::filesystem::path& dest_dir)
{
    // 移除开头的 "./" 或 "/"
    std::string path_str = entry_path.string();
    if (path_str.empty()) return {};

    // 去掉开头的 "./"
    if (path_str.size() >= 2 && path_str[0] == '.' && path_str[1] == '/') {
        path_str = path_str.substr(2);
    }
    // 去掉开头的 "/"
    if (!path_str.empty() && path_str[0] == '/') {
        path_str = path_str.substr(1);
    }

    // 安全检查：拒绝包含 ".." 的路径（防止路径穿越）
    std::filesystem::path clean_path(path_str);
    for (const auto& part : clean_path) {
        if (part == "..") {
            Logger::Warning("检测到可疑路径穿越，已跳过： " + path_str);
            return {};
        }
    }

    return dest_dir / clean_path;
}

std::vector<std::filesystem::path> ArchiveReader::ExtractAll(
    const std::filesystem::path& dest_dir)
{
    std::vector<std::filesystem::path> extracted_files;

    if (!opened_ || !archive_) {
        Logger::Error("归档文件未打开，无法提取");
        return extracted_files;
    }

    // 创建磁盘写入器
    disk_writer_ = archive_write_disk_new();
    if (!disk_writer_) {
        Logger::Error("无法创建磁盘写入器");
        return extracted_files;
    }

    // 设置写入选项：保留权限、时间戳，但不保留所有者
    int flags = ARCHIVE_EXTRACT_TIME
              | ARCHIVE_EXTRACT_PERM
              | ARCHIVE_EXTRACT_ACL
              | ARCHIVE_EXTRACT_FFLAGS
              | ARCHIVE_EXTRACT_SECURE_SYMLINKS
              | ARCHIVE_EXTRACT_SECURE_NODOTDOT;
    archive_write_disk_set_options(disk_writer_, flags);
    archive_write_disk_set_standard_lookup(disk_writer_);

    struct archive_entry* entry;
    int rc;

    Logger::Verbose("开始提取文件到：" + dest_dir.string());

    // 遍历归档中所有条目
    while ((rc = archive_read_next_header(archive_, &entry)) == ARCHIVE_OK) {
        // 获取条目路径并安全检查
        const char* entry_pathname = archive_entry_pathname(entry);
        if (!entry_pathname) continue;

        std::filesystem::path safe_path = SanitizePath(entry_pathname, dest_dir);
        if (safe_path.empty()) continue;

        // 更新条目路径为安全的绝对路径
        archive_entry_set_pathname(entry, safe_path.c_str());

        // 写入磁盘
        rc = archive_write_header(disk_writer_, entry);
        if (rc == ARCHIVE_OK) {
            // 复制数据（仅对普通文件）
            if (archive_entry_size(entry) > 0) {
                const void* buff;
                size_t size;
                la_int64_t offset;
                while ((rc = archive_read_data_block(archive_, &buff, &size, &offset)) == ARCHIVE_OK) {
                    ssize_t written = archive_write_data_block(disk_writer_, buff, size, offset);
                    if (written < 0) {
                        Logger::Warning("写入文件数据失败：" + safe_path.string());
                        break;
                    }
                }
                if (rc != ARCHIVE_EOF) {
                    Logger::Warning("读取归档数据失败：" + safe_path.string());
                }
            }

            // 完成当前条目
            archive_write_finish_entry(disk_writer_);
            extracted_files.push_back(safe_path);
        } else {
            Logger::Warning("无法提取文件：" + safe_path.string()
                          + " (" + std::string(archive_error_string(disk_writer_)) + ")");
        }
    }

    if (rc != ARCHIVE_EOF) {
        Logger::Warning("归档读取未正常结束："
                      + std::string(archive_error_string(archive_)));
    }

    Logger::Verbose("提取完成，共 " + std::to_string(extracted_files.size()) + " 个文件");
    return extracted_files;
}

std::optional<std::vector<char>> ArchiveReader::ReadEntry(
    const std::string& entry_name)
{
    if (!opened_ || !archive_) {
        Logger::Error("归档文件未打开");
        return std::nullopt;
    }

    // 重新打开归档以从头读取（libarchive 流式，无法回退）
    // 注意：此操作会使 ExtractAll 之后再调用 ReadEntry 有问题
    // 实际使用中应在 ExtractAll 之前调用 ReadEntry

    struct archive_entry* entry;
    int rc;

    while ((rc = archive_read_next_header(archive_, &entry)) == ARCHIVE_OK) {
        const char* pathname = archive_entry_pathname(entry);
        if (!pathname) continue;

        // 匹配条目名（支持 "./control" 和 "control" 两种形式）
        std::string current(pathname);
        if (current == entry_name ||
            current == "./" + entry_name ||
            current == entry_name.substr(entry_name.find_first_not_of("./"))) {

            // 检查是否是目录
            if (archive_entry_filetype(entry) == AE_IFDIR) {
                return std::nullopt;
            }

            // 读取条目内容到内存
            la_int64_t size = archive_entry_size(entry);
            if (size <= 0 || size > 10 * 1024 * 1024) {  // 限制最大 10MB
                Logger::Warning("条目过大或为空，跳过：" + std::string(pathname));
                return std::nullopt;
            }

            std::vector<char> data(static_cast<size_t>(size));
            ssize_t bytes_read = archive_read_data(archive_, data.data(), data.size());
            if (bytes_read < 0) {
                Logger::Error("读取条目失败：" + std::string(pathname));
                return std::nullopt;
            }
            data.resize(static_cast<size_t>(bytes_read));
            return data;
        }
    }

    return std::nullopt;  // 未找到匹配条目
}

void ArchiveReader::Close() {
    if (disk_writer_) {
        archive_write_free(disk_writer_);
        disk_writer_ = nullptr;
    }
    if (archive_) {
        archive_read_close(archive_);
        archive_read_free(archive_);
        archive_ = nullptr;
    }
    opened_ = false;
}

} // namespace deb2pkg
