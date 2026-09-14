#include "Theme.h"

namespace sc {

QString Theme::styleSheet()
{
    return QStringLiteral(R"qss(
* {
    font-family: "Noto Sans", "Noto Sans CJK SC", "DejaVu Sans",
                 "WenQuanYi Micro Hei", "Droid Sans", sans-serif;
    color: #EAF0F8;
    outline: none;
}

QWidget#appRoot {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #0A0D13, stop:1 #10141D);
}

/* ------------------------------------------------------------------ bars */
QWidget#topBar { background: #0D1119; border-bottom: 1px solid #1C2434; }
QWidget#navBar  { background: #0D1119; border-top: 1px solid #1C2434; }

QLabel#brandLogo {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                stop:0 #14A6C9, stop:1 #29D3F0);
    color: #06131A; font-size: 15px; font-weight: 800;
    border-radius: 10px;
    min-width: 34px; max-width: 34px;
    min-height: 34px; max-height: 34px;
}
QLabel#brandName { font-size: 15px; font-weight: 800; letter-spacing: 2px; }
QLabel#brandTag  { font-size: 11px; color: #8A94A8; }
QLabel#pageTitle { font-size: 16px; font-weight: 700; letter-spacing: 1px; color: #C7D0E0; }

QLabel#clockLabel { font-size: 20px; font-weight: 700; color: #EAF0F8; }
QLabel#dateLabel  { font-size: 12px; color: #8A94A8; }

QLabel#statusStrip {
    background: #0C0F16; color: #6F7A90; font-size: 11px; padding: 3px 16px;
}

/* ---------------------------------------------------------------- cards */
QFrame#card {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #151B27, stop:1 #10141E);
    border: 1px solid #222B3D;
    border-radius: 18px;
}
QLabel#cardTitle {
    font-size: 11px; font-weight: 700; letter-spacing: 2px; color: #6F7A90;
}

/* ------------------------------------------------------------- buttons */
QPushButton {
    background: #1A2233; border: 1px solid #2A3550; border-radius: 10px;
    padding: 8px 18px; color: #C7D0E0; font-size: 13px; font-weight: 600;
}
QPushButton:hover { background: #212C44; border-color: #3A4A70; color: #EAF0F8; }
QPushButton:pressed { background: #141A28; }
QPushButton:disabled { color: #4A5468; background: #141A26; border-color: #1C2434; }

QPushButton#primary {
    background: #143A46; border: 1px solid #29D3F0; color: #7FE6FF;
}
QPushButton#primary:hover { background: #17505E; }
QPushButton#primary:disabled { color: #3E6B76; background: #13222B; border-color: #1F3C46; }

QPushButton#danger { background: #3A1A1E; border: 1px solid #FF4D4F; color: #FF9B9C; }
QPushButton#danger:hover { background: #4D2024; }
QPushButton#danger:disabled { color: #6E4447; background: #201316; border-color: #3A1A1E; }

QPushButton#iconBtn {
    border-radius: 17px; font-size: 18px; padding: 0;
    min-width: 34px; max-width: 34px;
    min-height: 34px; max-height: 34px;
}

QPushButton#toggle { background: #1A2233; border: 1px solid #2A3550; color: #8A94A8; }
QPushButton#toggle:checked { background: #143A46; border: 1px solid #29D3F0; color: #7FE6FF; }

QPushButton#mode { background: transparent; border: 1px solid #2A3550; color: #8A94A8; }
QPushButton#mode:hover { border-color: #3A4A70; color: #C7D0E0; }
QPushButton#mode:checked { background: #143A46; border: 1px solid #29D3F0; color: #7FE6FF; }

/* -------------------------------------------------------------- slider */
QSlider::groove:horizontal {
    height: 6px; background: #1A2233; border-radius: 3px;
}
QSlider::sub-page:horizontal {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                stop:0 #14A6C9, stop:1 #29D3F0);
    border-radius: 3px;
}
QSlider::handle:horizontal {
    width: 20px; height: 20px; margin: -7px 0;
    background: #EAF0F8; border: 3px solid #29D3F0; border-radius: 10px;
}
QSlider::handle:horizontal:hover { background: #29D3F0; }
QSlider:disabled::sub-page:horizontal { background: #2A3550; }
QSlider:disabled::handle:horizontal { background: #4A5468; border-color: #2A3550; }

/* --------------------------------------------------------- progress bar */
QProgressBar {
    background: #1A2233; border: none; border-radius: 6px;
    height: 12px; text-align: center;
}
QProgressBar::chunk { border-radius: 6px; }

/* ------------------------------------------------------------ combo box */
QComboBox {
    background: #1A2233; border: 1px solid #2A3550; border-radius: 10px;
    padding: 8px 14px; color: #C7D0E0;
}
QComboBox:hover { border-color: #3A4A70; }
QComboBox::drop-down { border: none; width: 26px; }
QComboBox::down-arrow {
    border-left: 5px solid transparent; border-right: 5px solid transparent;
    border-top: 6px solid #8A94A8; margin-right: 8px;
}
QComboBox QAbstractItemView {
    background: #141A26; border: 1px solid #2A3550; color: #C7D0E0;
    selection-background-color: #143A46; selection-color: #7FE6FF; outline: 0;
}

/* ---------------------------------------------------------------- list */
QListWidget {
    background: #10151F; border: 1px solid #222B3D; border-radius: 14px;
    padding: 6px; outline: 0;
}
QListWidget::item {
    padding: 10px 14px; border-radius: 9px; color: #C7D0E0; margin: 2px 0;
}
QListWidget::item:hover { background: #1A2233; }
QListWidget::item:selected { background: #143A46; color: #7FE6FF; }

/* ---------------------------------------------------------- scrollbars */
QScrollBar:vertical { background: transparent; width: 8px; margin: 2px 2px 2px 0; }
QScrollBar::handle:vertical { background: #2A3550; border-radius: 4px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #3A4A70; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 0 2px 2px 2px; }
QScrollBar::handle:horizontal { background: #2A3550; border-radius: 4px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: #3A4A70; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

/* ------------------------------------------------------ media player */
QLabel#albumArt {
    background: qradialgradient(cx:0.5, cy:0.4, radius:0.7, fx:0.5, fy:0.4,
                                stop:0 #1E2A3E, stop:1 #10151F);
    border: 1px solid #222B3D; border-radius: 18px;
    color: #3A4A70; font-size: 56px;
}
QToolButton#transport {
    background: #1A2233; border: 1px solid #2A3550; border-radius: 20px;
    min-width: 40px; max-width: 40px;
    min-height: 40px; max-height: 40px;
    color: #C7D0E0; font-size: 18px; padding: 0;
}
QToolButton#transport:hover { border-color: #29D3F0; color: #29D3F0; }
QToolButton#transportPrimary {
    background: #143A46; border: 1px solid #29D3F0; color: #7FE6FF;
    border-radius: 20px;
    min-width: 44px; max-width: 44px;
    min-height: 44px; max-height: 44px;
    font-size: 19px; padding: 0;
}
QToolButton#transportPrimary:hover { background: #17505E; color: #EAF0F8; }

/* ----------------------------------------------------------- navigation */
QToolButton#navButton {
    background: transparent; border: none; border-radius: 12px;
    color: #8A94A8; font-size: 13px; font-weight: 600;
    padding: 10px 22px;
}
QToolButton#navButton:hover { background: #161D2B; color: #EAF0F8; }
QToolButton#navButton:checked {
    background: #12313C; color: #29D3F0;
    border-bottom: 3px solid #29D3F0;
}
)qss");
}

} // namespace sc
