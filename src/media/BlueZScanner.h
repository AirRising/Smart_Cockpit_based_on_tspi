#pragma once

#include <QDBusArgument>
#include <QDBusMessage>
#include <QObject>
#include <QVariantMap>

namespace sc {

// Minimal BlueZ (org.bluez) device scanner.
//
// Uses D-Bus ObjectManager to list known devices and watches
// InterfacesAdded so connected A2DP sinks can be picked up live.
class BlueZScanner : public QObject
{
    Q_OBJECT
public:
    explicit BlueZScanner(QObject *parent = nullptr);

    void scan();

signals:
    void deviceConnected(const QString &address, const QString &name);
    void errorOccurred(const QString &message);

private slots:
    void onInterfacesAdded(const QDBusMessage &message);

private:
    void handleDevice(const QString &path, const QVariantMap &properties);
};

} // namespace sc
