/**
 * @file logger.h
 * @brief 中文日志输出工具
 *
 * 提供统一的控制台输出接口，支持：
 * - 普通信息输出
 * - 成功/警告/错误消息（带图标）
 * - 进度信息
 * - 详细调试输出（verbose 模式）
 */

#pragma once

#include <string>
#include <iostream>
#include <mutex>

namespace deb2pkg {

/**
 * @brief 日志输出类
 *
 * 所有控制台输出均为中文，使用统一的前缀图标区分消息类型。
 * 线程安全（通过内部互斥锁保护）。
 */
class Logger {
public:
    /**
     * @brief 启用/禁用详细输出模式
     * @param enabled true 时输出调试级别的信息
     */
    static void SetVerbose(bool enabled) { verbose_ = enabled; }

    /**
     * @brief 输出普通信息（无前缀图标）
     * @param msg 信息文本
     */
    static void Info(const std::string& msg);

    /**
     * @brief 输出成功信息
     * @param msg 成功描述
     * 格式：✓ <msg>
     */
    static void Success(const std::string& msg);

    /**
     * @brief 输出警告信息
     * @param msg 警告描述
     * 格式：⚠ 警告：<msg>
     */
    static void Warning(const std::string& msg);

    /**
     * @brief 输出错误信息
     * @param msg 错误描述
     * 格式：✗ 错误：<msg>
     */
    static void Error(const std::string& msg);

    /**
     * @brief 输出错误信息（含解决建议）
     * @param msg 错误描述
     * @param solution 解决建议
     */
    static void Error(const std::string& msg, const std::string& solution);

    /**
     * @brief 输出进度信息
     * @param step 当前步骤描述
     * @param current 当前进度
     * @param total 总步骤数
     */
    static void Progress(const std::string& step, int current, int total);

    /**
     * @brief 输出详细调试信息（仅在 verbose 模式下）
     * @param msg 调试信息
     */
    static void Verbose(const std::string& msg);

    /**
     * @brief 输出分隔线
     */
    static void Separator();

private:
    static bool verbose_;
    static std::mutex mutex_;
};

} // namespace deb2pkg
