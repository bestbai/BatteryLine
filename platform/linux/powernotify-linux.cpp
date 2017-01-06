#include "var.h"

#include "powernotify-linux.h"
#include "../../systemhelper.h"

#include <QDebug>

PowerNotify::PowerNotify()
{
    if (!QDBusConnection::systemBus().isConnected())
        SystemHelper::SystemError(QString("[%1] Cannot connect to D-Bus' system bus").arg(BL_PLATFORM));

    // Gather line power information
    QDBusInterface dBusUPower("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", QDBusConnection::systemBus());
    QDBusReply<QList<QDBusObjectPath>> dBusReply = dBusUPower.call("EnumerateDevices");
    if (dBusReply.isValid() == false)
    {
        SystemHelper::SystemError(QString("[%1] Cannot get list of power devices\nError = %2, %3")
                                  .arg(BL_PLATFORM)
                                  .arg(dBusReply.error().name())
                                  .arg(dBusReply.error().message()));
    }

    QList<QDBusObjectPath> devList = dBusReply.value();
    for (int i = 0; i < devList.count(); i++)
    {
        QDBusInterface dBusDeviceType("org.freedesktop.UPower", devList[i].path(), "org.freedesktop.UPower.Device", QDBusConnection::systemBus());
        switch (dBusDeviceType.property("Type").toUInt())
        {
        case 1: // Line Power
            m_LinePower.append(devList[i].path());
            break;
        case 2: // Battery
            break;
        default: // Ignore the others
            break;
        }
    }

    bool result;
    QDBusConnection dBusSystem = QDBusConnection::systemBus();
    result = dBusSystem.connect("org.freedesktop.UPower", "/org/freedesktop/UPower/devices/DisplayDevice", "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(BatteryInfoChanged(QString, QVariantMap, QStringList)));
    for (int i = 0; i < m_LinePower.count(); i++)
        result &= dBusSystem.connect("org.freedesktop.UPower", m_LinePower[i], "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(ACLineInfoChanged(QString, QVariantMap, QStringList)));
    if (result == false)
        SystemHelper::SystemError(QString("[%1] Cannot register D-Bus System Bus").arg(BL_PLATFORM));
}

PowerNotify::~PowerNotify()
{
    // Unregister from power notification
    bool result;
    QDBusConnection dBusSystem = QDBusConnection::systemBus();
    result = dBusSystem.disconnect("org.freedesktop.UPower", "/org/freedesktop/UPower/devices/DisplayDevice", "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(BatteryInfoChanged(QString, QVariantMap, QStringList)));
    for (int i = 0; i < m_LinePower.count(); i++)
        result &= dBusSystem.connect("org.freedesktop.UPower", m_LinePower[i], "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(ACLineInfoChanged(QString, QVariantMap, QStringList)));
    if (result == false)
        SystemHelper::SystemError(QString("[%1] Cannot unregister D-Bus System Bus").arg(BL_PLATFORM));
}

// (QString interfaceName, QVariantMap changedProperties, QStringList invalidatedProperties)
void PowerNotify::BatteryInfoChanged(QString, QVariantMap changedProperties, QStringList)
{
    QVariant batteryLevel = changedProperties["Percentage"]; // double
    QVariant batteryState = changedProperties["State"] ;// uint

    if (batteryLevel.isValid() || batteryState.isValid()) // BatteryLevel or State has changed
        emit RedrawSignal();
}

// (QString interfaceName, QVariantMap changedProperties, QStringList invalidatedProperties)
void PowerNotify::ACLineInfoChanged(QString, QVariantMap changedProperties, QStringList)
{
    QVariant acLineOnline = changedProperties["Online"];
    if (acLineOnline.isValid())  // AC Adaptor has been plugged out or in
        emit RedrawSignal();
}

