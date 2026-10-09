/**
 * this is the file which will be used to receive the data from the arduino and process it
 * plan     
    * find the bluetooth device and connect to it
    * receive the data from the bluetooth device
 */

#include <dbus/dbus.h>
#include <iostream>

int main() {
    DBusError error;

    dbus_error_init(&error);

    DBusConnection* connection =
        dbus_bus_get(DBUS_BUS_SYSTEM, &error);

    if (!connection) {
        std::cerr << "Failed to connect to D-Bus: "
                  << error.message << '\n';

        dbus_error_free(&error);
        return 1;
    }

    std::cout << "Connected to D-Bus!\n";

    dbus_connection_unref(connection);
}