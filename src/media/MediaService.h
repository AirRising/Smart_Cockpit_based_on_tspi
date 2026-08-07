#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace sc {

// GStreamer playbin wrapper for local MP3 playback plus optional BlueZ A2DP
// sink selection (bluealsa). GStreamer bus messages are drained by a 200 ms
// QTimer instead of a GLib main loop, keeping the integration Qt-native.
class MediaService : public QObject
{
    Q_OBJECT
public:
    explicit MediaService(QObject *parent = nullptr);
    ~MediaService() override;

    // Calls gst_init() if needed and creates the playbin.
    bool init();
    void shutdown();

    bool openFile(const QString &path);
    void play();
    void pause();
    void stop();
    void seekTo(qint64 positionMs);

    bool isPlaying() const;
    qint64 durationMs() const;
    QString currentTrack() const;
    bool healthy() const;

    void setAudioSink(const QString &sinkDescription); // e.g. "alsasink", "bluealsa:..."
    QString audioSink() const;

    // Configure a connected A2DP device as output ("bluealsa:SRV=...,DEV=...") .
    void setBluetoothDevice(const QString &deviceAddress);

signals:
    void trackChanged(const QString &title);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void playbackStateChanged(bool playing);
    void errorOccurred(const QString &message);

private slots:
    void pollBus();

private:
    void *m_playbin = nullptr;      // GstElement*
    void *m_bus = nullptr;          // GstBus*
    QTimer m_pollTimer;
    QString m_audioSink = QStringLiteral("alsasink");
    QString m_currentTrack;
    qint64 m_durationMs = -1;
    bool m_playing = false;
    bool m_healthy = false;
};

} // namespace sc
