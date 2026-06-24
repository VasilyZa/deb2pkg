/**
 * @file pkgfile_resolver.cpp
 * @brief PkgfileResolver 实现
 *
 * 通过调用 pkgfile -s 命令查询库文件所属的 Arch 包。
 * 结果去重并过滤系统基础库（如 glibc 通常作为隐式依赖）。
 */

#include "pkgfile_resolver.h"
#include "core/executor.h"
#include "core/logger.h"

#include <algorithm>
#include <unordered_set>

namespace deb2pkg {

bool PkgfileResolver::IsAvailable() {
    return Executor::IsCommandAvailable("pkgfile");
}

bool PkgfileResolver::IsCacheReady() {
    // 检查 pkgfile 数据库是否存在
    auto result = Executor::RunCommand(
        {"pkgfile", "--update"},
        {},
        5
    );
    // pkgfile --update 在已有缓存时也会输出
    // 更好的方式是检查 /var/cache/pkgfile/ 目录
    auto check_result = Executor::RunCommand(
        {"sh", "-c", "ls /var/cache/pkgfile/*.files 2>/dev/null || echo EMPTY"},
        {},
        5
    );
    if (check_result.first) {
        return check_result.second.find("EMPTY") == std::string::npos;
    }
    return false;
}

std::optional<PkgfileMatch> PkgfileResolver::Resolve(const std::string& soname) {
    if (!IsAvailable()) {
        return std::nullopt;
    }

    // 执行 pkgfile -s <soname>
    auto result = Executor::RunCommandLines(
        {"pkgfile", "-s", soname},
        {},
        15
    );

    if (!result.first || result.second.empty()) {
        return std::nullopt;
    }

    PkgfileMatch match;
    match.soname = soname;

    // 解析输出：repo/pkgname
    // 例如: "core/glibc" 或 "extra/gtk3"
    for (const auto& line : result.second) {
        auto slash = line.find('/');
        if (slash == std::string::npos) continue;

        std::string repo = line.substr(0, slash);
        // 包名在后面，可能有空格（如 "core/glibc" 后面有空格）
        auto space = line.find_first_of(" \t\r\n", slash + 1);
        std::string pkg = (space != std::string::npos)
            ? line.substr(slash + 1, space - slash - 1)
            : line.substr(slash + 1);

        if (pkg.empty()) continue;

        // 跳过 lib32 包（当目标是 x86_64 时）
        if (pkg.find("lib32-") == 0) {
            continue;
        }

        // 取第一个非 lib32 的结果
        match.repo = repo;
        match.pkgname = pkg;
        match.success = true;
        break;
    }

    if (!match.success) {
        return std::nullopt;
    }

    Logger::Verbose("  pkgfile: " + soname + " → " + match.repo + "/" + match.pkgname);
    return match;
}

std::vector<std::string> PkgfileResolver::ResolveAll(
    const std::set<std::string>& sonames,
    std::vector<std::string>& unresolved)
{
    std::vector<std::string> depends;
    unresolved.clear();

    if (sonames.empty()) {
        return depends;
    }

    // 检查 pkgfile 是否可用
    if (!IsAvailable()) {
        Logger::Warning("pkgfile 未安装，无法自动匹配依赖包名");
        Logger::Warning("请安装 pkgfile: sudo pacman -S pkgfile");
        Logger::Warning("安装后运行: sudo pkgfile --update");

        // 将所有 sonames 标记为未匹配
        unresolved.assign(sonames.begin(), sonames.end());
        return depends;
    }

    if (!IsCacheReady()) {
        Logger::Warning("pkgfile 缓存为空，正在尝试更新...");
        auto update_result = Executor::RunCommand(
            {"sudo", "pkgfile", "--update"},
            {},
            120
        );
        if (!update_result.first) {
            Logger::Warning("pkgfile 缓存更新失败，请手动运行：sudo pkgfile --update");
        }
    }

    Logger::Info("正在查询 " + std::to_string(sonames.size()) + " 个共享库的 Arch 包名...");

    int resolved_count = 0;
    std::unordered_set<std::string> seen_pkgs;

    for (const auto& soname : sonames) {
        auto match = Resolve(soname);
        if (match && match->success) {
            if (seen_pkgs.find(match->pkgname) == seen_pkgs.end()) {
                depends.push_back(match->pkgname);
                seen_pkgs.insert(match->pkgname);
            }
            resolved_count++;
        } else {
            unresolved.push_back(soname);
        }
    }

    // 排序保持一致性
    std::sort(depends.begin(), depends.end());

    Logger::Info("  已匹配 " + std::to_string(resolved_count) + "/"
                + std::to_string(sonames.size()) + " 个共享库");

    if (!unresolved.empty()) {
        Logger::Warning("以下 " + std::to_string(unresolved.size())
                       + " 个共享库未能自动匹配 Arch 包名：");
        for (const auto& soname : unresolved) {
            Logger::Warning("  - " + soname);
        }
        Logger::Warning("请手动在 PKGBUILD 的 depends 数组中添加对应的包名");
    }

    return depends;
}

} // namespace deb2pkg
