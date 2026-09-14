#include "ReverseCameraPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "CameraService.h"
#include "CarService.h"
#include "VideoWidget.h"

namespace sc {

ReverseCameraPage::ReverseCameraPage(CameraService *camera, CarService *car,
                                     QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("reversePage"));
    setStyleSheet(QStringLiteral("#reversePage { background: black; }"));

    m_video = new VideoWidget(this);

    auto *badge = new QLabel(QStringLiteral("R"), this);
    badge->setObjectName(QStringLiteral("reverseBadge"));
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QStringLiteral(
        "background:#FF3B30; color:white; font-size:32px; font-weight:800;"
        "border-radius:26px; border:2px solid rgba(255,255,255,0.25);"));
    badge->setFixedSize(52, 52);

    auto *caption = new QLabel(QStringLiteral("倒车影像"), this);
    caption->setObjectName(QStringLiteral("reverseCaption"));
    caption->setStyleSheet(QStringLiteral(
        "color:rgba(255,255,255,0.85); font-size:14px; font-weight:700;"
        "letter-spacing:3px; background:rgba(0,0,0,0.45);"
        "border-radius:10px; padding:6px 16px;"));

    auto *overlayLayout = new QVBoxLayout(this);
    overlayLayout->setContentsMargins(0, 0, 12, 12);
    overlayLayout->setSpacing(0);
    overlayLayout->addWidget(m_video, 1);
    overlayLayout->addWidget(badge, 0, Qt::AlignRight | Qt::AlignBottom);
    overlayLayout->addWidget(caption, 0, Qt::AlignHCenter);

    connect(camera, &CameraService::frameReady, m_video, &VideoWidget::setFrame);
    connect(car, &CarService::steeringAngleChanged,
            m_video, &VideoWidget::setSteeringAngle);
}

} // namespace sc
