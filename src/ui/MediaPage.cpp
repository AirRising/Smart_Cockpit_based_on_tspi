#include "MediaPage.h"

#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

#include "MediaService.h"

namespace sc {

namespace {

QFrame *makeCard(QWidget *parent, const QString &title)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 16, 20, 20);
    layout->setSpacing(12);
    if (!title.isEmpty()) {
        auto *label = new QLabel(title, card);
        label->setObjectName(QStringLiteral("cardTitle"));
        layout->addWidget(label);
    }
    return card;
}

} // namespace

MediaPage::MediaPage(MediaService *media, QWidget *parent)
    : QWidget(parent)
    , m_media(media)
{
    setObjectName(QStringLiteral("mediaPage"));
    setStyleSheet(QStringLiteral("#mediaPage { background: transparent; }"));

    m_fileList = new QListWidget(this);

    auto *playlistCard = makeCard(this, QStringLiteral("音乐库"));
    auto *playlistLayout = qobject_cast<QVBoxLayout *>(playlistCard->layout());
    playlistLayout->addWidget(m_fileList, 1);

    auto *albumArt = new QLabel(QStringLiteral("\u266A"), this);
    albumArt->setObjectName(QStringLiteral("albumArt"));
    albumArt->setAlignment(Qt::AlignCenter);
    albumArt->setFixedSize(200, 200);

    m_trackLabel = new QLabel(QStringLiteral("无媒体"), this);
    m_trackLabel->setObjectName(QStringLiteral("trackLabel"));
    m_trackLabel->setAlignment(Qt::AlignCenter);
    m_trackLabel->setStyleSheet(QStringLiteral(
        "font-size:20px; font-weight:800; color:#EAF0F8;"));

    auto *metaLabel = new QLabel(QStringLiteral("本地音乐库"), this);
    metaLabel->setObjectName(QStringLiteral("cardTitle"));
    metaLabel->setAlignment(Qt::AlignCenter);

    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_positionSlider->setRange(0, 0);

    m_timeLabel = new QLabel(QStringLiteral("00:00 / 00:00"), this);
    m_timeLabel->setObjectName(QStringLiteral("timeLabel"));
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setStyleSheet(QStringLiteral("color:#8A94A8; font-size:12px;"));

    auto *playButton = new QToolButton(this);
    playButton->setText(QStringLiteral("\u25B6"));
    playButton->setObjectName(QStringLiteral("transportPrimary"));
    playButton->setCursor(Qt::PointingHandCursor);
    connect(playButton, &QToolButton::clicked, this, &MediaPage::openSelected);

    auto *pauseButton = new QToolButton(this);
    pauseButton->setText(QStringLiteral("\u275A\u275A"));
    pauseButton->setObjectName(QStringLiteral("transport"));
    pauseButton->setCursor(Qt::PointingHandCursor);
    connect(pauseButton, &QToolButton::clicked, this, [this]() { m_media->pause(); });

    auto *stopButton = new QToolButton(this);
    stopButton->setText(QStringLiteral("\u25A0"));
    stopButton->setObjectName(QStringLiteral("transport"));
    stopButton->setCursor(Qt::PointingHandCursor);
    connect(stopButton, &QToolButton::clicked, this, [this]() { m_media->stop(); });

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(14);
    buttonRow->addStretch();
    buttonRow->addWidget(playButton);
    buttonRow->addWidget(pauseButton);
    buttonRow->addWidget(stopButton);
    buttonRow->addStretch();

    auto *playerCard = makeCard(this, QStringLiteral("正在播放"));
    auto *playerLayout = qobject_cast<QVBoxLayout *>(playerCard->layout());
    playerLayout->addWidget(albumArt, 0, Qt::AlignHCenter);
    playerLayout->addWidget(m_trackLabel);
    playerLayout->addWidget(metaLabel);
    playerLayout->addSpacing(8);
    playerLayout->addWidget(m_positionSlider);
    playerLayout->addWidget(m_timeLabel);
    playerLayout->addLayout(buttonRow);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->setSpacing(18);
    layout->addWidget(playlistCard, 3);
    layout->addWidget(playerCard, 2);

    connect(m_fileList, &QListWidget::itemDoubleClicked, this, &MediaPage::openSelected);
    connect(m_positionSlider, &QSlider::sliderReleased, this, &MediaPage::onSeekReleased);
    connect(media, &MediaService::positionChanged, this, &MediaPage::onPositionChanged);
    connect(media, &MediaService::durationChanged, this, &MediaPage::onDurationChanged);
    connect(media, &MediaService::playbackStateChanged, this, &MediaPage::onPlaybackStateChanged);
    connect(media, &MediaService::trackChanged, this, &MediaPage::onTrackChanged);

    QString mediaDir = qEnvironmentVariable("SMART_COCKPIT_MEDIA_DIR");
    if (mediaDir.isEmpty())
        mediaDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    scanDirectory(mediaDir);
}

void MediaPage::scanDirectory(const QString &dir)
{
    m_fileList->clear();
    const QDir directory(dir);
    if (!directory.exists())
        return;
    const QStringList filters = {QStringLiteral("*.mp3"), QStringLiteral("*.flac"),
                                 QStringLiteral("*.wav"), QStringLiteral("*.ogg")};
    const QFileInfoList files = directory.entryInfoList(filters, QDir::Files, QDir::Name);
    for (const QFileInfo &info : files) {
        auto *item = new QListWidgetItem(info.fileName());
        item->setData(Qt::UserRole, info.absoluteFilePath());
        m_fileList->addItem(item);
    }
}

void MediaPage::openSelected()
{
    QListWidgetItem *item = m_fileList->currentItem();
    if (!item)
        return;
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty() && m_media->openFile(path))
        m_media->play();
}

void MediaPage::onPositionChanged(qint64 positionMs)
{
    m_sliderSyncing = true;
    m_positionSlider->setValue(positionMs);
    m_sliderSyncing = false;
    m_timeLabel->setText(QStringLiteral("%1 / %2")
                             .arg(formatTime(positionMs), formatTime(m_positionSlider->maximum())));
}

void MediaPage::onDurationChanged(qint64 durationMs)
{
    m_positionSlider->setRange(0, qMax<qint64>(0, durationMs));
}

void MediaPage::onPlaybackStateChanged(bool playing)
{
    Q_UNUSED(playing)
}

void MediaPage::onTrackChanged(const QString &title)
{
    m_trackLabel->setText(title.isEmpty() ? QStringLiteral("无音轨") : title);
}

void MediaPage::onSeekReleased()
{
    if (!m_sliderSyncing)
        m_media->seekTo(m_positionSlider->value());
}

QString MediaPage::formatTime(qint64 ms)
{
    const qint64 seconds = ms / 1000;
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

} // namespace sc
