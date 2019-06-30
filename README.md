# RaspberryPiOLEDMonitor
A simple RaspberryPi monitor using an I2C OLED Screen (SSD1306) and wiringPi lib.

Your OLED screen driver chip should be SSD1306 with I2C interface and its resolution should be 128x64. Connect your screen to Raspberry Pi GPIO I2C-1 (Pin3=SDA1 Pin5=SCL1), then run raspi-config to enable i2c interface.

How to compile: clone codes to RaspberryPi(running Raspbian). Then
```bash
cd RaspberryPiOLEDMonitor
chmod +x build.sh
./build.sh
```
Type ./OledMonitor to run this monitor. Several system information will be displayed on your OLED screen.

There will be 8 lines on the screen. Description are:

Line 1: Date and time

Line 2: Chip temperature

Line 3: Load average

Line 4 & Line 5: Network traffic on eth0

Line 6: Free space of root filesystem

Line 7: USB disk free space percentage (sda1 & sdb1)

If you don't have USB disk connect to RaspberryPi, you can modify the function or just remove GetSDA1SDB1()

This program currently only tested successfully on Raspberry 3B.
