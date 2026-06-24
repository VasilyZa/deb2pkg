/**
 * @file types.h
 * @brief 核心数据结构定义
 *
 * 定义整个 deb2pkg 工具使用的核心数据结构和枚举类型。
 * 各模块之间通过 PackageInfo 结构体传递打包信息。
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace deb2pkg {

/**
 * @brief 包类型枚举
 *
 * 通过魔数检测或文件扩展名确定输入文件的包类型
 */
enum class PackageType {
    Unknown,    ///< 无法识别的格式
    Deb,        ///< Debian 软件包 (.deb)
    Rpm         ///< RPM 软件包 (.rpm)
};

/**
 * @brief 错误码枚举
 *
 * 用于在整个工具中标识不同类型的错误，每种错误都附带中文解决建议
 */
enum class ErrorCode {
    Success = 0,

    // ---- 输入错误 ----
    FileNotFound,           ///< 文件不存在
    FileNotReadable,        ///< 文件无读取权限
    UnknownFormat,          ///< 无法识别的包格式

    // ---- 解压错误 ----
    ArDecompressFailed,     ///< ar 归档解压失败
    TarDecompressFailed,    ///< tar 压缩包解压失败
    RpmExtractFailed,       ///< RPM 提取失败
    CpioExtractFailed,      ///< CPIO 提取失败

    // ---- 解析错误 ----
    ControlParseFailed,     ///< deb control 文件解析失败
    RpmHeaderParseFailed,   ///< RPM 头部解析失败
    ElfReadFailed,          ///< ELF 文件读取失败

    // ---- 依赖错误 ----
    PkgfileNotAvailable,    ///< pkgfile 未安装
    PkgfileCacheMissing,    ///< pkgfile 缓存未初始化
    PkgfileSearchFailed,    ///< pkgfile 搜索失败

    // ---- 生成错误 ----
    PkgbuildWriteFailed,    ///< PKGBUILD 写入失败
    MakepkgFailed,          ///< makepkg 构建失败

    // ---- 系统错误 ----
    TempDirCreateFailed,    ///< 临时目录创建失败
    ExecFailed,             ///< 外部命令执行失败
    Sha256ComputeFailed,    ///< SHA256 校验计算失败

    // ---- 内部错误 ----
    InternalError           ///< 内部逻辑错误
};

/**
 * @brief 包元数据结构体
 *
 * 从 deb/rpm 包中提取的完整元数据，用于生成 PKGBUILD。
 * 这是各模块之间传递打包信息的标准载体。
 */
struct PackageInfo {
    // ---- 基本信息 ----
    std::string pkgname;        ///< Arch 化包名（小写，含 -bin 后缀）
    std::string orig_name;      ///< 原始包名（转换前）
    std::string version;        ///< 上游版本号（pkgver，不含连字符）
    std::string pkgrel{"1"};    ///< 修订号（pkgrel，默认为 1）
    std::string arch;           ///< Arch 架构标识 (x86_64, i686, aarch64, any)
    std::string desc;           ///< 软件描述（PKGBUILD 的 pkgdesc）
    std::string license_str;    ///< 许可证（SPDX 格式，如 GPL-3.0-only）
    std::string url;            ///< 上游项目 URL

    // ---- 依赖信息 ----
    std::vector<std::string> depends;       ///< Arch 包依赖数组
    std::vector<std::string> unresolved;    ///< 未能自动匹配的 soname

    // ---- 内部信息 ----
    PackageType type{PackageType::Unknown};             ///< 原始包类型
    std::filesystem::path source_path;                   ///< 原始 .deb/.rpm 文件路径
    std::filesystem::path extract_dir;                   ///< 提取出的数据文件目录
};

/**
 * @brief 路径修正规则
 *
 * 定义一条跨发行版路径差异的修正规则：
 * 如果文件的相对路径以 prefix 开头，则替换为 replacement。
 */
struct PathFixRule {
    std::string prefix;         ///< 匹配前缀（如 "/usr/lib/x86_64-linux-gnu"）
    std::string replacement;    ///< 替换为（如 "/usr/lib"）
    bool recursive{false};      ///< 是否递归替换路径中的所有匹配
};

/**
 * @brief ELF 共享库依赖信息
 */
struct ElfDependency {
    std::string soname;         ///< 库文件名（如 libc.so.6）
    std::string owner_pkg;      ///< 匹配到的 Arch 包名（如 glibc）
    bool resolved{false};       ///< 是否成功匹配
};

/**
 * @brief 错误信息结构体
 *
 * 包含错误码及对应的中文描述和解决建议
 */
struct ErrorResult {
    ErrorCode code{ErrorCode::Success};
    std::string message_cn;     ///< 中文错误描述
    std::string solution_cn;    ///< 中文解决建议
};

/**
 * @brief 整个转换流程的结果
 */
struct ConversionResult {
    bool success{false};
    PackageInfo pkg_info;
    std::vector<ElfDependency> unresolved_libs;     ///< 未能匹配的库依赖
    std::vector<std::string> warnings;               ///< 非致命警告信息
    std::optional<std::string> pkgbuild_path;        ///< 生成的 PKGBUILD 文件路径
    std::optional<std::string> built_pkg_path;       ///< makepkg 构建结果路径
};

} // namespace deb2pkg
