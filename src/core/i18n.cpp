/**
 * @file i18n.cpp
 * @brief I18n 实现 — 中英文翻译映射表
 *
 * 英文原文作为 key，通过翻译表映射到中文。
 * 未匹配的 key 直接返回英文原文（fallback）。
 */

#include "i18n.h"
#include <cstdlib>
#include <cstring>

namespace deb2pkg {

std::string I18n::lang_ = "en";
std::unordered_map<std::string, std::string> I18n::zh_map_;

void I18n::Init() {
    // 检测环境变量: LC_ALL > LC_MESSAGES > LANG
    const char* lc = std::getenv("LC_ALL");
    if (!lc || !*lc) lc = std::getenv("LC_MESSAGES");
    if (!lc || !*lc) lc = std::getenv("LANG");

    if (lc && (std::strstr(lc, "zh_CN") || std::strstr(lc, "zh_")
               || std::strstr(lc, "zh."))) {
        lang_ = "zh";
    } else {
        lang_ = "en";
    }
}

void I18n::SetLanguage(const std::string& lang) {
    if (lang == "zh" || lang == "cn" || lang == "zh_CN" || lang == "zh-CN") {
        lang_ = "zh";
    } else {
        lang_ = "en";
    }
}

void I18n::BuildZhMap() {
    if (!zh_map_.empty()) return;  // 已构建

    // ==================== 通用/日志 ====================
    zh_map_["deb2pkg — Debian/RPM to Arch PKGBUILD Converter"] = "deb2pkg — Debian/RPM 包转 Arch PKGBUILD 工具";

    // ==================== CLI 帮助 ====================
    zh_map_["Usage:"] = "用法：";
    zh_map_["Options:"] = "选项：";
    zh_map_["Examples:"] = "示例：";
    zh_map_["Notes:"] = "注意事项：";
    zh_map_["  -o, --output <dir>   Output directory for PKGBUILD (default: current dir)"] = "  -o, --output <目录>   指定 PKGBUILD 输出目录（默认为当前目录）";
    zh_map_["  -b, --build          Also run makepkg to build .pkg.tar.zst"] = "  -b, --build           自动调用 makepkg 编译生成 .pkg.tar.zst 安装包";
    zh_map_["  -k, --keep-temp      Keep temporary files (for debugging)"] = "  -k, --keep-temp       保留临时文件不删除（调试用）";
    zh_map_["  -v, --verbose        Verbose output"] = "  -v, --verbose         输出详细的处理过程信息";
    zh_map_["  -h, --help           Show this help"] = "  -h, --help            显示此帮助信息";
    zh_map_["  --lang <zh|en>       Force language (zh=Chinese, en=English)"] = "  --lang <zh|en>        强制指定界面语言（zh=中文, en=英文）";
    zh_map_["  deb2pkg package.deb"] = "  deb2pkg package.deb";
    zh_map_["  deb2pkg package.rpm -o ~/aur-packages -v"] = "  deb2pkg package.rpm -o ~/aur-packages -v";
    zh_map_["  deb2pkg package.deb -b --keep-temp"] = "  deb2pkg package.deb -b --keep-temp";
    zh_map_["  • pkgfile is needed for automatic dependency matching"] = "  • pkgfile 用于自动匹配库文件对应的 Arch 包名（需先安装并初始化）";
    zh_map_["  • makepkg requires base-devel package group"] = "  • makepkg 需要 base-devel 包组";
    zh_map_["  • Generated PKGBUILD follows the AUR specification"] = "  • 生成的 PKGBUILD 遵循 AUR 规范";

    // ==================== 错误信息 ====================
    zh_map_["File not found: the specified package file does not exist"] = "文件不存在：找不到指定的安装包文件";
    zh_map_["Please check:\n"
            "  1. Verify the file path is correct and has no typos\n"
            "  2. Use ls -l <path> to confirm the file exists\n"
            "  3. Use absolute or relative paths correctly\n"
            "  Example: deb2pkg /home/user/Downloads/package.deb"] =
        "请检查以下事项：\n"
        "  1. 确认文件路径是否正确，路径中是否含有拼写错误\n"
        "  2. 使用 ls -l <文件路径> 确认文件是否存在\n"
        "  3. 如果文件位于其他目录，请使用绝对路径或相对路径正确指定\n"
        "  示例：deb2pkg /home/user/下载/package.deb";

    zh_map_["File not readable: permission denied"] = "文件无法读取：没有读取该文件的权限";
    zh_map_["Please check:\n"
            "  1. Use ls -l <path> to check file permissions\n"
            "  2. Add read permission: chmod +r <path>\n"
            "  3. If it's a system-protected file, check if sudo is needed"] =
        "请检查以下事项：\n"
        "  1. 使用 ls -l <文件路径> 查看文件权限\n"
        "  2. 如权限不足，使用 chmod +r <文件路径> 添加读权限\n"
        "  3. 如果是系统保护文件，确认是否需要使用 sudo 运行";

    zh_map_["Unrecognized format: not a valid .deb or .rpm package"] = "无法识别的包格式：文件不是有效的 .deb 或 .rpm 包";
    zh_map_["Please check:\n"
            "  1. Make sure the file extension is .deb or .rpm\n"
            "  2. Use 'file <path>' to check the actual file type\n"
            "  3. Re-download the file (it may be corrupted)\n"
            "  4. This tool only supports Debian (.deb) and RPM (.rpm) packages"] =
        "请检查以下事项：\n"
        "  1. 确认文件扩展名为 .deb 或 .rpm\n"
        "  2. 使用 file <文件路径> 命令查看文件实际类型\n"
        "  3. 确认文件未损坏（重新下载后再试）\n"
        "  4. 本工具仅支持 Debian (.deb) 和 RPM (.rpm) 格式的安装包";

    zh_map_["Input path is not a regular file"] = "输入路径不是普通文件";
    zh_map_["Please provide a valid .deb or .rpm file path"] = "请确认传入的是 .deb 或 .rpm 文件路径";

    // ==================== 处理流程 ====================
    zh_map_["Input file:"] = "输入文件：";
    zh_map_["Package type:"] = "包类型：";
    zh_map_["Debian"] = "Debian";
    zh_map_["RPM"] = "RPM";
    zh_map_["Extracting Debian package..."] = "正在解压 Debian 软件包...";
    zh_map_["Extracting RPM package..."] = "正在解压 RPM 软件包...";
    zh_map_["Extracted"] = "已提取";
    zh_map_["files"] = "个文件";
    zh_map_["Missing Package field in control file"] = "control 文件中缺少 Package 字段";
    zh_map_["Missing Version field in control file"] = "control 文件中缺少 Version 字段";
    zh_map_["Missing License field in control file, set to 'custom'."] = "control 文件中缺少 License 字段，已设为 'custom'。请手动核实许可证信息";
    zh_map_["Debian version contains epoch, removed."] = "Debian 版本含 epoch，已去除，如有需要请手动添加到 PKGBUILD 的 epoch 字段";
    zh_map_["Fixing cross-distro path differences..."] = "正在修正跨发行版路径差异...";
    zh_map_["Fixed"] = "已修正";
    zh_map_["paths"] = "个路径";
    zh_map_["No path fixes needed"] = "无需修正路径差异";
    zh_map_["Scanning ELF binary dependencies..."] = "正在扫描 ELF 二进制依赖...";
    zh_map_["Scanned"] = "扫描了";
    zh_map_["ELF files, found"] = "个 ELF 文件，发现";
    zh_map_["unique shared library dependencies"] = "个唯一共享库依赖";
    zh_map_["Querying Arch package names for"] = "正在查询";
    zh_map_["shared libraries..."] = "个共享库的 Arch 包名...";
    zh_map_["Matched"] = "已匹配";
    zh_map_["shared libraries"] = "个共享库";
    zh_map_["Generating PKGBUILD..."] = "正在生成 PKGBUILD...";
    zh_map_["PKGBUILD generated:"] = "PKGBUILD 已生成：";
    zh_map_["Building with makepkg..."] = "正在调用 makepkg 构建安装包...";
    zh_map_["Build successful:"] = "构建成功：";
    zh_map_["Build failed, but PKGBUILD was generated"] = "PKGBUILD 已生成但 makepkg 构建失败，请手动修复后重新构建";

    // ==================== 总结 ====================
    zh_map_["======== Summary ========"] = "======== 处理总结 ========";
    zh_map_["Original package:"] = "  原始包:    ";
    zh_map_["Arch package name:"] = "  Arch 包名:  ";
    zh_map_["Version:"] = "  版本:       ";
    zh_map_["Architecture:"] = "  架构:       ";
    zh_map_["Dependencies:"] = "  依赖数:     ";
    zh_map_["Unmatched libs:"] = "未匹配库:   ";
    zh_map_["Temp directory:"] = "  临时目录:   ";
    zh_map_["============================"] = "============================";
    zh_map_["Tip:"] = "提示：有";
    zh_map_["shared libs were not auto-matched."] = "个共享库未自动匹配。";
    zh_map_["Please edit PKGBUILD to manually add the missing depends."] = "请编辑 PKGBUILD 手动补充 depends 数组。";
    zh_map_["You can use 'pkgfile -s <libname>' to manually find the package name."] = "可以使用 pkgfile -s <库名> 命令手动查找包名。";
    zh_map_["To build the package, run:"] = "要构建安装包，请运行：";

    // ==================== 依赖相关 ====================
    zh_map_["pkgfile not installed, cannot auto-match dependency package names"] = "pkgfile 未安装，无法自动匹配依赖包名";
    zh_map_["Please install pkgfile: sudo pacman -S pkgfile"] = "请安装 pkgfile: sudo pacman -S pkgfile";
    zh_map_["Then run: sudo pkgfile --update"] = "安装后运行: sudo pkgfile --update";
    zh_map_["pkgfile database not initialized, need to update package index"] = "pkgfile 数据库未初始化：需要更新包文件索引";
    zh_map_["Please run: sudo pkgfile --update"] = "请执行: sudo pkgfile --update";
    zh_map_["pkgfile cache is empty, trying to update..."] = "pkgfile 缓存为空，正在尝试更新...";
    zh_map_["pkgfile cache update failed, please run manually: sudo pkgfile --update"] = "pkgfile 缓存更新失败，请手动运行：sudo pkgfile --update";
    zh_map_["The following"] = "以下";
    zh_map_["shared libraries could not be auto-matched to Arch package names:"] = "个共享库未能自动匹配 Arch 包名：";
    zh_map_["Please manually add the corresponding package names to depends in PKGBUILD"] = "请手动在 PKGBUILD 的 depends 数组中添加对应的包名";

    // ==================== 文件和目录 ====================
    zh_map_["Cannot create extraction directory:"] = "无法创建解压目录：";
    zh_map_["Cannot create data directory:"] = "无法创建数据目录：";
    zh_map_["Cannot create output directory:"] = "无法创建输出目录：";
    zh_map_["Please check directory write permissions:"] = "请检查目录写入权限：";
    zh_map_["Cannot write PKGBUILD file:"] = "无法写入 PKGBUILD 文件：";
    zh_map_["Please check disk space and directory permissions"] = "请检查磁盘空间和目录权限";
    zh_map_["Could not create temporary directory:"] = "无法创建临时目录：";
    zh_map_["File path conflict during merge (skipped):"] = "路径合并时文件冲突（已跳过）：";
    zh_map_["Failed to move file:"] = "移动文件失败：";

    // ==================== makepkg ====================
    zh_map_["makepkg not installed. Please run: sudo pacman -S base-devel"] = "makepkg 未安装。请运行：sudo pacman -S base-devel";
    zh_map_["Do not run makepkg as root. Please switch to a normal user."] = "请勿以 root 用户运行 makepkg。请切换到普通用户后重试。";
    zh_map_["Running: makepkg -sf --noconfirm"] = "  执行: makepkg -sf --noconfirm";
    zh_map_["Working directory:"] = "  工作目录: ";
    zh_map_["makepkg build failed: missing dependencies."] = "makepkg 构建失败：缺少依赖包。请安装缺失的依赖后重试。";
    zh_map_["makepkg build failed: permission denied."] = "makepkg 构建失败：权限不足。请勿以 root 运行。";
    zh_map_["makepkg build failed: missing source file."] = "makepkg 构建失败：缺少源文件。";
    zh_map_["makepkg build failed. See output above for details."] = "makepkg 构建失败，详细错误信息请查看上方输出。";
    zh_map_["Manual build: cd"] = "  手动排查：cd ";
    zh_map_["You can try manual build: cd"] = "可以尝试手动构建：cd";

    // ==================== 解压相关 ====================
    zh_map_["ar command not found. Please install binutils: sudo pacman -S binutils"] = "未找到 ar 命令。请安装 binutils：sudo pacman -S binutils";
    zh_map_["ar command failed:"] = "ar 命令执行失败：";
    zh_map_["control.tar.* or data.tar.* not found after ar extraction"] = "ar 提取后未找到 control.tar.* 或 data.tar.*";
    zh_map_["control.tar.* not found in .deb"] = "未在 .deb 中找到 control.tar.* 文件";
    zh_map_["data.tar.* not found in .deb"] = "未在 .deb 中找到 data.tar.* 文件";
    zh_map_["control file not found in control.tar.*"] = "在 control.tar.* 中未找到 control 文件";
    zh_map_["Not a valid RPM file (magic check failed)"] = "文件不是有效的 RPM 包（魔数校验失败）";
    zh_map_["rpm2cpio not found. Please install rpm-tools: sudo pacman -S rpm-tools"] = "未找到 rpm2cpio 命令。请安装 rpm-tools：sudo pacman -S rpm-tools";
    zh_map_["Cannot extract RPM payload. The file may be corrupted."] = "无法提取 RPM payload。请确认文件未损坏。";
    zh_map_["You can try installing rpm-tools: sudo pacman -S rpm-tools"] = "可尝试安装 rpm-tools: sudo pacman -S rpm-tools";
    zh_map_["RPM Header size too short, cannot parse"] = "RPM Header 数据过短：无法解析";
    zh_map_["Failed to read RPM Lead"] = "无法读取 RPM Lead";
    zh_map_["RPM Signature Header magic mismatch, continuing..."] = "RPM Signature Header 魔数异常，继续尝试...";
    zh_map_["RPM Header magic check failed"] = "RPM Header 魔数校验失败";
    zh_map_["RPM Header data area out of bounds, file may be corrupted"] = "RPM Header 数据区超出边界，文件可能已损坏";
    zh_map_["Cannot read RPM Header structure"] = "无法读取 RPM Header 结构";
    zh_map_["Cannot read complete RPM Header data"] = "无法读取完整的 RPM Header 数据";
    zh_map_["Missing NAME tag in RPM Header"] = "RPM Header 中缺少 NAME 标签";
    zh_map_["Missing VERSION tag in RPM Header"] = "RPM Header 中缺少 VERSION 标签";
    zh_map_["Missing LICENSE tag in RPM Header, set to 'custom'"] = "RPM Header 中缺少 LICENSE 标签，已设为 'custom'";
    zh_map_["Cannot open RPM file:"] = "无法打开 RPM 文件：";

    // ==================== 其他 ====================
    zh_map_["Computing SHA256 checksum:"] = "正在计算 SHA256 校验值： ";
    zh_map_["Cannot compute SHA256, please fill sha256sums manually"] = "无法计算 SHA256，请手动填写 sha256sums 字段";
    zh_map_["Path crossing detected, skipped:"] = "检测到可疑路径穿越，已跳过： ";
    zh_map_["Cannot create archive reader"] = "无法创建 libarchive 读取器";
    zh_map_["Cannot open archive file:"] = "无法打开归档文件：";
    zh_map_["Archive not opened, cannot extract"] = "归档文件未打开，无法提取";
    zh_map_["Cannot create disk writer"] = "无法创建磁盘写入器";
    zh_map_["Failed to write file data:"] = "写入文件数据失败：";
    zh_map_["Failed to read archive data:"] = "读取归档数据失败：";
    zh_map_["Cannot extract file:"] = "无法提取文件：";
    zh_map_["Archive read did not end normally:"] = "归档读取未正常结束：";
    zh_map_["Entry too large or empty, skipping:"] = "条目过大或为空，跳过：";
    zh_map_["Failed to read entry:"] = "读取条目失败：";
    zh_map_["libarchive failed, falling back to ar command..."] = "  libarchive 方式未能获取完整数据，回退到 ar 命令方式...";
    zh_map_["Using libarchive to extract deb..."] = "使用 libarchive 方式解压 deb...";
    zh_map_["Using ar command to extract deb..."] = "使用 ar 命令方式解压 deb...";
    zh_map_["Reading RPM Header..."] = "正在读取 RPM Header...";
    zh_map_["Extracting RPM payload with libarchive..."] = "使用 libarchive 提取 RPM payload...";
    zh_map_["Trying bsdtar to extract RPM..."] = "尝试用 bsdtar 命令提取 RPM...";
    zh_map_["RPM Header parse complete"] = "RPM 元数据解析完成：";
    zh_map_["Debian metadata parse complete"] = "Debian 元数据解析完成：";
    zh_map_["Unknown error"] = "未知错误";
    zh_map_["Please report this issue to the developer"] = "请将此问题报告给开发者";

    // ==================== 命令行错误 ====================
    zh_map_["Missing value for -o option"] = "缺少 -o 参数的值";
    zh_map_["Unknown option:"] = "未知参数：";
    zh_map_["Use -h for help"] = "使用 -h 查看帮助";
    zh_map_["Extra positional argument:"] = "多余的位置参数：";
    zh_map_["Missing input file path"] = "缺少输入文件路径";
    zh_map_["Usage: deb2pkg <package.deb|package.rpm> [options]"] = "使用方式：deb2pkg <package.deb|package.rpm> [选项]";

    // ==================== 解压失败 ====================
    zh_map_["Debian package extraction failed"] = "Debian 包解压失败";
    zh_map_["Please check file integrity, try re-downloading"] = "请检查文件完整性，尝试重新下载";
    zh_map_["RPM package extraction failed"] = "RPM 包解压失败";
    zh_map_["Temporary directory creation failed"] = "临时目录创建失败";
    zh_map_["Cannot create temp directory:"] = "无法创建临时目录：";
    zh_map_["Verbose: temp directory:"] = "临时目录：";
    zh_map_["Verbose: temp files will be auto-cleaned"] = "临时文件将在程序退出时自动清理";
    zh_map_["Command execution timed out:"] = "命令执行超时：";  // This one may need a different key approach
    zh_map_["Pipe creation failed:"] = "创建管道失败：";
    zh_map_["Fork failed:"] = "创建子进程失败：";
}

const char* I18n::Tr(const char* en) {
    if (lang_ != "zh") return en;
    BuildZhMap();
    auto it = zh_map_.find(en);
    if (it != zh_map_.end()) {
        return it->second.c_str();
    }
    return en;
}

std::string I18n::Tr(const std::string& en) {
    if (lang_ != "zh") return en;
    BuildZhMap();
    auto it = zh_map_.find(en);
    if (it != zh_map_.end()) {
        return it->second;
    }
    return en;
}

} // namespace deb2pkg
