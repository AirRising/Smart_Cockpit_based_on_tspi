#include "MediaPage.h"

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

#include "MediaService.h"
#include "Theme.h"

namespace sc {

namespace {

QFrame *makeCard(QWidget *parent, const QString &title)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);
    if (!title.isEmpty()) {
        auto *label = new QLabel(title, card);
        label->setObjectName(QStringLiteral("cardTitle"));
        layout->addWidget(label);
    }
    return card;
}

// Album art placeholder: 120x120 rounded block with a note glyph. Shows the
// first character of the current track title once one is set. The block and
// the glyph are rendered into a pixmap cache on resize; each paint only
// blits the cache and draws the (single character) title initial.
class AlbumArtWidget : public QWidget
{
public:
    explicit AlbumArtWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(120, 120);
        buildGlyph();
        m_initialFont = font();
        m_initialFont.setPixelSize(44);
        m_initialFont.setWeight(QFont::Bold);
    }

    void setTitle(const QString &title)
    {
        const QString initial = title.isEmpty() ? QString() : title.left(1);
        if (initial == m_initial)
            return;
        m_initial = initial;
        update();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        rebuildCache();
        QWidget::resizeEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        if (m_staticLayer.isNull())
            rebuildCache();
        if (m_staticLayer.isNull())
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawPixmap(0, 0, m_staticLayer);

        if (!m_initial.isEmpty()) {
            painter.setFont(m_initialFont);
            painter.setPen(QColor(QLatin1String(Theme::TextPrimary)));
            painter.drawText(rect(), Qt::AlignCenter, m_initial);
        }
    }

private:
    // Eighth-note glyph in a 24x24 unit box; scaled into the cache on resize.
    void buildGlyph()
    {
        m_glyph.moveTo(9.0, 17.0);
        m_glyph.lineTo(9.0, 5.0);
        m_glyph.lineTo(18.0, 3.0);
        m_glyph.lineTo(18.0, 15.0);
        m_glyph.moveTo(9.0, 8.0);
        m_glyph.lineTo(18.0, 6.0);
        m_glyph.addEllipse(QRectF(4.5, 14.5, 5.0, 5.0));
        m_glyph.addEllipse(QRectF(13.5, 12.5, 5.0, 5.0));
    }

    void rebuildCache()
    {
        m_staticLayer = QPixmap();
        if (width() < 20 || height() < 20)
            return;

        const qreal dpr = devicePixelRatioF();
        m_staticLayer = QPixmap(size() * dpr);
        m_staticLayer.setDevicePixelRatio(dpr);
        m_staticLayer.fill(Qt::transparent);

        QPainter painter(&m_staticLayer);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF block = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(QColor(QLatin1String(Theme::CardBorder)), 1.0));
        painter.setBrush(QColor(QLatin1String(Theme::Bg)));
        painter.drawRoundedRect(block, Theme::RadiusCard, Theme::RadiusCard);

        const qreal side = 48.0;
        const QRectF iconRect((width() - side) / 2.0, (height() - side) / 2.0,
                              side, side);
        painter.translate(iconRect.topLeft());
        painter.scale(iconRect.width() / 24.0, iconRect.height() / 24.0);
        painter.setPen(QPen(QColor(QLatin1String(Theme::Disabled)), 1.6,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(m_glyph);
    }

    QPixmap m_staticLayer;
    QPainterPath m_glyph;
    QFont m_initialFont;
    QString m_initial;
};

// QLabel that keeps the full track title and re-computes the elided display
// text whenever it is assigned or the label itself is resized. Painting is
// entirely native QLabel; QFontMetrics is only used here.
class ElidedLabel : public QLabel
{
public:
    explicit ElidedLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    void setFullText(const QString &text)
    {
        m_fullText = text;
        updateElision();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        updateElision();
        QLabel::resizeEvent(event);
    }

private:
    void updateElision()
    {
        const int available = width() - 8;
        if (m_fullText.isEmpty() || available <= 0) {
            setText(m_fullText);
            return;
        }
        setText(QFontMetrics(font()).elidedText(m_fullText, Qt::ElideRight, available));
    }

    QString m_fullText;
};

// Transport button with a QPainter-drawn glyph (play / pause / stop).
// Only painting is customised; clicking is inherited from QToolButton.
class TransportButton : public QToolButton
{
public:
    enum class Kind { Play, Pause, Stop };

    TransportButton(Kind kind, bool primary, QWidget *parent = nullptr)
        : QToolButton(parent)
        , m_kind(kind)
        , m_primary(primary)
    {
        setFixedSize(primary ? 64 : 56, primary ? 64 : 56);
        buildGlyph();
    }

    QSize sizeHint() const override { return QSize(48, 48); }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QColor fg = m_primary ? QColor(QLatin1String(Theme::Cool))
                                    : QColor(QLatin1String(Theme::TextSecondary));
        const QRectF chip = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(m_primary ? QColor(QLatin1String(Theme::Accent))
                                      : QColor(QLatin1String(Theme::CardBorder)),
                            m_primary ? 1.2 : 1.0));
        painter.setBrush(isDown() ? QColor(QLatin1String(Theme::CardBorder))
                                  : QColor(QLatin1String(Theme::Bg)));
        painter.drawRoundedRect(chip, Theme::RadiusMax, Theme::RadiusMax);

        // Pre-built 24x24 unit-space glyph, scaled into the centre box.
        const qreal side = width() * 0.4;
        const QRectF iconRect((width() - side) / 2.0, (height() - side) / 2.0,
                              side, side);
        painter.save();
        painter.translate(iconRect.topLeft());
        painter.scale(iconRect.width() / 24.0, iconRect.height() / 24.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fg);
        painter.drawPath(m_glyph);
        painter.restore();
    }

private:
    void buildGlyph()
    {
        switch (m_kind) {
        case Kind::Play:
            m_glyph.moveTo(8.0, 5.0);
            m_glyph.lineTo(19.0, 12.0);
            m_glyph.lineTo(8.0, 19.0);
            m_glyph.closeSubpath();
            break;
        case Kind::Pause:
            m_glyph.addRect(QRectF(7.0, 5.0, 3.5, 14.0));
            m_glyph.addRect(QRectF(13.5, 5.0, 3.5, 14.0));
            break;
        case Kind::Stop:
            m_glyph.addRoundedRect(QRectF(6.5, 6.5, 11.0, 11.0), 2.0, 2.0);
            break;
        }
    }

    Kind m_kind;
    bool m_primary;
    QPainterPath m_glyph;
};

} // namespace

MediaPage::MediaPage(MediaService *media, QWidget *parent)
    : QWidget(parent)
    , m_media(media)
{
    setObjectName(QStringLiteral("mediaPage"));

    // ---------------------------------------------------------- player card
    auto *playerCard = makeCard(this, QStringLiteral("正在播放"));
    auto *playerLayout = qobject_cast<QVBoxLayout *>(playerCard->layout());

    m_art = new AlbumArtWidget(this);
    m_art->setObjectName(QStringLiteral("albumArt"));

    m_trackLabel = new ElidedLabel(this);
    m_trackLabel->setObjectName(QStringLiteral("trackLabel"));
    static_cast<ElidedLabel *>(m_trackLabel)->setFullText(QStringLiteral("无媒体"));

    auto *metaLabel = new QLabel(QStringLiteral("本地音乐库"), this);
    metaLabel->setObjectName(QStringLiteral("cardTitle"));

    auto *titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(8);
    titleColumn->addStretch(1);
    titleColumn->addWidget(m_trackLabel);
    titleColumn->addWidget(metaLabel);
    titleColumn->addStretch(1);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(16);
    topRow->addWidget(m_art);
    topRow->addLayout(titleColumn, 1);

    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_positionSlider->setRange(0, 0);

    m_timeLabel = new QLabel(QStringLiteral("00:00"), this);
    m_timeLabel->setObjectName(QStringLiteral("timeLabel"));
    m_timeLabel->setFont(Theme::monoFont(12, QFont::DemiBold)); // one-time
    m_durationLabel = new QLabel(QStringLiteral("00:00"), this);
    m_durationLabel->setObjectName(QStringLiteral("timeLabel"));
    m_durationLabel->setFont(Theme::monoFont(12, QFont::DemiBold)); // one-time

    auto *sliderRow = new QHBoxLayout;
    sliderRow->setSpacing(8);
    sliderRow->addWidget(m_timeLabel);
    sliderRow->addWidget(m_positionSlider, 1);
    sliderRow->addWidget(m_durationLabel);

    auto *playButton = new TransportButton(TransportButton::Kind::Play, true, this);
    playButton->setObjectName(QStringLiteral("transportPrimary"));
    playButton->setCursor(Qt::PointingHandCursor);
    connect(playButton, &QToolButton::clicked, this, &MediaPage::openSelected);

    auto *pauseButton = new TransportButton(TransportButton::Kind::Pause, false, this);
    pauseButton->setObjectName(QStringLiteral("transport"));
    pauseButton->setCursor(Qt::PointingHandCursor);
    connect(pauseButton, &QToolButton::clicked, this, [this]() { m_media->pause(); });

    auto *stopButton = new TransportButton(TransportButton::Kind::Stop, false, this);
    stopButton->setObjectName(QStringLiteral("transport"));
    stopButton->setCursor(Qt::PointingHandCursor);
    connect(stopButton, &QToolButton::clicked, this, [this]() { m_media->stop(); });

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(24);
    buttonRow->addStretch();
    buttonRow->addWidget(playButton);
    buttonRow->addWidget(pauseButton);
    buttonRow->addWidget(stopButton);
    buttonRow->addStretch();

    playerLayout->addLayout(topRow, 1);
    playerLayout->addLayout(sliderRow);
    playerLayout->addSpacing(8);
    playerLayout->addLayout(buttonRow);
    playerLayout->addSpacing(8);

    // -------------------------------------------------------- playlist card
    m_fileList = new QListWidget(this);

    auto *playlistCard = makeCard(this, QStringLiteral("音乐库"));
    auto *playlistLayout = qobject_cast<QVBoxLayout *>(playlistCard->layout());
    playlistLayout->addWidget(m_fileList, 1);

    // ----------------------------------------------------------------- layout
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(16);
    layout->addWidget(playerCard, 37);
    layout->addWidget(playlistCard, 63);

    connect(m_fileList, &QListWidget::itemDoubleClicked, this, &MediaPage::openSelected);
    connect(m_positionSlider, &QSlider::sliderReleased, this, &MediaPage::onSeekReleased);
    connect(media, &MediaService::positionChanged, this, &MediaPage::onPositionChanged);
    connect(media, &MediaService::durationChanged, this, &MediaPage::onDurationChanged);
    connect(media, &MediaService::playbackStateChanged, this, &MediaPage::onPlaybackStateChanged);
    connect(media, &MediaService::trackChanged, this, &MediaPage::onTrackChanged);
    connect(media, &MediaService::errorOccurred, this, [this](const QString &message) {
        emit mediaError(QStringLiteral("播放失败：") + message);
    });

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
    m_timeLabel->setText(formatTime(positionMs));
    m_durationLabel->setText(formatTime(m_positionSlider->maximum()));
}

void MediaPage::onDurationChanged(qint64 durationMs)
{
    m_positionSlider->setRange(0, qMax<qint64>(0, durationMs));
    m_durationLabel->setText(formatTime(durationMs));
}

void MediaPage::onPlaybackStateChanged(bool playing)
{
    Q_UNUSED(playing)
}

void MediaPage::onTrackChanged(const QString &title)
{
    static_cast<ElidedLabel *>(m_trackLabel)->setFullText(
        title.isEmpty() ? QStringLiteral("无音轨") : title);
    static_cast<AlbumArtWidget *>(m_art)->setTitle(title);
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
