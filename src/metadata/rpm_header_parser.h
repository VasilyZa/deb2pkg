/**
 * @file rpm_header_parser.h
 * @brief RPM 二进制 Header 解析器
 *
 * 手动解析 RPM 包中的 Header 二进制结构，
 * 无需依赖 rpm 库或 rpm 命令即可提取元数据。
 *
 * RPM Header 结构：
 *   struct rpm_header {
 *       uint32_t magic;      // 0x8eade801 (大端)
 *       uint32_t reserved;
 *       uint32_t index_count; // 索引条目数
 *       uint32_t data_size;   // 数据区字节数
 *   };
 *   后跟 index_count * 16 字节的索引条目，然后数据区。
 *
 * 每个索引条目 16 字节：
 *   struct rpm_index {
 *       uint32_t tag;    // RPM 标签 ID
 *       uint32_t type;   // 数据类型
 *       int32_t  offset; // 数据区偏移
 *       uint32_t count;  // 元素数量
 *   };
 */

#pragma once

#include "core/types.h"
#include <vector>
#include <filesystem>

namespace deb2pkg {

/**
 * @brief RPM Header 解析器
 *
 * 解析从 .rpm 文件中提取的 Header 二进制数据。
 * 支持 RPM v3 格式。
 */
class RpmHeaderParser {
public:
    /**
     * @brief 解析 RPM Header 数据
     * @param raw_header 从 RPM 文件读取的 Header 二进制数据
     * @param extract_dir 已解压的数据目录（用于备用信息推断）
     * @return 解析后的 PackageInfo
     */
    static PackageInfo Parse(const std::vector<char>& raw_header,
                             const std::filesystem::path& extract_dir);
};

} // namespace deb2pkg
