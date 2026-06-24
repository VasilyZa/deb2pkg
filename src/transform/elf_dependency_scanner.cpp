/**
 * @file elf_dependency_scanner.cpp
 * @brief ElfDependencyScanner 实现
 *
 * 两种方式解析 ELF 依赖：
 * 1. 调用 readelf -d（首选，简单可靠）
 * 2. 手动解析 ELF 结构（回退，使用系统 elf.h）
 */

#include "elf_dependency_scanner.h"
#include "core/executor.h"
#include "core/logger.h"

#include <fstream>
#include <cstring>
#include <elf.h>

namespace deb2pkg {

bool ElfDependencyScanner::IsElfFile(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) return false;

    unsigned char magic[4];
    file.read(reinterpret_cast<char*>(magic), 4);
    return file.good()
        && magic[0] == 0x7f
        && magic[1] == 'E'
        && magic[2] == 'L'
        && magic[3] == 'F';
}

std::vector<std::string> ElfDependencyScanner::GetNeededViaReadelf(
    const std::filesystem::path& elf_path)
{
    std::vector<std::string> sonames;

    auto result = Executor::RunCommandLines(
        {"readelf", "-d", elf_path.string()},
        {},
        10
    );

    if (!result.first) {
        return sonames;
    }

    // 解析 readelf -d 输出
    // 格式： 0x0000000000000001 (NEEDED)  共享库：[libc.so.6]
    for (const auto& line : result.second) {
        // 查找 "(NEEDED)" 关键字
        if (line.find("(NEEDED)") == std::string::npos) continue;
        // 提取方括号中的内容
        auto start = line.find('[');
        auto end = line.find(']', start);
        if (start != std::string::npos && end != std::string::npos && end > start) {
            sonames.push_back(line.substr(start + 1, end - start - 1));
        }
    }

    return sonames;
}

std::vector<std::string> ElfDependencyScanner::GetNeededViaElfParsing(
    const std::filesystem::path& elf_path)
{
    std::vector<std::string> sonames;

    std::ifstream file(elf_path, std::ios::binary);
    if (!file) return sonames;

    // 读取 ELF Header
    // 先判断是 32 位还是 64 位
    unsigned char e_ident[EI_NIDENT];
    file.read(reinterpret_cast<char*>(e_ident), EI_NIDENT);
    if (!file.good()) return sonames;

    bool is_64bit = (e_ident[EI_CLASS] == ELFCLASS64);

    file.seekg(0, std::ios::beg);

    if (is_64bit) {
        // ---- 64 位 ELF ----
        Elf64_Ehdr ehdr;
        file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
        if (!file.good()) return sonames;

        // 遍历 Program Headers 找 PT_DYNAMIC
        file.seekg(ehdr.e_phoff, std::ios::beg);
        Elf64_Phdr phdr;
        Elf64_Addr dynamic_addr = 0;
        Elf64_Xword dynamic_size = 0;

        for (Elf64_Half i = 0; i < ehdr.e_phnum; i++) {
            file.read(reinterpret_cast<char*>(&phdr), sizeof(phdr));
            if (!file.good()) break;
            if (phdr.p_type == PT_DYNAMIC) {
                dynamic_addr = phdr.p_vaddr;
                dynamic_size = phdr.p_filesz;
                // 获取文件偏移
                // 找到包含此虚拟地址的 LOAD 段，转换地址
                // 简化处理：使用 p_offset（假设只有一个 LOAD 段）
            }
        }

        if (dynamic_size == 0) return sonames;

        // 需要读取 Section Headers 来获取 .dynstr 段信息
        file.seekg(ehdr.e_shoff, std::ios::beg);
        std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
        for (Elf64_Half i = 0; i < ehdr.e_shnum; i++) {
            file.read(reinterpret_cast<char*>(&shdrs[i]), sizeof(Elf64_Shdr));
            if (!file.good()) break;
        }

        // 查找 .dynstr 和 .dynamic 段
        const char* dynstr_data = nullptr;
        size_t dynstr_size = 0;
        const Elf64_Dyn* dynamic_data = nullptr;
        size_t dynamic_count = 0;

        for (Elf64_Half i = 0; i < ehdr.e_shnum; i++) {
            if (shdrs[i].sh_type == SHT_STRTAB) {
                // 获取段名称
                // .shstrtab 在 ehdr.e_shstrndx
                if (i == ehdr.e_shstrndx) {
                    // 跳过 .shstrtab
                    continue;
                }
                // 简单方式：通过 .dynamic 段中的 DT_STRTAB 地址对应
            }
            if (shdrs[i].sh_type == SHT_DYNAMIC) {
                dynamic_count = shdrs[i].sh_size / sizeof(Elf64_Dyn);
                std::vector<char> dyn_buf(shdrs[i].sh_size);
                file.seekg(shdrs[i].sh_offset, std::ios::beg);
                file.read(dyn_buf.data(), shdrs[i].sh_size);
                if (!file.good()) break;

                // 解析 dynamic entries
                const Elf64_Dyn* dyn_entries =
                    reinterpret_cast<const Elf64_Dyn*>(dyn_buf.data());

                Elf64_Addr strtab_addr = 0;
                for (size_t j = 0; j < dynamic_count; j++) {
                    if (dyn_entries[j].d_tag == DT_STRTAB) {
                        strtab_addr = dyn_entries[j].d_un.d_val;
                    } else if (dyn_entries[j].d_tag == DT_STRSZ) {
                        dynstr_size = dyn_entries[j].d_un.d_val;
                    }
                }

                // 查找 strtab 在哪个 section 中
                for (Elf64_Half k = 0; k < ehdr.e_shnum; k++) {
                    if (strtab_addr >= shdrs[k].sh_addr &&
                        strtab_addr < shdrs[k].sh_addr + shdrs[k].sh_size) {
                        size_t offset_in_section = strtab_addr - shdrs[k].sh_addr;
                        dynstr_size = std::min(dynstr_size,
                            static_cast<size_t>(shdrs[k].sh_size - offset_in_section));
                        std::vector<char> strtab(dynstr_size);
                        file.seekg(shdrs[k].sh_offset + static_cast<std::streamoff>(offset_in_section), std::ios::beg);
                        file.read(strtab.data(), static_cast<std::streamsize>(dynstr_size));

                        // 提取 NEEDED 条目
                        for (size_t j = 0; j < dynamic_count; j++) {
                            if (dyn_entries[j].d_tag == DT_NEEDED) {
                                Elf64_Xword str_idx = dyn_entries[j].d_un.d_val;
                                if (str_idx < dynstr_size) {
                                    sonames.emplace_back(strtab.data() + str_idx);
                                }
                            }
                        }
                        return sonames;
                    }
                }
            }
        }
    } else {
        // ---- 32 位 ELF（逻辑类似，使用 Elf32_ 类型）----
        // 为保持代码简洁，此处仅实现 64 位版本
        // 32 位包的依赖解析通过 readelf 回退处理
        Logger::Verbose("ELF 手动解析暂不支持 32 位，请确保 readelf 可用");
        return sonames;
    }

    return sonames;
}

std::vector<std::string> ElfDependencyScanner::GetNeededLibraries(
    const std::filesystem::path& elf_path)
{
    // 优先使用 readelf
    auto sonames = GetNeededViaReadelf(elf_path);

    if (sonames.empty()) {
        // 回退到手动解析
        Logger::Verbose("readelf 未返回结果，尝试手动解析 ELF："
                       + elf_path.filename().string());
        sonames = GetNeededViaElfParsing(elf_path);
    }

    return sonames;
}

std::set<std::string> ElfDependencyScanner::ScanDirectory(
    const std::filesystem::path& root_dir)
{
    Logger::Info("正在扫描 ELF 二进制依赖...");

    std::set<std::string> all_sonames;

    if (!std::filesystem::exists(root_dir)) {
        Logger::Warning("扫描目录不存在：" + root_dir.string());
        return all_sonames;
    }

    int elf_count = 0;
    int total_needed = 0;

    // 遍历所有文件
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(
             root_dir,
             std::filesystem::directory_options::skip_permission_denied)) {

        if (!entry.is_regular_file() && !entry.is_symlink()) continue;

        const auto& file_path = entry.path();

        // 快速检查是否是 ELF
        if (!IsElfFile(file_path)) continue;

        elf_count++;

        // 获取 NEEDED 库
        auto sonames = GetNeededLibraries(file_path);

        if (!sonames.empty()) {
            Logger::Verbose("  " + file_path.filename().string()
                          + " → " + std::to_string(sonames.size()) + " 个依赖");
        }

        for (const auto& soname : sonames) {
            all_sonames.insert(soname);
            total_needed++;
        }
    }

    Logger::Info("  扫描了 " + std::to_string(elf_count) + " 个 ELF 文件，"
                "发现 " + std::to_string(all_sonames.size())
                + " 个唯一共享库依赖");

    return all_sonames;
}

} // namespace deb2pkg
