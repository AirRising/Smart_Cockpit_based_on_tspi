#include "BlueZScanner.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDebug>

namespace sc {

BlueZScanner::BlueZScanner(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::systemBus().connect(
        QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"), this, SLOT(onInterfacesAdded(QDBusMessage)));
}

void BlueZScanner::scan()
{
    QDBusMessage reply = QDBusConnection::systemBus().call(
        QDBusMessage::createMethodCall(QStringLiteral("org.bluez"), QStringLiteral("/"),
                                       QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                       QStringLiteral("GetManagedObjects")),
        QDBus::Block, 5000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        emit errorOccurred(reply.errorMessage());
        return;
    }

    QDBusArgument outer = reply.arguments().at(0).value<QDBusArgument>();
    outer.beginMap();
    while (!outer.atEnd()) {
        outer.beginMapEntry();
        QDBusObjectPath path;
        outer >> path;

        QDBusArgument interfaces = outer.asVariant().value<QDBusArgument>();
        interfaces.beginMap();
        while (!interfaces.atEnd()) {
            interfaces.beginMapEntry();
            QString interfaceName;
            interfaces >> interfaceName;
            const QVariantMap props = interfaces.asVariant().toMap();
            if (interfaceName == QStringLiteral("org.bluez.Device1"))
                handleDevice(path.path(), props);
            interfaces.endMapEntry();
        }
        interfaces.endMap();
        outer.endMapEntry();
    }
    outer.endMap();
}

void BlueZScanner::onInterfacesAdded(const QDBusMessage &message)
{
    Q_UNUSED(message);
    // Re-scan is simpler and cheap enough; production code may parse the
    // signal payload directly.
    scan();
}

void BlueZScanner::handleDevice(const QString &path, const QVariantMap &properties)
{
    const bool connected = properties.value(QStringLiteral("Connected")).toBool();
    if (!connected)
        return;

    const QString address = properties.value(QStringLiteral("Address")).toString();
    const QString name = properties.value(QStringLiteral("Alias")).toString();
    qInfo() << "Bluetooth device connected:" << name << address << "path" << path;
    emit deviceConnected(address, name);
}

} // namespace sc
