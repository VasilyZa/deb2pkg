/**
 * @file pkgbuild_writer.h
 * @brief PKGBUILD 文件生成模块
 *
 * 根据 PackageInfo 生成符合 Arch Linux AUR 规范的 PKGBUILD 文件。
 * 输出的 PKGBUILD 可直接运行 makepkg 命令构建。
 *
 * AUR PKGBUILD 规范参考：
 * https://wiki.archlinux.org/title/PKGBUILD
 */

#pragma once

#include "core/types.h"
#include <string>
#include <filesystem>

namespace deb2pkg {

/**
 * @brief PKGBUILD 生成器
 *
 * 使用模板生成 AUR 兼容的 PKGBUILD 内容。
 * 负责格式化所有字段、生成 package() 函数。
 */
class PkgbuildWriter {
public:
    /**
     * @brief 生成 PKGBUILD 内容
     * @param info 包元数据
     * @return PKGBUILD 完整文本内容
     */
    static std::string Generate(const PackageInfo& info);

    /**
     * @brief 将 PKGBUILD 写入文件
     * @param content PKGBUILD 文本内容
     * @param output_dir 输出目录（将创建 pkgname 子目录）
     * @param pkgname 包名
     * @return 写入的 PKGBUILD 文件完整路径
     */
    static std::filesystem::path WriteToFile(
        const std::string& content,
        const std::filesystem::path& output_dir,
        const std::string& pkgname);

    /**
     * @brief 生成 .install 安装脚本（用于刷新图标缓存和 desktop 数据库）
     * @return .install 文件内容
     */
    static std::string GenerateInstallScript();

private:
    /**
     * @brief 计算文件的 SHA256 校验和
     * @param file_path 文件路径
     * @return SHA256 十六进制字符串
     */
    static std::string ComputeSha256(const std::filesystem::path& file_path);

    /**
     * @brief 格式化 Bash 数组字符串
     * @param items 数组元素
     * @return 格式如 "('item1' 'item2' 'item3')"
     */
    static std::string FormatBashArray(const std::vector<std::string>& items);

    /**
     * @brief 转义字符串中的特殊字符（用于 Bash 单引号字符串）
     */
    static std::string EscapeBash(const std::string& str);
};

} // namespace deb2pkg
