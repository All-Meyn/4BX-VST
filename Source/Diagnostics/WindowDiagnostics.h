#pragma once

// TEMPORARY diagnostics for the Audacity 4 preview-fade investigation.
// Not for merging.

#include <string>

namespace fourbx::diag
{
    // Describes the native window and its parents: class name, enabled/visible,
    // style bits, and layered-window alpha. Windows only; returns "n/a" elsewhere.
    std::string describeWindowChain (void* nativeHandle);
}
