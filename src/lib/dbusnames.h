// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include <QLatin1StringView>

// Names of the D-Bus API (docs/ARCHITECTURE.md, "D-Bus API"). Plain strings
// only, so vdwcore stays free of D-Bus.
namespace Vdw::DBusNames
{
inline constexpr QLatin1StringView service("io.github.librv.vdw");
inline constexpr QLatin1StringView managerPath("/Manager");
inline constexpr QLatin1StringView managerInterface("io.github.librv.vdw.Manager");
inline constexpr QLatin1StringView scriptPath("/Script");
inline constexpr QLatin1StringView scriptInterface("io.github.librv.vdw.Script");
inline constexpr QLatin1StringView debugPath("/Debug");
inline constexpr QLatin1StringView debugInterface("io.github.librv.vdw.Debug");
}
