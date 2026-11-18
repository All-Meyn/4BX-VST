#pragma once

// TEMPORARY diagnostics for the Audacity 4 preview-fade investigation.
// Not for merging.

#include <string>

namespace fourbx::diag
{
    // Describes the native window and its parents: class name, enabled/visible,
    // style bits, and layered-window alpha. Windows only; returns "n/a" elsewhere.
    std::string describeWindowChain (void* nativeHandle);

    // Lists visible windows that are in front of the given window (siblings above it
    // at every level of its parent chain, then top-level windows above its top-level
    // window) and overlap it on screen. Windows only.
    std::string describeWindowsInFront (void* nativeHandle);
}
