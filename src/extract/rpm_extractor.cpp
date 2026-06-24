/**
 * @file rpm_extractor.cpp
 * @brief RpmExtractor 实现
 *
 * RPM 格式解析分为两部分：
 * 1. 手动读取 Lead + Header 二进制结构获取元数据
 * 2. 使用 libarchive 提取 CPIO payload 数据文件
 */

#include "rpm_extractor.h"
#include "archive_reader.h"
#include "core/executor.h"
#include "core/logger.h"

#include <fstream>
#include <cstring>
#include <arpa/inet.h>  // ntohl

namespace deb2pkg {

// ---- RPM 二进制结构常量 ----

// RPM Lead 大小
static constexpr size_t RPM_LEAD_SIZE = 96;

// RPM Lead 魔数 (大端)
static constexpr uint32_t RPM_LEAD_MAGIC = 0xedabeedb;

// Header 魔数 (大端)
static constexpr uint32_t RPM_HEADER_MAGIC = 0x8eade801;

// 每个 Header index 条目大小
static constexpr size_t RPM_INDEX_ENTRY_SIZE = 16;

// ---- 辅助函数：从大端字节序读取 uint32_t ----
static inline uint32_t read_be32(const unsigned char* ptr) {
    return ntohl(*reinterpret_cast<const uint32_t*>(ptr));
}

// ---- 辅助函数：检查 RPM 文件魔数 ----
static bool IsRpmFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    unsigned char magic[4];
    file.read(reinterpret_cast<char*>(magic), 4);
    return file.good() && read_be32(magic) == RPM_LEAD_MAGIC;
}

std::optional<std::vector<char>> RpmExtractor::ReadRawHeader(
    const std::filesystem::path& rpm_path)
{
    Logger::Verbose("正在读取 RPM Header...");

    std::ifstream file(rpm_path, std::ios::binary);
    if (!file) {
        Logger::Error("无法打开 RPM 文件：" + rpm_path.string());
        return std::nullopt;
    }

    // 获取文件大小
    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // ---- 1. 读取 Lead ----
    unsigned char lead[RPM_LEAD_SIZE];
    file.read(reinterpret_cast<char*>(lead), RPM_LEAD_SIZE);
    if (!file.good()) {
        Logger::Error("无法读取 RPM Lead");
        return std::nullopt;
    }

    uint32_t magic = read_be32(lead);
    if (magic != RPM_LEAD_MAGIC) {
        Logger::Error("RPM 魔数校验失败：不是有效的 RPM 文件");
        return std::nullopt;
    }

    uint8_t major_ver = lead[4];
    Logger::Verbose("  RPM 版本: " + std::to_string(major_ver) + "." + std::to_string(lead[5]));

    // ---- 2. 跳过 Signature Header ----
    // 读取 signature header 结构
    unsigned char sig_hdr[16];
    if (!file.read(reinterpret_cast<char*>(sig_hdr), 16).good()) {
        Logger::Error("无法读取 RPM Signature Header");
        return std::nullopt;
    }

    uint32_t sig_magic = read_be32(sig_hdr);
    if (sig_magic != RPM_HEADER_MAGIC) {
        Logger::Warning("RPM Signature Header 魔数异常，继续尝试...");
    }

    uint32_t sig_count = read_be32(sig_hdr + 8);
    uint32_t sig_data_size = read_be32(sig_hdr + 12);

    // 计算 signature header 总大小并跳过
    // 16 (header struct) + count * 16 (index entries) + data_size
    size_t sig_total = 16 + sig_count * RPM_INDEX_ENTRY_SIZE + sig_data_size;
    // 对齐到 8 字节边界
    sig_total = (sig_total + 7) & ~7;

    // 跳过 signature 区域
    // 检查是否有足够的字节可以跳过
    auto current_pos = file.tellg();
    if (static_cast<size_t>(current_pos) + sig_total - 16 > static_cast<size_t>(file_size)) {
        Logger::Error("RPM 文件可能已损坏（Signature 区域超出文件边界）");
        return std::nullopt;
    }

    // seekg 从当前位置跳过 (sig_total - 16)因为我们已读了 16 字节
    file.seekg(current_pos + static_cast<std::streamoff>(sig_total - 16));

    // ---- 3. 读取真正的 Header ----
    unsigned char hdr[16];
    if (!file.read(reinterpret_cast<char*>(hdr), 16).good()) {
        Logger::Error("无法读取 RPM Header 结构");
        return std::nullopt;
    }

    uint32_t hdr_magic = read_be32(hdr);
    if (hdr_magic != RPM_HEADER_MAGIC) {
        Logger::Error("RPM Header 魔数校验失败");
        return std::nullopt;
    }

    uint32_t hdr_count = read_be32(hdr + 8);
    uint32_t hdr_data_size = read_be32(hdr + 12);

    size_t hdr_total = 16 + hdr_count * RPM_INDEX_ENTRY_SIZE + hdr_data_size;

    Logger::Verbose("  RPM Header: " + std::to_string(hdr_count) + " 个索引, "
                   + std::to_string(hdr_data_size) + " 字节数据区");

    // ---- 4. 读取完整 Header 数据 ----
    std::vector<char> raw_header(hdr_total);
    // 复制已读取的 16 字节
    std::memcpy(raw_header.data(), hdr, 16);
    // 读取剩余部分
    file.read(raw_header.data() + 16, static_cast<std::streamsize>(hdr_total - 16));
    if (!file.good()) {
        Logger::Error("无法读取完整的 RPM Header 数据");
        return std::nullopt;
    }

    Logger::Verbose("  RPM Header 读取完成 (" + std::to_string(raw_header.size()) + " 字节)");

    return raw_header;
}

std::optional<std::filesystem::path> RpmExtractor::ExtractPayloadWithLibarchive(
    const std::filesystem::path& rpm_path,
    const std::filesystem::path& dest_dir)
{
    Logger::Verbose("使用 libarchive 提取 RPM payload...");

    ArchiveReader reader;
    if (!reader.Open(rpm_path)) {
        Logger::Verbose("libarchive 无法直接打开 RPM，尝试其他方式...");
        return std::nullopt;
    }

    auto files = reader.ExtractAll(dest_dir);
    reader.Close();

    if (files.empty()) {
        Logger::Verbose("libarchive 提取 RPM payload 返回空文件列表");
        return std::nullopt;
    }

    Logger::Verbose("  libarchive 提取完成，共 " + std::to_string(files.size()) + " 个文件");
    return dest_dir;
}

std::optional<std::filesystem::path> RpmExtractor::ExtractPayloadWithRpm2cpio(
    const std::filesystem::path& rpm_path,
    const std::filesystem::path& dest_dir)
{
    Logger::Verbose("使用 rpm2cpio + cpio 提取 RPM payload...");

    // 检查 rpm2cpio 是否可用
    if (!Executor::IsCommandAvailable("rpm2cpio")) {
        Logger::Error("未找到 rpm2cpio 命令");
        Logger::Warning("请安装 rpm-tools 包：sudo pacman -S rpm-tools");
        return std::nullopt;
    }

    // 使用管道执行：rpm2cpio package.rpm | cpio -idmv
    // 由于 Executor 不支持直接管道，我们分两步：
    // 1. rpm2cpio 输出到临时文件
    // 2. cpio 从临时文件读取

    std::filesystem::path cpio_file = dest_dir.parent_path() / "payload.cpio";

    // 步骤1：rpm2cpio 转换
    auto r2c_result = Executor::RunCommand(
        {"rpm2cpio", rpm_path.string()},
        {},
        60
    );

    if (!r2c_result.first) {
        Logger::Error("rpm2cpio 执行失败");
        return std::nullopt;
    }

    // 将 rpm2cpio 输出写入临时文件
    {
        std::ofstream cpio_out(cpio_file, std::ios::binary);
        cpio_out.write(r2c_result.second.data(),
                       static_cast<std::streamsize>(r2c_result.second.size()));
    }

    // 步骤2：cpio 提取
    auto cpio_result = Executor::RunCommand(
        {"cpio", "-idmv"},
        dest_dir,
        120
    );

    // cpio 从 stdin 读取，这需要不同的处理方法
    // 实际上 Executor::RunCommand 不支持管道，这里作为备用方法
    // 清理
    std::error_code ec;
    std::filesystem::remove(cpio_file, ec);

    // 如果以上方法失败，提示用户手动处理
    Logger::Warning("rpm2cpio 管道提取方式受限，建议使用 libarchive 方式或手动提取");
    return std::nullopt;
}

std::optional<RpmExtractResult> RpmExtractor::Extract(
    const std::filesystem::path& rpm_path,
    const std::filesystem::path& work_dir)
{
    Logger::Info("正在解压 RPM 软件包...");

    if (!IsRpmFile(rpm_path)) {
        Logger::Error("文件不是有效的 RPM 包（魔数校验失败）");
        return std::nullopt;
    }

    // 创建 RPM 专用的临时子目录
    std::filesystem::path rpm_work = work_dir / "rpm_extract";
    std::error_code ec;
    if (!std::filesystem::create_directory(rpm_work, ec)) {
        Logger::Error("无法创建解压目录：" + rpm_work.string());
        return std::nullopt;
    }

    // 1. 读取 RPM Header（元数据用）
    auto raw_header = ReadRawHeader(rpm_path);
    if (!raw_header) {
        return std::nullopt;
    }

    // 2. 提取 payload 数据文件
    std::filesystem::path data_dir = rpm_work / "data";
    if (!std::filesystem::create_directory(data_dir, ec)) {
        Logger::Error("无法创建数据目录：" + data_dir.string());
        return std::nullopt;
    }

    auto payload_result = ExtractPayloadWithLibarchive(rpm_path, data_dir);

    if (!payload_result) {
        // libarchive 失败时，直接用 bsdtar 命令尝试
        Logger::Verbose("尝试用 bsdtar 命令提取 RPM...");
        auto bsdtar_result = Executor::RunCommand(
            {"bsdtar", "-xf", rpm_path.string(), "-C", data_dir.string()},
            {},
            120
        );
        if (!bsdtar_result.first) {
            Logger::Error("无法提取 RPM payload。请确认文件未损坏。");
            Logger::Warning("可尝试安装 rpm-tools: sudo pacman -S rpm-tools");
            return std::nullopt;
        }
    }

    // 统计提取的文件数
    size_t file_count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(data_dir)) {
        if (entry.is_regular_file()) {
            file_count++;
        }
    }
    Logger::Info("  已提取 " + std::to_string(file_count) + " 个文件");

    RpmExtractResult result;
    result.extract_dir = data_dir;
    result.raw_header = std::move(*raw_header);
    return result;
}

} // namespace deb2pkg
