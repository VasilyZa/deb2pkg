/**
 * @file makepkg_builder.cpp
 * @brief MakepkgBuilder 实现
 *
 * 在 PKGBUILD 所在目录中调用 makepkg 命令构建。
 * 注意：
 * 1. 需要先准备好 source 文件（deb/rpm 包）和 data/ 目录
 * 2. makepkg 必须以非 root 用户身份运行
 * 3. 需要 base-devel 包组
 */

#include "makepkg_builder.h"
#include "core/executor.h"
#include "core/logger.h"


namespace deb2pkg {

bool MakepkgBuilder::IsAvailable() {
    return Executor::IsCommandAvailable("makepkg");
}

bool MakepkgBuilder::Build(const std::filesystem::path& pkgbuild_dir,
                            std::string& error_msg) {
    Logger::Info("正在调用 makepkg 构建安装包...");
    Logger::Separator();

    // 检查 makepkg 是否可用
    if (!IsAvailable()) {
        error_msg = "makepkg 未安装。请运行：sudo pacman -S base-devel";
        Logger::Error(error_msg);
        return false;
    }

    // 检查是否为 root 用户
    auto uid_result = Executor::RunCommand({"id", "-u"}, {}, 5);
    if (uid_result.first && uid_result.second == "0\n") {
        error_msg = "请勿以 root 用户运行 makepkg。"
                    "请切换到普通用户后重试。";
        Logger::Error(error_msg);
        return false;
    }

    // 运行 makepkg
    // -s / --syncdeps: 自动安装依赖
    // -f / --force: 强制重建
    // --noconfirm: 不询问确认
    Logger::Info("  执行: makepkg -sf --noconfirm");
    Logger::Info("  工作目录: " + pkgbuild_dir.string());

    auto result = Executor::RunCommand(
        {"makepkg", "-sf", "--noconfirm"},
        pkgbuild_dir,
        600  // 10 分钟超时
    );

    Logger::Separator();

    if (!result.first) {
        // 分析失败原因
        std::string output = result.second;

        if (output.find("cannot find") != std::string::npos ||
            output.find("not found") != std::string::npos) {
            error_msg = "makepkg 构建失败：缺少依赖包。";
            error_msg += " 请安装缺失的依赖后重试。";
        } else if (output.find("permission denied") != std::string::npos) {
            error_msg = "makepkg 构建失败：权限不足。请勿以 root 运行。";
        } else if (output.find("no such file") != std::string::npos) {
            error_msg = "makepkg 构建失败：缺少源文件。"
                        " 请确保 .deb/.rpm 文件在 PKGBUILD 目录中，"
                        " 且已解压出 data/ 目录。";
        } else if (output.find("FAILED") != std::string::npos ||
                   output.find("Error") != std::string::npos) {
            error_msg = "makepkg 构建失败，详细错误信息请查看上方输出。";
            error_msg += "\n  手动排查：cd " + pkgbuild_dir.string()
                       + " && makepkg -sf";
        } else {
            error_msg = "makepkg 构建失败，错误信息：\n" + output;
        }

        Logger::Error(error_msg);
        Logger::Warning("可以尝试手动构建：cd "
                       + pkgbuild_dir.string() + " && makepkg -sf");
        return false;
    }

    // 成功：查找生成的 .pkg.tar.zst 文件
    std::string pkg_ext;
    for (const auto& entry : std::filesystem::directory_iterator(pkgbuild_dir)) {
        std::string name = entry.path().filename().string();
        if (name.find(".pkg.tar") != std::string::npos) {
            pkg_ext = entry.path().string();
            break;
        }
    }

    if (!pkg_ext.empty()) {
        Logger::Success("构建成功：" + pkg_ext);
    } else {
        Logger::Success("makepkg 构建完成（未找到 .pkg.tar 文件，"
                       "请检查输出目录）");
    }

    return true;
}

} // namespace deb2pkg
