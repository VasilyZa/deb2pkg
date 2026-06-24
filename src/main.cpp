/**
 * @file main.cpp
 * @brief deb2pkg 主入口 — CLI 参数解析与流程编排
 *
 * 使用方式：
 *   deb2pkg <package.deb|package.rpm> [选项]
 *
 * 选项：
 *   -o, --output <dir>   输出目录（默认当前目录）
 *   -b, --build          自动调用 makepkg 构建
 *   -k, --keep-temp      保留临时文件
 *   -v, --verbose        详细输出
 *   -h, --help           显示帮助
 *
 * 主流程：
 *   检测类型 → 创建临时目录 → 解压 → 解析元数据
 *   → 路径修正 → ELF 依赖扫描 → pkgfile 解析
 *   → 生成 PKGBUILD → [可选: makepkg 构建] → 清理
 */

#include "core/types.h"
#include "core/errors.h"
#include "core/logger.h"
#include "core/executor.h"

#include "extract/deb_extractor.h"
#include "extract/rpm_extractor.h"

#include "metadata/deb_control_parser.h"
#include "metadata/rpm_header_parser.h"

#include "transform/path_fixer.h"
#include "transform/elf_dependency_scanner.h"

#include "pkgfile/pkgfile_resolver.h"

#include "generate/pkgbuild_writer.h"
#include "generate/makepkg_builder.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <memory>

using namespace deb2pkg;

// ---- 命令行选项 ----
struct CliOptions {
    std::filesystem::path input_file;
    std::filesystem::path output_dir = ".";
    bool build = false;
    bool keep_temp = false;
    bool verbose = false;
};

/**
 * @brief 临时目录守卫（RAII）
 *
 * 析构时自动清理临时目录，除非设置了 --keep-temp
 */
class TempDirGuard {
public:
    explicit TempDirGuard(const std::filesystem::path& path, bool keep)
        : path_(path), keep_(keep) {}

    ~TempDirGuard() {
        if (!keep_ && !path_.empty() && std::filesystem::exists(path_)) {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
            if (!ec) {
                Logger::Verbose("已清理临时目录：" + path_.string());
            }
        }
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    bool keep_;
};

// ---- 函数声明 ----
static void PrintHelp();
static bool ParseArguments(int argc, char* argv[], CliOptions& opts);
static PackageType DetectPackageType(const std::filesystem::path& file_path);
static std::optional<std::filesystem::path> CreateTempDir();
static bool CopySourceFile(const std::filesystem::path& src,
                           const std::filesystem::path& dst_dir);

// ---- 实现 ----

static void PrintHelp() {
    std::cout << R"(deb2pkg — 将 .deb/.rpm 安装包转换为 Arch Linux PKGBUILD

用法：
  deb2pkg <package.deb|package.rpm> [选项]

选项：
  -o, --output <目录>   指定 PKGBUILD 输出目录（默认为当前目录）
  -b, --build           自动调用 makepkg 编译生成 .pkg.tar.zst 安装包
  -k, --keep-temp       保留临时文件不删除（调试用）
  -v, --verbose         输出详细的处理过程信息
  -h, --help            显示此帮助信息

示例：
  deb2pkg google-chrome-stable_current_amd64.deb
  deb2pkg slack.rpm -o ~/aur-packages -v
  deb2pkg mysql-workbench.deb -b --keep-temp

注意事项：
  • pkgfile 用于自动匹配库文件对应的 Arch 包名（需先安装并初始化）
  • makepkg 需要 base-devel 包组
  • 生成的 PKGBUILD 遵循 AUR 规范
)";
}

static bool ParseArguments(int argc, char* argv[], CliOptions& opts) {
    if (argc < 2) {
        PrintHelp();
        return false;
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            PrintHelp();
            return false;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-b" || arg == "--build") {
            opts.build = true;
        } else if (arg == "-k" || arg == "--keep-temp") {
            opts.keep_temp = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                opts.output_dir = argv[++i];
            } else {
                Logger::Error("缺少 -o 参数的值");
                return false;
            }
        } else if (arg[0] == '-') {
            Logger::Error("未知参数：" + arg);
            Logger::Info("使用 -h 查看帮助");
            return false;
        } else {
            if (opts.input_file.empty()) {
                opts.input_file = arg;
            } else {
                Logger::Error("多余的位置参数：" + arg);
                return false;
            }
        }
    }

    if (opts.input_file.empty()) {
        Logger::Error("缺少输入文件路径");
        Logger::Info("使用方式：deb2pkg <package.deb|package.rpm> [选项]");
        return false;
    }

    return true;
}

static PackageType DetectPackageType(const std::filesystem::path& file_path) {
    // 方法1：通过扩展名快速判断
    std::string ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".deb") return PackageType::Deb;
    if (ext == ".rpm") return PackageType::Rpm;

    // 方法2：通过文件魔数判断
    std::ifstream file(file_path, std::ios::binary);
    if (!file) return PackageType::Unknown;

    unsigned char magic[8] = {};
    file.read(reinterpret_cast<char*>(magic), 8);

    // deb: "!<arch>\n" (ar 归档魔数)
    if (magic[0] == '!' && magic[1] == '<' &&
        magic[2] == 'a' && magic[3] == 'r' &&
        magic[4] == 'c' && magic[5] == 'h' &&
        magic[6] == '>' && magic[7] == '\n') {
        return PackageType::Deb;
    }

    // rpm: 0xedabeedb (大端)
    if (magic[0] == 0xed && magic[1] == 0xab &&
        magic[2] == 0xee && magic[3] == 0xdb) {
        return PackageType::Rpm;
    }

    return PackageType::Unknown;
}

static std::optional<std::filesystem::path> CreateTempDir() {
    std::filesystem::path tmp_base = std::filesystem::temp_directory_path();
    std::filesystem::path tmp_dir = tmp_base / "deb2pkg_XXXXXX";

    // 使用 mkdtemp 创建唯一目录
    std::string tmp_str = tmp_dir.string();
    if (mkdtemp(tmp_str.data()) == nullptr) {
        Logger::Error("无法创建临时目录：" + std::string(std::strerror(errno)));
        return std::nullopt;
    }

    return std::filesystem::path(tmp_str);
}

static bool CopySourceFile(const std::filesystem::path& src,
                           const std::filesystem::path& dst_dir) {
    std::error_code ec;
    std::filesystem::path dst = dst_dir / src.filename();

    if (std::filesystem::exists(dst)) {
        // 文件已存在（可能是从 work_dir 复制过来的），跳过
        return true;
    }

    std::filesystem::copy_file(src, dst, ec);
    if (ec) {
        Logger::Warning("复制源文件失败：" + ec.message());
        return false;
    }
    return true;
}

// ================================================================
// 主函数
// ================================================================
int main(int argc, char* argv[]) {
    // ---- 解析命令行参数 ----
    CliOptions opts;
    if (!ParseArguments(argc, argv, opts)) {
        return 1;
    }

    // 设置日志模式
    Logger::SetVerbose(opts.verbose);

    // 输出欢迎信息
    Logger::Info("deb2pkg — Debian/RPM 包转 Arch PKGBUILD 工具");
    Logger::Separator();

    // ---- 验证输入文件 ----
    if (!std::filesystem::exists(opts.input_file)) {
        auto err = GetErrorInfo(ErrorCode::FileNotFound);
        Logger::Error(err.message_cn, err.solution_cn);
        return 1;
    }

    if (!std::filesystem::is_regular_file(opts.input_file)) {
        Logger::Error("输入路径不是普通文件：" + opts.input_file.string(),
                      "请确认传入的是 .deb 或 .rpm 文件路径");
        return 1;
    }

    // 检查文件可读性
    std::error_code ec;
    auto file_size = std::filesystem::file_size(opts.input_file, ec);
    if (ec) {
        auto err = GetErrorInfo(ErrorCode::FileNotReadable);
        Logger::Error(err.message_cn, err.solution_cn);
        return 1;
    }
    Logger::Info("输入文件：" + opts.input_file.filename().string()
                + " (" + std::to_string(file_size / 1024) + " KB)");

    // ---- 检测包类型 ----
    PackageType pkg_type = DetectPackageType(opts.input_file);
    if (pkg_type == PackageType::Unknown) {
        auto err = GetErrorInfo(ErrorCode::UnknownFormat);
        Logger::Error(err.message_cn, err.solution_cn);
        return 1;
    }
    Logger::Info("包类型：" + std::string(pkg_type == PackageType::Deb ? "Debian" : "RPM"));
    Logger::Separator();

    // ---- 创建临时工作目录 ----
    auto tmp_dir_opt = CreateTempDir();
    if (!tmp_dir_opt) {
        auto err = GetErrorInfo(ErrorCode::TempDirCreateFailed);
        Logger::Error(err.message_cn, err.solution_cn);
        return 1;
    }
    TempDirGuard temp_guard(*tmp_dir_opt, opts.keep_temp);
    Logger::Verbose("临时目录：" + temp_guard.path().string());

    // ---- 解压文件 ----
    PackageInfo pkg_info;
    pkg_info.type = pkg_type;
    pkg_info.source_path = opts.input_file;

    if (pkg_type == PackageType::Deb) {
        auto extract_result = DebExtractor::Extract(opts.input_file, temp_guard.path());
        if (!extract_result) {
            Logger::Error("Debian 包解压失败", "请检查文件完整性，尝试重新下载");
            return 1;
        }
        pkg_info.extract_dir = extract_result->extract_dir;

        // 解析元数据
        pkg_info = DebControlParser::Parse(extract_result->control_content);
        pkg_info.type = pkg_type;
        pkg_info.source_path = opts.input_file;
        pkg_info.extract_dir = extract_result->extract_dir;
    } else {
        auto extract_result = RpmExtractor::Extract(opts.input_file, temp_guard.path());
        if (!extract_result) {
            Logger::Error("RPM 包解压失败", "请检查文件完整性，尝试重新下载");
            return 1;
        }
        pkg_info.extract_dir = extract_result->extract_dir;

        // 解析元数据
        pkg_info = RpmHeaderParser::Parse(extract_result->raw_header,
                                           extract_result->extract_dir);
        pkg_info.type = pkg_type;
        pkg_info.source_path = opts.input_file;
        pkg_info.extract_dir = extract_result->extract_dir;
    }

    Logger::Separator();

    // ---- 路径修正 ----
    std::vector<std::string> warnings;
    PathFixer::FixPaths(pkg_info.extract_dir, warnings);
    for (const auto& w : warnings) {
        Logger::Warning(w);
    }
    Logger::Separator();

    // ---- ELF 依赖扫描 ----
    auto sonames = ElfDependencyScanner::ScanDirectory(pkg_info.extract_dir);
    Logger::Separator();

    // ---- pkgfile 依赖解析 ----
    std::vector<std::string> unresolved;
    pkg_info.depends = PkgfileResolver::ResolveAll(sonames, unresolved);
    pkg_info.unresolved = unresolved;
    Logger::Separator();

    // ---- 生成 PKGBUILD ----
    std::string pkgbuild_content = PkgbuildWriter::Generate(pkg_info);

    // 确定输出目录
    std::filesystem::path output_dir = opts.output_dir;
    if (output_dir.is_relative()) {
        output_dir = std::filesystem::current_path() / output_dir;
    }

    auto pkgbuild_path = PkgbuildWriter::WriteToFile(
        pkgbuild_content, output_dir, pkg_info.pkgname);

    if (pkgbuild_path.empty()) {
        auto err = GetErrorInfo(ErrorCode::PkgbuildWriteFailed);
        Logger::Error(err.message_cn, err.solution_cn);
        return 1;
    }

    // ---- 复制源文件到 PKGBUILD 目录（makepkg 需要） ----
    // PKGBUILD 的 prepare() 函数会自动解压源文件，只需保证 .deb/.rpm 在 $srcdir 即可
    auto pkg_dir = pkgbuild_path.parent_path();
    CopySourceFile(opts.input_file, pkg_dir);

    Logger::Separator();

    // ---- 可选：makepkg 构建 ----
    bool build_success = true;
    if (opts.build) {
        std::string error_msg;
        build_success = MakepkgBuilder::Build(pkg_dir, error_msg);
        if (!build_success) {
            // 不直接返回错误，因为 PKGBUILD 已经生成
            Logger::Warning("PKGBUILD 已生成但 makepkg 构建失败，"
                           "请手动修复后重新构建");
        }
        Logger::Separator();
    }

    // ---- 输出总结 ----
    Logger::Info("");
    Logger::Info("======== 处理总结 ========");
    Logger::Info("  原始包:     " + pkg_info.orig_name);
    Logger::Info("  Arch 包名:  " + pkg_info.pkgname);
    Logger::Info("  版本:       " + pkg_info.version);
    Logger::Info("  架构:       " + pkg_info.arch);
    Logger::Info("  依赖数:     " + std::to_string(pkg_info.depends.size()) + " 个");
    if (!pkg_info.unresolved.empty()) {
        Logger::Warning("未匹配库:    " + std::to_string(pkg_info.unresolved.size()) + " 个");
    }
    Logger::Info("  PKGBUILD:   " + pkgbuild_path.string());

    if (opts.build && build_success) {
        Logger::Success("构建完成！安装包位于：" + pkg_dir.string());
    }

    if (!opts.keep_temp) {
        Logger::Verbose("临时文件将在程序退出时自动清理");
    } else {
        Logger::Info("  临时目录:   " + temp_guard.path().string());
    }

    Logger::Info("============================");
    Logger::Info("");

    if (!pkg_info.unresolved.empty()) {
        Logger::Info("提示：有 " + std::to_string(pkg_info.unresolved.size())
                    + " 个共享库未自动匹配。");
        Logger::Info("请编辑 PKGBUILD 手动补充 depends 数组。");
        Logger::Info("可以使用 pkgfile -s <库名> 命令手动查找包名。");
    }

    if (!opts.build) {
        Logger::Info("要构建安装包，请运行：");
        Logger::Info("  cd " + pkg_dir.string() + " && makepkg -sf");
    }

    return 0;
}
