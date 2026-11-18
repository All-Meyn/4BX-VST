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
 #include <algorithm>

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
std::string describeWindowsInFront (void* nativeHandle)
{
    auto self = static_cast<HWND> (nativeHandle);
    RECT ours {};
    if (self == nullptr || ! GetWindowRect (self, &ours))
        return "(no window)";

    const DWORD ourPid = GetCurrentProcessId();
    std::string out;
    int found = 0;

    for (HWND level = self; level != nullptr && found < 8;)
    {
        for (HWND w = GetWindow (level, GW_HWNDPREV); w != nullptr && found < 8; w = GetWindow (w, GW_HWNDPREV))
        {
            if (! IsWindowVisible (w))
                continue;

            RECT r {}, overlap {};
            if (! GetWindowRect (w, &r) || ! IntersectRect (&overlap, &r, &ours))
                continue;

            DWORD pid = 0;
            GetWindowThreadProcessId (w, &pid);

            char className[48] = {};
            GetClassNameA (w, className, (int) sizeof (className));

            const auto exStyle = (unsigned long long) GetWindowLongPtrW (w, GWL_EXSTYLE);
            BYTE alpha = 255; COLORREF key = 0; DWORD flags = 0;
            const bool layered = (exStyle & WS_EX_LAYERED) != 0;
            const bool hasAlpha = layered && GetLayeredWindowAttributes (w, &key, &alpha, &flags) && (flags & LWA_ALPHA) != 0;

            const int coverPct = (int) (100.0 * (double) ((overlap.right - overlap.left) * (overlap.bottom - overlap.top))
                                        / (double) std::max (1L, (ours.right - ours.left) * (ours.bottom - ours.top)));

            char buf[200];
            std::snprintf (buf, sizeof (buf), "%s{%.24s %s cover=%d%%%s a=%d ex=%llx}",
                           found > 0 ? " " : "", className,
                           pid == ourPid ? "same-proc" : "OTHER-proc",
                           coverPct, layered ? " LAYERED" : "", hasAlpha ? (int) alpha : -1, exStyle);
            out += buf;
            ++found;
        }

        if ((GetWindowLongPtrW (level, GWL_STYLE) & WS_CHILD) == 0)
            break;

        level = GetParent (level);
    }

    return found == 0 ? "nothing in front" : out;
}

} // namespace fourbx::diag

#else

namespace fourbx::diag
{
std::string describeWindowChain (void*) { return "n/a (not Windows)"; }
std::string describeWindowsInFront (void*) { return "n/a (not Windows)"; }
}

#endif
