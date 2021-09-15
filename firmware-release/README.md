Aurora_Web_Inverter_Monitor_DEBUG_FTP.d1_mini.bin
Firmware with FTP server to upload filesystem and DEBUG active

Aurora_Web_Inverter_Monitor_DEBUG.d1_mini.bin
Firmware with DEBUG active

Aurora_Web_Inverter_Monitor.d1_mini.bin
Firmware standard

Aurora_Web_Inverter_Monitor.spiffs.bin
Filesystem in binary

Follow the guide on my site to upload with a GUI
www.mischianti.org

esptool --port /dev/COM24 write_flash -fm dio 0x00000 Aurora_Web_Inverter_Monitor.d1_mini.bin

esptool --port /dev/COM24 write_flash -fm dio 0x10000 Aurora_Web_Inverter_Monitor.spiffs.bin
