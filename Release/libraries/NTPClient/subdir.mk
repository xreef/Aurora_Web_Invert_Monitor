################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
C:\eclipse\sloeber\arduinoPlugin\libraries\NTPClient\3.1.0\NTPClient.cpp 

LINK_OBJ += \
.\libraries\NTPClient\NTPClient.cpp.o 

CPP_DEPS += \
.\libraries\NTPClient\NTPClient.cpp.d 


# Each subdirectory must supply rules for building sources it contributes
libraries\NTPClient\NTPClient.cpp.o: C:\eclipse\sloeber\arduinoPlugin\libraries\NTPClient\3.1.0\NTPClient.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	"C:\eclipse\sloeber\arduinoPlugin\packages\esp8266\tools\xtensa-lx106-elf-gcc\1.20.0-26-gb404fb9-2/bin/xtensa-lx106-elf-g++" -D__ets__ -DICACHE_FLASH -U__STRICT_ANSI__ "-IC:\eclipse\sloeber\/arduinoPlugin/packages/esp8266/hardware/esp8266/2.4.2/tools/sdk/include" "-IC:\eclipse\sloeber\/arduinoPlugin/packages/esp8266/hardware/esp8266/2.4.2/tools/sdk/lwip2/include" "-IC:\eclipse\sloeber\/arduinoPlugin/packages/esp8266/hardware/esp8266/2.4.2/tools/sdk/libc/xtensa-lx106-elf/include" "-ID:/Projects/Arduino/sloeber-workspace-aurora/AuroraServerWebRest/Release/core" -c -Wall -Wextra  -Os -g -mlongcalls -mtext-section-literals -fno-exceptions -fno-rtti -falign-functions=4 -std=c++11 -MMD -ffunction-sections -fdata-sections -DF_CPU=80000000L -DLWIP_OPEN_SRC -DTCP_MSS=536   -DARDUINO=10802 -DARDUINO_ESP8266_WEMOS_D1MINI -DARDUINO_ARCH_ESP8266 -DARDUINO_BOARD="\"ESP8266_WEMOS_D1MINI\""   -DESP8266   -I"C:\eclipse\sloeber\arduinoPlugin\packages\esp8266\hardware\esp8266\2.4.2\cores\esp8266" -I"C:\eclipse\sloeber\arduinoPlugin\packages\esp8266\hardware\esp8266\2.4.2\variants\d1_mini" -I"C:\eclipse\sloeber\arduinoPlugin\libraries\ArduinoJson\6.2.3-beta\src" -I"D:\Projects\Arduino\sloeber-workspace-aurora\ArduinoThread" -I"D:\Projects\Arduino\sloeber-workspace-aurora\Aurora_communication_protocol" -I"C:\eclipse\sloeber\arduinoPlugin\packages\esp8266\hardware\esp8266\2.4.2\libraries\DNSServer\src" -I"C:\eclipse\sloeber\arduinoPlugin\packages\esp8266\hardware\esp8266\2.4.2\libraries\ESP8266WebServer\src" -I"C:\eclipse\sloeber\arduinoPlugin\packages\esp8266\hardware\esp8266\2.4.2\libraries\ESP8266WiFi\src" -I"C:\eclipse\sloeber\arduinoPlugin\libraries\NTPClient\3.1.0" -I"C:\eclipse\sloeber\arduinoPlugin\packages\esp8266\hardware\esp8266\2.4.2\libraries\SD\src" -I"C:\eclipse\sloeber\arduinoPlugin\packages\esp8266\hardware\esp8266\2.4.2\libraries\SoftwareSerial" -I"C:\eclipse\sloeber\arduinoPlugin\packages\esp8266\hardware\esp8266\2.4.2\libraries\SPI" -I"C:\eclipse\sloeber\arduinoPlugin\libraries\Time\1.5.0" -I"C:\eclipse\sloeber\arduinoPlugin\packages\esp8266\hardware\esp8266\2.4.2\libraries\Wire" -I"C:\eclipse\sloeber\arduinoPlugin\libraries\WiFiManager\0.14.0" -I"D:\Projects\Arduino\sloeber-workspace-lib\EMailSender" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -D__IN_ECLIPSE__=1 -x c++ "$<"  -o  "$@"
	@echo 'Finished building: $<'
	@echo ' '


