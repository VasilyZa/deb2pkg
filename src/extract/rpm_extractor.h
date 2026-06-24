/**
 * @file rpm_extractor.h
 * @brief RPM 软件包 (.rpm) 解压器
 *
 * RPM 文件结构：
 * 1. Lead (96 字节) — 魔数 0xedabeedb、版本号等
 * 2. Signature Header — GPG 签名（跳过）
 * 3. Header — 包元数据（名称、版本、依赖等，为二进制 key-value 结构）
 * 4. Payload — CPIO 归档（实际数据文件）
 *
 * 解压策略：
 * — 元数据：手动解析 RPM 原始二进制 Header 结构
 * — Payload：优先用 libarchive，回退到 rpm2cpio | cpio
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>

namespace deb2pkg {

/**
 * @brief RPM 解压结果
 */
struct RpmExtractResult {
    std::filesystem::path extract_dir;       ///< payload 数据解压目录
    std::vector<char> raw_header;            ///< RPM header 区域原始二进制数据
};

/**
 * @brief RPM 包解压器
 *
 * 负责从 .rpm 文件中提取 payload 数据文件和 header 元数据。
 * 不调用 rpm 安装命令，不污染宿主系统。
 */
class RpmExtractor {
public:
    /**
     * @brief 解压 RPM 包到工作目录
     * @param rpm_path .rpm 文件路径
     * @param work_dir 工作目录（将在此目录下创建子目录）
     * @return 解压结果（含 data 目录和 RPM header 数据），失败返回 nullopt
     */
    static std::optional<RpmExtractResult> Extract(
        const std::filesystem::path& rpm_path,
        const std::filesystem::path& work_dir);

private:
    /**
     * @brief 从 RPM 文件中读取 header 区域
     * @param rpm_path RPM 文件路径
     * @return header 二进制数据，失败返回 nullopt
     *
     * 解析 RPM 文件前部的 Lead + Signature Header，
     * 定位并读取真正的 Header 数据块。
     */
    static std::optional<std::vector<char>> ReadRawHeader(
        const std::filesystem::path& rpm_path);

    /**
     * @brief 使用 libarchive 提取 RPM payload（首选方案）
     *
     * libarchive 原生支持读取 RPM 文件格式，
     * 可以自动跳过 Header 并提取 CPIO payload。
     */
    static std::optional<std::filesystem::path> ExtractPayloadWithLibarchive(
        const std::filesystem::path& rpm_path,
        const std::filesystem::path& dest_dir);

    /**
     * @brief 使用 rpm2cpio + cpio 提取（回退方案）
     *
     * 需要系统安装 rpm-tools 和 cpio 包。
     */
    static std::optional<std::filesystem::path> ExtractPayloadWithRpm2cpio(
        const std::filesystem::path& rpm_path,
        const std::filesystem::path& dest_dir);
};

} // namespace deb2pkg
