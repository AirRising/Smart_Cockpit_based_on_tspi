#pragma once

#include <QByteArray>
#include <QMutex>
#include <QString>
#include <QThread>

namespace sc {

// SocketCAN receive thread.
//
// The thread owns the blocking read loop; every received frame is delivered
// to the main thread through the queued `frameReceived` signal. Sending is
// thread-safe and can be called from the GUI thread.
class CanManager : public QThread
{
    Q_OBJECT
public:
    explicit CanManager(QObject *parent = nullptr);
    ~CanManager() override;

    // Opens and binds a raw CAN socket (e.g. "vcan0" for host debugging,
    // "can0" on the device). Returns false on failure.
    bool open(const QString &interfaceName);
    void closeBus();

    bool isOpen() const;
    QString interfaceName() const;
    quint64 framesReceived() const;

    // Sends one standard frame (8 bytes max). Returns bytes sent or -1.
    int sendFrame(quint32 id, const QByteArray &data, bool extended = false);

signals:
    void frameReceived(quint32 id, QByteArray data, bool extended);
    void errorOccurred(const QString &message);

protected:
    void run() override;

private:
    QString m_interfaceName;
    int m_socket = -1;
    QMutex m_sendMutex;
    quint64 m_framesReceived = 0;
};

} // namespace sc
