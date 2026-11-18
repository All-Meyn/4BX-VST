#include "WindowDiagnostics.h"

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <cstdio>

namespace fourbx::diag
{
std::string describeWindowChain (void* nativeHandle)
{
    std::string out;
    auto hwnd = static_cast<HWND> (nativeHandle);

    for (int level = 0; hwnd != nullptr && level < 5; ++level)
    {
        const auto style   = (unsigned long long) GetWindowLongPtrW (hwnd, GWL_STYLE);
        const auto exStyle = (unsigned long long) GetWindowLongPtrW (hwnd, GWL_EXSTYLE);

        char className[48] = {};
        GetClassNameA (hwnd, className, (int) sizeof (className));

        const bool layered = (exStyle & WS_EX_LAYERED) != 0;
        BYTE alpha = 255;
        COLORREF key = 0;
        DWORD flags = 0;
        const bool hasAlpha = layered && GetLayeredWindowAttributes (hwnd, &key, &alpha, &flags) && (flags & LWA_ALPHA) != 0;

        char buf[256];
        std::snprintf (buf, sizeof (buf), "%s[%d %.24s %s%s%s a=%d st=%llx ex=%llx]",
                       level > 0 ? " < " : "", level, className,
                       IsWindowEnabled (hwnd) ? "en" : "DISABLED",
                       IsWindowVisible (hwnd) ? "" : " HIDDEN",
                       layered ? " LAYERED" : "",
                       hasAlpha ? (int) alpha : -1,
                       style, exStyle);
        out += buf;

        if ((style & WS_CHILD) == 0)
            break; // reached the top-level window

        hwnd = GetParent (hwnd);
    }

    return out;
}
} // namespace fourbx::diag

#else

namespace fourbx::diag
{
std::string describeWindowChain (void*) { return "n/a (not Windows)"; }
}

#endif
