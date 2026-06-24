/**
 * @file main.cpp
 * @brief deb2pkg 主入口 — CLI 参数解析与流程编排
 *
 * 使用方式：deb2pkg <package.deb|package.rpm> [选项]
 */

#include "core/types.h"
#include "core/errors.h"
#include "core/logger.h"
#include "core/executor.h"
#include "core/i18n.h"

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

struct CliOptions {
    std::filesystem::path input_file;
    std::filesystem::path output_dir = ".";
    std::string lang;
    bool build = false;
    bool keep_temp = false;
    bool verbose = false;
};

class TempDirGuard {
public:
    explicit TempDirGuard(const std::filesystem::path& path, bool keep)
        : path_(path), keep_(keep) {}
    ~TempDirGuard() {
        if (!keep_ && !path_.empty() && std::filesystem::exists(path_)) {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
            if (!ec) Logger::Verbose(_("Temp files auto-cleaned"));
        }
    }
    const std::filesystem::path& path() const { return path_; }
private:
    std::filesystem::path path_;
    bool keep_;
};

static void PrintHelp() {
    std::cout << _("deb2pkg — Debian/RPM to Arch PKGBUILD Converter") << "\n\n"
              << _("Usage:") << "\n"
              << _("  deb2pkg <package.deb|package.rpm> [options]") << "\n\n"
              << _("Options:") << "\n"
              << _("  -o, --output <dir>   Output directory for PKGBUILD (default: current dir)") << "\n"
              << _("  -b, --build          Also run makepkg to build .pkg.tar.zst") << "\n"
              << _("  -k, --keep-temp      Keep temporary files (for debugging)") << "\n"
              << _("  -v, --verbose        Verbose output") << "\n"
              << _("  -h, --help           Show this help") << "\n"
              << _("  --lang <zh|en>       Force language (zh=Chinese, en=English)") << "\n\n"
              << _("Examples:") << "\n"
              << _("  deb2pkg google-chrome-stable_current_amd64.deb") << "\n"
              << _("  deb2pkg slack.rpm -o ~/aur-packages -v") << "\n"
              << _("  deb2pkg mysql-workbench.deb -b --keep-temp") << "\n\n"
              << _("Notes:") << "\n"
              << _("  • pkgfile is needed for automatic dependency matching") << "\n"
              << _("  • makepkg requires base-devel package group") << "\n"
              << _("  • Generated PKGBUILD follows the AUR specification") << "\n";
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
        } else if (arg == "--lang") {
            if (i + 1 < argc) {
                opts.lang = argv[++i];
                I18n::SetLanguage(opts.lang);  // 即时生效
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                opts.output_dir = argv[++i];
            } else {
                Logger::Error(_("Missing value for -o option"));
                return false;
            }
        } else if (arg[0] == '-') {
            Logger::Error(std::string(_("Unknown option:")) + " " + arg);
            Logger::Info(_("Use -h for help"));
            return false;
        } else {
            if (opts.input_file.empty()) {
                opts.input_file = arg;
            } else {
                Logger::Error(std::string(_("Extra positional argument:")) + " " + arg);
                return false;
            }
        }
    }
    if (opts.input_file.empty()) {
        Logger::Error(_("Missing input file path"));
        Logger::Info(_("Usage: deb2pkg <package.deb|package.rpm> [options]"));
        return false;
    }
    return true;
}

static PackageType DetectPackageType(const std::filesystem::path& file_path) {
    std::string ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".deb") return PackageType::Deb;
    if (ext == ".rpm") return PackageType::Rpm;

    std::ifstream file(file_path, std::ios::binary);
    if (!file) return PackageType::Unknown;
    unsigned char magic[8] = {};
    file.read(reinterpret_cast<char*>(magic), 8);
    if (magic[0] == '!' && magic[1] == '<' && magic[2] == 'a' && magic[3] == 'r' &&
        magic[4] == 'c' && magic[5] == 'h' && magic[6] == '>' && magic[7] == '\n')
        return PackageType::Deb;
    if (magic[0] == 0xed && magic[1] == 0xab && magic[2] == 0xee && magic[3] == 0xdb)
        return PackageType::Rpm;
    return PackageType::Unknown;
}

static std::optional<std::filesystem::path> CreateTempDir() {
    std::filesystem::path tmp_base = std::filesystem::temp_directory_path();
    std::filesystem::path tmp_dir = tmp_base / "deb2pkg_XXXXXX";
    std::string tmp_str = tmp_dir.string();
    if (mkdtemp(tmp_str.data()) == nullptr) {
        Logger::Error(std::string(_("Cannot create temp directory:")) + " " + std::string(std::strerror(errno)));
        return std::nullopt;
    }
    return std::filesystem::path(tmp_str);
}

static bool CopySourceFile(const std::filesystem::path& src,
                           const std::filesystem::path& dst_dir) {
    std::error_code ec;
    std::filesystem::path dst = dst_dir / src.filename();
    if (std::filesystem::exists(dst)) return true;
    std::filesystem::copy_file(src, dst, ec);
    if (ec) return false;
    return true;
}

// ================================================================
int main(int argc, char* argv[]) {
    // ---- 初始化 i18n ----
    I18n::Init();

    // ---- 解析命令行参数 ----
    CliOptions opts;
    if (!ParseArguments(argc, argv, opts)) return 1;

    // 用户指定的语言覆盖自动检测
    if (!opts.lang.empty()) I18n::SetLanguage(opts.lang);

    Logger::SetVerbose(opts.verbose);

    // 欢迎信息
    Logger::Info(_("deb2pkg — Debian/RPM to Arch PKGBUILD Converter"));
    Logger::Separator();

    // ---- 验证输入文件 ----
    if (!std::filesystem::exists(opts.input_file)) {
        auto err = GetErrorInfo(ErrorCode::FileNotFound);
        Logger::Error(_(err.message_cn.c_str()), _(err.solution_cn.c_str()));
        return 1;
    }
    if (!std::filesystem::is_regular_file(opts.input_file)) {
        Logger::Error(_("Input path is not a regular file"),
                      _("Please provide a valid .deb or .rpm file path"));
        return 1;
    }
    std::error_code ec;
    auto file_size = std::filesystem::file_size(opts.input_file, ec);
    if (ec) {
        auto err = GetErrorInfo(ErrorCode::FileNotReadable);
        Logger::Error(_(err.message_cn.c_str()), _(err.solution_cn.c_str()));
        return 1;
    }
    Logger::Info(std::string(_("Input file:")) + " " + opts.input_file.filename().string()
                + " (" + std::to_string(file_size / 1024) + " KB)");

    // ---- 检测包类型 ----
    PackageType pkg_type = DetectPackageType(opts.input_file);
    if (pkg_type == PackageType::Unknown) {
        auto err = GetErrorInfo(ErrorCode::UnknownFormat);
        Logger::Error(_(err.message_cn.c_str()), _(err.solution_cn.c_str()));
        return 1;
    }
    Logger::Info(std::string(_("Package type:")) + " " +
                 (pkg_type == PackageType::Deb ? _("Debian") : _("RPM")));
    Logger::Separator();

    // ---- 创建临时工作目录 ----
    auto tmp_dir_opt = CreateTempDir();
    if (!tmp_dir_opt) {
        Logger::Error(_("Temporary directory creation failed"));
        return 1;
    }
    TempDirGuard temp_guard(*tmp_dir_opt, opts.keep_temp);
    Logger::Verbose(std::string(_("Verbose: temp directory:")) + " " + temp_guard.path().string());

    // ---- 解压文件 ----
    PackageInfo pkg_info;
    pkg_info.type = pkg_type;
    pkg_info.source_path = opts.input_file;

    if (pkg_type == PackageType::Deb) {
        Logger::Info(_("Extracting Debian package..."));
        auto er = DebExtractor::Extract(opts.input_file, temp_guard.path());
        if (!er) { Logger::Error(_("Debian package extraction failed"), _("Please check file integrity, try re-downloading")); return 1; }
        pkg_info.extract_dir = er->extract_dir;
        pkg_info = DebControlParser::Parse(er->control_content);
        pkg_info.type = pkg_type;
        pkg_info.source_path = opts.input_file;
        pkg_info.extract_dir = er->extract_dir;
    } else {
        Logger::Info(_("Extracting RPM package..."));
        auto er = RpmExtractor::Extract(opts.input_file, temp_guard.path());
        if (!er) { Logger::Error(_("RPM package extraction failed"), _("Please check file integrity, try re-downloading")); return 1; }
        pkg_info.extract_dir = er->extract_dir;
        pkg_info = RpmHeaderParser::Parse(er->raw_header, er->extract_dir);
        pkg_info.type = pkg_type;
        pkg_info.source_path = opts.input_file;
        pkg_info.extract_dir = er->extract_dir;
    }
    Logger::Separator();

    // ---- 路径修正 ----
    std::vector<std::string> warnings;
    PathFixer::FixPaths(pkg_info.extract_dir, warnings);
    for (const auto& w : warnings) Logger::Warning(w);
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

    std::filesystem::path output_dir = opts.output_dir;
    if (output_dir.is_relative())
        output_dir = std::filesystem::current_path() / output_dir;

    auto pkgbuild_path = PkgbuildWriter::WriteToFile(pkgbuild_content, output_dir, pkg_info.pkgname);
    if (pkgbuild_path.empty()) {
        Logger::Error(_("Cannot write PKGBUILD file:"),
                      _("Please check disk space and directory permissions"));
        return 1;
    }

    auto pkg_dir = pkgbuild_path.parent_path();
    CopySourceFile(opts.input_file, pkg_dir);

    Logger::Separator();

    // ---- 可选：makepkg 构建 ----
    bool build_success = true;
    if (opts.build) {
        std::string error_msg;
        build_success = MakepkgBuilder::Build(pkg_dir, error_msg);
        if (!build_success)
            Logger::Warning(_("Build failed, but PKGBUILD was generated"));
        Logger::Separator();
    }

    // ---- 输出总结 ----
    Logger::Info("");
    Logger::Info(_("======== Summary ========"));
    Logger::Info(std::string(_("Original package:")) + " " + pkg_info.orig_name);
    Logger::Info(std::string(_("Arch package name:")) + " " + pkg_info.pkgname);
    Logger::Info(std::string(_("Version:")) + " " + pkg_info.version);
    Logger::Info(std::string(_("Architecture:")) + " " + pkg_info.arch);
    Logger::Info(std::string(_("Dependencies:")) + " " + std::to_string(pkg_info.depends.size()));
    if (!pkg_info.unresolved.empty())
        Logger::Warning(std::string(_("Unmatched libs:")) + " " + std::to_string(pkg_info.unresolved.size()));
    Logger::Info(std::string("  PKGBUILD:   ") + pkgbuild_path.string());
    if (opts.keep_temp)
        Logger::Info(std::string(_("Temp directory:")) + " " + temp_guard.path().string());
    Logger::Info(_("============================"));
    Logger::Info("");

    if (!pkg_info.unresolved.empty()) {
        Logger::Info(std::string(_("Tip:")) + " " + std::to_string(pkg_info.unresolved.size())
                    + " " + _("shared libs were not auto-matched."));
        Logger::Info(_("Please edit PKGBUILD to manually add the missing depends."));
        Logger::Info(_("You can use 'pkgfile -s <libname>' to manually find the package name."));
    }
    if (!opts.build) {
        Logger::Info(_("To build the package, run:"));
        Logger::Info("  cd " + pkg_dir.string() + " && makepkg -sf");
    }
    return 0;
}
