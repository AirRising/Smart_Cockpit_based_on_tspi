#pragma once

#include <QWidget>

class QLabel;
class QListWidget;
class QSlider;

namespace sc {

class MediaService;

// Multimedia page: local MP3 file browser + transport controls + progress.
// Portrait layout: player card on top (art placeholder, title, seek bar,
// transport buttons), playlist below.
class MediaPage : public QWidget
{
    Q_OBJECT
public:
    explicit MediaPage(MediaService *media, QWidget *parent = nullptr);

signals:
    // Forwarded from MediaService::errorOccurred with a user-facing prefix;
    // MainWindow routes it to the status strip.
    void mediaError(const QString &message);

private slots:
    void openSelected();
    void onPositionChanged(qint64 positionMs);
    void onDurationChanged(qint64 durationMs);
    void onPlaybackStateChanged(bool playing);
    void onTrackChanged(const QString &title);
    void onSeekReleased();

private:
    void scanDirectory(const QString &dir);
    static QString formatTime(qint64 ms);

    MediaService *m_media = nullptr;
    QListWidget *m_fileList = nullptr;
    QSlider *m_positionSlider = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_durationLabel = nullptr;
    QLabel *m_trackLabel = nullptr;
    // AlbumArtWidget is defined in the .cpp (anonymous namespace); stored as
    // a base-class pointer here and cast back inside the .cpp only.
    QWidget *m_art = nullptr;
    bool m_sliderSyncing = false;
};

} // namespace sc
