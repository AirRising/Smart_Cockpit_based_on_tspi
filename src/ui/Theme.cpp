#include "Theme.h"

#include <QFontDatabase>

namespace sc {

QFont Theme::monoFont(int pixelSize, int weight)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(pixelSize);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

QString Theme::styleSheet()
{
    return QStringLiteral(R"qss(
* {
    font-family: "Noto Sans", "Noto Sans CJK SC", "DejaVu Sans",
                 "WenQuanYi Micro Hei", "Droid Sans", sans-serif;
    color: #E6EDF3;
    outline: none;
}

QWidget#appRoot { background: #0A0E14; }
QWidget#instrumentPage, QWidget#mediaPage, QWidget#climatePage {
    background: transparent;
}

/* Reverse camera page: black surround for the video letterbox; the badge
   and caption live on a strip below the video, never over it. */
QWidget#reversePage { background: black; }
QLabel#reverseBadge {
    background: #EF4444; color: #E6EDF3;
    font-size: 26px; font-weight: 800;
    border-radius: 12px;
    min-width: 48px; max-width: 48px;
    min-height: 48px; max-height: 48px;
}
QLabel#reverseCaption {
    color: #E6EDF3; font-size: 13px; font-weight: 700;
    background: rgba(10, 14, 20, 160);
    border-radius: 8px; padding: 6px 16px;
}

/* ------------------------------------------------------------------ bars */
QWidget#topBar { background: #0A0E14; border-bottom: 1px solid #21262D; }
QWidget#navBar  { background: #0A0E14; border-top: 1px solid #21262D; }

QLabel#brandLogo {
    background: #2F81F7;
    color: #0A0E14; font-size: 15px; font-weight: 800;
    border-radius: 8px;
    min-width: 34px; max-width: 34px;
    min-height: 34px; max-height: 34px;
}
QLabel#brandName { font-size: 15px; font-weight: 800; letter-spacing: 2px; }
QLabel#brandTag  { font-size: 11px; color: #8B949E; }
QLabel#pageTitle { font-size: 16px; font-weight: 700; letter-spacing: 1px; color: #C9D1D9; }

QLabel#clockLabel { font-size: 20px; font-weight: 700; color: #E6EDF3; }
QLabel#dateLabel  { font-size: 12px; color: #8B949E; }

/* Top-bar status pills (state/connection set via dynamic properties). */
QLabel#canStatus {
    border-radius: 10px; padding: 4px 10px; font-weight: 700;
}
QLabel#canStatus[connected="true"] {
    background: #0A0E14; color: #22C55E; border: 1px solid #22C55E;
}
QLabel#canStatus[connected="false"] {
    background: #161B22; color: #8B949E; border: 1px solid #21262D;
}

QLabel#statePill {
    border-radius: 12px; padding: 4px 14px; font-weight: 700;
}
QLabel#statePill[state="ok"] {
    background: #0A0E14; color: #22C55E; border: 1px solid #22C55E;
}
QLabel#statePill[state="warn"] {
    background: #0A0E14; color: #F59E0B; border: 1px solid #F59E0B;
}
QLabel#statePill[state="err"] {
    background: #0A0E14; color: #EF4444; border: 1px solid #EF4444;
}

QLabel#statusStrip {
    background: #0A0E14; color: #484F58; font-size: 11px; padding: 3px 16px;
}

/* ---------------------------------------------------------------- cards */
QFrame#card {
    background: #161B22;
    border: 1px solid #21262D;
    border-radius: 12px;
}
QLabel#cardTitle {
    font-size: 11px; font-weight: 700; letter-spacing: 2px; color: #8B949E;
}

/* Climate page readouts (text is plain; all styling lives here). */
QLabel#fanValue { color: #E6EDF3; }

QLabel#statusPill {
    border-radius: 12px; padding: 6px 14px;
    font-size: 12px; font-weight: 700;
}
QLabel#statusPill[state="ok"] {
    background: #0A0E14; color: #22C55E; border: 1px solid #22C55E;
}
QLabel#statusPill[state="warn"] {
    background: #0A0E14; color: #F59E0B; border: 1px solid #F59E0B;
}

/* Gear readout on the instrument page: big character + accent underline. */
QLabel#gearLabel {
    background: #0A0E14;
    border: 1px solid #21262D;
    border-bottom: 3px solid #2F81F7;
    border-radius: 12px;
    color: #E6EDF3; font-size: 44px; font-weight: 800;
}

/* ------------------------------------------------------------- buttons */
QPushButton {
    background: #21262D; border: 1px solid #30363D; border-radius: 8px;
    padding: 8px 18px; color: #C9D1D9; font-size: 13px; font-weight: 600;
}
QPushButton:hover { border-color: #2F81F7; color: #E6EDF3; }
QPushButton:pressed { background: #161B22; }
QPushButton:disabled { color: #484F58; background: #161B22; border-color: #21262D; }

QPushButton#primary {
    background: #161B22; border: 1px solid #2F81F7; color: #38BDF8;
}
QPushButton#primary:hover { background: #21262D; }
QPushButton#primary:disabled { color: #484F58; background: #161B22; border-color: #21262D; }

QPushButton#cancelPending { background: #161B22; border: 1px solid #EF4444; color: #EF4444; }
QPushButton#cancelPending:hover { background: #21262D; }
QPushButton#cancelPending:disabled { color: #484F58; background: #161B22; border-color: #21262D; }

QPushButton#iconBtn {
    border-radius: 12px; font-size: 18px; padding: 0;
    min-width: 34px; max-width: 34px;
    min-height: 34px; max-height: 34px;
}

QPushButton#toggle { background: #21262D; border: 1px solid #30363D; color: #8B949E; }
QPushButton#toggle:checked { background: #161B22; border: 1px solid #2F81F7; color: #38BDF8; }

QPushButton#mode { background: transparent; border: 1px solid #30363D; color: #8B949E; }
QPushButton#mode:hover { border-color: #2F81F7; color: #C9D1D9; }
QPushButton#mode:checked { background: #161B22; border: 1px solid #2F81F7; color: #38BDF8; }

/* -------------------------------------------------------------- slider */
QSlider::groove:horizontal {
    height: 6px; background: #21262D; border-radius: 3px;
}
QSlider::sub-page:horizontal {
    background: #2F81F7;
    border-radius: 3px;
}
QSlider::handle:horizontal {
    width: 20px; height: 20px; margin: -7px 0;
    background: #E6EDF3; border: 3px solid #2F81F7; border-radius: 10px;
}
QSlider::handle:horizontal:hover { background: #2F81F7; }
QSlider:disabled::sub-page:horizontal { background: #30363D; }
QSlider:disabled::handle:horizontal { background: #484F58; border-color: #30363D; }

/* --------------------------------------------------------- progress bar */
QProgressBar {
    background: #21262D; border: none; border-radius: 6px;
    height: 12px; text-align: center;
}
QProgressBar::chunk { background: #2F81F7; border-radius: 6px; }

/* Climate page fan-level bar: thinner, flat accent chunk. */
QProgressBar#fanBar {
    background: #21262D; border: none; border-radius: 4px;
    min-height: 6px; max-height: 6px;
}
QProgressBar#fanBar::chunk { background: #2F81F7; border-radius: 4px; }
QProgressBar#fanBar:disabled::chunk { background: #484F58; }

/* ------------------------------------------------------------ combo box */
QComboBox {
    background: #21262D; border: 1px solid #30363D; border-radius: 8px;
    padding: 8px 14px; color: #C9D1D9;
}
QComboBox:hover { border-color: #2F81F7; }
QComboBox::drop-down { border: none; width: 26px; }
QComboBox::down-arrow {
    border-left: 5px solid transparent; border-right: 5px solid transparent;
    border-top: 6px solid #8B949E; margin-right: 8px;
}
QComboBox QAbstractItemView {
    background: #161B22; border: 1px solid #30363D; color: #C9D1D9;
    selection-background-color: #21262D; selection-color: #38BDF8; outline: 0;
}

/* ---------------------------------------------------------------- list */
QListWidget {
    background: #0A0E14; border: 1px solid #21262D; border-radius: 12px;
    padding: 6px; outline: 0;
}
QListWidget::item {
    padding: 10px 14px; border-radius: 8px; color: #C9D1D9; margin: 2px 0;
}
QListWidget::item:hover { background: #161B22; }
QListWidget::item:selected { background: #21262D; color: #38BDF8; }

/* ---------------------------------------------------------- scrollbars */
QScrollBar:vertical { background: transparent; width: 8px; margin: 2px 2px 2px 0; }
QScrollBar::handle:vertical { background: #30363D; border-radius: 4px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #484F58; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 0 2px 2px 2px; }
QScrollBar::handle:horizontal { background: #30363D; border-radius: 4px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: #484F58; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

/* ------------------------------------------------------ media player */
/* Track title / time readouts (albumArt + transport buttons are custom-
   painted widgets, they take their colours from Theme constants in code). */
QLabel#trackLabel { font-size: 20px; font-weight: 800; color: #E6EDF3; }
QLabel#timeLabel  { color: #8B949E; }

/* ----------------------------------------------------------- navigation */
QToolButton#navButton {
    background: transparent; border: none; border-radius: 12px;
    color: #8B949E; font-size: 13px; font-weight: 600;
    padding: 10px 22px;
}
QToolButton#navButton:hover { background: #161B22; color: #E6EDF3; }
QToolButton#navButton:checked {
    background: #161B22; color: #2F81F7;
    border-bottom: 2px solid #2F81F7;
}
)qss");
}

} // namespace sc
