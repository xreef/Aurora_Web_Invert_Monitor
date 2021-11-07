/*
 * This work is licensed under the
 * Creative Commons Attribution-NonCommercial-NoDerivs 3.0 Italy License.
 * To view a copy of this license, visit
 * http://creativecommons.org/licenses/by-nc-nd/3.0/it/
 * or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 *
 * Renzo Mischianti <www.mischianti.org>
 *
 * https://www.mischianti.org/category/project/web-monitoring-station-for-abb-aurora-inverter-ex-power-one-now-fimer/
 *
 */
#include "Arduino.h"
#include <Aurora.h>
#include <SD.h>
#define FS_NO_GLOBALS
#include "FS.h"
#include <Thread.h>

#include <ESP8266WiFi.h>

#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>        // Include the mDNS library

#include <ESP8266WebServer.h>
#include <ESP8266WebServerSecure.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <SPI.h>

#define SEND_EMAIL
//#define FORCE_SEND_ERROR_MESSAGE

#ifdef SEND_EMAIL
#include <EMailSender.h>
#endif

#define WRITE_SETTINGS_IF_EXIST

// SD
#define SD_WRONG_WRITE_NUMBER_ALERT 10
#define CS_PIN D8

#include <Timezone.h>    // https://github.com/JChristensen/Timezone

#define WS_ACTIVE

#ifdef WS_ACTIVE
#include <WebSocketsServer.h>
#endif

#define WS_PORT 8081
#ifdef WS_ACTIVE
	WebSocketsServer webSocket = WebSocketsServer(WS_PORT);

	void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
#endif

// HEAP file
#define CONFIG_FILE_HEAP 4096
#define BATTERY_STATE_HEAP 4096
#define ALARM_IN_A_DAY 4096
#define INVERTER_INFO_HEAP 4096
#define PRODUCTION_AND_CUMULATED_HEAP 8192+8192
#define CUMULATED_VALUES_HEAP 2048
#define CUMULATED_VALUES_TOT_HEAP 4096

// Battery voltage resistance
#define BAT_RES_VALUE_GND 20.0
#define BAT_RES_VALUE_VCC 10.0

// Uncomment to enable server ftp.
//#define SERVER_FTP
//#define SERVER_HTTPS
#define HARDWARE_SERIAL

#ifdef SERVER_FTP
#include <ESP8266FtpServer.h>
#endif

#ifdef SERVER_HTTPS
#include <ESP8266WebServerSecure.h>
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
#define REALTIME_INTERVAL 5

#define DAILY_INTERVAL 10
#define CUMULATIVE_INTERVAL 10
#define CUMULATIVE_TOTAL_INTERVAL 10
#define STATE_INTERVAL 10
#define STATE_STORE_INTERVAL 10
#define BATTERY_INTERVAL 20
#define STATIC_DATA_INTERVAL 6 * 60

// LED
#ifdef HARDWARE_SERIAL
	#define ERROR_PIN D3
#else
	#define ERROR_PIN D1
#endif

#define INVERTER_COMMUNICATION_CONTROL_PIN D0
// Inverte inizialization
#ifdef HARDWARE_SERIAL
Aurora inverter = Aurora(2, &Serial, INVERTER_COMMUNICATION_CONTROL_PIN);
#else
Aurora inverter = Aurora(2, D2, D3, INVERTER_COMMUNICATION_CONTROL_PIN);
#endif

void manageStaticDataCallback ();
void leggiProduzioneCallback();
void realtimeDataCallbak();
void leggiStatoInverterCallback();
void leggiStatoBatteriaCallback();
void updateLocalTimeWithNTPCallback();
float getBatteryVoltage();
Timezone getTimezoneData(const String code);
time_t getLocalTime(void);

bool isFileSaveOK = true;
bool saveJSonToAFile(DynamicJsonDocument *doc, String filename);
JsonObject getJSonFromFile(DynamicJsonDocument *doc, String filename, bool forceCleanONJsonError = true);

void errorLed(bool flag);

Thread RealtimeData = Thread();

Thread ManageStaticData = Thread();

Thread LeggiStatoInverter = Thread();
Thread LeggiStatoBatteria = Thread();

Thread LeggiProduzione = Thread();

#define HTTP_REST_PORT 8080
ESP8266WebServer httpRestServer(HTTP_REST_PORT);

#ifdef SERVER_HTTPS
static const char serverCert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDDjCCAfagAwIBAgIQNqQC531UdrtOiCvz8JHQTDANBgkqhkiG9w0BAQsFADAU
MRIwEAYDVQQDDAlsb2NhbGhvc3QwHhcNMTgwOTMwMjAwMDQ1WhcNMzgwOTMwMjAx
MDQ0WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwggEiMA0GCSqGSIb3DQEBAQUAA4IB
DwAwggEKAoIBAQCrY932OMWnQynPBqtOOm/4dR6aQaSXdeLAUcRjlDy+AEl8C0Xe
sBbwi1dpBatqyFpnYnoWkfduLOCJM4x8J1aoufMHO/XkNP19b8tViFpsmEiwtQnQ
uSKQy8tGtlehYHBEqSqv1E5OkgTAlgYtAEJcY1Ih8DG1vWhMJ73CJcR5fkSQ1OMR
6AUaJORxLyKk0pgmKHmXeue2jP9aiuf1gHJQE9BcIRLpX9jElEA9rTvH5f9yLieg
gP8r1x6BmB/gsyIQ0mnft6aJpyEaJ2XLL4ggnqpHOjrVUZ5AKzk2GQrnQlaZnnZ2
xiiY2R9rp6U6lDWivPgOpaQUZ1c8jGvaV/2hAgMBAAGjXDBaMA4GA1UdDwEB/wQE
AwIFoDAUBgNVHREEDTALgglsb2NhbGhvc3QwEwYDVR0lBAwwCgYIKwYBBQUHAwEw
HQYDVR0OBBYEFOtRfncRTORMIF/wndrep6DmLsZUMA0GCSqGSIb3DQEBCwUAA4IB
AQABwfZ54kRSMGSHXQkIkSJaI8DRByMCro+WCjZnWawfaqJOpihbVC5riZ1oQuyD
gNe8sMoqI2R/xKthUaYsrXb50yVlZ2QLiW9MzOHnd6DkivDNGVBMsu0pKG7nLEiZ
1wnoMxzIrkSYakHLBPhmMMeCl7ahRC29xRMlup46okxcH5xTirM27TUl3Oy8mZcY
UpT+QiqXdcWVFAsTKZfJRzCa1+Su480clKeUuFTYSnuyQ1CNCcnoj3RtR7p8TAzk
Ht6qiI8BQ8kcPiRcYCdWK5g2SBs5QpaAB3/S3IxN8CAu5+CiTkPjmdcCijUJ1VoL
/VxlMAt54NKKnN1MfCgVh4Gb
-----END CERTIFICATE-----
)EOF";

static const char serverKey[] PROGMEM =  R"EOF(
-----BEGIN RSA PRIVATE KEY-----
MIIEowIBAAKCAQEAq2Pd9jjFp0MpzwarTjpv+HUemkGkl3XiwFHEY5Q8vgBJfAtF
3rAW8ItXaQWrashaZ2J6FpH3bizgiTOMfCdWqLnzBzv15DT9fW/LVYhabJhIsLUJ
0LkikMvLRrZXoWBwRKkqr9ROTpIEwJYGLQBCXGNSIfAxtb1oTCe9wiXEeX5EkNTj
EegFGiTkcS8ipNKYJih5l3rntoz/Worn9YByUBPQXCES6V/YxJRAPa07x+X/ci4n
oID/K9cegZgf4LMiENJp37emiachGidlyy+IIJ6qRzo61VGeQCs5NhkK50JWmZ52
dsYomNkfa6elOpQ1orz4DqWkFGdXPIxr2lf9oQIDAQABAoIBAB/Y9NvV/NRx5Ij1
wktNDJVsnf0oCX+jhjkaeJXQa+EaiI0mQxt4OSsFmX6IcSvsgvAHGoyrHwE4EZkt
HQPNA4ti0kgb2jtHpXrzlSMVrUfUnF1JpsNEQ6oIVIOVSn9QPkxj6uy1VL/A3mUy
+37NN4eXZSGtUm9k/MZ59AbpobK5eAXmCxvgss4ZPDW3QGnXqVT8bu6RXaF7Ph64
KMUajkrGEyU9ff8MRzykb3kBit0nchOBvtETRg39L2MtXpnJoLzElZPr5ZbcwHk2
Xp4iuL8D6hoiA0r0jAeYNYZkTW96u4u0eysyUV4xbVuOmozFHCiE/zOa0uW7xjrp
hzPiE2kCgYEA1CnXyUCWiaIN29T//PfuZRiAHFWxG3RaiR7VprMZGsebx7aEVRsP
EPgw0PILAOvNQIWwJWClCoKIWSf0grgDp+QhKAtyk5AaEspr7fXCXf8IK6xh+tDz
OxlRbvQlYwWOFsXLZ3cu1vPuIAUEL4ez/EKeYQ1GC/W86cRBcM6r0mMCgYEAzs1c
r34Tv7Asrec+BxZKrPbq1gaLG4ivVoiZTpo+6MOIGI236tKZIWouy2GklknoE36N
rxz2KrPAFwg6V8N7q1CG/yNn5WJbasPLnb7ygauZ6yiyC4bqaXSKjrZJ2P4UYVOX
t66K/Qr7Ud5gvDfStXPFdun0dUEgfDZqovpS7SsCgYBi5yqft8s1V+UsAIxhCdcJ
K7W0/8FzMfduioBAmKbwU/Lr08q2vcl1OK3RCbRVdpcVJ/0oP3hQgO882KJkOZIC
txc5yrRb08ZD0jckE/fKx7OwYEjAmp14hGHw3kF7esB1Hzml/upH7Ciqpov/+DvQ
MeIRDhYER0cMlp+HDeENTwKBgEXSYlvCForewYcJjxC3fwj86PbQCMGIGaL+xbwb
KehOtDGOD62R4y+7+Qaj9fzkAR4r2UxpW9e5Dr74ATLGhoelzZ5w5tA0sCbQ6ntd
D+Wl+XbDK7HmoFhwh6N9eltwFZNytMPIg5bB0W6nxUNnGZY3+1CV1vqLvZsSiFh0
afE3AoGBAMDHbH2YCshMkDJ5RqZs3gVFQX5jXbUh1+j8/bJrWyW49TErGqWOFKxy
0hJbrLFftgnNuABWQw2Ve11rdboXGqIGhLNksRDFPHOYhgrzvw41DCllI7d3n5ij
sYyW+iXYsiV5NCrE9KvlKkjxA5FJMdb8qiq5uZ03PxxtNTyVhfy+
-----END RSA PRIVATE KEY-----
)EOF";

#define SECURE_HTTP_PORT 443
BearSSL::ESP8266WebServerSecure httpServer(SECURE_HTTP_PORT);
#else
#define HTTP_PORT 80
ESP8266WebServer httpServer(HTTP_PORT);
#endif

void restServerRouting();
void serverRouting();

WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0*60*60, 60*60*1000);

Thread UpdateLocalTimeWithNTP = Thread();

bool fixedTime = false;
bool sdStarted = false;
bool wifiConnected = false;

float setPrecision(float val, byte precision);

#define DSP_GRID_POWER_ALL_FILENAME									F("power.jso")		/* Global */
#define DSP_GRID_CURRENT_ALL_FILENAME								F("current.jso")
#define DSP_GRID_VOLTAGE_ALL_FILENAME								F("voltage.jso")

#define BATTERY_RT												F("bat_rt")		/* Global */
#define WIFI_SIGNAL_STRENGHT_RT									F("wifi_rt")		/* Global */
#define DSP_GRID_POWER_ALL_TYPE_RT								F("power_rt")		/* Global */

#define DSP_GRID_POWER_ALL_TYPE									F("power")		/* Global */
#define DSP_GRID_CURRENT_ALL_TYPE								F("current")
#define DSP_GRID_VOLTAGE_ALL_TYPE								F("voltage")

#define CUMULATED_ENERGY_TYPE									F("cumulated")
#define ERROR_TYPE												F("error")
#define ERROR_INVERTER_TYPE										F("error_inverter")

#ifdef SEND_EMAIL
EMailSender emailSend("", "");
#endif

#ifdef SERVER_FTP
FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial
#endif

int timeOffset = 0;

String codeDST = "GTM";

void setup() {
	pinMode(A0, INPUT);

	pinMode(ERROR_PIN, OUTPUT);
	digitalWrite(ERROR_PIN, HIGH);

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
	fs::File configFile = SPIFFS.open(F("/mc/config.txt"), "r");
	if (configFile) {
//		 while (configFile.available())
//		    {
//		      Serial1.write(configFile.read());
//		    }
//
		DEBUG_PRINTLN(F("done."));
		DynamicJsonDocument doc(CONFIG_FILE_HEAP);
		ArduinoJson::DeserializationError error = deserializeJson(doc, configFile);
		// close the file:
		configFile.close();

		if (error){
			// if the file didn't open, print an error:
			DEBUG_PRINT(F("Error parsing JSON "));
			DEBUG_PRINTLN(error.c_str());

		}else{
			JsonObject rootObj = doc.as<JsonObject>();
			JsonObject preferences = rootObj[F("preferences")];
			bool isGTM = preferences.containsKey(F("GTM"));
			if (isGTM){
				JsonObject GTM = preferences[F("GTM")];
				bool isValue = GTM.containsKey(F("value"));
				if (isValue){
					int value = GTM[F("value")];

					DEBUG_PRINT(F("Impostazione GTM+"))
					DEBUG_PRINTLN(value)

//					timeClient.setTimeOffset(value*60*60);
					timeOffset = value*60*60;
				}
			}

			bool isDST = preferences.containsKey(F("DST"));
			if (isDST){
				JsonObject DST = preferences[F("DST")];
				bool isCode = DST.containsKey(F("code"));
				if (isCode){
					const String code = DST[F("code")];
//					const String desc = DST[F("description")];

					codeDST = code;
					DEBUG_PRINT(F("Impostazione DST "))
					DEBUG_PRINTLN(code)
//					DEBUG_PRINT(F("Description DST "))
//					DEBUG_PRINTLN(desc)

//					timeClient.setTimeOffset(value*60*60);
//					timeOffset = value*60*60;
				}
			}else{
				codeDST = "GTM";
			}

			JsonObject serverConfig = rootObj[F("server")];
			bool isStatic = serverConfig[F("isStatic")];
			if (isStatic==true){
				const char* address = serverConfig[F("address")];
				const char* gatway = serverConfig[F("gatway")];
				const char* netMask = serverConfig[F("netMask")];

				const char* dns1 = serverConfig[F("dns1")];
				const char* dns2 = serverConfig[F("dns2")];

				const char* _hostname = serverConfig[F("hostname")];
				//start-block2
				IPAddress _ip;
				bool parseSuccess;
				parseSuccess = _ip.fromString(address);
				if (parseSuccess) {
					DEBUG_PRINTLN(F("Address correctly parsed!"));
				}

				IPAddress _gw;
				parseSuccess = _gw.fromString(gatway);
				if (parseSuccess) {
					DEBUG_PRINTLN(F("Gatway correctly parsed!"));
				}

				IPAddress _sn;
				parseSuccess = _sn.fromString(netMask);
				if (parseSuccess) {
					DEBUG_PRINTLN(F("Subnet correctly parsed!"));
				}

				IPAddress _dns1;
				IPAddress _dns2;
				bool isDNS = false;
				if (dns1 && sizeof(_dns1) > 7 && dns2 && sizeof(_dns2) > 7 ){
					parseSuccess = _dns1.fromString(dns1);
					if (parseSuccess) {
						DEBUG_PRINTLN(F("DNS 1 correctly parsed!"));
						isDNS = true;
					}

					parseSuccess = _dns2.fromString(dns2);
					if (parseSuccess) {
						DEBUG_PRINTLN(F("DNS 2 correctly parsed!"));
					}
					//end-block2
				}

				DEBUG_PRINT(F("Set static data..."));
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
				DEBUG_PRINTLN(F("done."));
			}
		}
	}else{
		DEBUG_PRINTLN(F("fail."));
	}

	//reset saved settings
//	wifiManager.resetSettings();


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

    DEBUG_PRINT(F("IP --> "));
    DEBUG_PRINTLN(WiFi.localIP());
    DEBUG_PRINT(F("GW --> "));
    DEBUG_PRINTLN(WiFi.gatewayIP());
    DEBUG_PRINT(F("SM --> "));
    DEBUG_PRINTLN(WiFi.subnetMask());

    DEBUG_PRINT(F("DNS 1 --> "));
    DEBUG_PRINTLN(WiFi.dnsIP(0));

    DEBUG_PRINT(F("DNS 2 --> "));
    DEBUG_PRINTLN(WiFi.dnsIP(1));


#ifndef WRITE_SETTINGS_IF_EXIST
    if (!SPIFFS.exists(F("/settings.json"))){
#endif
		DEBUG_PRINT(F("Recreate settings file..."));
		fs::File settingsFile = SPIFFS.open(F("/settings.json"), "w");
		if (!settingsFile) {
			DEBUG_PRINTLN(F("fail."));
		}else{

			DEBUG_PRINTLN(F("done."));
			DynamicJsonDocument doc(2048);
			JsonObject postObj = doc.to<JsonObject>();
//			postObj[F("localIP")] = WiFi.localIP().toString();
			postObj[F("localRestPort")] = HTTP_REST_PORT;
			postObj[F("localWSPort")] = WS_PORT;

			String buf;
			serializeJson(postObj, buf);

			settingsFile.print(F("var settings = "));
			settingsFile.print(buf);
			settingsFile.print(";");

			settingsFile.close();

	//		serializeJson(doc, settingsFile);
			DEBUG_PRINTLN(F("success."));
		}

#ifndef WRITE_SETTINGS_IF_EXIST
    }
#endif

	// Start inverter serial
	DEBUG_PRINT(F("Initializing Inverter serial..."));
	inverter.begin();
	DEBUG_PRINTLN(F("initialization done."));

	// Start inizialization of SD cart
	DEBUG_PRINT(F("Initializing SD card..."));

	  // Initialize SD library
	  while (!SD.begin(CS_PIN)) {
		Serial.println(F("Failed to initialize SD library"));
		delay(1000);
		sdStarted = false;
	  }
	  sdStarted = true;

//	if (!SD.begin(CS_PIN, SPI_HALF_SPEED)) {
//		DEBUG_PRINTLN(F("initialization failed!"));
//		sdStarted = false;
//		// return to stop all
//		return;
//	}else{
//		sdStarted = true;
//
//	}
	DEBUG_PRINTLN(F("Inizialization done."));

	ManageStaticData.onRun(manageStaticDataCallback);
	ManageStaticData.setInterval(STATIC_DATA_INTERVAL * 60 * 1000);

	// Start thread, and set it every 1 minutes
	LeggiStatoInverter.onRun(leggiStatoInverterCallback);
	LeggiStatoInverter.setInterval(STATE_INTERVAL * 60 * 1000);

	// Start thread, and set it every 60
	LeggiStatoBatteria.onRun(leggiStatoBatteriaCallback);
	LeggiStatoBatteria.setInterval(BATTERY_INTERVAL * 60 * 1000);

	// Start thread, and set it every 1 minutes
	LeggiProduzione.onRun(leggiProduzioneCallback);
	LeggiProduzione.setInterval(60 * 1000);

	RealtimeData.onRun(realtimeDataCallbak);
	RealtimeData.setInterval(1000 * REALTIME_INTERVAL);

	int timeToRetry = 10;
	while ( WiFi.status() != WL_CONNECTED && timeToRetry>0 ) {
	    delay ( 500 );
	    DEBUG_PRINT ( "." );
	    timeToRetry--;
	}
	wifiConnected = true;

//#ifndef SERVER_FTP

	timeClient.begin();
	updateLocalTimeWithNTPCallback();

	UpdateLocalTimeWithNTP.onRun(updateLocalTimeWithNTPCallback);
	UpdateLocalTimeWithNTP.setInterval(60*60 * 1000);

	restServerRouting();
	httpRestServer.begin();
    DEBUG_PRINTLN(F("HTTP REST Server Started"));

#ifdef SERVER_HTTPS
	httpServer.setRSACert(new BearSSLX509List(serverCert), new BearSSLPrivateKey(serverKey));
#endif
	serverRouting();
	httpServer.begin();
#ifdef SERVER_HTTPS
    DEBUG_PRINTLN(F("HTTPS Server Started"));
#else
    DEBUG_PRINTLN(F("HTTP Server Started"));
#endif

#ifdef SERVER_FTP
//    SPIFFS.format();
    ftpSrv.begin(F("aurora"),F("aurora"));    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
    DEBUG_PRINTLN(F("FTP Server Started"));
#endif

    if (!MDNS.begin(hostname)) {             // Start the mDNS responder for esp8266.local
    	DEBUG_PRINTLN(F("Error setting up mDNS responder!"));
    }
    DEBUG_PRINT(hostname);
    DEBUG_PRINTLN(F(" --> mDNS responder started"));

    DEBUG_PRINT(F("ERROR --> "));
    DEBUG_PRINTLN(!(fixedTime && sdStarted&& wifiConnected));

#ifdef WS_ACTIVE
	webSocket.begin();
	webSocket.onEvent(webSocketEvent);
	DEBUG_PRINTLN(F("WS Server Started"));
#endif

	errorLed(!(fixedTime && sdStarted&& wifiConnected && isFileSaveOK));

//    digitalWrite(ERROR_PIN, !(fixedTime && sdStarted&& wifiConnected && isFileSaveOK));

	if (fixedTime && sdStarted){
		DEBUG_PRINTLN(F("FIRST LOAD..."))
		leggiStatoInverterCallback();
		leggiStatoBatteriaCallback();
		manageStaticDataCallback();
		leggiProduzioneCallback();
	}
}

void loop() {
//#ifndef SERVER_FTP
	// Activate thread
	if (fixedTime && LeggiProduzione.shouldRun()) {
		LeggiProduzione.run();
	}

	if (fixedTime && RealtimeData.shouldRun()) {
		RealtimeData.run();
	}

	if (fixedTime && sdStarted && LeggiStatoInverter.shouldRun()) {
		LeggiStatoInverter.run();
	}

	if (fixedTime && sdStarted && LeggiStatoBatteria.shouldRun()) {
		LeggiStatoBatteria.run();
	}

	if (fixedTime && sdStarted && ManageStaticData.shouldRun()) {
		ManageStaticData.run();
	}
	if (UpdateLocalTimeWithNTP.shouldRun()) {
		UpdateLocalTimeWithNTP.run();
	}

#ifdef WS_ACTIVE
	webSocket.loop();
#endif

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

void leggiStatoBatteriaCallback() {
	DEBUG_PRINT(F("Thread call (leggiStatoBatteriaCallback) --> "));
	DEBUG_PRINT(getEpochStringByParams(getLocalTime()));
	DEBUG_PRINT(F(" MEM --> "));
	DEBUG_PRINTLN(ESP.getFreeHeap())

	float bv = getBatteryVoltage();
	DEBUG_PRINT(F(" BATTERY --> "));
	DEBUG_PRINTLN(bv);
	DEBUG_PRINTLN(bv>2);
	DEBUG_PRINTLN(bv>1);
	if (bv>1) {
		String scopeDirectory = F("battery");
		if (!SD.exists(scopeDirectory)) {
			SD.mkdir(scopeDirectory);
		}
		String filename = scopeDirectory +"/"+ getEpochStringByParams(getLocalTime(), (char*) "%Y%m%d") + F(".jso");

		DynamicJsonDocument doc(BATTERY_STATE_HEAP);
		JsonObject obj;

		obj = getJSonFromFile(&doc, filename);

		obj[F("lastUpdate")] = getEpochStringByParams(getLocalTime());

		JsonObject data;
		if (!obj.containsKey(F("data"))) {
			data = obj.createNestedObject(F("data"));
		} else {
			data = obj[F("data")];
		}

		data[getEpochStringByParams(getLocalTime(),(char*) "%H%M")]=bv;

		DEBUG_PRINTLN(F("done."));

		isFileSaveOK = saveJSonToAFile(&doc, filename);

	}

}
int lastAlarm = 0;

bool saveInverterStats(String scopeDirectory, Aurora::DataState dataState){
	if (!SD.exists(scopeDirectory)) {
		SD.mkdir(scopeDirectory);
	}

	String filename = scopeDirectory + F("/alarStat.jso");

	DynamicJsonDocument doc(2048);
	JsonObject rootObj = doc.to<JsonObject>();

	rootObj[F("lastUpdate")] = getEpochStringByParams(getLocalTime());

	rootObj[F("alarmStateParam")] = dataState.alarmState;
	rootObj[F("alarmState")] = dataState.getAlarmState();

	rootObj[F("channel1StateParam")] = dataState.channel1State;
	rootObj[F("channel1State")] = dataState.getDcDcChannel1State();

	rootObj[F("channel2StateParam")] = dataState.channel2State;
	rootObj[F("channel2State")] = dataState.getDcDcChannel2State();

	rootObj[F("inverterStateParam")] = dataState.inverterState;
	rootObj[F("inverterState")] = dataState.getInverterState();

	DEBUG_PRINTLN(F("done."));

	isFileSaveOK = saveJSonToAFile(&doc, filename);

	return isFileSaveOK;
}

struct LastDataState {
	byte asp;
	byte c1sp;
	byte c2sp;
	byte isp;

	bool variationFromPrevious = false;
	bool inverterProblem = false;
	bool firstElement = true;
	bool needNotify = false;
};

LastDataState manageLastDataState(String scopeDirectory, Aurora::DataState dataState){
	LastDataState lds;

	String dayDirectory = getEpochStringByParams(getLocalTime(), (char*) "%Y%m%d");

	String filenameAL = scopeDirectory + '/' + dayDirectory + F("/alarms.jso");

	DynamicJsonDocument docAS(ALARM_IN_A_DAY);
	JsonObject obj;

	obj = getJSonFromFile(&docAS, filenameAL);

	obj[F("lastUpdate")] = getEpochStringByParams(getLocalTime());

	JsonArray data;
	if (!obj.containsKey(F("data"))) {
		data = obj.createNestedArray(F("data"));
	} else {
		data = obj[F("data")];
	}

	bool inverterProblem = dataState.alarmState > 0 || (dataState.inverterState != 2 && dataState.inverterState != 1);

	lds.inverterProblem = inverterProblem;

	bool firstElement = data.size() == 0;

	lds.firstElement = firstElement;

	if (inverterProblem || !firstElement) {
		JsonObject lastData;
		if (data.size() > 0) {
			lastData = data[data.size() - 1];

			lds.asp = lastData[F("asp")];
			lds.c1sp = lastData[F("c1sp")];
			lds.c2sp = lastData[F("c2sp")];
			lds.isp = lastData[F("isp")];

		}

		bool variationFromPrevious = (data.size() > 0
				&& (lastData[F("asp")] != dataState.alarmState
						|| lastData[F("c1sp")] != dataState.channel1State
						|| lastData[F("c2sp")] != dataState.channel2State
						|| lastData[F("isp")] != dataState.inverterState));

		lds.variationFromPrevious = variationFromPrevious;

		byte ldState = lastData[F("isp")];

		DEBUG_PRINT(F("Last data state --> "));
		DEBUG_PRINTLN(ldState);

		DEBUG_PRINT(F("Data state --> "));
		DEBUG_PRINTLN(dataState.inverterState);

		DEBUG_PRINT(F("Last data vs data state is different --> "));
		DEBUG_PRINTLN(lastData[F("isp")] != dataState.inverterState);


		DEBUG_PRINT(F("Inverter problem --> "));
		DEBUG_PRINTLN(inverterProblem);

		DEBUG_PRINT(F("firstElement --> "));
		DEBUG_PRINTLN(firstElement);

		DEBUG_PRINT(F("Variation From Previous --> "));
		DEBUG_PRINTLN(variationFromPrevious);

		lds.needNotify = false;

		if ((inverterProblem && firstElement)
				|| (!firstElement && variationFromPrevious)) {

			lds.needNotify = true;

			if (docAS.memoryUsage()>(ALARM_IN_A_DAY-1024)){
				DEBUG_PRINTLN(F("Too much data in a day, removed 4 elements"));
				data.remove(0);
				data.remove(1);
				data.remove(2);
				data.remove(3);
//				for (int i = 0; i<4; i++){
//					data.remove[(const char*)i];
//				}
			}

			JsonObject objArrayData = data.createNestedObject();

			objArrayData[F("h")] = getEpochStringByParams(getLocalTime(),
					(char*) "%H%M");

			objArrayData[F("asp")] = dataState.alarmState;
			//			objArrayData[F("as")] = dataState.getAlarmState();

			objArrayData[F("c1sp")] = dataState.channel1State;
			//			objArrayData[F("c1s")] = dataState.getDcDcChannel1State();

			objArrayData[F("c2sp")] = dataState.channel2State;
			//			objArrayData[F("c2s")] = dataState.getDcDcChannel2State();

			objArrayData[F("isp")] = dataState.inverterState;
			//			objArrayData["is"] = dataState.getInverterState();

			DEBUG_PRINTLN(F("Store alarms --> "));
			//				serializeJson(doc, Serial);
			DEBUG_PRINT(docAS.memoryUsage());
			DEBUG_PRINTLN();

			if (!SD.exists(scopeDirectory + '/' + dayDirectory)) {
				SD.mkdir(scopeDirectory + '/' + dayDirectory);
			}

			isFileSaveOK = saveJSonToAFile(&docAS, filenameAL);
		}
	}
	return lds;
}

#ifdef WS_ACTIVE
void sendWSMessageAlarm(Aurora::DataState dataState, bool inverterProblem, bool variationFromPrevious){
	if (inverterProblem || variationFromPrevious){
		DEBUG_PRINT(F(" MEM Condition --> "));
		DEBUG_PRINTLN(ESP.getFreeHeap())

		DynamicJsonDocument docws(512);
		JsonObject objws = docws.to<JsonObject>();
		String dateFormatted = getEpochStringByParams(getLocalTime());
		objws[F("type")] = ERROR_INVERTER_TYPE;
		objws[F("date")] = dateFormatted;

		JsonObject objValue = objws.createNestedObject(F("value"));

		objValue[F("inverterProblem")] = inverterProblem;

		objValue[F("asp")] = dataState.alarmState;
		objValue[F("c1sp")] = dataState.channel1State;
		objValue[F("c2sp")] = dataState.channel2State;
		objValue[F("isp")] = dataState.inverterState;

		objValue[F("alarm")] = dataState.getAlarmState();
		objValue[F("ch1state")] = dataState.getDcDcChannel1State();
		objValue[F("ch2state")] = dataState.getDcDcChannel2State();
		objValue[F("state")] = dataState.getInverterState();

		String buf;
		serializeJson(objws, buf);

		webSocket.broadcastTXT(buf);
	}
}
#endif

void leggiStatoInverterCallback() {
	DEBUG_PRINT(F("Thread call (LeggiStatoInverterCallback) --> "));
	DEBUG_PRINT(getEpochStringByParams(getLocalTime()));
	DEBUG_PRINT(F(" MEM --> "));
	DEBUG_PRINTLN(ESP.getFreeHeap())

	Aurora::DataState dataState = inverter.readState();
#ifdef FORCE_SEND_ERROR_MESSAGE
		if (lastAlarm==0){
			dataState.alarmState = 2;
			lastAlarm = 2;
		}else{
			dataState.alarmState = 0;
			lastAlarm = 0;
		}
		dataState.state.readState = 1;
#endif

	DEBUG_PRINT(F("Read state --> "));
	DEBUG_PRINTLN(dataState.state.readState);

	if (dataState.state.readState == 1) {
		DEBUG_PRINTLN(F("done."));

		DEBUG_PRINT(F("Create json..."));

		String scopeDirectory = F("alarms");

		saveInverterStats(scopeDirectory, dataState);

		LastDataState lds = manageLastDataState(scopeDirectory, dataState);

		sendWSMessageAlarm(dataState, lds.inverterProblem, lds.variationFromPrevious);
#ifdef SEND_EMAIL

				DEBUG_PRINT(F("MEM "));
				DEBUG_PRINTLN(ESP.getFreeHeap());

				if (lds.needNotify){
					DEBUG_PRINT(F("Open config file..."));
					fs::File configFile = SPIFFS.open(F("/mc/config.txt"), "r");
					if (configFile) {
						DEBUG_PRINTLN(F("done."));
						DynamicJsonDocument doc(CONFIG_FILE_HEAP);
						DeserializationError error = deserializeJson(doc, configFile);
						if (error) {
							// if the file didn't open, print an error:
							DEBUG_PRINT(F("Error parsing JSON "));
							DEBUG_PRINTLN(error.c_str());
						}

						// close the file:
						configFile.close();

						DEBUG_PRINT(F("MEM "));
						DEBUG_PRINTLN(ESP.getFreeHeap());

						JsonObject rootObj = doc.as<JsonObject>();

						DEBUG_PRINT(F("After read config check serverSMTP and emailNotification "));
						DEBUG_PRINTLN(rootObj.containsKey(F("serverSMTP")) && rootObj.containsKey(F("emailNotification")));

						if (rootObj.containsKey(F("serverSMTP")) && rootObj.containsKey(F("emailNotification"))){
	//						JsonObject serverConfig = rootObj["server"];
							JsonObject serverSMTP = rootObj[F("serverSMTP")];
							JsonObject emailNotification = rootObj[F("emailNotification")];

							bool isNotificationEnabled = (emailNotification.containsKey(F("isNotificationEnabled")))?emailNotification[F("isNotificationEnabled")]:false;

							DEBUG_PRINT(F("isNotificationEnabled "));
							DEBUG_PRINTLN(isNotificationEnabled);

							if (isNotificationEnabled){
								const char* serverSMTPAddr = serverSMTP[F("server")];
								emailSend.setSMTPServer(serverSMTPAddr);
								uint16_t portSMTP = serverSMTP[F("port")];
								emailSend.setSMTPPort(portSMTP);
								const char* loginSMTP = serverSMTP[F("login")];
								emailSend.setEMailLogin(loginSMTP);
								const char* passwordSMTP = serverSMTP[F("password")];
								emailSend.setEMailPassword(passwordSMTP);
								const char* fromSMTP = serverSMTP[F("from")];
								emailSend.setEMailFrom(fromSMTP);

								DEBUG_PRINT(F("server "));
								DEBUG_PRINTLN(serverSMTPAddr);
								DEBUG_PRINT(F("port "));
								DEBUG_PRINTLN(portSMTP);
								DEBUG_PRINT(F("login "));
								DEBUG_PRINTLN(loginSMTP);
								DEBUG_PRINT(F("password "));
								DEBUG_PRINTLN(passwordSMTP);
								DEBUG_PRINT(F("from "));
								DEBUG_PRINTLN(fromSMTP);

								EMailSender::EMailMessage message;
								const String sub = emailNotification[F("subject")];
								message.subject = sub;

								JsonArray emailList = emailNotification[F("emailList")];

								DEBUG_PRINT(F("Email list "));

								for (uint8_t i=0; i<emailList.size(); i++){
									JsonObject emailElem = emailList[i];

	//								byte asp = lastData[F("asp")];
	//								byte c1sp = lastData[F("c1sp")];
	//								byte c2sp = lastData[F("c2sp")];
	//								byte isp = lastData[F("isp")];

									const String alarm = emailElem[F("alarm")];
									const String ch1 = emailElem[F("ch1")];
									const String ch2 = emailElem[F("ch2")];
									const String state = emailElem[F("state")];

									DEBUG_PRINT(F("State value "));
									DEBUG_PRINTLN(state);

									DEBUG_PRINT(F("State value on_problem comparison "));
									DEBUG_PRINTLN(state==F("on_problem"));

									DEBUG_PRINT(F("Alarm value "));
									DEBUG_PRINTLN(alarm);

									DEBUG_PRINT(F("Alarm all comparison "));
									DEBUG_PRINTLN(alarm==F("all"));

									bool allNotification = (
											(alarm==F("all") && lds.asp != dataState.alarmState)
											||
											(ch1==F("all") && lds.c1sp != dataState.channel1State)
											||
											(ch2==F("all") && lds.c2sp != dataState.channel2State)
											||
											(state==F("all") && lds.isp != dataState.inverterState)
									);

									bool onProblem = (
											(alarm==F("on_problem") && dataState.alarmState > 0)
											||
											(ch1==F("on_problem") && dataState.channel1State != 2)
											||
											(ch2==F("on_problem") && dataState.channel1State != 2)
											||
											(state==F("on_problem") && (dataState.inverterState != 2 && dataState.inverterState != 1))
									);

									DEBUG_PRINT(F("Check allNotification "));
									DEBUG_PRINTLN(allNotification);

									DEBUG_PRINT(F("Check onProblem "));
									DEBUG_PRINTLN(onProblem);

									DEBUG_PRINT(F("MEM "));
									DEBUG_PRINTLN(ESP.getFreeHeap());


									if (
											allNotification
											||
											onProblem
									){
										const String mp = emailNotification[F("messageProblem")];
										const String mnp = emailNotification[F("messageNoProblem")];
										message.message = ((lds.inverterProblem)?mp:mnp)+
														F("<br>Alarm: ")+dataState.getAlarmState()+
														F("<br>CH1: ")+dataState.getDcDcChannel1State() +
														F("<br>CH2: ")+dataState.getDcDcChannel2State()+
														F("<br>Stato: ")+dataState.getInverterState();

										EMailSender::Response resp = emailSend.send(emailElem[F("email")], message);

										DEBUG_PRINTLN(F("Sending status: "));
										const String em = emailElem[F("email")];
										DEBUG_PRINTLN(em);
										DEBUG_PRINTLN(resp.status);
										DEBUG_PRINTLN(resp.code);
										DEBUG_PRINTLN(resp.desc);
									}


								}
							}
						}
					}else{
						DEBUG_PRINTLN(F("fail."));
					}
				}
#endif
//			}
//
//		}
//		}
	}

}

void manageStaticDataCallback () {
	DEBUG_PRINT(F("Thread call (manageStaticDataCallback) --> "));
	DEBUG_PRINT(getEpochStringByParams(getLocalTime()));
	DEBUG_PRINT(F(" MEM --> "));
	DEBUG_PRINTLN(ESP.getFreeHeap())

	DEBUG_PRINT(F("Data version read... "));
	Aurora::DataVersion dataVersion = inverter.readVersion();
	DEBUG_PRINTLN(dataVersion.state.readState);

	DEBUG_PRINT(F("Firmware release read... "));
	Aurora::DataFirmwareRelease firmwareRelease = inverter.readFirmwareRelease();
	DEBUG_PRINTLN(firmwareRelease.state.readState);

	DEBUG_PRINT(F("System SN read... "));
	Aurora::DataSystemSerialNumber systemSN = inverter.readSystemSerialNumber();
	DEBUG_PRINTLN(systemSN.readState);

	DEBUG_PRINT(F("Manufactoru Week Year read... "));
	Aurora::DataManufacturingWeekYear manufactoryWeekYear = inverter.readManufacturingWeekYear();
	DEBUG_PRINTLN(manufactoryWeekYear.state.readState);

	DEBUG_PRINT(F("systemPN read... "));
	Aurora::DataSystemPN systemPN = inverter.readSystemPN();
	DEBUG_PRINTLN(systemPN.readState);

	DEBUG_PRINT(F("configStatus read... "));
	Aurora::DataConfigStatus configStatus = inverter.readConfig();
	DEBUG_PRINTLN(configStatus.state.readState);

	if (dataVersion.state.readState == 1){
    	DEBUG_PRINTLN(F("done."));

    	DEBUG_PRINT(F("Create json..."));

		String scopeDirectory = F("static");
		if (!SD.exists(scopeDirectory)){
			SD.mkdir(scopeDirectory);
		}

		String filename = scopeDirectory+ F("/invinfo.jso");

		DynamicJsonDocument doc(INVERTER_INFO_HEAP);
		JsonObject rootObj = doc.to<JsonObject>();

		rootObj[F("lastUpdate")] = getEpochStringByParams(getLocalTime());

		rootObj[F("modelNameParam")] = dataVersion.par1;
		rootObj[F("modelName")] = dataVersion.getModelName().name;
		rootObj[F("modelNameIndoorOutdoorType")] = dataVersion.getIndoorOutdoorAndType();

		rootObj[F("gridStandardParam")] = dataVersion.par2;
		rootObj[F("gridStandard")] = dataVersion.getGridStandard();

		rootObj[F("trasformerLessParam")] = dataVersion.par3;
		rootObj[F("trasformerLess")] = dataVersion.getTrafoOrNonTrafo();

		rootObj[F("windOrPVParam")] = dataVersion.par4;
		rootObj[F("windOrPV")] = dataVersion.getWindOrPV();

		rootObj[F("firmwareRelease")] = firmwareRelease.release;
		rootObj[F("systemSN")] = systemSN.SerialNumber;

		rootObj[F("systemPN")] = systemPN.PN;

		JsonObject mWY = rootObj.createNestedObject(F("manufactory"));
		mWY[F("Year")] = manufactoryWeekYear.Year;
		mWY[F("Week")] = manufactoryWeekYear.Week;

		JsonObject cs = rootObj.createNestedObject(F("configStatus"));
		cs[F("code")] = configStatus.configStatus;
		cs[F("desc")] = configStatus.getConfigStatus();

		DEBUG_PRINTLN(F("done."));

		isFileSaveOK = saveJSonToAFile(&doc, filename);
	}

}

void cumulatedEnergyDaily(tm nowDt){
	String scopeDirectory = F("monthly");
	// Save cumulative data
	if (nowDt.tm_min % CUMULATIVE_INTERVAL == 0) {
		Aurora::DataCumulatedEnergy dce = inverter.readCumulatedEnergy(
		CUMULATED_DAILY_ENERGY);
		DEBUG_PRINT(F("Read state --> "));
		DEBUG_PRINTLN(dce.state.readState);

		if (dce.state.readState == 1) {
			unsigned long energy = dce.energy;

			if (!SD.exists(scopeDirectory)) {
				SD.mkdir(scopeDirectory);
			}

			Aurora::DataDSP dsp = inverter.readDSP(DSP_POWER_PEAK_TODAY_ALL);
			float powerPeak = dsp.value;

			if (energy && energy > 0) {
				String filename = scopeDirectory + "/"
						+ getEpochStringByParams(getLocalTime(), (char*) "%Y%m")
						+ F(".jso");
				String tagName = getEpochStringByParams(getLocalTime(), (char*) "%d");

				DynamicJsonDocument doc(PRODUCTION_AND_CUMULATED_HEAP);
				JsonObject obj;

				obj = getJSonFromFile(&doc, filename);

				obj[F("lastUpdate")] = getEpochStringByParams(getLocalTime());

				JsonObject series;
				if (!obj.containsKey(F("series"))) {
					series = obj.createNestedObject(F("series"));
				} else {
					series = obj[F("series")];
				}

				JsonObject dayData = series.createNestedObject(tagName);
				dayData[F("pow")] = energy;
				dayData[F("powPeak")] = setPrecision(powerPeak, 1);

				DEBUG_PRINT(F("Store cumulated energy --> "));
//				serializeJson(doc, Serial);
				DEBUG_PRINT(doc.memoryUsage());
				DEBUG_PRINTLN();

				isFileSaveOK = saveJSonToAFile(&doc, filename);
			}

		}
	}
}

#ifdef WS_ACTIVE
void sendWSCumulatedEnergy(	unsigned long energyLifetime,
							unsigned long energyYearly,
							unsigned long energyMonthly,
							unsigned long energyWeekly,
							unsigned long energyDaily){
					DynamicJsonDocument docws(512);
					JsonObject objws = docws.to<JsonObject>();
					String dateFormatted = getEpochStringByParams(getLocalTime());
					objws["type"] = CUMULATED_ENERGY_TYPE;
					objws["date"] = dateFormatted;

					JsonObject objValue = objws.createNestedObject("value");

					objValue[F("lastUpdate")] = getEpochStringByParams(getLocalTime());

					if (energyLifetime>0){
						objValue[F("energyLifetime")] = energyLifetime;
					}
					if (energyYearly>0){
						objValue[F("energyYearly")] = energyYearly;
					}
					if (energyMonthly>0){
						objValue[F("energyMonthly")] = energyMonthly;
					}
					if (energyWeekly>0){
						objValue[F("energyWeekly")] = energyWeekly;
					}
					if (energyDaily>0){
						objValue[F("energyDaily")] = energyDaily;
					}

					String buf;
					serializeJson(objws, buf);

					webSocket.broadcastTXT(buf);
}
#endif
void readTotals(tm nowDt){
	String scopeDirectory = F("states");
	// Save cumulative data
	if (nowDt.tm_min % CUMULATIVE_TOTAL_INTERVAL == 0) {

		DEBUG_PRINTLN(F("Get all totals."));
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


			if (!SD.exists(scopeDirectory)) {
				SD.mkdir(scopeDirectory);
			}
			if (dce.state.readState == 1 && energyLifetime) {
#ifdef WS_ACTIVE
				sendWSCumulatedEnergy(energyLifetime, energyYearly, energyMonthly, energyWeekly, energyDaily);
#endif

				String filename = scopeDirectory + F("/lastStat.jso");
				String tagName = getEpochStringByParams(getLocalTime(), (char*) "%d");

				DynamicJsonDocument doc(CUMULATED_VALUES_HEAP);
				JsonObject obj = doc.to<JsonObject>();;

				obj[F("lastUpdate")] = getEpochStringByParams(getLocalTime());

				if (energyLifetime>0){
					obj[F("energyLifetime")] = energyLifetime;
				}
				if (energyYearly>0){
					obj[F("energyYearly")] = energyYearly;
				}
				if (energyMonthly>0){
					obj[F("energyMonthly")] = energyMonthly;
				}
				if (energyWeekly>0){
					obj[F("energyWeekly")] = energyWeekly;
				}
				if (energyDaily>0){
					obj[F("energyDaily")] = energyDaily;
				}

				obj[F("W")] = getEpochStringByParams(getLocalTime(), (char*) "%Y%W");
				obj[F("M")] = getEpochStringByParams(getLocalTime(), (char*) "%Y%m");
				obj[F("Y")] = getEpochStringByParams(getLocalTime(), (char*) "%Y");

				DEBUG_PRINT(F("Store energyLifetime energy --> "));
				//				serializeJson(doc, Serial);
				DEBUG_PRINT(doc.memoryUsage());
				DEBUG_PRINTLN();

				isFileSaveOK = saveJSonToAFile(&doc, filename);

				{
					DynamicJsonDocument docW(CUMULATED_VALUES_TOT_HEAP);
					JsonObject objW;

					String dir = scopeDirectory + F("/weeks");
					if (!SD.exists(dir)) {
						SD.mkdir(dir);
					}

					String filenamew = dir +'/'+getEpochStringByParams(getLocalTime(), (char*) "%Y")+ F(".jso");

					objW = getJSonFromFile(&docW, filenamew);
					objW[getEpochStringByParams(getLocalTime(), (char*) "%Y%W")] = energyWeekly;

					isFileSaveOK = saveJSonToAFile(&docW, filenamew);
				}
				{
					DynamicJsonDocument docM(CUMULATED_VALUES_TOT_HEAP);
					JsonObject objM;

					String dir = scopeDirectory + F("/months");
					if (!SD.exists(dir)) {
						SD.mkdir(dir);
					}

					String filenamem = dir +'/'+getEpochStringByParams(getLocalTime(), (char*) "%Y")+ F(".jso");

					objM = getJSonFromFile(&docM, filenamem);

					objM[getEpochStringByParams(getLocalTime(), (char*) "%Y%m")] = energyMonthly;

					isFileSaveOK = saveJSonToAFile(&docM, filenamem);
				}
				{
					DynamicJsonDocument docY(CUMULATED_VALUES_TOT_HEAP);
					JsonObject objY;

					String filenamey = scopeDirectory + F("/years.jso");

					objY = getJSonFromFile(&docY, filenamey);

					objY[getEpochStringByParams(getLocalTime(), (char*) "%Y")] = energyYearly;

					isFileSaveOK = saveJSonToAFile(&docY, filenamey);
				}

			}
		}
	}

}
#ifdef WS_ACTIVE
void sendSimpleWSMessage(String type, String val){
	DynamicJsonDocument docws(512);
	JsonObject objws = docws.to<JsonObject>();
	String dateFormatted = getEpochStringByParams(getLocalTime());
	objws["type"] = type;
	objws["date"] = dateFormatted;

	objws["value"] = val;

	String buf;
	serializeJson(objws, buf);

	webSocket.broadcastTXT(buf);
}
#endif

void readProductionDaily(tm nowDt){
	String scopeDirectory = F("product");
	if (!SD.exists(scopeDirectory)) {
		SD.mkdir(scopeDirectory);
	}
	for (int i = 0; i < 3; i++) {
		if (nowDt.tm_min % DAILY_INTERVAL == 0) {
			byte read = DSP_GRID_POWER_ALL;
			String filename = DSP_GRID_POWER_ALL_FILENAME;
			String type = DSP_GRID_POWER_ALL_TYPE;

			switch (i) {
			case (1):
				read = DSP_GRID_CURRENT_ALL;
				filename = DSP_GRID_CURRENT_ALL_FILENAME;
				type = DSP_GRID_CURRENT_ALL_TYPE;
				break;
			case (2):
				read = DSP_GRID_VOLTAGE_ALL;
				filename = DSP_GRID_VOLTAGE_ALL_FILENAME;
				type = DSP_GRID_VOLTAGE_ALL_TYPE;
				break;
			}

			Aurora::DataDSP dsp = inverter.readDSP(read);
			DEBUG_PRINT(F("Read state --> "));
			DEBUG_PRINTLN(dsp.state.readState);
			if (dsp.state.readState == 1) {
				float val = dsp.value;

				if (val && val > 0) {
#ifdef WS_ACTIVE
				sendSimpleWSMessage(type, String(setPrecision(val, 1)));
#endif

					String dataDirectory = getEpochStringByParams(getLocalTime(),
							(char*) "%Y%m%d");

					if (i == 0
							&& !SD.exists(
									scopeDirectory + '/' + dataDirectory)) {
						SD.mkdir(scopeDirectory + '/' + dataDirectory);
					}

					String filenameDir = scopeDirectory + "/" + dataDirectory
							+ "/" + filename;

					DynamicJsonDocument doc(PRODUCTION_AND_CUMULATED_HEAP);
					JsonObject obj;

					obj = getJSonFromFile(&doc, filenameDir);

					obj[F("lastUpdate")] = getEpochStringByParams(getLocalTime());

					JsonArray data;
					if (!obj.containsKey(F("data"))) {
						data = obj.createNestedArray(F("data"));
					} else {
						data = obj["data"];
					}

					JsonObject objArrayData = data.createNestedObject();
					objArrayData[F("h")] = getEpochStringByParams(getLocalTime(),
							(char*) "%H%M");
					objArrayData[F("val")] = setPrecision(val, 1);
					DEBUG_PRINTLN(F("Store production --> "));
					DEBUG_PRINT(doc.memoryUsage());
					DEBUG_PRINTLN();

					isFileSaveOK = saveJSonToAFile(&doc, filenameDir);
				}
			}
		}
	}

}

#ifdef WS_ACTIVE
void realtimeDataCallbak() {
	DEBUG_PRINT(F("Thread call (realtimeDataCallbak) --> "));
	DEBUG_PRINT(getEpochStringByParams(getLocalTime()));
	DEBUG_PRINT(F(" MEM --> "));
	DEBUG_PRINTLN(ESP.getFreeHeap());


	byte read = DSP_GRID_POWER_ALL;
	String type = DSP_GRID_POWER_ALL_TYPE_RT;

//			read = DSP_GRID_CURRENT_ALL;
//			filename = DSP_GRID_CURRENT_ALL_FILENAME;
//			type = DSP_GRID_CURRENT_ALL_TYPE;
//			read = DSP_GRID_VOLTAGE_ALL;
//			filename = DSP_GRID_VOLTAGE_ALL_FILENAME;
//			type = DSP_GRID_VOLTAGE_ALL_TYPE;

	Aurora::DataDSP dsp = inverter.readDSP(read);
	DEBUG_PRINT(F("Read DSP state --> "));
	DEBUG_PRINTLN(dsp.state.readState);
	if (dsp.state.readState == 1) {
		float val = dsp.value;

		if (val && val > 0) {
			sendSimpleWSMessage(type, String(setPrecision(val, 1)));
		}
	}

	float bv = getBatteryVoltage();
	DEBUG_PRINT(F("BATTERY --> "));
	DEBUG_PRINTLN(bv);

	type = BATTERY_RT;
	if (bv>1) {
		sendSimpleWSMessage(type, String(setPrecision(bv, 2)));
	}

	DEBUG_PRINT(F("WIFI SIGNAL STRENGHT --> "));
	DEBUG_PRINTLN(WiFi.RSSI());

	type = WIFI_SIGNAL_STRENGHT_RT;
	sendSimpleWSMessage(type, String(WiFi.RSSI()));
}
#endif

void leggiProduzioneCallback() {
	DEBUG_PRINT(F("Thread call (leggiProduzioneCallback) --> "));
	DEBUG_PRINT(getEpochStringByParams(getLocalTime()));
	DEBUG_PRINT(F(" MEM --> "));
	DEBUG_PRINTLN(ESP.getFreeHeap());

	tm nowDt = getDateTimeByParams(getLocalTime());

	cumulatedEnergyDaily(nowDt);
	readTotals(nowDt);
	readProductionDaily(nowDt);

	errorLed(!(fixedTime && sdStarted&& wifiConnected && isFileSaveOK));

//    digitalWrite(ERROR_PIN, !(fixedTime && sdStarted&& wifiConnected && isFileSaveOK));
}

JsonObject getJSonFromFile(DynamicJsonDocument *doc, String filename, bool forceCleanONJsonError ) {
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
			DEBUG_PRINTLN(error.c_str());

			if (forceCleanONJsonError){
				return doc->to<JsonObject>();
			}
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
	httpRestServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
	httpRestServer.sendHeader(F("Access-Control-Max-Age"), F("600"));
	httpRestServer.sendHeader(F("Access-Control-Allow-Methods"), F("PUT,POST,GET,OPTIONS"));
	httpRestServer.sendHeader(F("Access-Control-Allow-Headers"), F("*"));
};

void streamFileOnRest(String filename){
	if (SD.exists(filename)){
		myFileSDCart = SD.open(filename);
		if (myFileSDCart){
			if (myFileSDCart.available()){
				DEBUG_PRINT(F("Stream file..."));
				DEBUG_PRINT(filename);
				httpRestServer.streamFile(myFileSDCart, F("application/json"));
				DEBUG_PRINTLN(F("...done."));
			}else{
				DEBUG_PRINTLN(F("Data not available!"));
				httpRestServer.send(204, F("text/html"), F("Data not available!"));
			}
			myFileSDCart.close();
		}else{
			DEBUG_PRINT(filename);
			DEBUG_PRINTLN(F(" not found!"));

			httpRestServer.send(204, F("text/html"), F("No content found!"));
		}
	}else{
		DEBUG_PRINT(filename);
		DEBUG_PRINTLN(F(" not found!"));
		httpRestServer.send(204, F("text/html"), F("File not exist!"));
	}
}
String getContentType(String filename){
  if(filename.endsWith(F(".htm"))) 		return F("text/html");
  else if(filename.endsWith(F(".html"))) 	return F("text/html");
  else if(filename.endsWith(F(".css"))) 	return F("text/css");
  else if(filename.endsWith(F(".js"))) 	return F("application/javascript");
  else if(filename.endsWith(F(".json"))) 	return F("application/json");
  else if(filename.endsWith(F(".png"))) 	return F("image/png");
  else if(filename.endsWith(F(".gif"))) 	return F("image/gif");
  else if(filename.endsWith(F(".jpg"))) 	return F("image/jpeg");
  else if(filename.endsWith(F(".ico"))) 	return F("image/x-icon");
  else if(filename.endsWith(F(".xml"))) 	return F("text/xml");
  else if(filename.endsWith(F(".pdf"))) 	return F("application/x-pdf");
  else if(filename.endsWith(F(".zip"))) 	return F("application/x-zip");
  else if(filename.endsWith(F(".gz"))) 	return F("application/x-gzip");
  return F("text/plain");
}

bool handleFileRead(String path){  // send the right file to the client (if it exists)
  DEBUG_PRINT(F("handleFileRead: "));
  DEBUG_PRINTLN(path);

  if(path.endsWith("/")) path += F("index.html");           // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + F(".gz");
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){  // If the file exists, either as a compressed archive, or normal
    if(SPIFFS.exists(pathWithGz))                          // If there's a compressed version available
      path += F(".gz");                                         // Use the compressed version
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
				httpRestServer.send(204, F("text/html"), F("Data not available!"));
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

	if (httpRestServer.arg(F("day"))== "" || httpRestServer.arg(F("type"))== "" ){     //Parameter not found
		httpRestServer.send(400, F("text/html"), F("Missing required parameter!"));
		DEBUG_PRINTLN(F("No parameter"));
	}else{     //Parameter found
		DEBUG_PRINT(F("Read file: "));
		String filename = "product/"+httpRestServer.arg("day")+"/"+httpRestServer.arg("type")+F(".jso");

		DEBUG_PRINTLN(filename);

		streamFileOnRest(filename);
	}
}

void getBatteryInfo(){
	DEBUG_PRINTLN(F("getBatteryInfo"));

	setCrossOrigin();

	if (httpRestServer.arg(F("day"))== "" ){     //Parameter not found
		httpRestServer.send(400, F("text/html"), F("Missing required parameter!"));
		DEBUG_PRINTLN(F("No parameter"));
	}else{     //Parameter found
		DEBUG_PRINT(F("Read file: "));
		String filename = "battery/"+httpRestServer.arg(F("day"))+".jso";

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
		httpRestServer.send(400, F("text/html"), F("Missing required parameter!"));
		DEBUG_PRINTLN(F("No parameter"));
	}else{     //Parameter found
		DEBUG_PRINT(F("Read file: "));
		String filename = scopeDirectory+'/'+httpRestServer.arg(F("month"))+F(".jso");
		streamFileOnRest(filename);
	}
}

void getHistoricalValue(){
	DEBUG_PRINTLN(F("getHistoricalValue"));

	setCrossOrigin();
	String scopeDirectory = F("states");

	if (httpRestServer.arg("frequence")== ""){     //Parameter not found
		httpRestServer.send(400, F("text/html"), F("Missing required parameter!"));
		DEBUG_PRINTLN(F("No frequence parameter"));
	}else{     //Parameter found
		String filename;
		if (httpRestServer.arg("frequence") == F("years")){
			filename = scopeDirectory+'/'+httpRestServer.arg("frequence")+".jso";
		}else{
			if (httpRestServer.arg("year")== ""){
				httpRestServer.send(400, F("text/html"), F("Missing required parameter!"));
				DEBUG_PRINTLN(F("No year parameter"));
			}else{
				filename = scopeDirectory+'/'+httpRestServer.arg("frequence")+'/'+httpRestServer.arg("year")+F(".jso");
			}
		}
		DEBUG_PRINT(F("Read file: "));
		streamFileOnRest(filename);
	}
}

void postConfigFile() {
	DEBUG_PRINTLN(F("postConfigFile"));

	setCrossOrigin();

    String postBody = httpRestServer.arg("plain");
    DEBUG_PRINTLN(postBody);

	DynamicJsonDocument doc(CONFIG_FILE_HEAP);
	DeserializationError error = deserializeJson(doc, postBody);
	if (error) {
		// if the file didn't open, print an error:
		DEBUG_PRINT(F("Error parsing JSON "));
		DEBUG_PRINTLN(error.c_str());

		String msg = error.c_str();

		httpRestServer.send(400, F("text/html"), "Error in parsin json body! <br>"+msg);

	}else{
		JsonObject postObj = doc.as<JsonObject>();

		DEBUG_PRINT(F("HTTP Method: "));
		DEBUG_PRINTLN(httpRestServer.method());

        if (httpRestServer.method() == HTTP_POST) {
            if ((postObj.containsKey("server"))) {

            	DEBUG_PRINT(F("Open config file..."));
            	fs::File configFile = SPIFFS.open(F("/mc/config.txt"), "w");
            	if (!configFile) {
            	    DEBUG_PRINTLN(F("fail."));
            	    httpRestServer.send(304, F("text/html"), F("Fail to store data, can't open file!"));
            	}else{
            		DEBUG_PRINTLN(F("done."));
            		serializeJson(doc, configFile);
//            		httpRestServer.sendHeader("Content-Length", String(postBody.length()));
            		httpRestServer.send(201, F("application/json"), postBody);

//            		DEBUG_PRINTLN(F("Sent reset page"));
//					  delay(5000);
//					  ESP.restart();
//					  delay(2000);
            	}
            }
            else {
            	httpRestServer.send(204, F("text/html"), F("No data found, or incorrect!"));
            }
        }
    }
}

void getConfigFile(){
	DEBUG_PRINTLN(F("getConfigFile"));

	setCrossOrigin();

	DEBUG_PRINT(F("Read file: "));
	if (SPIFFS.exists(F("/mc/config.txt"))){
    	fs::File configFile = SPIFFS.open(F("/mc/config.txt"), "r");
		if (configFile){
			if (configFile.available()){
				DEBUG_PRINT(F("Stream file..."));
				httpRestServer.streamFile(configFile, F("application/json"));
				DEBUG_PRINTLN(F("done."));
			}else{
				DEBUG_PRINTLN(F("File not found!"));
				httpRestServer.send(204, F("text/html"), F("No content found!"));
			}
			configFile.close();
		}
	}else{
		DEBUG_PRINTLN(F("File not found!"));
	}
}

void getInverterInfo(){
	DEBUG_PRINTLN(F("getInverterInfo"));

	setCrossOrigin();

	String scopeDirectory = F("static");

	DEBUG_PRINT(F("Read file: "));
	String filename = scopeDirectory+F("/invinfo.jso");
	streamFileOnRest(filename);
}

void inverterDayWithProblem() {
	DEBUG_PRINTLN(F("inverterDayWithProblem"));

	setCrossOrigin();

	String scopeDirectory = F("alarms");

	myFileSDCart = SD.open(scopeDirectory);


	DynamicJsonDocument doc(ALARM_IN_A_DAY);
	JsonObject rootObj = doc.to<JsonObject>();

	JsonArray data = rootObj.createNestedArray("data");

	if (myFileSDCart){
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
	}
	if (data.size() > 0) {
		DEBUG_PRINT(F("Stream file..."));
		String buf;
		serializeJson(rootObj, buf);
		httpRestServer.send(200, F("application/json"), buf);
		DEBUG_PRINTLN(F("done."));
	} else {
		DEBUG_PRINTLN(F("No content found!"));
		httpRestServer.send(204, F("text/html"), F("No content found!"));
	}
}

void getInverterDayState() {
	DEBUG_PRINTLN(F("getInverterDayState"));

	setCrossOrigin();

	String scopeDirectory = F("alarms");

	DEBUG_PRINT(F("Read file: "));

	if (httpRestServer.arg("day") == "") {     //Parameter not found
		httpRestServer.send(400, F("text/html"), F("Missing required parameter!"));
		DEBUG_PRINTLN(F("No parameter"));
	} else {     //Parameter found

		String filename;
		String parameter = httpRestServer.arg("day");
		filename = scopeDirectory + '/' + parameter + F("/alarms.jso");

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
	filename = scopeDirectory+F("/alarStat.jso");

	streamFileOnRest(filename);
}

float getBatteryVoltage(){
	//************ Measuring Battery Voltage ***********
	float sample1 = 0;

	for (int i = 0; i < 100; i++) {
		sample1 = sample1 + analogRead(A0); //read the voltage from the divider circuit
		delay(2);
	}
	sample1 = sample1 / 100;
	DEBUG_PRINT(F("AnalogRead..."));
	DEBUG_PRINTLN(sample1);
	float batVolt = (sample1 * 3.3 * (BAT_RES_VALUE_VCC + BAT_RES_VALUE_GND) / BAT_RES_VALUE_GND) / 1023;
	return batVolt;
}

void getServerState(){
	DEBUG_PRINTLN(F("getServerState"));

	setCrossOrigin();

	DynamicJsonDocument doc(2048);
	JsonObject rootObj = doc.to<JsonObject>();

	JsonObject net = rootObj.createNestedObject("network");
	rootObj[F("lastUpdate")] = getEpochStringByParams(getLocalTime());

	net[F("ip")] = WiFi.localIP().toString();
	net[F("gw")] = WiFi.gatewayIP().toString();
	net[F("nm")] = WiFi.subnetMask().toString();

	net[F("dns1")] = WiFi.dnsIP(0).toString();
	net[F("dns2")] = WiFi.dnsIP(1).toString();

	net[F("signalStrengh")] = WiFi.RSSI();

	JsonObject chip = rootObj.createNestedObject("chip");
	chip[F("chipId")] = ESP.getChipId();
	chip[F("flashChipId")] = ESP.getFlashChipId();
	chip[F("flashChipSize")] = ESP.getFlashChipSize();
	chip[F("flashChipRealSize")] = ESP.getFlashChipRealSize();

	chip[F("freeHeap")] = ESP.getFreeHeap();

	chip[F("batteryVoltage")] = getBatteryVoltage(); //sample1;//setPrecision(batVolt,2);

	DEBUG_PRINT(F("Stream file..."));
	String buf;
	serializeJson(rootObj, buf);
	httpRestServer.send(200, F("application/json"), buf);
	DEBUG_PRINTLN(F("done."));
}
void getReset(){
	DEBUG_PRINTLN(F("getReset"));

	setCrossOrigin();

//	DynamicJsonDocument doc(2048);
//	JsonObject rootObj = doc.to<JsonObject>();
//
//	JsonObject net = rootObj.createNestedObject("network");
//	rootObj[F("lastUpdate")] = getEpochStringByParams(getLocalTime());
//
//	net[F("ip")] = WiFi.localIP().toString();
//	net[F("gw")] = WiFi.gatewayIP().toString();
//	net[F("nm")] = WiFi.subnetMask().toString();
//
//	net[F("dns1")] = WiFi.dnsIP(0).toString();
//	net[F("dns2")] = WiFi.dnsIP(1).toString();
//
//	net[F("signalStrengh")] = WiFi.RSSI();
//
//	JsonObject chip = rootObj.createNestedObject("chip");
//	chip[F("chipId")] = ESP.getChipId();
//	chip[F("flashChipId")] = ESP.getFlashChipId();
//	chip[F("flashChipSize")] = ESP.getFlashChipSize();
//	chip[F("flashChipRealSize")] = ESP.getFlashChipRealSize();
//
//	chip[F("freeHeap")] = ESP.getFreeHeap();
//
//	chip[F("batteryVoltage")] = getBatteryVoltage(); //sample1;//setPrecision(batVolt,2);
//
//	DEBUG_PRINT(F("Stream file..."));
//	String buf;
//	serializeJson(rootObj, buf);
//	httpRestServer.send(200, F("application/json"), buf);
//	DEBUG_PRINTLN(F("done."));
	DEBUG_PRINT(F("Reset..."));
	WiFiManager wifiManager;
	wifiManager.resetSettings();

	DEBUG_PRINT(SPIFFS.remove(F("/mc/config.txt")));

	DEBUG_PRINTLN(F("done"));

}

void sendCrossOriginHeader(){
	DEBUG_PRINTLN(F("sendCORSHeader"));

	httpRestServer.sendHeader(F("access-control-allow-credentials"), F("false"));

	setCrossOrigin();

	httpRestServer.send(204);
}

void restServerRouting() {
//	httpRestServer.header("Access-Control-Allow-Headers: Authorization, Content-Type");
//
    httpRestServer.on(F("/"), HTTP_GET, []() {
    	httpRestServer.send(200, F("text/html"),
            F("Welcome to the Inverter Centraline REST Web Server"));
    });
    httpRestServer.on(F("/production"), HTTP_GET, getProduction);
    httpRestServer.on(F("/productionTotal"), HTTP_GET, getProductionTotal);
    httpRestServer.on(F("/monthly"), HTTP_GET, getMontlyValue);

    httpRestServer.on(F("/historical"), HTTP_GET, getHistoricalValue);

    httpRestServer.on(F("/config"), HTTP_OPTIONS, sendCrossOriginHeader);
    httpRestServer.on(F("/config"), HTTP_POST, postConfigFile);
    httpRestServer.on(F("/config"), HTTP_GET, getConfigFile);

    httpRestServer.on(F("/inverterInfo"), HTTP_GET, getInverterInfo);
    httpRestServer.on(F("/inverterState"), HTTP_GET, getInverterLastState);
    httpRestServer.on(F("/inverterDayWithProblem"), HTTP_GET, inverterDayWithProblem);
    httpRestServer.on(F("/inverterDayState"), HTTP_GET, getInverterDayState);

    httpRestServer.on(F("/serverState"), HTTP_GET, getServerState);

    httpRestServer.on(F("/battery"), HTTP_GET, getBatteryInfo);

    httpRestServer.on(F("/reset"), HTTP_GET, getReset);


}

void serverRouting() {
	  httpServer.onNotFound([]() {                              // If the client requests any URI
		  DEBUG_PRINTLN(F("On not found"));
	    if (!handleFileRead(httpServer.uri())){                  // send it if it exists
	    	DEBUG_PRINTLN(F("Not found"));
	    	httpServer.send(404, F("text/plain"), F("404: Not Found")); // otherwise, respond with a 404 (Not Found) error
	    }
	  });

	  DEBUG_PRINTLN(F("Set cache!"));

	  httpServer.serveStatic("/settings.json", SPIFFS, "/settings.json","no-cache, no-store, must-revalidate");
	  httpServer.serveStatic("/", SPIFFS, "/","max-age=31536000");
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
			Timezone tz = getTimezoneData(codeDST);
			tm tempTime = timeDate.getDateTime();
			time_t currentUTCTime = tz.toUTC(mktime(&tempTime));

			if (timeDate.state.readState){
				DEBUG_PRINTLN(F("Inverter Time retrieved."));
				// Set correct time in Arduino Time librery
				adjustTime(currentUTCTime); // - timeOffset);
				fixedTime = true;
			}else{
				DEBUG_PRINTLN(F("Inverter Time not retrieved."));
				DEBUG_PRINTLN(F("DANGER, SYSTEM INCONCISTENCY"));
			}
		}

		Timezone tz = getTimezoneData(codeDST);

		DEBUG_PRINTLN(getEpochStringByParams(now()));
		DEBUG_PRINTLN(tz.utcIsDST(now()));
		DEBUG_PRINTLN(getEpochStringByParams(tz.toLocal(now())));

	}

	errorLed(!(fixedTime && sdStarted&& wifiConnected && isFileSaveOK));
//    digitalWrite(ERROR_PIN, !(fixedTime && sdStarted&& wifiConnected && isFileSaveOK));
}

float setPrecision(float val, byte precision){
	return ((int)(val*(10*precision)))/(float)(precision*10);
}

uint8_t sdWrongReadNumber = 0;
bool alreadySendNotification = false;

unsigned long lastTimeSendErrorNotification = millis();

#define DEBOUNCE_ERROR_NOTIFICATION 5000

#ifdef WS_ACTIVE
void sendWSErrorCentraline(){
	DEBUG_PRINTLN(F("SEND sendWSErrorCentraline!"));
	if (sdWrongReadNumber==1 || (lastTimeSendErrorNotification+DEBOUNCE_ERROR_NOTIFICATION<millis() && sdWrongReadNumber % 10 == 0) ){
		DynamicJsonDocument docws(512);
		JsonObject objws = docws.to<JsonObject>();
		String dateFormatted = getEpochStringByParams(getLocalTime());
		objws["type"] = ERROR_TYPE;
		objws["date"] = dateFormatted;

		JsonObject objValue = objws.createNestedObject("value");

		objValue[F("h")] = getEpochStringByParams(getLocalTime());

		const String ft = (fixedTime)?F("OK"):F("NO");
		const String sd = (sdStarted)?F("OK"):F("NO");
		const String wc = (wifiConnected)?F("OK"):F("NO");
		const String sf = (isFileSaveOK)?F("OK"):F("NO");

		objValue[F("fixedTime")] = fixedTime;
		objValue[F("sdStarted")] = sdStarted;
		objValue[F("wifiConnected")] = wifiConnected;
		objValue[F("isFileSaveOK")] = isFileSaveOK;
		objValue[F("sdWrongReadNumber")] = sdWrongReadNumber;



		String buf;
		serializeJson(objws, buf);

		webSocket.broadcastTXT(buf);

		lastTimeSendErrorNotification = millis();
	}
}
#endif

void errorLed(bool flag){
	if (flag){
		sdWrongReadNumber++;
	}else{
		sdWrongReadNumber = 0;
		alreadySendNotification = false;
	}

#ifdef WS_ACTIVE
	if (!alreadySendNotification &&
	(
			!fixedTime
			|| !sdStarted
			|| !wifiConnected
			|| !isFileSaveOK
	)
	){
		sendWSErrorCentraline();
	}
#endif

	DEBUG_PRINT(F("alreadySendNotification -> "));
	DEBUG_PRINTLN(alreadySendNotification);

	DEBUG_PRINT(F("sdWrongReadNumber -> "));
	DEBUG_PRINTLN(sdWrongReadNumber);



	digitalWrite(ERROR_PIN, flag);
	if (!alreadySendNotification && sdWrongReadNumber>=SD_WRONG_WRITE_NUMBER_ALERT){
		alreadySendNotification = true;

#ifdef SEND_EMAIL

    	DEBUG_PRINT(F("Open config file..."));
		fs::File configFile = SPIFFS.open(F("/mc/config.txt"), "r");
		if (configFile) {
			DEBUG_PRINTLN("done.");
			DynamicJsonDocument doc(CONFIG_FILE_HEAP);
			DeserializationError error = deserializeJson(doc, configFile);
			if (error) {
				// if the file didn't open, print an error:
				DEBUG_PRINT(F("Error parsing JSON "));
				DEBUG_PRINTLN(error.c_str());
			}

			// close the file:
			configFile.close();

			JsonObject rootObj = doc.as<JsonObject>();

			DEBUG_PRINT(F("After read config check serverSMTP and preferences "));
			DEBUG_PRINTLN(rootObj.containsKey(F("serverSMTP")) && rootObj.containsKey(F("preferences")));

			if (rootObj.containsKey(F("serverSMTP")) && rootObj.containsKey(F("preferences"))){
				JsonObject serverSMTP = rootObj[F("serverSMTP")];
				JsonObject preferences = rootObj[F("preferences")];

				DEBUG_PRINT(F("(preferences.containsKey(adminEmail) && preferences[adminEmail]!="")"));
				DEBUG_PRINTLN((preferences.containsKey(F("adminEmail")) && preferences[F("adminEmail")]!=""));

				if (preferences.containsKey(F("adminEmail")) && preferences[F("adminEmail")]!=""){
					const char* serverSMTPAddr = serverSMTP[F("server")];
					emailSend.setSMTPServer(serverSMTPAddr);
					uint16_t portSMTP = serverSMTP[F("port")];
					emailSend.setSMTPPort(portSMTP);
					const char* loginSMTP = serverSMTP[F("login")];
					emailSend.setEMailLogin(loginSMTP);
					const char* passwordSMTP = serverSMTP[F("password")];
					emailSend.setEMailPassword(passwordSMTP);
					const char* fromSMTP = serverSMTP[F("from")];
					emailSend.setEMailFrom(fromSMTP);

					DEBUG_PRINT(F("server "));
					DEBUG_PRINTLN(serverSMTPAddr);
					DEBUG_PRINT(F("port "));
					DEBUG_PRINTLN(portSMTP);
					DEBUG_PRINT(F("login "));
					DEBUG_PRINTLN(loginSMTP);
					DEBUG_PRINT(F("password "));
					DEBUG_PRINTLN(passwordSMTP);
					DEBUG_PRINT(F("from "));
					DEBUG_PRINTLN(fromSMTP);

					EMailSender::EMailMessage message;
					const String sub = F("Inverter error!");
					message.subject = sub;

					const String mp = F("Error on inverter centraline");
					const String ft = (fixedTime)?F("OK"):F("NO");
					const String sd = (sdStarted)?F("OK"):F("NO");
					const String wc = (wifiConnected)?F("OK"):F("NO");
					const String sf = (isFileSaveOK)?F("OK"):F("NO");

					message.message = mp+
									F("<br>Time fixed: ")+ft+
									F("<br>SD Initialized: ")+sd+
									F("<br>Wifi Connecter :P: ")+wc+
									F("<br>Saving file ok: ")+sf+
									F("<br>Wrong saving attempts: ")+sdWrongReadNumber
									;

					const char* emailToSend = preferences[F("adminEmail")];
					EMailSender::Response resp = emailSend.send(emailToSend, message);

					DEBUG_PRINTLN(F("Sending status: "));
					DEBUG_PRINTLN(emailToSend);
					DEBUG_PRINTLN(resp.status);
					DEBUG_PRINTLN(resp.code);
					DEBUG_PRINTLN(resp.desc);
				}
			}
		}
#endif
	}
}

Timezone getTimezoneData(const String code){
//	DEBUG_PRINT("CODE DST RETRIVED: ");
//	DEBUG_PRINTLN(code);
	if (code==F("AETZ")){
		// Australia Eastern Time Zone (Sydney, Melbourne)
		TimeChangeRule aEDT = {"AEDT", First, Sun, Oct, 2, 660};    // UTC + 11 hours
		TimeChangeRule aEST = {"AEST", First, Sun, Apr, 3, 600};    // UTC + 10 hours
		Timezone tzTmp = Timezone(aEDT, aEST);
		return tzTmp;
	}else if (code==F("CET")){
//		DEBUG_PRINTLN(F("CET FIND"));
		// Central European Time (Frankfurt, Paris)
		TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
		TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
		Timezone tzTmp = Timezone(CEST, CET);
		return tzTmp;
	}else if(code==F("MSK")){
		// Moscow Standard Time (MSK, does not observe DST)
		TimeChangeRule msk = {"MSK", Last, Sun, Mar, 1, 180};
		Timezone tzTmp = Timezone(msk);
		return tzTmp;
	}else if(code==F("UK")){
		// United Kingdom (London, Belfast)
		TimeChangeRule BST = {"BST", Last, Sun, Mar, 1, 60};        // British Summer Time
		TimeChangeRule GMT = {"GMT", Last, Sun, Oct, 2, 0};         // Standard Time
		Timezone tzTmp = Timezone(BST, GMT);
		return tzTmp;
	}else if(code==F("USCTZ")){
		// US Central Time Zone (Chicago, Houston)
		TimeChangeRule usCDT = {"CDT", Second, Sun, Mar, 2, -300};
		TimeChangeRule usCST = {"CST", First, Sun, Nov, 2, -360};
		Timezone tzTmp = Timezone(usCDT, usCST);
		return tzTmp;
	}else if(code==F("USMTZ")){
		// US Mountain Time Zone (Denver, Salt Lake City)
		TimeChangeRule usMDT = {"MDT", Second, Sun, Mar, 2, -360};
		TimeChangeRule usMST = {"MST", First, Sun, Nov, 2, -420};
		Timezone tzTmp = Timezone(usMDT, usMST);
		return tzTmp;
	}else if(code==F("ARIZONA")){
		// Arizona is US Mountain Time Zone but does not use DST
		TimeChangeRule usMST = {"MST", First, Sun, Nov, 2, -420};
		Timezone tzTmp = Timezone(usMST);
		return tzTmp;
	}else if(code==F("USPTZ")){
		// US Pacific Time Zone (Las Vegas, Los Angeles)
		TimeChangeRule usPDT = {"PDT", Second, Sun, Mar, 2, -420};
		TimeChangeRule usPST = {"PST", First, Sun, Nov, 2, -480};
		Timezone tzTmp = Timezone(usPDT, usPST);
		return tzTmp;
	}else if(code==F("UTC")){
		// UTC
		TimeChangeRule utcRule = {"UTC", Last, Sun, Mar, 1, 0};     // UTC
		Timezone tzTmp = Timezone(utcRule);
		return tzTmp;
	}else{
		// UTC
//		DEBUG_PRINT("TIME_OFFSET ");
		int to = timeOffset/60;
//		DEBUG_PRINTLN(to);
		TimeChangeRule utcRule = {"GTM", Last, Sun, Mar, 1, to};     // UTC
		Timezone tzTmp = Timezone(utcRule);
		return tzTmp;

	}


}

time_t getLocalTime(void){
	Timezone tz = getTimezoneData(codeDST);

//	DEBUG_PRINTLN(getEpochStringByParams(now()));
//	DEBUG_PRINTLN(tz.utcIsDST(now()));
//	DEBUG_PRINTLN(getEpochStringByParams(tz.toLocal(now())));
//
	return tz.toLocal(now());
}

#ifdef WS_ACTIVE
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
			webSocket.sendTXT(num, "{\"connection\": false}");

            DEBUG_PRINT(F(" Disconnected "));
            DEBUG_PRINTLN(num, DEC);

//            DEBUG_PRINTF_AI(F("[%u] Disconnected!\n"), num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
//                DEBUG_PRINTF_AI(F("[%u] Connected from %d.%d.%d.%d url: %s\n"), num, ip[0], ip[1], ip[2], ip[3], payload);

                DEBUG_PRINT(num);
                DEBUG_PRINT(F("Connected from: "));
                DEBUG_PRINT(ip.toString());
                DEBUG_PRINT(F(" "));
                DEBUG_PRINTLN((char*)payload);

				// send message to client
				webSocket.sendTXT(num, "{\"connection\": true}");
            }
            break;
        case WStype_TEXT:
//        	DEBUG_PRINTF_AI(F("[%u] get Text: %s\n"), num, payload);

            // send message to client
            // webSocket.sendTXT(num, "message here");

            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            break;
        case WStype_BIN:
//        	DEBUG_PRINTF_AI(F("[%u] get binary length: %u\n"), num, length);
            hexdump(payload, length);

            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
        case WStype_PING:
        case WStype_PONG:

//        	DEBUG_PRINTF_AI(F("[%u] get binary length: %u\n"), num, length);
        	DEBUG_PRINT(F("WS : "))
        	DEBUG_PRINT(type)
        	DEBUG_PRINT(F(" - "))
			DEBUG_PRINTLN((char*)payload);

            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
    }

}
#endif
