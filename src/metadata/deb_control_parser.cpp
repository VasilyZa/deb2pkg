/**
 * @file deb_control_parser.cpp
 * @brief DebControlParser 实现
 *
 * 解析 debian/control 文件（RFC 822 格式）：
 *   Key: Value
 *   续行以空格或 Tab 开头
 */

#include "deb_control_parser.h"
#include "core/logger.h"

#include <algorithm>
#include <unordered_map>

namespace deb2pkg {

/**
 * @brief 将 Debian 架构名映射为 Arch 架构标识
 */
static std::string MapArchitecture(const std::string& deb_arch) {
    static const std::unordered_map<std::string, std::string> arch_map = {
        {"amd64",   "x86_64"},
        {"i386",    "i686"},
        {"arm64",   "aarch64"},
        {"armhf",   "armv7h"},
        {"armel",   "arm"},
        {"mips64el","mips64el"},
        {"ppc64el", "ppc64le"},
        {"s390x",   "s390x"},
        {"all",     "any"},
    };
    auto it = arch_map.find(deb_arch);
    return (it != arch_map.end()) ? it->second : deb_arch;
}

/**
 * @brief 将包名转换为符合 Arch 规范的 pkgname
 *
 * Arch 包名规范：
 * - 只能包含小写字母、数字、连字符(-)、下划线(_)、加号(+)、点(.)
 * - 必须以字母数字或下划线开头
 * - 二进制预编译包按约定加 -bin 后缀（由调用者判断）
 */
static std::string SanitizePkgname(const std::string& name) {
    std::string result;
    for (char c : name) {
        if (c >= 'A' && c <= 'Z') {
            result += static_cast<char>(c + 32);  // 转小写
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                   || c == '-' || c == '_' || c == '+' || c == '.') {
            result += c;
        } else {
            result += '_';  // 替换非法字符
        }
    }
    // 确保以合法字符开头
    if (!result.empty() && !((result[0] >= 'a' && result[0] <= 'z')
                              || (result[0] >= '0' && result[0] <= '9')
                              || result[0] == '_')) {
        result = "_" + result;
    }
    return result;
}

PackageInfo DebControlParser::Parse(const std::vector<char>& raw_control) {
    PackageInfo info;
    info.type = PackageType::Deb;

    // 将原始数据转换为字符串
    std::string content(raw_control.begin(), raw_control.end());

    // 解析 RFC 822 字段
    // 规则：每行要么是 "Key: Value"，要么是续行（以空格/Tab 开头）
    std::unordered_map<std::string, std::string> fields;
    std::string current_key;

    // 手动按行切分（避免 musl 下 std::istringstream 崩溃）
    size_t line_start = 0;
    while (line_start < content.size()) {
        size_t line_end = content.find('\n', line_start);
        if (line_end == std::string::npos) line_end = content.size();
        std::string line = content.substr(line_start, line_end - line_start);
        line_start = line_end + 1;

        // 去掉行尾的 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            // 空行表示段落结束（deb control 可能只有一个段落）
            continue;
        }

        if (line[0] == ' ' || line[0] == '\t') {
            // 续行：追加到当前字段值
            if (!current_key.empty()) {
                // 去掉开头的空白但保留一个空格
                size_t start = line.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    fields[current_key] += " " + line.substr(start);
                }
            }
        } else {
            // 新字段
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                current_key = line.substr(0, colon);
                // 去掉值开头的空白
                size_t value_start = line.find_first_not_of(" \t", colon + 1);
                std::string value = (value_start != std::string::npos)
                    ? line.substr(value_start)
                    : "";

                // 已存在的 key 追加（多值字段）
                auto it = fields.find(current_key);
                if (it != fields.end()) {
                    it->second += ", " + value;
                } else {
                    fields[current_key] = value;
                }
            }
        }
    }

    // ---- 提取字段并填充 PackageInfo ----

    // 原始包名
    auto package_it = fields.find("Package");
    if (package_it != fields.end()) {
        info.orig_name = package_it->second;
        info.pkgname = SanitizePkgname(info.orig_name);
        // 默认加 -bin 后缀
        if (info.pkgname.size() < 4 ||
            info.pkgname.substr(info.pkgname.size() - 4) != "-bin") {
            info.pkgname += "-bin";
        }
    } else {
        Logger::Warning("control 文件中缺少 Package 字段");
        info.orig_name = "unknown";
        info.pkgname = "unknown-bin";
    }

    // 版本号
    // Debian 版本格式: [epoch:]version-revision (如 149.0.7827.196-1)
    // Arch 的要求: pkgver 不能含连字符，pkgrel 必须为数字
    auto ver_it = fields.find("Version");
    if (ver_it != fields.end()) {
        info.version = ver_it->second;

        // 去除 epoch（如 1:2.0.3 → 2.0.3）
        size_t colon = info.version.find(':');
        if (colon != std::string::npos) {
            std::string epoch = info.version.substr(0, colon);
            std::string rest = info.version.substr(colon + 1);
            Logger::Warning("Debian 版本含 epoch (" + epoch + ")，已去除，"
                           "如有需要请手动添加到 PKGBUILD 的 epoch 字段");
            info.version = rest;
        }

        // 在最后一个 '-' 处拆分为 pkgver 和 pkgrel
        size_t last_dash = info.version.rfind('-');
        if (last_dash != std::string::npos) {
            info.pkgrel = info.version.substr(last_dash + 1);
            info.version = info.version.substr(0, last_dash);
        }

        // 将 pkgver 中剩余的 '-' 替换为 '.' 或 '_'
        // (Arch 不允许 pkgver 含连字符)
        for (char& c : info.version) {
            if (c == '-') c = '.';
        }

        // 确保 pkgrel 只含数字
        std::string clean_rel;
        for (char c : info.pkgrel) {
            if (c >= '0' && c <= '9') clean_rel += c;
        }
        if (clean_rel.empty()) clean_rel = "1";
        info.pkgrel = clean_rel;

    } else {
        Logger::Warning("control 文件中缺少 Version 字段");
        info.version = "0.0.0";
    }

    // 架构
    auto arch_it = fields.find("Architecture");
    if (arch_it != fields.end()) {
        info.arch = MapArchitecture(arch_it->second);
    } else {
        info.arch = "any";
    }

    // 描述
    auto desc_it = fields.find("Description");
    if (desc_it != fields.end()) {
        std::string desc = desc_it->second;
        // 取第一行作为短描述（第一行是摘要，续行是长描述）
        size_t synopsis_end = desc.find('\n');
        if (synopsis_end == std::string::npos) {
            // 没有换行符（全在一行里），取整个值
            info.desc = desc;
        } else {
            info.desc = desc.substr(0, synopsis_end);
        }
    } else {
        info.desc = "A package converted from Debian";
    }

    // 许可证
    auto license_it = fields.find("License");
    if (license_it != fields.end() && !license_it->second.empty()) {
        info.license_str = license_it->second;
    } else {
        info.license_str = "custom";
        Logger::Warning("control 文件中缺少 License 字段，已设为 'custom'。"
                       "请手动核实许可证信息");
    }

    // 上游 URL
    auto url_it = fields.find("Homepage");
    if (url_it != fields.end() && !url_it->second.empty()) {
        info.url = url_it->second;
    } else {
        // 尝试从其他字段推断
        auto src_it = fields.find("Vcs-Browser");
        if (src_it != fields.end() && !src_it->second.empty()) {
            info.url = src_it->second;
        }
    }

    Logger::Verbose("Debian 元数据解析完成：");
    Logger::Verbose("  原始包名: " + info.orig_name);
    Logger::Verbose("  Arch 包名: " + info.pkgname);
    Logger::Verbose("  版本: " + info.version);
    Logger::Verbose("  架构: " + info.arch);
    Logger::Verbose("  描述: " + info.desc);
    Logger::Verbose("  许可证: " + info.license_str);
    Logger::Verbose("  URL: " + info.url);

    return info;
}

} // namespace deb2pkg
