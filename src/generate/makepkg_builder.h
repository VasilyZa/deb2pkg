/**
 * @file makepkg_builder.h
 * @brief makepkg 构建模块 — 可选地自动调用 makepkg 生成 Arch 安装包
 *
 * 当用户指定 -b/--build 参数时，
 * 在生成的 PKGBUILD 目录中调用 makepkg 构建 .pkg.tar.zst。
 */

#pragma once

#include <string>
#include <filesystem>

namespace deb2pkg {

/**
 * @brief makepkg 构建器
 *
 * 封装 makepkg 命令调用，处理依赖安装和打包流程。
 */
class MakepkgBuilder {
public:
    /**
     * @brief 在指定目录中调用 makepkg 构建安装包
     * @param pkgbuild_dir 包含 PKGBUILD 的目录
     * @param error_msg [out] 失败时的错误信息
     * @return true 表示构建成功
     */
    static bool Build(const std::filesystem::path& pkgbuild_dir,
                      std::string& error_msg);

    /**
     * @brief 检查 makepkg 是否可用
     */
    static bool IsAvailable();
};

} // namespace deb2pkg
