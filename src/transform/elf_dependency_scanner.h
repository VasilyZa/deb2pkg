/**
 * @file elf_dependency_scanner.h
 * @brief ELF 二进制依赖分析模块
 *
 * 遍历解压目录中的所有 ELF 可执行文件和共享库，
 * 通过 readelf -d 命令或手动 ELF 解析，
 * 提取 DT_NEEDED 条目（动态链接库的 soname）。
 */

#pragma once

#include <string>
#include <vector>
#include <set>
#include <filesystem>

namespace deb2pkg {

/**
 * @brief ELF 依赖扫描器
 *
 * 扫描给定目录下的所有 ELF 文件，
 * 提取其依赖的共享库 soname 列表（去重）。
 */
class ElfDependencyScanner {
public:
    /**
     * @brief 扫描目录下所有 ELF 文件的共享库依赖
     * @param root_dir 扫描根目录
     * @return 所有被依赖的 soname 集合（如 {"libc.so.6", "libpthread.so.0"}）
     */
    static std::set<std::string> ScanDirectory(
        const std::filesystem::path& root_dir);

    /**
     * @brief 检查文件是否为 ELF 格式
     * @param file_path 文件路径
     * @return true 表示是 ELF 文件
     */
    static bool IsElfFile(const std::filesystem::path& file_path);

    /**
     * @brief 获取单个 ELF 文件的 NEEDED 库列表
     * @param elf_path ELF 文件路径
     * @return soname 列表
     */
    static std::vector<std::string> GetNeededLibraries(
        const std::filesystem::path& elf_path);

private:
    /**
     * @brief 通过 readelf -d 命令获取依赖（首选方案）
     */
    static std::vector<std::string> GetNeededViaReadelf(
        const std::filesystem::path& elf_path);

    /**
     * @brief 手动解析 ELF 获取依赖（回退方案）
     *
     * 使用系统 elf.h 头文件解析 ELF 结构。
     */
    static std::vector<std::string> GetNeededViaElfParsing(
        const std::filesystem::path& elf_path);
};

} // namespace deb2pkg
