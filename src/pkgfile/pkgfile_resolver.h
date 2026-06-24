/**
 * @file pkgfile_resolver.h
 * @brief pkgfile 调用封装 — 将共享库 soname 映射为 Arch 官方包名
 *
 * pkgfile 是 Arch Linux 的包文件查询工具，
 * 可以根据文件名反向查找所属的软件包。
 *
 * 调用方式：pkgfile -s <soname>
 * 输出格式：core/glibc
 *          extra/gtk3
 * 未找到时输出为空。
 */

#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <set>
#include <optional>

namespace deb2pkg {

/**
 * @brief pkgfile 查询结果
 */
struct PkgfileMatch {
    std::string soname;     ///< 查询的 soname
    std::string pkgname;    ///< Arch 包名（如 "glibc"）
    std::string repo;       ///< 所属仓库（如 "core"）
    bool success{false};    ///< 是否成功匹配
};

/**
 * @brief pkgfile 依赖解析器
 *
 * 检查 pkgfile 工具可用性，查询缓存状态，
 * 对每个 soname 调用 pkgfile 查询所属 Arch 包名。
 */
class PkgfileResolver {
public:
    /**
     * @brief 检查 pkgfile 是否已安装且可用
     */
    static bool IsAvailable();

    /**
     * @brief 检查 pkgfile 缓存是否已就绪
     */
    static bool IsCacheReady();

    /**
     * @brief 查询单个 soname 所属的 Arch 包
     * @param soname 库文件名（如 "libc.so.6"）
     * @return 匹配结果，失败返回 nullopt
     */
    static std::optional<PkgfileMatch> Resolve(const std::string& soname);

    /**
     * @brief 批量解析 soname 列表
     * @param sonames 去重后的 soname 集合
     * @param unresolved [out] 未能匹配的 soname 列表
     * @return 匹配到的 Arch 包名列表（去重）
     */
    static std::vector<std::string> ResolveAll(
        const std::set<std::string>& sonames,
        std::vector<std::string>& unresolved);
};

} // namespace deb2pkg
