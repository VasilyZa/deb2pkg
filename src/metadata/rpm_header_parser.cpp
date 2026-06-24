/**
 * @file rpm_header_parser.cpp
 * @brief RpmHeaderParser 实现 — 手动解析 RPM 二进制 Header
 *
 * 解析 RPM Tag 区域以提取包元数据：
 * NAME, VERSION, RELEASE, ARCH, SUMMARY, LICENSE, URL 等。
 */

#include "rpm_header_parser.h"
#include "core/logger.h"

#include <cstring>
#include <arpa/inet.h>
#include <unordered_map>
#include <algorithm>
#include <set>

namespace deb2pkg {

// ---- RPM Tag ID 常量 ----
enum RpmTag : uint32_t {
    RPMTAG_NAME              = 1000,
    RPMTAG_VERSION           = 1001,
    RPMTAG_RELEASE           = 1002,
    RPMTAG_SUMMARY           = 1004,
    RPMTAG_DESCRIPTION       = 1005,
    RPMTAG_SIZE              = 1009,
    RPMTAG_LICENSE           = 1014,
    RPMTAG_GROUP             = 1016,
    RPMTAG_URL               = 1020,
    RPMTAG_ARCH              = 1022,
    RPMTAG_PROVIDENAME       = 1047,
    RPMTAG_REQUIRENAME       = 1049,
    RPMTAG_REQUIREFLAGS      = 1048,
};

// ---- RPM 数据类型 ----
enum RpmType : uint32_t {
    RPM_NULL         = 0,
    RPM_INT32        = 4,
    RPM_STRING       = 6,
    RPM_BIN          = 7,
    RPM_STRING_ARRAY = 8,
    RPM_I18NSTRING   = 9,
};

// ---- 索引条目结构 ----
struct RpmIndexEntry {
    uint32_t tag;
    uint32_t type;
    int32_t  offset;  // 相对数据区起始的偏移
    uint32_t count;
};

// ---- 辅助：从大端读取 uint32_t ----
static inline uint32_t be32(const void* ptr) {
    return ntohl(*static_cast<const uint32_t*>(ptr));
}

// ---- 辅助：从大端读取 int32_t ----
static inline int32_t be32s(const void* ptr) {
    return static_cast<int32_t>(ntohl(*static_cast<const uint32_t*>(ptr)));
}

// ---- 架构映射 ----
static std::string MapRpmArch(const std::string& rpm_arch) {
    static const std::unordered_map<std::string, std::string> arch_map = {
        {"x86_64",  "x86_64"},
        {"amd64",   "x86_64"},
        {"i386",    "i686"},
        {"i486",    "i686"},
        {"i586",    "i686"},
        {"i686",    "i686"},
        {"aarch64", "aarch64"},
        {"arm64",   "aarch64"},
        {"armv7hl", "armv7h"},
        {"armv7l",  "armv7h"},
        {"ppc64le", "ppc64le"},
        {"noarch",  "any"},
    };
    auto it = arch_map.find(rpm_arch);
    return (it != arch_map.end()) ? it->second : rpm_arch;
}

// ---- 包名规范化 ----
static std::string SanitizePkgname(const std::string& name) {
    std::string result;
    for (char c : name) {
        if (c >= 'A' && c <= 'Z') {
            result += static_cast<char>(c + 32);
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                   || c == '-' || c == '_' || c == '+' || c == '.') {
            result += c;
        } else {
            result += '_';
        }
    }
    if (!result.empty() && !((result[0] >= 'a' && result[0] <= 'z')
                              || (result[0] >= '0' && result[0] <= '9')
                              || result[0] == '_')) {
        result = "_" + result;
    }
    return result;
}

PackageInfo RpmHeaderParser::Parse(const std::vector<char>& raw_header,
                                    const std::filesystem::path& /*extract_dir*/)
{
    PackageInfo info;
    info.type = PackageType::Rpm;

    if (raw_header.size() < 16) {
        Logger::Error("RPM Header 数据过短：无法解析");
        info.pkgname = "unknown-bin";
        info.version = "0.0.0";
        info.arch = "any";
        info.desc = "Unknown RPM package";
        info.license_str = "custom";
        return info;
    }

    const unsigned char* data = reinterpret_cast<const unsigned char*>(raw_header.data());

    // 解析 header 结构
    uint32_t magic      = be32(data);
    // uint32_t reserved   = be32(data + 4);
    uint32_t index_count = be32(data + 8);
    uint32_t data_size   = be32(data + 12);

    if (magic != 0x8eade801) {
        Logger::Warning("RPM Header 魔数异常: 0x"
                       + std::to_string(magic) + ", 预期 0x8eade801");
    }

    // 数据区起始位置 (相对于 raw_header 开头)
    const unsigned char* store = data + 16 + index_count * 16;

    // 验证边界
    if (16 + index_count * 16 + data_size > raw_header.size()) {
        Logger::Error("RPM Header 数据区超出边界，文件可能已损坏");
        info.pkgname = "unknown-bin";
        info.version = "0.0.0";
        info.arch = "any";
        info.desc = "Corrupted RPM package";
        info.license_str = "custom";
        return info;
    }

    // ---- 遍历所有索引条目 ----
    struct TagValue {
        uint32_t type;
        std::string str_value;              // 单字符串
        std::vector<std::string> str_array; // 字符串数组
        int32_t int_value;                  // 整数值
    };
    std::unordered_map<uint32_t, TagValue> tags;

    for (uint32_t i = 0; i < index_count; i++) {
        const unsigned char* entry_ptr = data + 16 + i * 16;
        RpmIndexEntry entry;
        entry.tag    = be32(entry_ptr);
        entry.type   = be32(entry_ptr + 4);
        entry.offset = be32s(entry_ptr + 8);
        entry.count  = be32(entry_ptr + 12);

        if (entry.offset < 0 ||
            static_cast<uint32_t>(entry.offset) >= data_size) {
            Logger::Verbose("  Tag #" + std::to_string(entry.tag)
                          + " 偏移异常，跳过");
            continue;
        }

        const unsigned char* value_ptr = store + entry.offset;

        TagValue tv;
        tv.type = entry.type;

        switch (entry.type) {
        case RPM_STRING:
        case RPM_I18NSTRING: {
            // 单个 null-terminated 字符串
            const char* str_start = reinterpret_cast<const char*>(value_ptr);
            size_t max_len = data_size - static_cast<uint32_t>(entry.offset);
            size_t str_len = strnlen(str_start, max_len);
            tv.str_value = std::string(str_start, str_len);
            break;
        }
        case RPM_STRING_ARRAY: {
            // 多个 null-terminated 字符串连续存放
            const char* p = reinterpret_cast<const char*>(value_ptr);
            size_t remaining = data_size - static_cast<uint32_t>(entry.offset);
            for (uint32_t j = 0; j < entry.count && remaining > 0; j++) {
                size_t len = strnlen(p, remaining);
                if (len > 0 || (j < entry.count - 1)) {
                    tv.str_array.emplace_back(p, len);
                    p += len + 1;
                    if (remaining > len + 1) remaining -= (len + 1);
                    else break;
                }
            }
            break;
        }
        case RPM_INT32: {
            if (entry.count > 0) {
                tv.int_value = be32s(value_ptr);
            }
            break;
        }
        default:
            // 忽略其他类型
            break;
        }

        tags[entry.tag] = tv;
    }

    // ---- 提取字段 ----

    // 名称
    auto name_it = tags.find(RPMTAG_NAME);
    if (name_it != tags.end()) {
        info.orig_name = name_it->second.str_value;
        info.pkgname = SanitizePkgname(info.orig_name);
        if (info.pkgname.size() < 4 ||
            info.pkgname.substr(info.pkgname.size() - 4) != "-bin") {
            info.pkgname += "-bin";
        }
    } else {
        info.orig_name = "unknown";
        info.pkgname = "unknown-bin";
        Logger::Warning("RPM Header 中缺少 NAME 标签");
    }

    // 版本号
    // RPM 有独立的 VERSION 和 RELEASE 标签，完美映射到 Arch 的 pkgver + pkgrel
    auto ver_it = tags.find(RPMTAG_VERSION);
    auto rel_it = tags.find(RPMTAG_RELEASE);
    if (ver_it != tags.end()) {
        info.version = ver_it->second.str_value;

        // Arch pkgver 不能含连字符，替换为 '.'
        for (char& c : info.version) {
            if (c == '-') c = '.';
        }

        if (rel_it != tags.end()) {
            info.pkgrel = rel_it->second.str_value;
            // 确保 pkgrel 只含数字
            std::string clean_rel;
            for (char c : info.pkgrel) {
                if (c >= '0' && c <= '9') clean_rel += c;
            }
            if (clean_rel.empty()) clean_rel = "1";
            info.pkgrel = clean_rel;
        }
    } else {
        info.version = "0.0.0";
        Logger::Warning("RPM Header 中缺少 VERSION 标签");
    }

    // 架构
    auto arch_it = tags.find(RPMTAG_ARCH);
    if (arch_it != tags.end()) {
        info.arch = MapRpmArch(arch_it->second.str_value);
    } else {
        info.arch = "any";
    }

    // 描述：优先用 SUMMARY，备选 DESCRIPTION
    auto summary_it = tags.find(RPMTAG_SUMMARY);
    auto desc_it = tags.find(RPMTAG_DESCRIPTION);
    if (summary_it != tags.end() && !summary_it->second.str_value.empty()) {
        info.desc = summary_it->second.str_value;
    } else if (desc_it != tags.end() && !desc_it->second.str_value.empty()) {
        // 取 DESCRIPTION 的第一行
        info.desc = desc_it->second.str_value;
        size_t nl = info.desc.find('\n');
        if (nl != std::string::npos) {
            info.desc = info.desc.substr(0, nl);
        }
    } else {
        info.desc = "A package converted from RPM";
    }

    // 许可证
    auto lic_it = tags.find(RPMTAG_LICENSE);
    if (lic_it != tags.end() && !lic_it->second.str_value.empty()) {
        info.license_str = lic_it->second.str_value;
    } else {
        info.license_str = "custom";
        Logger::Warning("RPM Header 中缺少 LICENSE 标签，已设为 'custom'");
    }

    // URL
    auto url_it = tags.find(RPMTAG_URL);
    if (url_it != tags.end() && !url_it->second.str_value.empty()) {
        info.url = url_it->second.str_value;
    }

    Logger::Verbose("RPM 元数据解析完成：");
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
