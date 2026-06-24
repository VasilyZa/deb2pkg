/**
 * @file errors.h
 * @brief 错误信息映射 — 为每种错误码提供中文描述和解决建议
 *
 * 所有面向用户的错误提示统一在这里定义，确保信息一致、专业、可操作。
 */

#pragma once

#include "types.h"
#include <unordered_map>

namespace deb2pkg {

/**
 * @brief 根据错误码返回对应的中文错误信息
 * @param code 错误码
 * @return 包含中文描述和解决建议的 ErrorResult
 */
inline ErrorResult GetErrorInfo(ErrorCode code) {
    static const std::unordered_map<ErrorCode, ErrorResult> error_map = {
        // ---- 输入错误 ----
        {ErrorCode::FileNotFound, {
            ErrorCode::FileNotFound,
            "文件不存在：找不到指定的安装包文件",
            "请检查以下事项：\n"
            "  1. 确认文件路径是否正确，路径中是否含有拼写错误\n"
            "  2. 使用 ls -l <文件路径> 确认文件是否存在\n"
            "  3. 如果文件位于其他目录，请使用绝对路径或相对路径正确指定\n"
            "  示例：deb2pkg /home/user/下载/package.deb"
        }},
        {ErrorCode::FileNotReadable, {
            ErrorCode::FileNotReadable,
            "文件无法读取：没有读取该文件的权限",
            "请检查以下事项：\n"
            "  1. 使用 ls -l <文件路径> 查看文件权限\n"
            "  2. 如权限不足，使用 chmod +r <文件路径> 添加读权限\n"
            "  3. 如果是系统保护文件，确认是否需要使用 sudo 运行"
        }},
        {ErrorCode::UnknownFormat, {
            ErrorCode::UnknownFormat,
            "无法识别的包格式：文件不是有效的 .deb 或 .rpm 包",
            "请检查以下事项：\n"
            "  1. 确认文件扩展名为 .deb 或 .rpm\n"
            "  2. 使用 file <文件路径> 命令查看文件实际类型\n"
            "  3. 确认文件未损坏（重新下载后再试）\n"
            "  4. 本工具仅支持 Debian (.deb) 和 RPM (.rpm) 格式的安装包"
        }},

        // ---- 解压错误 ----
        {ErrorCode::ArDecompressFailed, {
            ErrorCode::ArDecompressFailed,
            "ar 归档解压失败：无法解析 .deb 包的 ar 归档格式",
            "请检查以下事项：\n"
            "  1. 确认系统已安装 binutils 包：sudo pacman -S binutils\n"
            "  2. 使用 ar t <文件路径> 验证归档完整性\n"
            "  3. 重新下载 .deb 包后再试（当前包可能已损坏）"
        }},
        {ErrorCode::TarDecompressFailed, {
            ErrorCode::TarDecompressFailed,
            "tar 压缩包解压失败：control.tar.* 或 data.tar.* 解压出错",
            "请检查以下事项：\n"
            "  1. 确认 .deb 包完整未损坏（重新下载后再试）\n"
            "  2. 检查磁盘空间是否充足：df -h\n"
            "  3. 确认临时目录有写入权限"
        }},
        {ErrorCode::RpmExtractFailed, {
            ErrorCode::RpmExtractFailed,
            "RPM 包提取失败：无法提取 RPM 包中的文件",
            "请检查以下事项：\n"
            "  1. 确认 .rpm 包完整未损坏（重新下载后再试）\n"
            "  2. 检查磁盘空间是否充足：df -h\n"
            "  3. 如果问题持续，尝试手动验证：rpm -K <文件路径>（需安装 rpm-tools）"
        }},
        {ErrorCode::CpioExtractFailed, {
            ErrorCode::CpioExtractFailed,
            "CPIO 归档提取失败：无法从 CPIO 流中提取文件",
            "请检查以下事项：\n"
            "  1. 确认 cpio 工具已安装：sudo pacman -S cpio\n"
            "  2. .rpm 包可能使用了不支持的压缩格式\n"
            "  3. 请将 .rpm 包发送给开发者协助排查"
        }},

        // ---- 解析错误 ----
        {ErrorCode::ControlParseFailed, {
            ErrorCode::ControlParseFailed,
            "deb control 文件解析失败：无法读取包元数据",
            "请检查以下事项：\n"
            "  1. .deb 包可能不标准，control 文件格式异常\n"
            "  2. 尝试手动查看：ar x package.deb && tar xf control.tar.* && cat control\n"
            "  3. 如果 control 文件存在但无法解析，可能是编码问题"
        }},
        {ErrorCode::RpmHeaderParseFailed, {
            ErrorCode::RpmHeaderParseFailed,
            "RPM 头部解析失败：无法读取 RPM 包元数据",
            "请检查以下事项：\n"
            "  1. .rpm 包可能使用了较新的 RPM 格式版本\n"
            "  2. 尝试用 rpm 命令查看：rpm -qp <文件路径>\n"
            "  3. 如果 rpm 命令也失败，说明包文件已损坏"
        }},
        {ErrorCode::ElfReadFailed, {
            ErrorCode::ElfReadFailed,
            "ELF 文件读取失败：无法解析二进制程序的依赖库信息",
            "请检查以下事项：\n"
            "  1. 确认 binutils 已安装：sudo pacman -S binutils\n"
            "  2. 如果特定文件解析失败，可能是该文件本身损坏\n"
            "  3. 使用 readelf -d <文件路径> 手动检查该 ELF 文件"
        }},

        // ---- 依赖错误 ----
        {ErrorCode::PkgfileNotAvailable, {
            ErrorCode::PkgfileNotAvailable,
            "pkgfile 工具未安装：无法自动匹配库文件对应的 Arch 包名",
            "请执行以下操作：\n"
            "  1. 安装 pkgfile：sudo pacman -S pkgfile\n"
            "  2. 初始化数据库：sudo pkgfile --update\n"
            "  3. 安装完成后重新运行 deb2pkg"
        }},
        {ErrorCode::PkgfileCacheMissing, {
            ErrorCode::PkgfileCacheMissing,
            "pkgfile 数据库未初始化：需要更新包文件索引",
            "请执行以下操作：\n"
            "  1. 更新 pkgfile 数据库：sudo pkgfile --update\n"
            "  2. 确认网络连接正常（需要从 Arch 镜像站下载数据库）\n"
            "  3. 更新完成后重新运行 deb2pkg"
        }},
        {ErrorCode::PkgfileSearchFailed, {
            ErrorCode::PkgfileSearchFailed,
            "pkgfile 搜索失败：在 Arch 官方仓库中未找到匹配的库文件包名",
            "可能的原因和解决方法：\n"
            "  1. 该库来自 AUR（非官方仓库），pkgfile 无法索引\n"
            "  2. 请在生成的 PKGBUILD 中手动将 soname 加入 depends 数组\n"
            "  3. 搜索 AUR：yay -Ss <库名> 或访问 aur.archlinux.org"
        }},

        // ---- 生成错误 ----
        {ErrorCode::PkgbuildWriteFailed, {
            ErrorCode::PkgbuildWriteFailed,
            "PKGBUILD 文件写入失败：目标目录无写入权限",
            "请检查以下事项：\n"
            "  1. 确认输出目录有写入权限：ls -ld <输出目录>\n"
            "  2. 如权限不足，使用 -o 参数指定其他输出目录\n"
            "  3. 检查磁盘空间：df -h"
        }},
        {ErrorCode::MakepkgFailed, {
            ErrorCode::MakepkgFailed,
            "makepkg 构建失败：打包过程出错",
            "请尝试以下方法：\n"
            "  1. 确认 base-devel 已安装：sudo pacman -S base-devel\n"
            "  2. 进入 PKGBUILD 所在目录手动运行 makepkg 查看详细错误\n"
            "  3. 检查 depends 数组中的包是否都已安装\n"
            "  4. 不要以 root 用户运行 makepkg"
        }},

        // ---- 系统错误 ----
        {ErrorCode::TempDirCreateFailed, {
            ErrorCode::TempDirCreateFailed,
            "临时目录创建失败：系统临时目录不可用或磁盘已满",
            "请检查以下事项：\n"
            "  1. 检查磁盘空间：df -h /tmp\n"
            "  2. 确认 /tmp 目录有写入权限\n"
            "  3. 尝试手动创建：mktemp -d"
        }},
        {ErrorCode::ExecFailed, {
            ErrorCode::ExecFailed,
            "外部命令执行失败：调用系统命令时出错",
            "请检查以下事项：\n"
            "  1. 确认相关系统工具已安装\n"
            "  2. 检查 PATH 环境变量是否正确\n"
            "  3. 如果问题持续，请报告具体的失败命令"
        }},
        {ErrorCode::Sha256ComputeFailed, {
            ErrorCode::Sha256ComputeFailed,
            "SHA256 校验计算失败：无法计算源文件的哈希值",
            "请检查以下事项：\n"
            "  1. 确认源文件可读且未损坏\n"
            "  2. 检查磁盘空间：df -h\n"
            "  3. 如果文件过大，确保有足够内存"
        }},

        // ---- 内部错误 ----
        {ErrorCode::InternalError, {
            ErrorCode::InternalError,
            "内部错误：程序遇到意外的逻辑错误",
            "请将此问题报告给开发者，并提供以下信息：\n"
            "  1. 完整的终端输出\n"
            "  2. 导致出错的 .deb/.rpm 文件名\n"
            "  3. 操作系统版本：uname -a"
        }},
    };

    auto it = error_map.find(code);
    if (it != error_map.end()) {
        return it->second;
    }
    return {ErrorCode::InternalError,
            "未知错误",
            "请将此问题报告给开发者"};
}

} // namespace deb2pkg
