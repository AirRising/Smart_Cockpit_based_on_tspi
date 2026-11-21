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

    m_video = new VideoWidget(this);

    auto *caption = new QLabel(QStringLiteral("倒车影像"), this);
    caption->setObjectName(QStringLiteral("reverseCaption"));
    caption->setAlignment(Qt::AlignCenter);

    auto *badge = new QLabel(QStringLiteral("R"), this);
    badge->setObjectName(QStringLiteral("reverseBadge"));
    badge->setAlignment(Qt::AlignCenter);

    // Bottom strip below the video: caption centred, R badge on the right.
    // It lives outside the video area, so nothing ever occludes the feed.
    auto *bottomStrip = new QHBoxLayout;
    bottomStrip->setContentsMargins(16, 8, 16, 16);
    bottomStrip->setSpacing(8);
    bottomStrip->addStretch(1);
    bottomStrip->addWidget(caption, 0, Qt::AlignVCenter);
    bottomStrip->addStretch(1);
    bottomStrip->addWidget(badge, 0, Qt::AlignRight | Qt::AlignVCenter);

    auto *overlayLayout = new QVBoxLayout(this);
    overlayLayout->setContentsMargins(0, 0, 0, 0);
    overlayLayout->setSpacing(0);
    overlayLayout->addWidget(m_video, 1);
    overlayLayout->addLayout(bottomStrip);

    connect(camera, &CameraService::frameReady, m_video, &VideoWidget::setFrame);
    connect(car, &CarService::steeringAngleChanged,
            m_video, &VideoWidget::setSteeringAngle);
}

} // namespace sc
