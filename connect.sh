#!/bin/bash

DEVICE="98:DA:60:02:82:11"
CHANNEL="1"
RFCOMM_DEV="0"

echo "Connecting to HC-06..."

# Make sure Bluetooth service is running
sudo systemctl start bluetooth

# Remove an existing RFCOMM binding if one exists
if rfcomm -a | grep -q "rfcomm${RFCOMM_DEV}"; then
    echo "Removing old RFCOMM binding..."
    sudo rfcomm release "$RFCOMM_DEV" 2>/dev/null || true
    sleep 1
fi

# Create RFCOMM binding
echo "Binding $DEVICE on channel $CHANNEL..."
sudo rfcomm bind "$RFCOMM_DEV" "$DEVICE" "$CHANNEL"

# Check result
if [ -e "/dev/rfcomm${RFCOMM_DEV}" ]; then
    echo "Connected!"
    echo "Device: /dev/rfcomm${RFCOMM_DEV}"
else
    echo "Failed to create /dev/rfcomm${RFCOMM_DEV}"
    exit 1
fi
