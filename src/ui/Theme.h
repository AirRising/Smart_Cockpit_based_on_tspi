#pragma once

#include <QString>

namespace sc {

// Central visual theme for the whole HMI.
//
// Applies one global stylesheet (dark automotive look) in main.cpp so every
// page stays consistent: cards, nav bar, sliders, buttons, list, scrollbars.
class Theme
{
public:
    // Palette accents (also referenced by custom-painted widgets).
    static constexpr const char *Accent = "#29D3F0";   // cyan
    static constexpr const char *AccentSoft = "#14A6C9";
    static constexpr const char *Amber = "#FFB400";
    static constexpr const char *Danger = "#FF4D4F";
    static constexpr const char *Ok = "#2FBF71";
    static constexpr const char *TextPrimary = "#EAF0F8";
    static constexpr const char *TextSecondary = "#8A94A8";

    static QString styleSheet();
};

} // namespace sc
