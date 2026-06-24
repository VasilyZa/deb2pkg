/**
 * @file deb_extractor.cpp
 * @brief DebExtractor 实现
 *
 * .deb 解压策略：
 * 1. 用 ar 命令提取 .deb 中的成员文件到临时目录
 * 2. 用 ArchiveReader（libarchive）解压 control.tar.* 中的 control 文件
 * 3. 用 ArchiveReader 解压 data.tar.* 到数据目录
 */

#include "deb_extractor.h"
#include "archive_reader.h"
#include "core/executor.h"
#include "core/logger.h"

#include <fstream>
#include <algorithm>

namespace deb2pkg {

std::optional<DebExtractResult> DebExtractor::Extract(
    const std::filesystem::path& deb_path,
    const std::filesystem::path& work_dir)
{
    Logger::Info("正在解压 Debian 软件包...");

    // 创建 deb 专用的临时子目录
    std::filesystem::path deb_work = work_dir / "deb_extract";
    std::error_code ec;
    if (!std::filesystem::create_directory(deb_work, ec)) {
        Logger::Error("无法创建解压目录：" + deb_work.string());
        return std::nullopt;
    }

    // 优先尝试 libarchive 方式
    auto result = ExtractWithLibarchive(deb_path, deb_work);
    if (result) {
        return result;
    }

    Logger::Info("  libarchive 方式未能获取完整数据，回退到 ar 命令方式...");
    return ExtractWithAr(deb_path, deb_work);
}

std::optional<DebExtractResult> DebExtractor::ExtractWithLibarchive(
    const std::filesystem::path& deb_path,
    const std::filesystem::path& work_dir)
{
    Logger::Verbose("使用 libarchive 方式解压 deb...");

    // 打开 .deb 文件（ar 格式）
    ArchiveReader ar_reader;
    if (!ar_reader.Open(deb_path)) {
        return std::nullopt;
    }

    // 首先提取 ar 中的成员文件到临时目录
    auto ar_members = ar_reader.ExtractAll(work_dir);

    if (ar_members.empty()) {
        Logger::Verbose("libarchive 未能提取 ar 成员，回退...");
        return std::nullopt;
    }

    // 查找 control.tar.* 和 data.tar.*
    std::filesystem::path control_tar_path;
    std::filesystem::path data_tar_path;

    for (const auto& member : ar_members) {
        std::string filename = member.filename().string();
        Logger::Verbose("  ar 成员: " + filename);

        if (filename.find("control.tar") != std::string::npos) {
            control_tar_path = member;
        } else if (filename.find("data.tar") != std::string::npos) {
            data_tar_path = member;
        }
    }

    if (control_tar_path.empty()) {
        Logger::Warning("未在 .deb 中找到 control.tar.* 文件");
        return std::nullopt;
    }
    if (data_tar_path.empty()) {
        Logger::Warning("未在 .deb 中找到 data.tar.* 文件");
        return std::nullopt;
    }

    // 解压 control.tar.* 获取 control 文件
    ArchiveReader ctrl_reader;
    if (!ctrl_reader.Open(control_tar_path)) {
        return std::nullopt;
    }

    auto control_data = ctrl_reader.ReadEntry("control");
    if (!control_data) {
        control_data = ctrl_reader.ReadEntry("./control");
    }
    ctrl_reader.Close();

    if (!control_data) {
        Logger::Warning("在 control.tar.* 中未找到 control 文件");
        return std::nullopt;
    }

    // 解压 data.tar.* 到 data 目录
    std::filesystem::path data_dir = work_dir / "data";
    std::error_code ec;
    if (!std::filesystem::create_directory(data_dir, ec)) {
        Logger::Error("无法创建数据目录：" + data_dir.string());
        return std::nullopt;
    }

    ArchiveReader data_reader;
    if (!data_reader.Open(data_tar_path)) {
        return std::nullopt;
    }

    auto extracted_files = data_reader.ExtractAll(data_dir);
    data_reader.Close();

    Logger::Info("  已提取 " + std::to_string(extracted_files.size()) + " 个文件");

    DebExtractResult result;
    result.extract_dir = data_dir;
    result.control_content = std::move(*control_data);
    return result;
}

std::optional<DebExtractResult> DebExtractor::ExtractWithAr(
    const std::filesystem::path& deb_path,
    const std::filesystem::path& work_dir)
{
    Logger::Verbose("使用 ar 命令方式解压 deb...");

    // 检查 ar 命令是否可用
    if (!Executor::IsCommandAvailable("ar")) {
        Logger::Error("未找到 ar 命令。请安装 binutils：sudo pacman -S binutils");
        return std::nullopt;
    }

    // 执行 ar x 提取所有成员
    Logger::Verbose("  执行: ar x " + deb_path.string());
    auto ar_result = Executor::RunCommand(
        {"ar", "x", deb_path.string()},
        work_dir,
        30
    );

    if (!ar_result.first) {
        Logger::Error("ar 命令执行失败：" + ar_result.second);
        return std::nullopt;
    }

    // 在工作目录下查找生成的文件
    std::filesystem::path control_tar_path;
    std::filesystem::path data_tar_path;

    for (const auto& entry : std::filesystem::directory_iterator(work_dir)) {
        std::string filename = entry.path().filename().string();
        if (filename.find("control.tar") != std::string::npos) {
            control_tar_path = entry.path();
        } else if (filename.find("data.tar") != std::string::npos) {
            data_tar_path = entry.path();
        }
    }

    if (control_tar_path.empty() || data_tar_path.empty()) {
        Logger::Error("ar 提取后未找到 control.tar.* 或 data.tar.*");
        Logger::Verbose("  目录内容：");
        for (const auto& entry : std::filesystem::directory_iterator(work_dir)) {
            Logger::Verbose("    " + entry.path().filename().string());
        }
        return std::nullopt;
    }

    // 解压 control.tar.* 获取 control 文件
    ArchiveReader ctrl_reader;
    if (!ctrl_reader.Open(control_tar_path)) {
        return std::nullopt;
    }
    auto control_data = ctrl_reader.ReadEntry("control");
    if (!control_data) {
        control_data = ctrl_reader.ReadEntry("./control");
    }
    ctrl_reader.Close();

    if (!control_data) {
        Logger::Warning("在 control.tar.* 中未找到 control 文件");
        return std::nullopt;
    }

    // 解压 data.tar.* 到 data 目录
    std::filesystem::path data_dir = work_dir / "data";
    std::error_code ec;
    if (!std::filesystem::create_directory(data_dir, ec)) {
        Logger::Error("无法创建数据目录：" + data_dir.string());
        return std::nullopt;
    }

    ArchiveReader data_reader;
    if (!data_reader.Open(data_tar_path)) {
        return std::nullopt;
    }
    auto extracted_files = data_reader.ExtractAll(data_dir);
    data_reader.Close();

    Logger::Info("  已提取 " + std::to_string(extracted_files.size()) + " 个文件");

    DebExtractResult result;
    result.extract_dir = data_dir;
    result.control_content = std::move(*control_data);
    return result;
}

} // namespace deb2pkg
