#pragma once

#include <QFont>
#include <QString>

namespace sc {

// Central visual theme for the whole HMI.
//
// Applies one global stylesheet (dark automotive look) in main.cpp so every
// page stays consistent: cards, nav bar, sliders, buttons, list, scrollbars.
// All colours / radii / spacing used by custom-painted widgets must come from
// the tokens below -- no magic numbers in .cpp files.
class Theme
{
public:
    // Surfaces & text (design tokens).
    static constexpr const char *Bg = "#0A0E14";
    static constexpr const char *Card = "#161B22";
    static constexpr const char *CardBorder = "#21262D";
    static constexpr const char *TextPrimary = "#E6EDF3";
    static constexpr const char *TextSecondary = "#8B949E";
    static constexpr const char *Disabled = "#484F58";

    // Accent & semantic colours.
    static constexpr const char *Accent = "#2F81F7";   // primary accent
    static constexpr const char *AccentSoft = "#2563EB";
    static constexpr const char *Ok = "#22C55E";       // system health only
    static constexpr const char *Warn = "#F59E0B";
    static constexpr const char *Err = "#EF4444";
    static constexpr const char *Cool = "#38BDF8";     // accent-coloured text
    static constexpr const char *Warm = "#FB923C";

    // Geometry tokens (px). All spacing is a multiple of SpacingUnit.
    static constexpr int RadiusSmall = 8;
    static constexpr int RadiusCard = 12;
    static constexpr int RadiusMax = 16;
    static constexpr int SpacingUnit = 8;

    // Monospaced font for numeric readouts: fixed-width digits keep the text
    // from jittering horizontally when the value changes.
    static QFont monoFont(int pixelSize, int weight = QFont::Bold);

    static QString styleSheet();
};

} // namespace sc
