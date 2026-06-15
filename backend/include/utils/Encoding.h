#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mis::utils {

// 将系统默认编码（中文 Windows 为 GBK）转为 UTF-8
inline std::string toUtf8(const std::string& input)
{
    if (input.empty()) return input;

#ifdef _WIN32
    // GBK/ACP → UTF-16
    int wlen = MultiByteToWideChar(CP_ACP, 0, input.c_str(), static_cast<int>(input.size()),
                                   nullptr, 0);
    if (wlen <= 0) return input;  // 转换失败，返回原文
    std::wstring wide(wlen, 0);
    MultiByteToWideChar(CP_ACP, 0, input.c_str(), static_cast<int>(input.size()),
                        &wide[0], wlen);

    // UTF-16 → UTF-8
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wlen,
                                   nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return input;
    std::string utf8(ulen, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wlen,
                        &utf8[0], ulen, nullptr, nullptr);
    return utf8;
#else
    return input;
#endif
}

// 安全获取异常消息（转为 UTF-8）
inline std::string safeError(const std::exception& ex)
{
    return toUtf8(ex.what());
}

} // namespace mis::utils
