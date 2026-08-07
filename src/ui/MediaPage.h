#pragma once

#include <QWidget>

class QLabel;
class QListWidget;
class QSlider;

namespace sc {

class MediaService;

// Multimedia page: local MP3 file browser + transport controls + progress.
class MediaPage : public QWidget
{
    Q_OBJECT
public:
    explicit MediaPage(MediaService *media, QWidget *parent = nullptr);

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
    QLabel *m_trackLabel = nullptr;
    bool m_sliderSyncing = false;
};

} // namespace sc
