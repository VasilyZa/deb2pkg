/**
 * @file i18n.h
 * @brief 国际化模块 — 根据系统 locale 自动选择中/英文
 *
 * 使用方式:
 *   I18n::Init();              // 自动检测 LANG 环境变量
 *   I18n::SetLanguage("zh");   // 或强制指定语言
 *   std::cout << _("Hello");   // 输出翻译后的文本
 *
 * 英文原文作为 key，中文通过翻译表映射。未找到翻译时直接返回原文。
 */

#pragma once

#include <string>
#include <unordered_map>

namespace deb2pkg {

/**
 * @brief 国际化/本地化管理器
 */
class I18n {
public:
    /**
     * @brief 从环境变量自动检测语言
     * 检查 LANG/LC_ALL/LC_MESSAGES, zh_CN* → "zh", 其余 → "en"
     */
    static void Init();

    /**
     * @brief 强制设置语言
     * @param lang "zh" 或 "en"
     */
    static void SetLanguage(const std::string& lang);

    /**
     * @brief 获取当前语言代码
     */
    static const std::string& GetLanguage() { return lang_; }

    /**
     * @brief 翻译文本
     * @param en 英文原文 (作为 key)
     * @return 当前语言的翻译文本
     */
    static const char* Tr(const char* en);

    /**
     * @brief 翻译文本 (string 版本)
     */
    static std::string Tr(const std::string& en);

private:
    static std::string lang_;
    static std::unordered_map<std::string, std::string> zh_map_;
    static void BuildZhMap();
};

} // namespace deb2pkg

/// 快捷翻译宏
#define _(s) deb2pkg::I18n::Tr(s)
