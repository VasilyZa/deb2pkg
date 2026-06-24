/**
 * @file path_fixer.cpp
 * @brief PathFixer 实现
 *
 * 在提取的文件树中扫描并修正跨发行版路径差异。
 * 通过重命名（rename）移动文件并清理空目录。
 */

#include "path_fixer.h"
#include "core/logger.h"

#include <algorithm>
#include <set>
#include <cstring>

namespace deb2pkg {

/**
 * @brief 路径修正规则表
 *
 * 按优先级排序。前缀越长的规则排在前面，
 * 避免短规则误匹配（如 /usr/lib64 应在 /usr/lib 之前）。
 */
struct FixRule {
    std::string prefix;         ///< 需要匹配的目录前缀
    std::string replacement;    ///< 替换为的目录
    bool merge{true};           ///< true=将文件移动到目标目录, false=仅重命名前缀
};

static const std::vector<FixRule>& GetFixRules() {
    static const std::vector<FixRule> rules = {
        // 多架构 GNU 库目录 → /usr/lib
        {"/usr/lib/x86_64-linux-gnu/",  "/usr/lib/", true},
        {"/usr/lib/i386-linux-gnu/",    "/usr/lib/", true},
        {"/usr/lib/i686-linux-gnu/",    "/usr/lib/", true},
        {"/usr/lib/aarch64-linux-gnu/", "/usr/lib/", true},
        {"/usr/lib/arm-linux-gnueabihf/","/usr/lib/", true},
        {"/usr/lib/arm-linux-gnueabi/", "/usr/lib/", true},
        {"/usr/lib/powerpc64le-linux-gnu/","/usr/lib/", true},
        {"/usr/lib/s390x-linux-gnu/",   "/usr/lib/", true},
        // /lib 多架构目录 → /usr/lib
        {"/lib/x86_64-linux-gnu/",      "/usr/lib/", true},
        {"/lib/i386-linux-gnu/",        "/usr/lib/", true},
        {"/lib/aarch64-linux-gnu/",     "/usr/lib/", true},
        // /lib64 和 /usr/lib64 → /usr/lib
        {"/lib64/",                     "/usr/lib/", true},
        {"/usr/lib64/",                 "/usr/lib/", true},
    };
    return rules;
}

int PathFixer::FixPaths(const std::filesystem::path& extract_dir,
                         std::vector<std::string>& warnings) {
    Logger::Info("正在修正跨发行版路径差异...");

    int fix_count = 0;
    const auto& rules = GetFixRules();

    // 收集所有文件，检查是否需要修正
    // 按路径排序，确保长路径先处理
    std::vector<std::filesystem::path> all_files;
    if (std::filesystem::exists(extract_dir)) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(extract_dir)) {
            if (entry.is_regular_file() || entry.is_symlink()) {
                all_files.push_back(entry.path());
            }
        }
    }

    // 按路径字符串长度降序排序（深层目录中的文件先处理）
    std::sort(all_files.begin(), all_files.end(),
              [](const auto& a, const auto& b) {
                  return a.string().size() > b.string().size();
              });

    // 跟踪已处理的路径，避免重复
    std::set<std::string> processed;

    for (const auto& file_path : all_files) {
        if (processed.count(file_path.string())) continue;

        // 获取相对路径
        std::string abs_str = file_path.string();
        std::string extract_str = extract_dir.string();

        // 确保 extract_dir 路径以 / 结尾
        if (extract_str.back() != '/') extract_str += '/';

        if (abs_str.size() <= extract_str.size()) continue;
        std::string relative = abs_str.substr(extract_str.size());

        // 查找匹配的修正规则
        for (const auto& rule : rules) {
            if (relative.compare(0, rule.prefix.size(), rule.prefix) == 0) {
                // 构造新路径
                std::string new_relative = rule.replacement
                    + relative.substr(rule.prefix.size());

                // 去掉可能出现的双斜杠
                // (已经通过前缀和后缀保证不会)

                std::filesystem::path new_path = extract_dir / new_relative;

                // 创建目标目录
                std::error_code ec;
                std::filesystem::create_directories(new_path.parent_path(), ec);

                // 如果目标文件已存在
                if (std::filesystem::exists(new_path)) {
                    warnings.push_back(
                        "路径合并时文件冲突（已跳过）：" + relative
                        + " → " + new_relative);
                    Logger::Verbose("  跳过冲突文件：" + relative);
                    processed.insert(file_path.string());
                    break;
                }

                // 重命名（移动）文件
                std::filesystem::rename(file_path, new_path, ec);
                if (ec) {
                    warnings.push_back(
                        "移动文件失败：" + relative
                        + " (" + ec.message() + ")");
                    Logger::Warning("无法移动文件：" + relative + " → " + new_relative);
                } else {
                    Logger::Verbose("  路径修正：" + relative + " → " + new_relative);
                    fix_count++;
                }

                processed.insert(file_path.string());
                break;
            }
        }
    }

    // 清理空目录
    // 收集所有目录（按深度降序）
    std::vector<std::filesystem::path> all_dirs;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(extract_dir,
             std::filesystem::directory_options::skip_permission_denied)) {
        if (entry.is_directory()) {
            all_dirs.push_back(entry.path());
        }
    }
    std::sort(all_dirs.begin(), all_dirs.end(),
              [](const auto& a, const auto& b) {
                  return a.string().size() > b.string().size();
              });

    for (const auto& dir : all_dirs) {
        std::error_code ec;
        if (std::filesystem::is_empty(dir, ec) && !ec) {
            std::filesystem::remove(dir, ec);
            if (!ec) {
                Logger::Verbose("  移除空目录：" + dir.string());
            }
        }
    }

    if (fix_count > 0) {
        Logger::Info("  已修正 " + std::to_string(fix_count) + " 个路径");
    } else {
        Logger::Info("  无需修正路径差异");
    }

    return fix_count;
}

} // namespace deb2pkg
