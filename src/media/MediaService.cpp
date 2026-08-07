#include "MediaService.h"

#include <QDebug>
#include <QFileInfo>
#include <QUrl>

#include <gst/gst.h>

namespace sc {

MediaService::MediaService(QObject *parent)
    : QObject(parent)
{
    m_pollTimer.setInterval(200);
    connect(&m_pollTimer, &QTimer::timeout, this, &MediaService::pollBus);
}

MediaService::~MediaService()
{
    shutdown();
}

bool MediaService::init()
{
    if (m_playbin)
        return true;

    gst_init(nullptr, nullptr);
    m_playbin = gst_element_factory_make("playbin", "playbin");
    if (!m_playbin) {
        emit errorOccurred(QStringLiteral("gst_element_factory_make(playbin) failed"));
        return false;
    }
    m_bus = gst_element_get_bus(GST_ELEMENT(m_playbin));
    m_healthy = true;
    m_pollTimer.start();
    return true;
}

void MediaService::shutdown()
{
    m_pollTimer.stop();
    if (m_playbin) {
        gst_element_set_state(GST_ELEMENT(m_playbin), GST_STATE_NULL);
        gst_object_unref(GST_OBJECT(m_playbin));
        m_playbin = nullptr;
    }
    if (m_bus) {
        gst_object_unref(GST_OBJECT(m_bus));
        m_bus = nullptr;
    }
    m_healthy = false;
}

bool MediaService::openFile(const QString &path)
{
    if (!m_playbin || !QFileInfo::exists(path))
        return false;

    const QUrl uri = QUrl::fromLocalFile(path);
    g_object_set(m_playbin, "uri", uri.toString().toUtf8().constData(), nullptr);
    g_object_set(m_playbin, "audio-sink",
                 gst_parse_bin_from_description(m_audioSink.toUtf8().constData(), true, nullptr),
                 nullptr);
    m_currentTrack = QFileInfo(path).completeBaseName();
    emit trackChanged(m_currentTrack);
    emit durationChanged(-1); // unknown until the pipeline reports it
    return gst_element_set_state(GST_ELEMENT(m_playbin), GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE;
}

void MediaService::play()
{
    if (!m_playbin)
        return;
    gst_element_set_state(GST_ELEMENT(m_playbin), GST_STATE_PLAYING);
}

void MediaService::pause()
{
    if (!m_playbin)
        return;
    gst_element_set_state(GST_ELEMENT(m_playbin), GST_STATE_PAUSED);
}

void MediaService::stop()
{
    if (!m_playbin)
        return;
    gst_element_set_state(GST_ELEMENT(m_playbin), GST_STATE_READY);
}

void MediaService::seekTo(qint64 positionMs)
{
    if (!m_playbin)
        return;
    gst_element_seek_simple(GST_ELEMENT(m_playbin), GST_FORMAT_TIME,
                            GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                            positionMs * GST_MSECOND);
}

bool MediaService::isPlaying() const { return m_playing; }
qint64 MediaService::durationMs() const { return m_durationMs; }
QString MediaService::currentTrack() const { return m_currentTrack; }
bool MediaService::healthy() const { return m_healthy; }

void MediaService::setAudioSink(const QString &sinkDescription)
{
    m_audioSink = sinkDescription;
}

QString MediaService::audioSink() const
{
    return m_audioSink;
}

void MediaService::setBluetoothDevice(const QString &deviceAddress)
{
    // bluealsa exposes an ALSA device per A2DP sink; we wrap it in alsasink:
    //   alsasink device="bluealsa:SRV=org.bluealsa,DEV=AA:BB:CC:DD:EE:FF,PROFILE=a2dp"
    setAudioSink(QStringLiteral(
                     "alsasink device=\"bluealsa:SRV=org.bluealsa,DEV=%1,PROFILE=a2dp\"")
                     .arg(deviceAddress));
    qInfo() << "A2DP audio sink set for" << deviceAddress;
}

void MediaService::pollBus()
{
    if (!m_bus)
        return;

    GstMessage *message = gst_bus_pop_filtered(
        GST_BUS(m_bus),
        GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR | GST_MESSAGE_STATE_CHANGED |
                       GST_MESSAGE_TAG | GST_MESSAGE_DURATION));
    if (!message)
        return;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS: {
        m_playing = false;
        emit playbackStateChanged(false);
        break;
    }
    case GST_MESSAGE_ERROR: {
        GError *error = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_error(message, &error, &debug);
        m_healthy = false;
        emit errorOccurred(QString::fromUtf8(error ? error->message : "GStreamer error"));
        if (error) g_error_free(error);
        g_free(debug);
        m_playing = false;
        emit playbackStateChanged(false);
        break;
    }
    case GST_MESSAGE_TAG: {
        GstTagList *tags = nullptr;
        gst_message_parse_tag(message, &tags);
        gchar *title = nullptr;
        if (tags && gst_tag_list_get_string(tags, GST_TAG_TITLE, &title)) {
            m_currentTrack = QString::fromUtf8(title);
            g_free(title);
            emit trackChanged(m_currentTrack);
        }
        if (tags)
            gst_tag_list_unref(tags);
        break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(m_playbin)) {
            GstState oldState = GST_STATE_VOID_PENDING;
            GstState newState = GST_STATE_VOID_PENDING;
            GstState pending = GST_STATE_VOID_PENDING;
            gst_message_parse_state_changed(message, &oldState, &newState, &pending);
            const bool playing = newState == GST_STATE_PLAYING;
            if (playing != m_playing) {
                m_playing = playing;
                emit playbackStateChanged(m_playing);
            }
        }
        break;
    }
    case GST_MESSAGE_DURATION:
    default:
        break;
    }
    gst_message_unref(message);

    // Keep position/duration fresh every poll tick.
    gint64 positionNs = 0;
    gint64 durationNs = 0;
    if (gst_element_query_position(GST_ELEMENT(m_playbin), GST_FORMAT_TIME, &positionNs))
        emit positionChanged(positionNs / GST_MSECOND);
    if (gst_element_query_duration(GST_ELEMENT(m_playbin), GST_FORMAT_TIME, &durationNs)) {
        const qint64 ms = durationNs / GST_MSECOND;
        if (ms != m_durationMs) {
            m_durationMs = ms;
            emit durationChanged(m_durationMs);
        }
    }
}

} // namespace sc
