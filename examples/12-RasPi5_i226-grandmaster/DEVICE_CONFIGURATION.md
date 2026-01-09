# Device Configuration Guide

## Overview

The GPS-disciplined PTP grandmaster supports flexible device configuration via command-line arguments. **Nothing is hardcoded** - all device paths can be customized for your hardware setup.

## Quick Start: Auto-Detection

Run the device detection script to identify your hardware:

```bash
chmod +x scripts/detect-devices.sh
sudo ./scripts/detect-devices.sh
```

This will output a suggested command line for your system.

## Command-Line Arguments

All device paths can be specified via command-line arguments:

```bash
sudo ./ptp_grandmaster [OPTIONS]

Options:
  -i, --interface <name>   Network interface (default: eth1)
  -p, --phc <device>       PHC device (default: /dev/ptp0)
  -g, --gps <device>       GPS serial device (default: /dev/ttyACM0)
  -s, --pps <device>       PPS device (default: /dev/pps0)
  -r, --rtc <device>       RTC device (default: /dev/rtc1)
  -v, --verbose            Verbose output
  -h, --help               Show this help message
```

## Common GPS Device Configurations

### USB GPS with ACM Driver (u-blox, most USB GPS)
```bash
sudo ./ptp_grandmaster --gps=/dev/ttyACM0 --verbose
```

### USB-to-Serial GPS Adapter (FTDI, Prolific, CP210x)
```bash
sudo ./ptp_grandmaster --gps=/dev/ttyUSB0 --verbose
```

### GPIO UART GPS (Raspberry Pi GPIO 14/15)
```bash
sudo ./ptp_grandmaster --gps=/dev/ttyAMA0 --verbose
```

### Hardware UART GPS
```bash
sudo ./ptp_grandmaster --gps=/dev/ttyS0 --verbose
```

## Identifying Your Devices

### Find GPS Serial Port
```bash
# List all serial devices
ls -l /dev/tty{ACM,USB,AMA}* /dev/ttyS0

# Monitor for GPS NMEA data
sudo cat /dev/ttyACM0 | grep -E '\$GP|\$GN'
```

### Find PTP Hardware Clock
```bash
# List PTP devices
ls -l /dev/ptp*

# Show which network interface owns the PHC
ls -l /sys/class/ptp/ptp*/device/net/
```

### Find PPS Device
```bash
# List PPS devices
ls -l /dev/pps*

# Check PPS status
cat /sys/class/pps/pps0/assert
```

### Find RTC Device
```bash
# List RTC devices
ls -l /dev/rtc*

# Show RTC names
for rtc in /dev/rtc*; do
    echo "$rtc: $(cat /sys/class/rtc/$(basename $rtc)/name)"
done

# Test RTC access
sudo hwclock -r --rtc=/dev/rtc1
```

## Configuration File (Optional)

For permanent configuration, edit `/etc/ptp-grandmaster.conf`:

```bash
sudo cp etc/ptp-grandmaster.conf /etc/
sudo nano /etc/ptp-grandmaster.conf
```

Example configuration:
```ini
interface=eth1
phc=/dev/ptp0
gps=/dev/ttyUSB0
pps=/dev/pps0
rtc=/dev/rtc0
verbose=true
```

## Systemd Service Installation

For automatic startup at boot:

```bash
# Copy service file
sudo cp etc/systemd/ptp-grandmaster.service /etc/systemd/system/

# Edit configuration if needed
sudo nano /etc/ptp-grandmaster.conf

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable ptp-grandmaster
sudo systemctl start ptp-grandmaster

# Check status
sudo systemctl status ptp-grandmaster
sudo journalctl -u ptp-grandmaster -f
```

## Hardware-Specific Examples

### Raspberry Pi 5 + i226 NIC + u-blox GPS + DS3231 RTC
```bash
sudo ./ptp_grandmaster \
    --interface=eth1 \
    --phc=/dev/ptp0 \
    --gps=/dev/ttyACM0 \
    --pps=/dev/pps0 \
    --rtc=/dev/rtc1 \
    --verbose
```

### Standard PC + Intel i210 + USB GPS + Primary RTC
```bash
sudo ./ptp_grandmaster \
    --interface=enp2s0 \
    --phc=/dev/ptp0 \
    --gps=/dev/ttyUSB0 \
    --pps=/dev/pps0 \
    --rtc=/dev/rtc0 \
    --verbose
```

### Embedded ARM + GPIO UART GPS + Secondary RTC
```bash
sudo ./ptp_grandmaster \
    --interface=eth0 \
    --phc=/dev/ptp0 \
    --gps=/dev/ttyAMA0 \
    --pps=/dev/pps0 \
    --rtc=/dev/rtc1 \
    --verbose
```

## Troubleshooting

### GPS Not Detected
```bash
# Check device exists and permissions
ls -l /dev/ttyACM0
# Should show: crw-rw---- 1 root dialout

# Add user to dialout group
sudo usermod -a -G dialout $USER

# Or run as root
sudo ./ptp_grandmaster --gps=/dev/ttyACM0 --verbose
```

### PPS Not Working
```bash
# Verify PPS device exists
ls -l /dev/pps0

# Check kernel module loaded
lsmod | grep pps

# Verify Device Tree overlay
grep pps /boot/firmware/config.txt
```

### RTC Not Accessible
```bash
# Check RTC permissions
ls -l /dev/rtc1

# Test RTC manually
sudo hwclock -r --rtc=/dev/rtc1

# Check I2C bus (for DS3231)
sudo i2cdetect -y 1
```

### PHC Not Found
```bash
# Verify network interface has hardware timestamping
ethtool -T eth1

# Check PTP capability
ethtool -T eth1 | grep "PTP Hardware Clock"
```

## Device Tree Configuration

### GPS PPS on GPIO (Raspberry Pi)

Edit `/boot/firmware/config.txt`:

```ini
# GPS 1PPS on GPIO18 (Pin 12)
dtoverlay=pps-gpio,gpiopin=18,pull=down
```

Other common GPIO pins:
- GPIO4 (Pin 7): `gpiopin=4`
- GPIO17 (Pin 11): `gpiopin=17`
- GPIO27 (Pin 13): `gpiopin=27`

### RTC on I2C (DS3231)

```ini
# DS3231 on hardware I2C (I2C1)
dtparam=i2c_arm=on
dtoverlay=i2c-rtc,ds3231

# DS3231 on software I2C (GPIO23/24)
dtoverlay=i2c-rtc-gpio,ds3231,addr=0x68,i2c_gpio_sda=23,i2c_gpio_scl=24
```

## Support for Different GPS Modules

The code supports any GPS module that outputs:
- **NMEA-0183 protocol** (standard text sentences)
- **$GPRMC** message (time, date, position, status)
- **$GPGGA** message (fix quality, satellite count)
- **1PPS signal** (pulse-per-second timing reference)

Tested GPS modules:
- ✅ u-blox G70xx series (USB, UART)
- ✅ u-blox NEO-M8N (UART)
- ✅ GlobalSat BU-353 S4 (USB)
- ✅ Adafruit Ultimate GPS (UART)
- ✅ Beitian BN-880 (UART)

Most GPS modules are compatible with configuration adjustments.
