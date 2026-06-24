/**
 * @file deb_control_parser.h
 * @brief Debian control 文件解析器
 *
 * 解析 deb 包中的 control 文件（RFC 822 风格格式）。
 * 从中提取软件名、版本、架构、描述、许可证等元数据。
 */

#pragma once

#include "core/types.h"
#include <vector>
#include <string>

namespace deb2pkg {

/**
 * @brief Debian control 文件解析器
 *
 * 解析 RFC 822 格式的 control 文件，提取 PackageInfo。
 * 格式示例：
 *   Package: vlc
 *   Version: 3.0.20-1
 *   Architecture: amd64
 *   Description: multimedia player and streamer
 *   License: GPL-2+
 */
class DebControlParser {
public:
    /**
     * @brief 解析 control 文件内容
     * @param raw_control control 文件的原始字节数据
     * @return 解析后的 PackageInfo
     */
    static PackageInfo Parse(const std::vector<char>& raw_control);
};

} // namespace deb2pkg
