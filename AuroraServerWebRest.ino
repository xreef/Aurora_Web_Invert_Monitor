#include "Arduino.h"
#include <Aurora.h>
#include <SD.h>
#define FS_NO_GLOBALS
#include "FS.h"
#include <Thread.h>

#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>        // Include the mDNS library

#include <ESP8266WebServer.h>
#include <ESP8266WebServerSecure.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <SPI.h>

#include <ArduinoJson.h>

#include <EMailSender.h>

// Uncomment to enable server ftp.
 #define SERVER_FTP

#ifdef SERVER_FTP
#include <ESP8266FtpServer.h>
#endif

// Uncomment to enable printing out nice debug messages.
#define AURORA_SERVER_DEBUG

// Define where debug output will be printed.
#define DEBUG_PRINTER Serial1

// Setup debug printing macros.
#ifdef AURORA_SERVER_DEBUG
	#define DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
	#define DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
#else
	#define DEBUG_PRINT(...) {}
	#define DEBUG_PRINTLN(...) {}
#endif


char hostname[] = "InverterCentraline";

// Interval get data
#define DAILY_INTERVAL 5
#define CUMULATIVE_INTERVAL 15
#define CUMULATIVE_TOTAL_INTERVAL 5
#define STATE_INTERVAL 5
#define STATIC_DATA_INTERVAL 3 * 60

// SD
#define CS_PIN D8

// Inverte inizialization
Aurora inverter = Aurora(2, D2, D3, D1);

void manageStaticDataCallback ();
void leggiProduzioneCallback();
void leggiStatoInverterCallback();
void updateLocalTimeWithNTPCallback();

bool saveJSonToAFile(DynamicJsonDocument *doc, String filename);
JsonObject getJSonFromFile(DynamicJsonDocument *doc, String filename);

Thread ManageStaticData = Thread();

Thread LeggiStatoInverter = Thread();

Thread LeggiProduzione = Thread();

#define HTTP_REST_PORT 8080
ESP8266WebServer httpRestServer(HTTP_REST_PORT);

// The certificate is stored in PMEM
static const uint8_t x509[] PROGMEM = {
		0x30, 0x82, 0x01, 0xd3, 0x30, 0x82, 0x01, 0x3c, 0x02, 0x09, 0x00, 0xc5,
		  0xfc, 0x28, 0xef, 0xf9, 0x7f, 0x1d, 0x23, 0x30, 0x0d, 0x06, 0x09, 0x2a,
		  0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x30, 0x2e,
		  0x31, 0x18, 0x30, 0x16, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x0f, 0x52,
		  0x65, 0x6e, 0x7a, 0x6f, 0x4d, 0x69, 0x73, 0x63, 0x68, 0x69, 0x61, 0x6e,
		  0x74, 0x69, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
		  0x09, 0x31, 0x32, 0x37, 0x2e, 0x30, 0x2e, 0x30, 0x2e, 0x31, 0x30, 0x1e,
		  0x17, 0x0d, 0x31, 0x38, 0x31, 0x31, 0x32, 0x34, 0x32, 0x31, 0x31, 0x31,
		  0x32, 0x38, 0x5a, 0x17, 0x0d, 0x33, 0x32, 0x30, 0x38, 0x30, 0x32, 0x32,
		  0x31, 0x31, 0x31, 0x32, 0x38, 0x5a, 0x30, 0x2e, 0x31, 0x18, 0x30, 0x16,
		  0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x0f, 0x52, 0x65, 0x6e, 0x7a, 0x6f,
		  0x4d, 0x69, 0x73, 0x63, 0x68, 0x69, 0x61, 0x6e, 0x74, 0x69, 0x31, 0x12,
		  0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x09, 0x31, 0x32, 0x37,
		  0x2e, 0x30, 0x2e, 0x30, 0x2e, 0x31, 0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06,
		  0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
		  0x03, 0x81, 0x8d, 0x00, 0x30, 0x81, 0x89, 0x02, 0x81, 0x81, 0x00, 0xba,
		  0x62, 0xa9, 0xc2, 0xa6, 0x7e, 0x87, 0x19, 0x58, 0xe3, 0x21, 0x52, 0xfe,
		  0xee, 0x83, 0x08, 0xca, 0xeb, 0x09, 0xb1, 0x50, 0x70, 0x9d, 0xe1, 0xe4,
		  0xb6, 0xa2, 0x5e, 0xca, 0xc8, 0x5a, 0x6e, 0x95, 0x8e, 0x7b, 0x50, 0x69,
		  0x23, 0x38, 0x5d, 0x65, 0x9f, 0x05, 0x68, 0xe5, 0x07, 0x04, 0x67, 0xac,
		  0x8e, 0x85, 0x61, 0x5d, 0x68, 0x01, 0xcc, 0x1a, 0x60, 0xfc, 0xd7, 0x07,
		  0xce, 0x61, 0x59, 0x5b, 0x88, 0xa7, 0xe6, 0xb7, 0xe7, 0xf4, 0x26, 0x6f,
		  0xa4, 0xe2, 0xfa, 0x6a, 0x2f, 0x06, 0xbe, 0x90, 0x28, 0x9f, 0xb1, 0x0e,
		  0x7e, 0xb3, 0xf1, 0x5c, 0xaa, 0x84, 0xcf, 0x79, 0xc1, 0xfe, 0xc2, 0x03,
		  0x0f, 0x6e, 0x6b, 0xfc, 0x3b, 0xfb, 0x4d, 0x1f, 0x0c, 0x13, 0xd8, 0x5b,
		  0x91, 0xac, 0x05, 0x21, 0x36, 0x76, 0x4e, 0x40, 0x3f, 0xce, 0x77, 0xe5,
		  0x4a, 0xf1, 0x49, 0x95, 0xa0, 0x34, 0xbd, 0x02, 0x03, 0x01, 0x00, 0x01,
		  0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
		  0x0b, 0x05, 0x00, 0x03, 0x81, 0x81, 0x00, 0x61, 0x6f, 0xc6, 0x6c, 0x30,
		  0x61, 0xaf, 0x5c, 0x52, 0xf3, 0x0e, 0x08, 0x44, 0xce, 0xf3, 0x32, 0x99,
		  0x89, 0x75, 0x23, 0x6b, 0x04, 0x1f, 0x2d, 0xd3, 0xd8, 0x96, 0x41, 0xdf,
		  0x56, 0xb4, 0x50, 0x36, 0xbf, 0x64, 0x86, 0xdd, 0x70, 0x06, 0x29, 0x93,
		  0x50, 0xa3, 0xd3, 0x44, 0xba, 0x45, 0xb1, 0x1b, 0x73, 0x68, 0x43, 0x99,
		  0x72, 0x70, 0x49, 0x74, 0x87, 0x32, 0x89, 0x6a, 0xe3, 0x43, 0x38, 0x13,
		  0x62, 0x39, 0xd1, 0xa5, 0x61, 0x14, 0xa7, 0x8f, 0xb1, 0x93, 0x6a, 0xba,
		  0x69, 0x16, 0xa1, 0xd1, 0xf2, 0x89, 0xcf, 0x48, 0xf0, 0x3d, 0x4e, 0x88,
		  0x9d, 0xbb, 0xa0, 0x4b, 0x13, 0x60, 0xf2, 0x58, 0x8f, 0x0b, 0xcc, 0x37,
		  0x7c, 0x55, 0xb4, 0x28, 0x7f, 0xb3, 0x64, 0x19, 0xf4, 0xed, 0x84, 0xe5,
		  0x0b, 0x65, 0x19, 0x06, 0xa0, 0xa6, 0xbd, 0x19, 0x3d, 0xa6, 0x65, 0x5b,
		  0xf7, 0x0b, 0xc9
};

// And so is the key.  These could also be in DRAM
static const uint8_t rsakey[] PROGMEM = {
		  0x30, 0x82, 0x02, 0x5d, 0x02, 0x01, 0x00, 0x02, 0x81, 0x81, 0x00, 0xba,
		  0x62, 0xa9, 0xc2, 0xa6, 0x7e, 0x87, 0x19, 0x58, 0xe3, 0x21, 0x52, 0xfe,
		  0xee, 0x83, 0x08, 0xca, 0xeb, 0x09, 0xb1, 0x50, 0x70, 0x9d, 0xe1, 0xe4,
		  0xb6, 0xa2, 0x5e, 0xca, 0xc8, 0x5a, 0x6e, 0x95, 0x8e, 0x7b, 0x50, 0x69,
		  0x23, 0x38, 0x5d, 0x65, 0x9f, 0x05, 0x68, 0xe5, 0x07, 0x04, 0x67, 0xac,
		  0x8e, 0x85, 0x61, 0x5d, 0x68, 0x01, 0xcc, 0x1a, 0x60, 0xfc, 0xd7, 0x07,
		  0xce, 0x61, 0x59, 0x5b, 0x88, 0xa7, 0xe6, 0xb7, 0xe7, 0xf4, 0x26, 0x6f,
		  0xa4, 0xe2, 0xfa, 0x6a, 0x2f, 0x06, 0xbe, 0x90, 0x28, 0x9f, 0xb1, 0x0e,
		  0x7e, 0xb3, 0xf1, 0x5c, 0xaa, 0x84, 0xcf, 0x79, 0xc1, 0xfe, 0xc2, 0x03,
		  0x0f, 0x6e, 0x6b, 0xfc, 0x3b, 0xfb, 0x4d, 0x1f, 0x0c, 0x13, 0xd8, 0x5b,
		  0x91, 0xac, 0x05, 0x21, 0x36, 0x76, 0x4e, 0x40, 0x3f, 0xce, 0x77, 0xe5,
		  0x4a, 0xf1, 0x49, 0x95, 0xa0, 0x34, 0xbd, 0x02, 0x03, 0x01, 0x00, 0x01,
		  0x02, 0x81, 0x80, 0x36, 0x21, 0xd5, 0xa0, 0x1c, 0xee, 0xfe, 0x99, 0xd4,
		  0x01, 0x13, 0x7a, 0xa1, 0x63, 0xf0, 0x56, 0xab, 0x68, 0x9c, 0x06, 0x0d,
		  0x90, 0xc7, 0xaa, 0x05, 0xdd, 0x2d, 0x47, 0x4e, 0xa9, 0xe5, 0xe9, 0xdc,
		  0x31, 0xe7, 0x8a, 0xb1, 0x1e, 0x73, 0x8e, 0x5c, 0xa7, 0x54, 0xd0, 0xe4,
		  0x43, 0xa7, 0x79, 0xdc, 0xd9, 0xff, 0xcf, 0x09, 0x6b, 0xdd, 0xa9, 0xc3,
		  0xb7, 0x8b, 0x77, 0x80, 0x62, 0xe6, 0x4e, 0xa8, 0x2b, 0x42, 0x10, 0xe6,
		  0x8a, 0x05, 0x4e, 0xa5, 0xff, 0x16, 0x03, 0x41, 0x11, 0x7b, 0x97, 0xb6,
		  0x25, 0x95, 0xe1, 0xf2, 0x40, 0xc5, 0x88, 0x47, 0x82, 0x4d, 0x3e, 0xd8,
		  0x5f, 0xad, 0x52, 0x03, 0xd2, 0xe2, 0xc3, 0xce, 0xfd, 0x3e, 0x6e, 0x7c,
		  0x3e, 0xfe, 0xe3, 0xa0, 0x01, 0x60, 0xff, 0xf6, 0x01, 0xd2, 0xf9, 0xfa,
		  0xc0, 0xcb, 0x4a, 0x26, 0xc4, 0x9e, 0xdd, 0xcd, 0xda, 0x9f, 0x01, 0x02,
		  0x41, 0x00, 0xee, 0xff, 0x8e, 0xc6, 0x3b, 0x18, 0xa9, 0x6f, 0x1a, 0x2c,
		  0x61, 0x06, 0xb4, 0xdc, 0xca, 0x7b, 0x68, 0xd1, 0x2b, 0xef, 0x12, 0x98,
		  0x55, 0xdb, 0xe8, 0x81, 0x49, 0xc2, 0x70, 0xed, 0xd5, 0x55, 0xb5, 0xeb,
		  0xf4, 0x38, 0x25, 0x25, 0x5f, 0x89, 0x1d, 0xe5, 0xf6, 0xb2, 0x0e, 0x13,
		  0x21, 0x7a, 0x07, 0x36, 0x53, 0xfc, 0xb2, 0x0f, 0xc9, 0xef, 0xa1, 0x3c,
		  0x56, 0x0c, 0xb1, 0x16, 0x1d, 0x9d, 0x02, 0x41, 0x00, 0xc7, 0xa4, 0xf6,
		  0x6c, 0xbd, 0x1d, 0x08, 0x5f, 0xa6, 0xd8, 0x30, 0x25, 0x97, 0x81, 0xc8,
		  0xcb, 0x74, 0xa1, 0xef, 0x8d, 0xe7, 0xbe, 0x21, 0x0f, 0xec, 0xe7, 0x65,
		  0xe9, 0xb1, 0xab, 0x10, 0x5b, 0x6b, 0x8c, 0xac, 0x1e, 0x3e, 0x50, 0x3c,
		  0x02, 0x0d, 0x49, 0x94, 0x24, 0xbb, 0x03, 0xb4, 0xad, 0x2b, 0x5a, 0x79,
		  0xb0, 0x9e, 0xf6, 0x58, 0x5d, 0x4e, 0x97, 0x57, 0x11, 0xca, 0x5c, 0x59,
		  0xa1, 0x02, 0x41, 0x00, 0x9e, 0xf8, 0x4d, 0xb7, 0x7d, 0x47, 0x82, 0x2b,
		  0xec, 0x74, 0xe8, 0x74, 0xd5, 0x88, 0xa7, 0x06, 0x3f, 0x4a, 0x22, 0xb6,
		  0xfa, 0xdf, 0x68, 0xfc, 0xc5, 0x42, 0x7a, 0x15, 0x63, 0x98, 0x4e, 0xf6,
		  0x9b, 0xf3, 0x3e, 0x96, 0xb9, 0xde, 0x8a, 0x15, 0x62, 0x55, 0xbc, 0x29,
		  0xe3, 0x42, 0xc6, 0x59, 0xac, 0xc2, 0x6e, 0x4a, 0xff, 0x05, 0x91, 0x84,
		  0x5a, 0xf3, 0x0f, 0x29, 0x92, 0x00, 0xeb, 0xe1, 0x02, 0x40, 0x05, 0xf1,
		  0x7c, 0x40, 0x8a, 0x74, 0xb5, 0xce, 0x1b, 0x2a, 0x6e, 0x6c, 0x80, 0x11,
		  0x26, 0x08, 0x20, 0x85, 0xbd, 0x9a, 0xec, 0xde, 0x35, 0x1f, 0xc3, 0x3e,
		  0xb4, 0x42, 0xfb, 0xbe, 0x0a, 0xf3, 0x9d, 0xc5, 0x07, 0x4e, 0xb3, 0x2e,
		  0x32, 0x4b, 0x21, 0x58, 0x22, 0x67, 0xe1, 0x85, 0x5f, 0xb8, 0x94, 0x04,
		  0xd2, 0x80, 0x96, 0x8a, 0xe0, 0xe0, 0x8e, 0x39, 0x65, 0x27, 0x2b, 0x6e,
		  0x0a, 0x61, 0x02, 0x41, 0x00, 0xe1, 0x3b, 0xa8, 0xa4, 0x75, 0xe9, 0xd6,
		  0xd7, 0x6e, 0x0f, 0x88, 0x00, 0x69, 0x5f, 0xe2, 0x3d, 0x68, 0x5d, 0x40,
		  0x03, 0x18, 0xa6, 0xed, 0x3b, 0x85, 0x8d, 0x97, 0x4c, 0x46, 0x21, 0xe8,
		  0x6b, 0x7b, 0xa2, 0x3f, 0x46, 0x3c, 0x4c, 0xc6, 0xaf, 0xf9, 0x2c, 0x39,
		  0x19, 0x06, 0x86, 0x63, 0xd2, 0x6c, 0x25, 0x8d, 0x21, 0x35, 0xec, 0xd4,
		  0x7c, 0x36, 0xea, 0x6e, 0x0a, 0xcb, 0x02, 0xa9, 0x51
};

#define HTTP_PORT 80
ESP8266WebServer httpServer(HTTP_PORT);

//#define SECURE_HTTP_PORT 443
//ESP8266WebServerSecure httpServer(SECURE_HTTP_PORT);

void restServerRouting();
void serverRouting();

WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 1*60*60, 60*60*1000);

Thread UpdateLocalTimeWithNTP = Thread();

bool fixedTime = false;

float setPrecision(float val, byte precision);

#define DSP_GRID_POWER_ALL_FILENAME									"power.jso"		/* Global */
#define DSP_GRID_CURRENT_ALL_FILENAME								"current.jso"
#define DSP_GRID_VOLTAGE_ALL_FILENAME								"voltage.jso"

EMailSender emailSend("smtp.mischianti@gmail.com", "cicciolo77.");

#ifdef SERVER_FTP
FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial
#endif

int timeOffset = 0;

void setup() {
	#ifdef AURORA_SERVER_DEBUG
		// Inizilization of serial debug
		Serial1.begin(19200);
		Serial1.setTimeout(500);
		// Wait to finish inizialization
		delay(600);
	#endif

	DEBUG_PRINT(F("Inizializing FS..."));
	if (SPIFFS.begin()){
		DEBUG_PRINTLN(F("done."));
	}else{
		DEBUG_PRINTLN(F("fail."));
	}
    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
//    wifiManager.setConfigPortalTimeout(10);
	WiFi.hostname(hostname);
	wifi_station_set_hostname(hostname);

	DEBUG_PRINT(F("Open config file..."));
	fs::File configFile = SPIFFS.open("/mc/config.txt", "r");
	if (configFile) {
//		 while (configFile.available())
//		    {
//		      Serial1.write(configFile.read());
//		    }
//
	    DEBUG_PRINTLN("done.");
		DynamicJsonDocument doc;
		DeserializationError error = deserializeJson(doc, configFile);
		// close the file:
		configFile.close();

		if (error){
			// if the file didn't open, print an error:
			DEBUG_PRINT(F("Error parsing JSON "));
			DEBUG_PRINTLN(error.c_str());

		}else{
			JsonObject rootObj = doc.as<JsonObject>();
			JsonObject preferences = rootObj["preferences"];
			bool isGTM = preferences.containsKey("GTM");
			if (isGTM){
				JsonObject GTM = preferences["GTM"];
				bool isValue = GTM.containsKey("value");
				if (isValue){
					int value = GTM["value"];

					DEBUG_PRINT("Impostazione GTM+")
					DEBUG_PRINTLN(value)

					timeClient.setTimeOffset(value*60*60);
					timeOffset = value*60*60;
				}
			}

			JsonObject serverConfig = rootObj["server"];
			bool isStatic = serverConfig["isStatic"];
			if (isStatic==true){
				const char* address = serverConfig["address"];
				const char* gatway = serverConfig["gatway"];
				const char* netMask = serverConfig["netMask"];

				const char* dns1 = serverConfig["dns1"];
				const char* dns2 = serverConfig["dns2"];

				const char* _hostname = serverConfig["hostname"];
				//start-block2
				IPAddress _ip;
				bool parseSuccess;
				parseSuccess = _ip.fromString(address);
				if (parseSuccess) {
					DEBUG_PRINTLN("Address correctly parsed!");
				}

				IPAddress _gw;
				parseSuccess = _gw.fromString(gatway);
				if (parseSuccess) {
					DEBUG_PRINTLN("Gatway correctly parsed!");
				}

				IPAddress _sn;
				parseSuccess = _sn.fromString(netMask);
				if (parseSuccess) {
					DEBUG_PRINTLN("Subnet correctly parsed!");
				}

				IPAddress _dns1;
				IPAddress _dns2;
				bool isDNS = false;
				if (dns1 && sizeof(_dns1) > 7 && dns2 && sizeof(_dns2) > 7 ){
					parseSuccess = _dns1.fromString(dns1);
					if (parseSuccess) {
						DEBUG_PRINTLN("DNS 1 correctly parsed!");
						isDNS = true;
					}

					parseSuccess = _dns2.fromString(dns2);
					if (parseSuccess) {
						DEBUG_PRINTLN("DNS 2 correctly parsed!");
					}
					//end-block2
				}
				if (isDNS){
					wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn, _dns1, _dns2);
				}else{
					wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
				}

				if (_hostname && sizeof(_hostname)>1){
					strcpy(hostname, _hostname);
				}
				// IPAddress(85, 37, 17, 12), IPAddress(8, 8, 8, 8)
	//
	//		    emailSend.setEMailLogin("smtp.mischianti@gmail.com");
			}
		}
	}else{
	    DEBUG_PRINTLN("fail.");
	}

    //reset saved settings
//    wifiManager.resetSettings();

    //set custom ip for portal
    //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    //fetches ssid and pass from eeprom and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    wifiManager.autoConnect("InverterCentralineConfiguration");
    //or use this for auto generated name ESP + ChipID
    //wifiManager.autoConnect();


    //if you get here you have connected to the WiFi
    DEBUG_PRINTLN(F("WIFIManager connected!"));

    DEBUG_PRINT("IP --> ");
    DEBUG_PRINTLN(WiFi.localIP());
    DEBUG_PRINT("GW --> ");
    DEBUG_PRINTLN(WiFi.gatewayIP());
    DEBUG_PRINT("SM --> ");
    DEBUG_PRINTLN(WiFi.subnetMask());

    DEBUG_PRINT("DNS 1 --> ");
    DEBUG_PRINTLN(WiFi.dnsIP(0));

    DEBUG_PRINT("DNS 2 --> ");
    DEBUG_PRINTLN(WiFi.dnsIP(1));

	// Start inverter serial
	DEBUG_PRINT(F("Initializing Inverter serial..."));
	inverter.begin();
	DEBUG_PRINTLN(F("initialization done."));

	// Start inizialization of SD cart
	DEBUG_PRINT(F("Initializing SD card..."));
	if (!SD.begin(CS_PIN)) {
		DEBUG_PRINTLN(F("initialization failed!"));
		return;
	}
	DEBUG_PRINTLN(F("Inizialization done."));

	ManageStaticData.onRun(manageStaticDataCallback);
	ManageStaticData.setInterval(STATIC_DATA_INTERVAL * 60 * 1000);

	// Start thread, and set it every 1 minutes
	LeggiStatoInverter.onRun(leggiStatoInverterCallback);
	LeggiStatoInverter.setInterval(STATE_INTERVAL * 60 * 1000);

	// Start thread, and set it every 1 minutes
	LeggiProduzione.onRun(leggiProduzioneCallback);
	LeggiProduzione.setInterval(60 * 1000);

	int timeToRetry = 10;
	while ( WiFi.status() != WL_CONNECTED && timeToRetry>0 ) {
	    delay ( 500 );
	    DEBUG_PRINT ( "." );
	    timeToRetry--;
	}

//#ifndef SERVER_FTP

	timeClient.begin();
	updateLocalTimeWithNTPCallback();

	UpdateLocalTimeWithNTP.onRun(updateLocalTimeWithNTPCallback);
	UpdateLocalTimeWithNTP.setInterval(60*60 * 1000);

	restServerRouting();
	httpRestServer.begin();
    DEBUG_PRINTLN(F("HTTP REST Server Started"));

//	httpServer.setServerKeyAndCert_P(rsakey, sizeof(rsakey), x509, sizeof(x509));
	serverRouting();
	httpServer.begin();
    DEBUG_PRINTLN(F("HTTPS Server Started"));
//#endif

#ifdef SERVER_FTP
//    SPIFFS.format();
    ftpSrv.begin("aurora","aurora");    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
    DEBUG_PRINTLN(F("FTP Server Started"));
#endif

    if (!MDNS.begin(hostname)) {             // Start the mDNS responder for esp8266.local
    	DEBUG_PRINTLN("Error setting up MDNS responder!");
    }
    DEBUG_PRINT(hostname);
    DEBUG_PRINTLN(" --> mDNS responder started");
}

void loop() {
//#ifndef SERVER_FTP
	// Activate thread
	if (fixedTime && LeggiProduzione.shouldRun()) {
		LeggiProduzione.run();
	}

	if (fixedTime && LeggiStatoInverter.shouldRun()) {
		LeggiStatoInverter.run();
	}

	if (fixedTime && ManageStaticData.shouldRun()) {
		ManageStaticData.run();
	}
	if (UpdateLocalTimeWithNTP.shouldRun()) {
		UpdateLocalTimeWithNTP.run();
	}

	httpRestServer.handleClient();
	httpServer.handleClient();

	timeClient.update();
//#endif

#ifdef SERVER_FTP
	  ftpSrv.handleFTP();        //make sure in loop you call handleFTP()!!
#endif

}

// We can read/write a file at time in the SD card
File myFileSDCart; // @suppress("Ambiguous problem")

void leggiStatoInverterCallback() {
	DEBUG_PRINT(F("Thread call (LeggiStatoInverterCallback) --> "));
	DEBUG_PRINTLN(getEpochStringByParams(now()));

	Aurora::DataState dataState = inverter.readState();
	if (dataState.state.readState == true) {
		DEBUG_PRINTLN(F("done."));

		DEBUG_PRINT(F("Create json..."));

		String scopeDirectory = F("alarms");
		if (!SD.exists(scopeDirectory)) {
			SD.mkdir(scopeDirectory);
		}

		String filename = scopeDirectory + "/alarStat.jso";

		DynamicJsonDocument doc;
		JsonObject rootObj = doc.to<JsonObject>();

		rootObj["lastUpdate"] = getEpochStringByParams(now());

		rootObj["alarmStateParam"] = dataState.alarmState;
		rootObj["alarmState"] = dataState.getAlarmState();

		rootObj["channel1StateParam"] = dataState.channel1State;
		rootObj["channel1State"] = dataState.getDcDcChannel1State();

		rootObj["channel2StateParam"] = dataState.channel2State;
		rootObj["channel2State"] = dataState.getDcDcChannel2State();

		rootObj["inverterStateParam"] = dataState.inverterState;
		rootObj["inverterState"] = dataState.getInverterState();

		DEBUG_PRINTLN(F("done."));

		saveJSonToAFile(&doc, filename);

		// If alarm present or inverterState not running
//		if (dataState.alarmState>0 || dataState.inverterState!=2){

		String dayDirectory = getEpochStringByParams(now(), (char*) "%Y%m%d");

		String filenameAL = scopeDirectory + '/' + dayDirectory + "/alarms.jso";

		DynamicJsonDocument docAS;
		JsonObject obj;

		obj = getJSonFromFile(&docAS, filenameAL);

		obj["lastUpdate"] = getEpochStringByParams(now());

		JsonArray data;
		if (!obj.containsKey("data")) {
			data = obj.createNestedArray("data");
		} else {
			data = obj["data"];
		}

		bool inverterProblem = dataState.alarmState > 0 || dataState.inverterState != 2;

		bool firstElement = data.size() == 0;

		if (inverterProblem || !firstElement) {
			JsonObject lastData;
			if (data.size() > 0) {
				lastData = data[data.size() - 1];
			}

			bool variationFromPrevious = (data.size() > 0
					&& (lastData["asp"] != dataState.alarmState
							|| lastData["c1sp"] != dataState.channel1State
							|| lastData["c2sp"] != dataState.channel2State
							|| lastData["isp"] != dataState.inverterState));

			byte ldState = lastData["isp"];

			DEBUG_PRINT(F("Last data state --> "));
			DEBUG_PRINTLN(ldState);

			DEBUG_PRINT(F("Data state --> "));
			DEBUG_PRINTLN(dataState.inverterState);

			DEBUG_PRINT(F("Last data vs data state is different --> "));
			DEBUG_PRINTLN(lastData["isp"] != dataState.inverterState);


			DEBUG_PRINT(F("Inverter problem --> "));
			DEBUG_PRINTLN(inverterProblem);

			DEBUG_PRINT(F("firstElement --> "));
			DEBUG_PRINTLN(firstElement);

			DEBUG_PRINT(F("Variation From Previous --> "));
			DEBUG_PRINTLN(variationFromPrevious);

			if ((inverterProblem && firstElement)
					|| (!firstElement && variationFromPrevious)) {



				JsonObject objArrayData = data.createNestedObject();

				objArrayData["h"] = getEpochStringByParams(now(),
						(char*) "%H%M");

				objArrayData["asp"] = dataState.alarmState;
				//			objArrayData["as"] = dataState.getAlarmState();

				objArrayData["c1sp"] = dataState.channel1State;
				//			objArrayData["c1s"] = dataState.getDcDcChannel1State();

				objArrayData["c2sp"] = dataState.channel2State;
				//			objArrayData["c2s"] = dataState.getDcDcChannel2State();

				objArrayData["isp"] = dataState.inverterState;
				//			objArrayData["is"] = dataState.getInverterState();

				DEBUG_PRINTLN(F("Store alarms --> "));
				//				serializeJson(doc, Serial);
				DEBUG_PRINT(docAS.memoryUsage());
				DEBUG_PRINTLN();

				if (!SD.exists(scopeDirectory + '/' + dayDirectory)) {
					SD.mkdir(scopeDirectory + '/' + dayDirectory);
				}

				saveJSonToAFile(&docAS, filenameAL);

				DEBUG_PRINT(F("Open config file..."));
				fs::File configFile = SPIFFS.open("/mc/config.txt", "r");
				if (configFile) {
				    DEBUG_PRINTLN("done.");
					DynamicJsonDocument doc;
					DeserializationError error = deserializeJson(doc, configFile);
					if (error) {
						// if the file didn't open, print an error:
						DEBUG_PRINT(F("Error parsing JSON "));
						DEBUG_PRINTLN(error);
					}

					// close the file:
					configFile.close();

					JsonObject rootObj = doc.as<JsonObject>();

				    DEBUG_PRINT(F("After read config check serverSMTP and emailNotification "));
				    DEBUG_PRINTLN(rootObj.containsKey("serverSMTP") && rootObj.containsKey("emailNotification"));

					if (rootObj.containsKey("serverSMTP") && rootObj.containsKey("emailNotification")){
//						JsonObject serverConfig = rootObj["server"];
						JsonObject serverSMTP = rootObj["serverSMTP"];
						JsonObject emailNotification = rootObj["emailNotification"];

						bool isNotificationEnabled = (emailNotification.containsKey("isNotificationEnabled"))?emailNotification["isNotificationEnabled"]:false;

					    DEBUG_PRINT(F("isNotificationEnabled "));
					    DEBUG_PRINTLN(isNotificationEnabled);

						if (isNotificationEnabled){
							const char* serverSMTPAddr = serverSMTP["server"];
							emailSend.setSMTPServer(serverSMTPAddr);
							uint16_t portSMTP = serverSMTP["port"];
							emailSend.setSMTPPort(portSMTP);
							const char* loginSMTP = serverSMTP["login"];
							emailSend.setEMailLogin(loginSMTP);
							const char* passwordSMTP = serverSMTP["password"];
							emailSend.setEMailPassword(passwordSMTP);
							const char* fromSMTP = serverSMTP["from"];
							emailSend.setEMailFrom(fromSMTP);

							DEBUG_PRINT("server ");
							DEBUG_PRINTLN(serverSMTPAddr);
							DEBUG_PRINT("port ");
							DEBUG_PRINTLN(portSMTP);
							DEBUG_PRINT("login ");
							DEBUG_PRINTLN(loginSMTP);
							DEBUG_PRINT("password ");
							DEBUG_PRINTLN(passwordSMTP);
							DEBUG_PRINT("from ");
							DEBUG_PRINTLN(fromSMTP);

							EMailSender::EMailMessage message;
							const String sub = emailNotification["subject"];
							message.subject = sub;

							JsonArray emailList = emailNotification["emailList"];

						    DEBUG_PRINT(F("Email list "));

							for (uint8_t i=0; i<emailList.size(); i++){
								JsonObject emailElem = emailList[i];

								byte asp = lastData["asp"];
								byte c1sp = lastData["c1sp"];
								byte c2sp = lastData["c2sp"];
								byte isp = lastData["isp"];

								const String alarm = emailElem["alarm"];
								const String ch1 = emailElem["ch1"];
								const String ch2 = emailElem["ch2"];
								const String state = emailElem["state"];

							    DEBUG_PRINT(F("State value "));
							    DEBUG_PRINTLN(state);

							    DEBUG_PRINT(F("State value on_problem comparison "));
							    DEBUG_PRINTLN(state=="on_problem");

							    DEBUG_PRINT(F("Alarm value "));
							    DEBUG_PRINTLN(alarm);

							    DEBUG_PRINT(F("Alarm all comparison "));
							    DEBUG_PRINTLN(alarm=="all");

								bool allNotification = (
										(alarm=="all" && asp != dataState.alarmState)
										||
										(ch1=="all" && c1sp != dataState.channel1State)
										||
										(ch2=="all" && c2sp != dataState.channel2State)
										||
										(state=="all" && isp != dataState.inverterState)
								);

								bool onProblem = (
										(alarm=="on_problem" && dataState.alarmState > 0)
										||
										(ch1=="on_problem" && dataState.channel1State != 2)
										||
										(ch2=="on_problem" && dataState.channel1State != 2)
										||
										(state=="on_problem" && dataState.inverterState != 2)
								);

							    DEBUG_PRINT(F("Check allNotification "));
							    DEBUG_PRINTLN(allNotification);

							    DEBUG_PRINT(F("Check onProblem "));
							    DEBUG_PRINTLN(onProblem);


								if (
										allNotification
										||
										onProblem
								){
									const String mp = emailNotification["messageProblem"];
									const String mnp = emailNotification["messageNoProblem"];
									message.message = ((inverterProblem)?mp:mnp)+
													"<br>Alarm: "+dataState.getAlarmState()+
													"<br>CH1: "+dataState.getDcDcChannel1State() +
													"<br>CH2: "+dataState.getDcDcChannel2State()+
													"<br>Stato: "+dataState.getInverterState();

									EMailSender::Response resp = emailSend.send(emailElem["email"], message);

									DEBUG_PRINTLN("Sending status: ");
									const String em = emailElem["email"];
									DEBUG_PRINTLN(em);
									DEBUG_PRINTLN(resp.status);
									DEBUG_PRINTLN(resp.code);
									DEBUG_PRINTLN(resp.desc);
								}


							}
						}
					}
				}else{
				    DEBUG_PRINTLN("fail.");
				}

			}

		}
//		}
	}

}

void manageStaticDataCallback () {
	DEBUG_PRINT(F("Thread call (manageStaticDataCallback) --> "));
	DEBUG_PRINTLN(getEpochStringByParams(now()));

	DEBUG_PRINT(F("Data version read... "));
	Aurora::DataVersion dataVersion = inverter.readVersion();

	DEBUG_PRINT(F("Firmware release read... "));
	Aurora::DataFirmwareRelease firmwareRelease = inverter.readFirmwareRelease();

	DEBUG_PRINT(F("System SN read... "));
	Aurora::DataSystemSerialNumber systemSN = inverter.readSystemSerialNumber();

	DEBUG_PRINT(F("Manufactoru Week Year read... "));
	Aurora::DataManufacturingWeekYear manufactoryWeekYear = inverter.readManufacturingWeekYear();

	DEBUG_PRINT(F("systemPN read... "));
	Aurora::DataSystemPN systemPN = inverter.readSystemPN();

	DEBUG_PRINT(F("configStatus read... "));
	Aurora::DataConfigStatus configStatus = inverter.readConfig();

	if (dataVersion.state.readState == true){
    	DEBUG_PRINTLN(F("done."));

    	DEBUG_PRINT(F("Create json..."));

		String scopeDirectory = F("static");
		if (!SD.exists(scopeDirectory)){
			SD.mkdir(scopeDirectory);
		}

		String filename = scopeDirectory+"/invinfo.jso";

		DynamicJsonDocument doc;
		JsonObject rootObj = doc.to<JsonObject>();

		rootObj["lastUpdate"] = getEpochStringByParams(now());

		rootObj["modelNameParam"] = dataVersion.par1;
		rootObj["modelName"] = dataVersion.getModelName().name;
		rootObj["modelNameIndoorOutdoorType"] = dataVersion.getIndoorOutdoorAndType();

		rootObj["gridStandardParam"] = dataVersion.par2;
		rootObj["gridStandard"] = dataVersion.getGridStandard();

		rootObj["trasformerLessParam"] = dataVersion.par3;
		rootObj["trasformerLess"] = dataVersion.getTrafoOrNonTrafo();

		rootObj["windOrPVParam"] = dataVersion.par4;
		rootObj["windOrPV"] = dataVersion.getWindOrPV();

		rootObj["firmwareRelease"] = firmwareRelease.release;
		rootObj["systemSN"] = systemSN.SerialNumber;

		rootObj["systemPN"] = systemPN.PN;

		JsonObject mWY = rootObj.createNestedObject("manufactory");
		mWY["Year"] = manufactoryWeekYear.Year;
		mWY["Week"] = manufactoryWeekYear.Week;

		JsonObject cs = rootObj.createNestedObject("configStatus");
		cs["code"] = configStatus.configStatus;
		cs["desc"] = configStatus.getConfigStatus();

		DEBUG_PRINTLN(F("done."));

		saveJSonToAFile(&doc, filename);
	}

}

void leggiProduzioneCallback() {
	DEBUG_PRINT(F("Thread call (leggiProduzioneCallback) --> "));
	DEBUG_PRINTLN(getEpochStringByParams(now()));

	tm nowDt = getDateTimeByParams(now());

	// Save cumulative data
	if (nowDt.tm_min % CUMULATIVE_INTERVAL == 0) {
		Aurora::DataCumulatedEnergy dce = inverter.readCumulatedEnergy(
		CUMULATED_DAILY_ENERGY);
		DEBUG_PRINT(F("Read state --> "));
		DEBUG_PRINTLN(dce.state.readState);

		if (dce.state.readState == 1) {
			unsigned long energy = dce.energy;

			String scopeDirectory = F("monthly");
			if (!SD.exists(scopeDirectory)) {
				SD.mkdir(scopeDirectory);
			}

			Aurora::DataDSP dsp = inverter.readDSP(DSP_POWER_PEAK_TODAY_ALL);
			float powerPeak = dsp.value;

			if (energy && energy > 0) {
				String filename = scopeDirectory + F("/")
						+ getEpochStringByParams(now(), (char*) "%Y%m")
						+ ".jso";
				String tagName = getEpochStringByParams(now(), (char*) "%d");

				DynamicJsonDocument doc;
				JsonObject obj;

				obj = getJSonFromFile(&doc, filename);

				obj["lastUpdate"] = getEpochStringByParams(now());

				JsonObject series;
				if (!obj.containsKey("series")) {
					series = obj.createNestedObject("series");
				} else {
					series = obj["series"];
				}

				JsonObject dayData = series.createNestedObject(tagName);
				dayData["pow"] = energy;
				dayData["powPeak"] = setPrecision(powerPeak, 1);

				DEBUG_PRINT(F("Store cumulated energy --> "));
//				serializeJson(doc, Serial);
				DEBUG_PRINT(doc.memoryUsage());
				DEBUG_PRINTLN();

				saveJSonToAFile(&doc, filename);
			}

		}
	}

	// Save cumulative data
	if (nowDt.tm_min % CUMULATIVE_TOTAL_INTERVAL == 0) {

		DEBUG_PRINTLN("Get all totals.");
		Aurora::DataCumulatedEnergy dce = inverter.readCumulatedEnergy(
		CUMULATED_TOTAL_ENERGY_LIFETIME);
		unsigned long energyLifetime = dce.energy;
		DEBUG_PRINTLN(energyLifetime);

		if (dce.state.readState == 1) {

			dce = inverter.readCumulatedEnergy(CUMULATED_YEARLY_ENERGY);
			unsigned long energyYearly = dce.energy;
			DEBUG_PRINTLN(energyYearly);

			dce = inverter.readCumulatedEnergy(CUMULATED_MONTHLY_ENERGY);
			unsigned long energyMonthly = dce.energy;
			DEBUG_PRINTLN(energyMonthly);

			dce = inverter.readCumulatedEnergy(CUMULATED_WEEKLY_ENERGY);
			unsigned long energyWeekly = dce.energy;
			DEBUG_PRINTLN(energyWeekly);

			dce = inverter.readCumulatedEnergy(CUMULATED_DAILY_ENERGY);
			unsigned long energyDaily = dce.energy;
			DEBUG_PRINTLN(energyDaily);

			String scopeDirectory = F("states");
			if (!SD.exists(scopeDirectory)) {
				SD.mkdir(scopeDirectory);
			}
			if (dce.state.readState == 1 && energyLifetime) {
				String filename = scopeDirectory + F("/lastStat.jso");
				String tagName = getEpochStringByParams(now(), (char*) "%d");

				DynamicJsonDocument doc;
				JsonObject obj = doc.to<JsonObject>();;

//				obj = getJSonFromFile(&doc, filename);
//
//				if (obj.containsKey("energyWeekly")){
//					if (obj["energyWeekly"]>energyWeekly){
//						String dir = scopeDirectory +"/weeks";
//						if (!SD.exists(dir)) {
//							SD.mkdir(dir);
//						}
//						obj.remove("lastUpdate");
//						obj.remove("energyLifetime");
//						obj.remove("energyYearly");
//						obj.remove("energyDaily");
//						obj.remove("energyMonthly");
//
//						obj.remove("M");
//						obj.remove("Y");
//
//						String filenamem = dir +'/'+getEpochStringByParams(now(), (char*) "%Y")+ F(".jso");
//						DynamicJsonDocument docM;
//						JsonObject objM = getJSonFromFile(&docM, filenamem);
//
//						objM["lastUpdate"] = getEpochStringByParams(now());
//
//						JsonArray dataM;
//						if (!objM.containsKey("data")) {
//							dataM = objM.createNestedArray("data");
//						} else {
//							dataM = objM["data"];
//						}
//
//						dataM.add(obj);
//
//						saveJSonToAFile(&docM, filenamem);
//					}
//				}
//				if (obj.containsKey("energyMonthly")){
//					if (obj["energyMonthly"]>energyMonthly){
//						String dir = scopeDirectory +"/months";
//						if (!SD.exists(dir)) {
//							SD.mkdir(dir);
//						}
//
//						obj.remove("lastUpdate");
//						obj.remove("energyLifetime");
//						obj.remove("energyWeekly");
//						obj.remove("energyDaily");
//
//						obj.remove("W");
//						obj.remove("Y");
//
//						String filenamem = dir +'/'+getEpochStringByParams(now(), (char*) "%Y")+ F(".jso");
//						DynamicJsonDocument docM;
//						JsonObject objM = getJSonFromFile(&docM, filenamem);
//
//						objM["lastUpdate"] = getEpochStringByParams(now());
//
//						JsonArray dataM;
//						if (!objM.containsKey("data")) {
//							dataM = objM.createNestedArray("data");
//						} else {
//							dataM = objM["data"];
//						}
//
//						dataM.add(obj);
//
//						saveJSonToAFile(&docM, filenamem);
//					}
//				}
//				if (obj.containsKey("energyYearly")){
//					if (obj["energyYearly"]>energyYearly){
//
//						obj.remove("lastUpdate");
//
//						obj.remove("energyWeekly");
//						obj.remove("energyMonthly");
//						obj.remove("energyDaily");
//
//						obj.remove("W");
//						obj.remove("M");
//
//						String filenamem = scopeDirectory + F("/years.jso");
//						DynamicJsonDocument docM;
//						JsonObject objM = getJSonFromFile(&docM, filenamem);
//
//						objM["lastUpdate"] = getEpochStringByParams(now());
//
//						JsonArray dataM;
//						if (!objM.containsKey("data")) {
//							dataM = objM.createNestedArray("data");
//						} else {
//							dataM = objM["data"];
//						}
//
//						dataM.add(obj);
//
//						saveJSonToAFile(&docM, filenamem);
//					}
//				}

				obj["lastUpdate"] = getEpochStringByParams(now());

//			JsonObject dayData = obj.createNestedObject(tagName);
				obj["energyLifetime"] = energyLifetime;
				obj["energyYearly"] = energyYearly;
				obj["energyMonthly"] = energyMonthly;
				obj["energyWeekly"] = energyWeekly;
				obj["energyDaily"] = energyDaily;

				obj["W"] = getEpochStringByParams(now(), (char*) "%Y%W");
				obj["M"] = getEpochStringByParams(now(), (char*) "%Y%m");
				obj["Y"] = getEpochStringByParams(now(), (char*) "%Y");

				DEBUG_PRINT(F("Store energyLifetime energy --> "));
				//				serializeJson(doc, Serial);
				DEBUG_PRINT(doc.memoryUsage());
				DEBUG_PRINTLN();

				saveJSonToAFile(&doc, filename);

				DynamicJsonDocument docW;
				JsonObject objW;

				{
					String dir = scopeDirectory +"/weeks";
					String filenamem = dir +'/'+getEpochStringByParams(now(), (char*) "%Y")+ F(".jso");

					objW = getJSonFromFile(&doc, filename);
					objW[getEpochStringByParams(now(), (char*) "%Y%W")] = energyWeekly;

					saveJSonToAFile(&docW, filename);
				}
				{
					String dir = scopeDirectory +"/months";
					String filenamem = dir +'/'+getEpochStringByParams(now(), (char*) "%Y")+ F(".jso");

					objW = getJSonFromFile(&doc, filename);

					objW[getEpochStringByParams(now(), (char*) "%Y%m")] = energyMonthly;

					saveJSonToAFile(&docW, filename);
				}
				{
					String filenamem = scopeDirectory + F("/years.jso");

					objW = getJSonFromFile(&doc, filename);

					objW[getEpochStringByParams(now(), (char*) "%Y")] = energyYearly;

					saveJSonToAFile(&docW, filename);
				}

			}
		}
	}

	String scopeDirectory = F("product");
	if (!SD.exists(scopeDirectory)) {
		SD.mkdir(scopeDirectory);
	}
	for (int i = 0; i < 3; i++) {
		if (nowDt.tm_min % DAILY_INTERVAL == 0) {
			byte read = DSP_GRID_POWER_ALL;
			String filename = DSP_GRID_POWER_ALL_FILENAME;

			switch (i) {
			case (1):
				read = DSP_GRID_CURRENT_ALL;
				filename = DSP_GRID_CURRENT_ALL_FILENAME;
				break;
			case (2):
				read = DSP_GRID_VOLTAGE_ALL;
				filename = DSP_GRID_VOLTAGE_ALL_FILENAME;
				break;
			}

			Aurora::DataDSP dsp = inverter.readDSP(read);
			DEBUG_PRINT(F("Read state --> "));
			DEBUG_PRINTLN(dsp.state.readState);
			if (dsp.state.readState == 1) {
				float val = dsp.value;

				if (val && val > 0) {
					String dataDirectory = getEpochStringByParams(now(),
							(char*) "%Y%m%d");

					if (i == 0
							&& !SD.exists(
									scopeDirectory + '/' + dataDirectory)) {
						SD.mkdir(scopeDirectory + '/' + dataDirectory);
					}

					String filenameDir = scopeDirectory + F("/") + dataDirectory
							+ F("/") + filename;

					DynamicJsonDocument doc;
					JsonObject obj;

					obj = getJSonFromFile(&doc, filenameDir);

					obj["lastUpdate"] = getEpochStringByParams(now());

					JsonArray data;
					if (!obj.containsKey("data")) {
						data = obj.createNestedArray("data");
					} else {
						data = obj["data"];
					}

					JsonObject objArrayData = data.createNestedObject();
					objArrayData["h"] = getEpochStringByParams(now(),
							(char*) "%H%M");
					objArrayData["val"] = setPrecision(val, 1);
					DEBUG_PRINTLN(F("Store production --> "));
					DEBUG_PRINT(doc.memoryUsage());
					DEBUG_PRINTLN();

					saveJSonToAFile(&doc, filenameDir);
				}
			}
		}
	}
}

JsonObject getJSonFromFile(DynamicJsonDocument *doc, String filename) {
	// open the file for reading:
	myFileSDCart = SD.open(filename);
	if (myFileSDCart) {
		// read from the file until there's nothing else in it:
//			if (myFileSDCart.available()) {
//				firstWrite = false;
//			}

		DeserializationError error = deserializeJson(*doc, myFileSDCart);
		if (error) {
			// if the file didn't open, print an error:
			DEBUG_PRINT(F("Error parsing JSON "));
			DEBUG_PRINTLN(error);
		}

		// close the file:
		myFileSDCart.close();

		return doc->as<JsonObject>();
	} else {
		// if the file didn't open, print an error:
		DEBUG_PRINT(F("Error opening (or file not exists) "));
		DEBUG_PRINTLN(filename);

		DEBUG_PRINTLN(F("Empty json created"));
		return doc->to<JsonObject>();
	}

}

bool saveJSonToAFile(DynamicJsonDocument *doc, String filename) {
	SD.remove(filename);

	// open the file. note that only one file can be open at a time,
	// so you have to close this one before opening another.
	DEBUG_PRINTLN(F("Open file in write mode"));
	myFileSDCart = SD.open(filename, FILE_WRITE);
	if (myFileSDCart) {
		DEBUG_PRINT(F("Filename --> "));
		DEBUG_PRINTLN(filename);

		DEBUG_PRINT(F("Start write..."));

		serializeJson(*doc, myFileSDCart);

		DEBUG_PRINT(F("..."));
		// close the file:
		myFileSDCart.close();
		DEBUG_PRINTLN(F("done."));

		return true;
	} else {
		// if the file didn't open, print an error:
		DEBUG_PRINT(F("Error opening "));
		DEBUG_PRINTLN(filename);

		return false;
	}
}

void setCrossOrigin(){
	httpRestServer.sendHeader("Access-Control-Allow-Origin", "*");
	httpRestServer.sendHeader("Access-Control-Max-Age", "10000");
	httpRestServer.sendHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
	httpRestServer.sendHeader("Access-Control-Allow-Headers", "*");
};

void streamFileOnRest(String filename){
	if (SD.exists(filename)){
		myFileSDCart = SD.open(filename);
		if (myFileSDCart){
			if (myFileSDCart.available()){
				DEBUG_PRINT(F("Stream file..."));
				httpRestServer.streamFile(myFileSDCart, "application/json");
				DEBUG_PRINTLN(F("done."));
			}else{
				DEBUG_PRINTLN(F("Data not available!"));
				httpRestServer.send(204, "text/html", "Data not available!");
			}
			myFileSDCart.close();
		}else{
			DEBUG_PRINTLN(F("File not found!"));
			httpRestServer.send(204, "text/html", "No content found!");
		}
	}else{
		DEBUG_PRINTLN(F("File not found!"));
		httpRestServer.send(204, "text/html", "File not exist!");
	}
}
String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".json")) return "application/json";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){  // send the right file to the client (if it exists)
  DEBUG_PRINTLN("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.html";           // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){  // If the file exists, either as a compressed archive, or normal
    if(SPIFFS.exists(pathWithGz))                          // If there's a compressed version available
      path += ".gz";                                         // Use the compressed version
    fs::File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = httpServer.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    DEBUG_PRINTLN(String(F("\tSent file: ")) + path + String(F(" of size ")) + sent);
    return true;
  }
  DEBUG_PRINTLN(String(F("\tFile Not Found: ")) + path);
  return false;                                          // If the file doesn't exist, return false
}

void streamFile(const String filename){
	if (SPIFFS.exists(filename)){
		fs::File fileToStream = SPIFFS.open(filename, "r");
		if (fileToStream){
			if (fileToStream.available()){
				DEBUG_PRINT(F("Stream file..."));
				const String appContext = getContentType(filename);
				httpRestServer.streamFile(fileToStream, appContext);
				DEBUG_PRINTLN(F("done."));
			}else{
				DEBUG_PRINTLN(F("Data not available!"));
				httpRestServer.send(204, "text/html", "Data not available!");
			}
			fileToStream.close();
		}else{
			DEBUG_PRINTLN(F("File not found!"));
			httpRestServer.send(204, "text/html", "No content found!");
		}
	}else{
		DEBUG_PRINTLN(F("File not found!"));
		httpRestServer.send(204, "text/html", "File not exist!");
	}
}

void getProduction(){
	DEBUG_PRINTLN(F("getProduction"));

	setCrossOrigin();

	if (httpRestServer.arg("day")== "" || httpRestServer.arg("type")== "" ){     //Parameter not found
		httpRestServer.send(400, "text/html", "Missing required parameter!");
		DEBUG_PRINTLN(F("No parameter"));
	}else{     //Parameter found
		DEBUG_PRINT(F("Read file: "));
		String filename = "product/"+httpRestServer.arg("day")+"/"+httpRestServer.arg("type")+".jso";

		DEBUG_PRINTLN(filename);

		streamFileOnRest(filename);
	}
}

void getProductionTotal(){
	DEBUG_PRINTLN(F("getProduction"));

	setCrossOrigin();


	DEBUG_PRINT(F("Read file: "));
	String scopeDirectory = F("states");
	String filename =  scopeDirectory+F("/lastStat.jso");


	DEBUG_PRINTLN(filename);

	streamFileOnRest(filename);

}

void getMontlyValue(){
	DEBUG_PRINTLN(F("getMontlyValue"));

	setCrossOrigin();
	String scopeDirectory = F("monthly");

	if (httpRestServer.arg("month")== ""){     //Parameter not found
		httpRestServer.send(400, "text/html", "Missing required parameter!");
		DEBUG_PRINTLN(F("No parameter"));
	}else{     //Parameter found
		DEBUG_PRINT(F("Read file: "));
		String filename = scopeDirectory+'/'+httpRestServer.arg("month")+".jso";
		streamFileOnRest(filename);
	}
}

void postConfigFile() {
	DEBUG_PRINTLN(F("postConfigFile"));

	setCrossOrigin();

    String postBody = httpRestServer.arg("plain");
    DEBUG_PRINTLN(postBody);

	DynamicJsonDocument doc;
	DeserializationError error = deserializeJson(doc, postBody);
	if (error) {
		// if the file didn't open, print an error:
		DEBUG_PRINT(F("Error parsing JSON "));
		DEBUG_PRINTLN(error);

		String msg = error.c_str();

		httpRestServer.send(400, "text/html", "Error in parsin json body! <br>"+msg);

	}else{
		JsonObject postObj = doc.as<JsonObject>();

		DEBUG_PRINT(F("HTTP Method: "));
		DEBUG_PRINTLN(httpRestServer.method());

        if (httpRestServer.method() == HTTP_POST) {
            if ((postObj.containsKey("server"))) {

            	DEBUG_PRINT(F("Open config file..."));
            	fs::File configFile = SPIFFS.open("/mc/config.txt", "w");
            	if (!configFile) {
            	    DEBUG_PRINTLN("fail.");
            	    httpRestServer.send(304, "text/html", "Fail to store data, can't open file!");
            	}else{
            		DEBUG_PRINTLN("done.");
            		serializeJson(doc, configFile);
            		httpRestServer.send(201, "application/json", postBody);
//
//            		delay(10000);
//            		ESP.reset();
            	}
            }
            else {
            	httpRestServer.send(204, "text/html", "No data found, or incorrect!");
            }
        }
    }
}

void getConfigFile(){
	DEBUG_PRINTLN(F("getConfigFile"));

	setCrossOrigin();

	DEBUG_PRINT(F("Read file: "));
	if (SPIFFS.exists("/mc/config.txt")){
    	fs::File configFile = SPIFFS.open("/mc/config.txt", "r");
		if (configFile){
			if (configFile.available()){
				DEBUG_PRINT(F("Stream file..."));
				httpRestServer.streamFile(configFile, "application/json");
				DEBUG_PRINTLN(F("done."));
			}else{
				DEBUG_PRINTLN(F("File not found!"));
				httpRestServer.send(204, "text/html", "No content found!");
			}
			configFile.close();
		}
	}else{
		DEBUG_PRINTLN(F("File not found!"));
		httpRestServer.send(204, "text/html", "No content found!");
	}
}

void getInverterInfo(){
	DEBUG_PRINTLN(F("getInverterInfo"));

	setCrossOrigin();

	String scopeDirectory = F("static");

	DEBUG_PRINT(F("Read file: "));
	String filename = scopeDirectory+"/invinfo.jso";
	streamFileOnRest(filename);
}

void inverterDayWithProblem() {
	DEBUG_PRINTLN(F("inverterDayWithProblem"));

	setCrossOrigin();

	String scopeDirectory = F("alarms");

	myFileSDCart = SD.open(scopeDirectory);

	DynamicJsonDocument doc;
	JsonObject rootObj = doc.to<JsonObject>();

	JsonArray data = rootObj.createNestedArray("data");

	while (true) {

		File entry = myFileSDCart.openNextFile();
		if (!entry) {
			// no more files
			break;
		}
		DEBUG_PRINTLN(entry.name());
		if (entry.isDirectory()) {
			data.add(entry.name());
		}
		entry.close();
	}

	myFileSDCart.close();

	if (data.size() > 0) {
		DEBUG_PRINT(F("Stream file..."));
		String buf;
		serializeJson(rootObj, buf);
		httpRestServer.send(200, "application/json", buf);
		DEBUG_PRINTLN(F("done."));
	} else {
		DEBUG_PRINTLN(F("No content found!"));
		httpRestServer.send(204, "text/html", "No content found!");
	}
}

void getInverterDayState() {
	DEBUG_PRINTLN(F("getInverterDayState"));

	setCrossOrigin();

	String scopeDirectory = F("alarms");

	DEBUG_PRINT(F("Read file: "));

	if (httpRestServer.arg("day") == "") {     //Parameter not found
		httpRestServer.send(400, "text/html", "Missing required parameter!");
		DEBUG_PRINTLN(F("No parameter"));
	} else {     //Parameter found

		String filename;
		String parameter = httpRestServer.arg("day");
		filename = scopeDirectory + '/' + parameter + "/alarms.jso";

		DEBUG_PRINTLN(filename);

		streamFileOnRest(filename);
	}
}

void getInverterLastState(){
	DEBUG_PRINTLN(F("getInverterLastState"));

	setCrossOrigin();

	String scopeDirectory = F("alarms");

	DEBUG_PRINT(F("Read file: "));

	String filename;
	String parameter = httpRestServer.arg("day");
	filename = scopeDirectory+"/alarStat.jso";

	streamFileOnRest(filename);
}

void sendCrossOriginHeader(){
	DEBUG_PRINTLN(F("sendCORSHeader"));

	httpRestServer.sendHeader("access-control-allow-credentials", "false");

	setCrossOrigin();

	httpRestServer.send(204);
}

void restServerRouting() {
//	httpRestServer.header("Access-Control-Allow-Headers: Authorization, Content-Type");
//
    httpRestServer.on("/", HTTP_GET, []() {
    	httpRestServer.send(200, "text/html",
            "Welcome to the Inverter Centraline REST Web Server");
    });
    httpRestServer.on("/production", HTTP_GET, getProduction);
    httpRestServer.on("/productionTotal", HTTP_GET, getProductionTotal);
    httpRestServer.on("/monthly", HTTP_GET, getMontlyValue);

    httpRestServer.on("/config", HTTP_OPTIONS, sendCrossOriginHeader);
    httpRestServer.on("/config", HTTP_POST, postConfigFile);
    httpRestServer.on("/config", HTTP_GET, getConfigFile);

    httpRestServer.on("/inverterInfo", HTTP_GET, getInverterInfo);
    httpRestServer.on("/inverterState", HTTP_GET, getInverterLastState);
    httpRestServer.on("/inverterDayWithProblem", HTTP_GET, inverterDayWithProblem);
    httpRestServer.on("/inverterDayState", HTTP_GET, getInverterDayState);
}

void serverRouting() {
	  httpServer.onNotFound([]() {                              // If the client requests any URI
	    if (!handleFileRead(httpServer.uri()))                  // send it if it exists
	    	httpServer.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
	  });
}


void updateLocalTimeWithNTPCallback(){
	DEBUG_PRINT(F("Thread call (updateLocalTimeWithNTPCallback) --> "));
	DEBUG_PRINTLN(getEpochStringByParams(now()));

	if (!fixedTime){
		DEBUG_PRINTLN(F("Update NTP Time."));
		if (timeClient.forceUpdate()){
			unsigned long epoch = timeClient.getEpochTime();
			DEBUG_PRINTLN(epoch);

			DEBUG_PRINTLN(F("NTP Time retrieved."));
			adjustTime(epoch);
			fixedTime = true;
		}else{
			DEBUG_PRINTLN(F("NTP Time not retrieved."));

			DEBUG_PRINTLN(F("Get Inverter Time."));
			// Get DateTime from inverter
			Aurora::DataTimeDate timeDate = inverter.readTimeDate();

			if (timeDate.state.readState){
				DEBUG_PRINTLN(F("Inverter Time retrieved."));
				// Set correct time in Arduino Time librery
				adjustTime(timeDate.epochTime - timeOffset);
				fixedTime = true;
			}else{
				DEBUG_PRINTLN(F("Inverter Time not retrieved."));
				DEBUG_PRINTLN(F("DANGER, SYSTEM INCONCISTENCY"));
			}
		}

		DEBUG_PRINTLN(getEpochStringByParams(now()));
	}
}

float setPrecision(float val, byte precision){
	return ((int)(val*(10*precision)))/(float)(precision*10);
}
