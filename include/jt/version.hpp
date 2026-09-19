#pragma once

namespace jt {

// The one place the version string lives (FIX_ME A.2): every tool's
// --version output renders this, and tests pin the binaries against it.
inline constexpr char JT_VERSION[] = "0.1.0";

} // namespace jt
