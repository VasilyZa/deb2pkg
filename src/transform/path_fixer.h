/**
 * @file path_fixer.h
 * @brief 路径修复模块 — 修正不同发行版之间的目录路径差异
 *
 * 处理以下路径转换：
 *   /usr/lib/x86_64-linux-gnu/ → /usr/lib/
 *   /usr/lib/i386-linux-gnu/   → /usr/lib/
 *   /usr/lib/aarch64-linux-gnu/→ /usr/lib/
 *   /usr/lib64/                 → /usr/lib/
 *   /lib/x86_64-linux-gnu/      → /usr/lib/
 *   /lib64/                     → /usr/lib/
 *
 * 通过重命名/移动文件实现，不复制，保留内容不变。
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace deb2pkg {

/**
 * @brief 路径修复器
 *
 * 在提取出的文件树中搜索并修正发行版特有的库文件路径。
 */
class PathFixer {
public:
    /**
     * @brief 执行路径修正
     * @param extract_dir 提取文件根目录（如 work_dir/data/）
     * @param warnings [out] 修正过程中产生的警告信息
     * @return 修正的路径数量
     */
    static int FixPaths(const std::filesystem::path& extract_dir,
                        std::vector<std::string>& warnings);
};

} // namespace deb2pkg
