#include "CanManager.h"

#include <QDebug>

#include <cerrno>
#include <cstddef>
#include <cstring>

#if defined(Q_OS_LINUX)
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace sc {

CanManager::CanManager(QObject *parent)
    : QThread(parent)
{
}

CanManager::~CanManager()
{
    closeBus();
}

bool CanManager::open(const QString &interfaceName)
{
    m_interfaceName = interfaceName;
#if defined(Q_OS_LINUX)
    m_socket = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socket < 0) {
        emit errorOccurred(QStringLiteral("socket() failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
        return false;
    }

    const QByteArray iface = m_interfaceName.toLocal8Bit();
    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, iface.constData(), IFNAMSIZ - 1);
    if (::ioctl(m_socket, SIOCGIFINDEX, &ifr) < 0) {
        emit errorOccurred(QStringLiteral("interface %1 not found: %2")
                               .arg(m_interfaceName, QString::fromLocal8Bit(strerror(errno))));
        ::close(m_socket);
        m_socket = -1;
        return false;
    }

    struct sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(m_socket, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        emit errorOccurred(QStringLiteral("bind() failed: %1").arg(QString::fromLocal8Bit(strerror(errno))));
        ::close(m_socket);
        m_socket = -1;
        return false;
    }

    qInfo() << "CAN socket open on" << m_interfaceName;
    start();
    return true;
#else
    emit errorOccurred(QStringLiteral("SocketCAN is only available on Linux"));
    return false;
#endif
}

void CanManager::closeBus()
{
    if (isRunning()) {
        requestInterruption();
        wait(1000);
    }
#if defined(Q_OS_LINUX)
    if (m_socket >= 0) {
        ::close(m_socket);
        m_socket = -1;
    }
#endif
}

bool CanManager::isOpen() const
{
    return m_socket >= 0;
}

QString CanManager::interfaceName() const
{
    return m_interfaceName;
}

quint64 CanManager::framesReceived() const
{
    return m_framesReceived;
}

int CanManager::sendFrame(quint32 id, const QByteArray &data, bool extended)
{
#if defined(Q_OS_LINUX)
    if (m_socket < 0)
        return -1;
    struct can_frame frame {};
    frame.can_id = id & CAN_SFF_MASK;
    if (extended)
        frame.can_id |= CAN_EFF_FLAG;
    const int len = qMin(data.size(), 8);
    frame.can_dlc = static_cast<quint8>(len);
    std::memcpy(frame.data, data.constData(), static_cast<size_t>(len));

    QMutexLocker locker(&m_sendMutex);
    const ssize_t n = ::write(m_socket, &frame, sizeof(frame));
    return n == sizeof(frame) ? len : -1;
#else
    Q_UNUSED(id) Q_UNUSED(data) Q_UNUSED(extended)
    return -1;
#endif
}

void CanManager::run()
{
#if defined(Q_OS_LINUX)
    while (!isInterruptionRequested()) {
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(m_socket, &readFds);
        struct timeval timeout {};
        timeout.tv_sec = 0;
        timeout.tv_usec = 100 * 1000; // 100 ms, lets us observe interruption requests

        const int ready = ::select(m_socket + 1, &readFds, nullptr, nullptr, &timeout);
        if (ready < 0) {
            if (isInterruptionRequested())
                break;
            emit errorOccurred(QStringLiteral("select() failed: %1")
                                   .arg(QString::fromLocal8Bit(strerror(errno))));
            continue;
        }
        if (ready == 0 || !FD_ISSET(m_socket, &readFds))
            continue;

        struct can_frame frame {};
        const ssize_t n = ::read(m_socket, &frame, sizeof(frame));
        if (n < 0) {
            if (!isInterruptionRequested())
                emit errorOccurred(QStringLiteral("read() failed: %1")
                                       .arg(QString::fromLocal8Bit(strerror(errno))));
            continue;
        }
        if (n < static_cast<ssize_t>(offsetof(struct can_frame, data) + frame.can_dlc))
            continue;

        ++m_framesReceived;
        const bool extended = (frame.can_id & CAN_EFF_FLAG) != 0;
        const quint32 id = frame.can_id & CAN_EFF_MASK;
        emit frameReceived(id,
                           QByteArray(reinterpret_cast<const char *>(frame.data), frame.can_dlc),
                           extended);
    }
#endif
}

} // namespace sc
