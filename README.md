<div>
<a href="https://www.mischianti.org/forums/forum/mischiantis-projects/abb-ex-power-one-aurora-web-inverter-monitor-wim/"><img
  src="https://github.com/xreef/LoRa_E32_Series_Library/raw/master/resources/buttonSupportForumEnglish.png" alt="Support forum ABB (ex Power One) Aurora Web Inverter Monitor (WIM)"
   align="right"></a>
</div>
<div>
<a href="https://www.mischianti.org/it/forums/forum/i-progetti-di-mischianti/abb-ex-power-one-aurora-web-inverter-monitor-wim/"><img
  src="https://github.com/xreef/LoRa_E32_Series_Library/raw/master/resources/buttonSupportForumItaliano.png" alt="Forum supporto ABB (ex Power One) Aurora Web Inverter Monitor (WIM)"
  align="right"></a>
</div>



#
#

ABB (ex Power One) Aurora Web Inverter Monitor (WIM): project introduction
==============================================================================

by [Renzo Mischianti]

[![Watch the video](https://img.youtube.com/vi/uInRM3YqIv0/hqdefault.jpg)](https://www.youtube.com/watch?v=uInRM3YqIv0)

Hi all, I put solar panels over my roof some years agò, the company that
installed them had also guaranteed me a production monitoring and
analysis system, but they forgot to tell me that it would be free only
for the first year, and I would have had to pay to access my data that
are stored on a site, the cost is not so enough (70€ for year) **but I
felt cheated**.

![ABB Aurora Web Inverter Monitor Station Introduction](https://www.mischianti.org/wp-content/uploads/2020/06/ABB-Aurora-Web-Inverter-Centraline-Logging-Introduction-1024x586.jpg)

ABB Aurora Web Inverter Monitor Station Introduction

So my solution is to create an autonomous centraline with an esp8266
that grab and store data from inverter and show me **chart and various
data of production** and send me an **email if there are some
problems**.

It is a quite user-friendly browser based monitoring solution, It’s
allows to track energy produced on a solar power plant in a simple and
intuitive fashion. It’s can track key energy metrics as well as the
energy produced throughout the lifetime of their solar power plant. 

Here the video when the project WOR work in progress

I created a simple PCB milled and tested for some month of activity
without problem.

![](https://www.mischianti.org/wp-content/uploads/2019/03/ABBAuroraOK-1024x850.jpg)

ABB Aurora PCB multiple step

ABB Aurora Web Monitor

Library dependencies 
-----------------------------------------------------


ArduinoJson
ArduinoThread
aurora_communication_protocol
DNSServer
EMailSender
ESP8266mDNS
ESP8266SdFat
ESP8266WebServer
ESP8266WiFi
Hash
NTPClient
SD
SDFS
SPI
TimeLib
Timezone
WebSockets
WiFiManager
Wire

Inverter Aurora ABB (ex PowerOne now Fimer) supported 
-----------------------------------------------------

Here a partial list of Aurora PV series supported

-   PVI-2000
-   PVI-2000-OUTD
-   PVI-3600
-   PVI-3.6-OUTD
-   PVI-5000-OUTD
-   PVI-6000-OUTD
-   3-phase interface (3G74)
-   PVI-CENTRAL-50 module
-   PVI-4.2-OUTD
-   PVI-3.6-OUTD
-   PVI-3.3-OUTD
-   **PVI-3.0-OUTD**
-   PVI-12.5-OUTD
-   PVI-10.0-OUTD
-   PVI-4.6-I-OUTD
-   PVI-3.8-I-OUTD
-   PVI-12.0-I-OUTD (output 480 VAC)
-   PVI-10.0-I-OUTD (output 480 VAC)
-   PVI-12.0-I-OUTD (output 208 VAC)
-   PVI-10.0-I-OUTD (output 208 VAC)
-   PVI-12.0-I-OUTD (output 380 VAC)
-   PVI-10.0-I-OUTD (output 380 VAC)
-   PVI-12.0-I-OUTD (output 600 VAC)
-   PVI-10.0-I-OUTD (output 600 VAC)”
-   PVI-CENTRAL-250
-   PVI-10.0-I-OUTD (output 480 VAC current limit 12 A)
-   TRIO-27.6-TL-OUTD
-   TRIO-20-TL
-   UNO-2.0-I
-   UNO-2.5-I
-   PVI-CENTRAL-350 Liquid Cooled (control board)
-   PVI-CENTRAL-350 Liquid Cooled (display board)
-   PVI-CENTRAL-350 Liquid Cooled (AC gathering)

My inverter is in bold.

Introduction
------------

My idea is to use an esp8266 (Wemos D1) with enough power to manage an
http server, a rest server and ftp server, naturally with an IC can
interface my inverter (ABB Autora – ex PowerOne), all data taken from
the inverter will be stored in an SD.

![](https://www.mischianti.org/wp-content/uploads/2019/05/elementInCommunication-e1557000905880.jpg)

ABB Aurora inverter centraline components

Phisical layers as you can see in the image are very simple, I add some
additional logic layer.

First I create a library to manage a full set of informations of the
inverter from the interface RS-485 available, than I create a series of
thread (simulated) with specified delay to get data and store they in an
SD in JSON format, than I create a full set of REST api to retrieve this
set of information, a WebSocket server for realtime data, and a
responsive web app to show all this data finally a configurable
notification system via mail.

![](https://www.mischianti.org/wp-content/uploads/2019/05/swLayer.jpg)

ABB Aurora inverter centraline software layer

Monitor specs and device
------------------------

My selected microcontroller is an WeMos D1 mini, I choice this esp8266
device because It’s very low cost and have sufficient specs to do all
features I have in my mind. Here a mini guide on how to configure your
IDE “[WeMos D1 mini (esp8266), pinout, specs and IDE
configuration](https://www.mischianti.org/2019/08/20/wemos-d1-mini-esp8266-specs-and-ide-configuration-part-1/)“.

### Pinouts

![](https://www.mischianti.org/wp-content/uploads/2019/05/D1wemosPinout.jpg)

WeMos D1 mini pinout

I think that an interesting thing is that It has more Hardware Serial,
so you can use Serial for communication with Inverter and Serial1 D4
(only Transmission) to debug. You can check how to connect debug
USBtoTTL device on “[WeMos D1 mini (esp8266), debug on secondary
UART](https://www.mischianti.org/2019/09/19/wemos-d1-mini-esp8266-debug-on-secondary-uart-part-3/)“.

Sketch OTA update File system EEPROM WiFi config

We are going to put **WebServer data in SPIFFS**, the size needed is
less than 2Mb. SPIFFS is explained in this article “[WeMos D1 mini
(esp8266), integrated SPIFFS
Filesystem](https://www.mischianti.org/2019/08/30/wemos-d1-mini-esp8266-integrated-spiffs-filesistem-part-2/)“.

To update WebServer pages I use an integrated FTP server “[FTP server on
esp8266 and
esp32](https://www.mischianti.org/2020/02/08/ftp-server-on-esp8266-and-esp32/)“.

To **store logging data we must add an SD** card, It’s not sure use
SPIFFS (exist a 16Mb version of esp8266) because have a write cycle
limitation. You can connect directly via an SD adapter, but I prefer a
module to better fit in my case. You can find information on how to
connect SD card in this article “[How to use SD card with esp8266, esp32
and
Arduino](https://www.mischianti.org/2019/12/15/how-to-use-sd-card-with-esp8266-and-arduino/)“.

Aurora ABB (ex PowerOne) communicate via RS-485 connection, so the most
important features is the communication protocol, and for first I create
a complete library to interface on this interface via Arduino, esp8266
or esp32 device.

I use a 18650 rechargeable battery as UPS to grant server active when
It’s nigth and there aren’t energy production, I use the schema from
this article “[Emergency power bank
homemade](https://www.mischianti.org/2019/01/24/emergency-power-bank-homemade/)“.

To logging data It’s also important get current date and time, so I
choice to try to get data from NPT server, if It isn’t possible I get
data from internal clock of inverter.

To connect device I use and fix WIFIManager thar start esp8266 as Access
Point and give an interface to set connection parameter.

Thanks
------
<ol><li><a rel="noreferrer noopener" href="https://www.mischianti.org/2019/05/15/abb-ex-power-one-aurora-web-inverter-monitor-wim-project-introduction-1/" target="_blank">ABB Aurora Web Inverter Monitor (WIM): project introduction</a></li><li><a rel="noreferrer noopener" href="https://www.mischianti.org/2020/09/01/abb-aurora-web-inverter-monitor-wim-wiring-arduino-to-rs-485-2/" target="_blank">ABB Aurora Web Inverter Monitor (WIM): wiring Arduino to RS-485</a></li><li><a rel="noreferrer noopener" href="https://www.mischianti.org/2020/09/11/abb-aurora-web-inverter-monitor-wim-storage-devices-3/" target="_blank">ABB Aurora Web Inverter Monitor (WIM): storage devices</a></li><li><a rel="noreferrer noopener" href="https://www.mischianti.org/it/2020/09/30/abb-aurora-web-inverter-monitor-wim-notifiche-e-debug-4/" target="_blank">ABB Aurora Web Inverter Monitor (WIM): debug and notification</a></li><li><a rel="noreferrer noopener" href="https://www.mischianti.org/2020/10/05/abb-aurora-web-inverter-centraline-wic-set-time-and-manage-battery-ups-part-5/" target="_blank">ABB Aurora Web Inverter Monitor (WIM): set time and UPS</a></li><li><a rel="noreferrer noopener" href="https://www.mischianti.org/2021/01/04/abb-power-one-aurora-web-inverter-monitor-wim-wifi-configuration-and-rest-server-6/" target="_blank">ABB Aurora Web Inverter Monitor (WIM): WIFI configuration and REST Server</a></li><li><a rel="noreferrer noopener" href="https://www.mischianti.org/2021/01/12/abb-aurora-web-inverter-monitor-wim-websocket-and-web-server-7/" target="_blank">ABB Aurora Web Inverter Monitor (WIM): WebSocket and Web Server</a></li><li><a rel="noreferrer noopener" href="https://www.mischianti.org/2021/01/18/abb-aurora-web-inverter-monitor-wim-wiring-and-pcb-soldering-8/" target="_blank">ABB Aurora Web Inverter Monitor (WIM): Wiring and PCB soldering</a></li><li><a rel="noreferrer noopener" href="https://www.mischianti.org/2021/01/24/abb-aurora-web-inverter-monitor-wim-upload-the-sketch-and-front-end-9/" target="_blank">ABB Aurora Web Inverter Monitor (WIM): upload the sketch and front end</a></li><li><a rel="noreferrer noopener" href="https://www.mischianti.org/2021/02/03/abb-aurora-web-inverter-monitor-wim-3d-printed-case-to-complete-project-10/" target="_blank">ABB Aurora web inverter Monitor (WIM): 3D printed case to complete project</a></li><li><a href="https://www.mischianti.org/2021/02/09/abb-power-one-aurora-web-inverter-monitor-wim-repair-e013-error-11/" target="_blank" rel="noreferrer noopener">ABB Aurora web inverter monitor (WIM): repair E013 error<br></a></li></ol>

<p><a href="https://github.com/xreef/Aurora_Web_Invert_Monitor" target="_blank" rel="noreferrer noopener">GitHub repository with all code BE and FE transpiled</a></p>