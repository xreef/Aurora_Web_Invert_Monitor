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
#define DAILY_INTERVAL 10
#define CUMULATIVE_INTERVAL 10
#define CUMULATIVE_TOTAL_INTERVAL 10
#define STATE_INTERVAL 10
#define STATIC_DATA_INTERVAL 6 * 60

// SD
#define CS_PIN D8

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
void leggiStatoInverterCallback();
void updateLocalTimeWithNTPCallback();

bool isFileSaveOK = true;
bool saveJSonToAFile(DynamicJsonDocument *doc, String filename);
JsonObject getJSonFromFile(DynamicJsonDocument *doc, String filename, bool forceCleanONJsonError = true);

void errorLed(bool flag);

Thread ManageStaticData = Thread();

Thread LeggiStatoInverter = Thread();

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
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 1*60*60, 60*60*1000);

Thread UpdateLocalTimeWithNTP = Thread();

bool fixedTime = false;
bool sdStarted = false;
bool wifiConnected = false;

float setPrecision(float val, byte precision);

#define DSP_GRID_POWER_ALL_FILENAME									F("power.jso")		/* Global */
#define DSP_GRID_CURRENT_ALL_FILENAME								F("current.jso")
#define DSP_GRID_VOLTAGE_ALL_FILENAME								F("voltage.jso")

EMailSender emailSend("smtp", "pwd");

#ifdef SERVER_FTP
FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial
#endif

int timeOffset = 0;

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
			JsonObject preferences = rootObj[F("preferences")];
			bool isGTM = preferences.containsKey(F("GTM"));
			if (isGTM){
				JsonObject GTM = preferences[F("GTM")];
				bool isValue = GTM.containsKey(F("value"));
				if (isValue){
					int value = GTM[F("value")];

					DEBUG_PRINT(F("Impostazione GTM+"))
					DEBUG_PRINTLN(value)

					timeClient.setTimeOffset(value*60*60);
					timeOffset = value*60*60;
				}
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


	DEBUG_PRINT(F("Recreate settings file..."));
	fs::File settingsFile = SPIFFS.open(F("/settings.json"), "w");
	if (!settingsFile) {
	    DEBUG_PRINTLN(F("fail."));
	}else{
		DEBUG_PRINTLN(F("done."));
		DynamicJsonDocument doc;
		JsonObject postObj = doc.to<JsonObject>();
		postObj[F("localIP")] = WiFi.localIP().toString();
		postObj[F("localRestPort")] = HTTP_REST_PORT;

		String buf;
		serializeJson(postObj, buf);

		settingsFile.print(F("var settings = "));
		settingsFile.print(buf);
		settingsFile.print(";");

		settingsFile.close();

//		serializeJson(doc, settingsFile);
	    DEBUG_PRINTLN(F("success."));
	}

	// Start inverter serial
	DEBUG_PRINT(F("Initializing Inverter serial..."));
	inverter.begin();
	DEBUG_PRINTLN(F("initialization done."));

	// Start inizialization of SD cart
	DEBUG_PRINT(F("Initializing SD card..."));
	if (!SD.begin(CS_PIN, SPI_FULL_SPEED)) {
		DEBUG_PRINTLN(F("initialization failed!"));
		sdStarted = false;
		// return to stop all
		return;
	}
	sdStarted = true;
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
    ftpSrv.begin("aurora","aurora");    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
    DEBUG_PRINTLN(F("FTP Server Started"));
#endif

    if (!MDNS.begin(hostname)) {             // Start the mDNS responder for esp8266.local
    	DEBUG_PRINTLN(F("Error setting up MDNS responder!"));
    }
    DEBUG_PRINT(hostname);
    DEBUG_PRINTLN(F(" --> mDNS responder started"));

    DEBUG_PRINT(F("ERROR --> "));
    DEBUG_PRINTLN(!(fixedTime && sdStarted&& wifiConnected));

	errorLed(!(fixedTime && sdStarted&& wifiConnected && isFileSaveOK));

//    digitalWrite(ERROR_PIN, !(fixedTime && sdStarted&& wifiConnected && isFileSaveOK));

    DEBUG_PRINTLN(F("FIRST LOAD..."))
    leggiStatoInverterCallback();
    manageStaticDataCallback();
    leggiProduzioneCallback();
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
	DEBUG_PRINT(getEpochStringByParams(now()));
	DEBUG_PRINT(F(" MEM --> "));
	DEBUG_PRINTLN(ESP.getFreeHeap())

	Aurora::DataState dataState = inverter.readState();
	DEBUG_PRINT(F("Read state --> "));
	DEBUG_PRINTLN(dataState.state.readState);

	if (dataState.state.readState == 1) {
		DEBUG_PRINTLN(F("done."));

		DEBUG_PRINT(F("Create json..."));

		String scopeDirectory = F("alarms");
		if (!SD.exists(scopeDirectory)) {
			SD.mkdir(scopeDirectory);
		}

		String filename = scopeDirectory + F("/alarStat.jso");

		DynamicJsonDocument doc;
		JsonObject rootObj = doc.to<JsonObject>();

		rootObj[F("lastUpdate")] = getEpochStringByParams(now());

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

		// If alarm present or inverterState not running
//		if (dataState.alarmState>0 || dataState.inverterState!=2){

		String dayDirectory = getEpochStringByParams(now(), (char*) "%Y%m%d");

		String filenameAL = scopeDirectory + '/' + dayDirectory + F("/alarms.jso");

		DynamicJsonDocument docAS;
		JsonObject obj;

		obj = getJSonFromFile(&docAS, filenameAL);

		obj[F("lastUpdate")] = getEpochStringByParams(now());

		JsonArray data;
		if (!obj.containsKey(F("data"))) {
			data = obj.createNestedArray(F("data"));
		} else {
			data = obj[F("data")];
		}

		bool inverterProblem = dataState.alarmState > 0 || (dataState.inverterState != 2 && dataState.inverterState != 1);

		bool firstElement = data.size() == 0;

		if (inverterProblem || !firstElement) {
			JsonObject lastData;
			if (data.size() > 0) {
				lastData = data[data.size() - 1];
			}

			bool variationFromPrevious = (data.size() > 0
					&& (lastData[F("asp")] != dataState.alarmState
							|| lastData[F("c1sp")] != dataState.channel1State
							|| lastData[F("c2sp")] != dataState.channel2State
							|| lastData[F("isp")] != dataState.inverterState));

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

			if ((inverterProblem && firstElement)
					|| (!firstElement && variationFromPrevious)) {



				JsonObject objArrayData = data.createNestedObject();

				objArrayData[F("h")] = getEpochStringByParams(now(),
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

				DEBUG_PRINT(F("Open config file..."));
				fs::File configFile = SPIFFS.open(F("/mc/config.txt"), "r");
				if (configFile) {
				    DEBUG_PRINTLN(F("done."));
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

								byte asp = lastData[F("asp")];
								byte c1sp = lastData[F("c1sp")];
								byte c2sp = lastData[F("c2sp")];
								byte isp = lastData[F("isp")];

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
										(alarm==F("all") && asp != dataState.alarmState)
										||
										(ch1==F("all") && c1sp != dataState.channel1State)
										||
										(ch2==F("all") && c2sp != dataState.channel2State)
										||
										(state==F("all") && isp != dataState.inverterState)
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


								if (
										allNotification
										||
										onProblem
								){
									const String mp = emailNotification[F("messageProblem")];
									const String mnp = emailNotification[F("messageNoProblem")];
									message.message = ((inverterProblem)?mp:mnp)+
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

		}
//		}
	}

}

void manageStaticDataCallback () {
	DEBUG_PRINT(F("Thread call (manageStaticDataCallback) --> "));
	DEBUG_PRINT(getEpochStringByParams(now()));
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

		DynamicJsonDocument doc;
		JsonObject rootObj = doc.to<JsonObject>();

		rootObj[F("lastUpdate")] = getEpochStringByParams(now());

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
						+ getEpochStringByParams(now(), (char*) "%Y%m")
						+ F(".jso");
				String tagName = getEpochStringByParams(now(), (char*) "%d");

				DynamicJsonDocument doc;
				JsonObject obj;

				obj = getJSonFromFile(&doc, filename);

				obj[F("lastUpdate")] = getEpochStringByParams(now());

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
				String filename = scopeDirectory + F("/lastStat.jso");
				String tagName = getEpochStringByParams(now(), (char*) "%d");

				DynamicJsonDocument doc;
				JsonObject obj = doc.to<JsonObject>();;

				obj[F("lastUpdate")] = getEpochStringByParams(now());

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

				obj[F("W")] = getEpochStringByParams(now(), (char*) "%Y%W");
				obj[F("M")] = getEpochStringByParams(now(), (char*) "%Y%m");
				obj[F("Y")] = getEpochStringByParams(now(), (char*) "%Y");

				DEBUG_PRINT(F("Store energyLifetime energy --> "));
				//				serializeJson(doc, Serial);
				DEBUG_PRINT(doc.memoryUsage());
				DEBUG_PRINTLN();

				isFileSaveOK = saveJSonToAFile(&doc, filename);

				{
					DynamicJsonDocument docW;
					JsonObject objW;

					String dir = scopeDirectory + F("/weeks");
					if (!SD.exists(dir)) {
						SD.mkdir(dir);
					}

					String filenamew = dir +'/'+getEpochStringByParams(now(), (char*) "%Y")+ F(".jso");

					objW = getJSonFromFile(&docW, filenamew);
					objW[getEpochStringByParams(now(), (char*) "%Y%W")] = energyWeekly;

					isFileSaveOK = saveJSonToAFile(&docW, filenamew);
				}
				{
					DynamicJsonDocument docM;
					JsonObject objM;

					String dir = scopeDirectory + F("/months");
					if (!SD.exists(dir)) {
						SD.mkdir(dir);
					}

					String filenamem = dir +'/'+getEpochStringByParams(now(), (char*) "%Y")+ F(".jso");

					objM = getJSonFromFile(&docM, filenamem);

					objM[getEpochStringByParams(now(), (char*) "%Y%m")] = energyMonthly;

					isFileSaveOK = saveJSonToAFile(&docM, filenamem);
				}
				{
					DynamicJsonDocument docY;
					JsonObject objY;

					String filenamey = scopeDirectory + F("/years.jso");

					objY = getJSonFromFile(&docY, filenamey);

					objY[getEpochStringByParams(now(), (char*) "%Y")] = energyYearly;

					isFileSaveOK = saveJSonToAFile(&docY, filenamey);
				}

			}
		}
	}

}

void readProductionDaily(tm nowDt){
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

					String filenameDir = scopeDirectory + "/" + dataDirectory
							+ "/" + filename;

					DynamicJsonDocument doc;
					JsonObject obj;

					obj = getJSonFromFile(&doc, filenameDir);

					obj[F("lastUpdate")] = getEpochStringByParams(now());

					JsonArray data;
					if (!obj.containsKey(F("data"))) {
						data = obj.createNestedArray(F("data"));
					} else {
						data = obj["data"];
					}

					JsonObject objArrayData = data.createNestedObject();
					objArrayData[F("h")] = getEpochStringByParams(now(),
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

void leggiProduzioneCallback() {
	DEBUG_PRINT(F("Thread call (leggiProduzioneCallback) --> "));
	DEBUG_PRINT(getEpochStringByParams(now()));
	DEBUG_PRINT(F(" MEM --> "));
	DEBUG_PRINTLN(ESP.getFreeHeap());

	tm nowDt = getDateTimeByParams(now());

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
			DEBUG_PRINTLN(error);

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

	if (httpRestServer.arg(F("day"))== "" || httpRestServer.arg(F("type"))== "" ){     //Parameter not found
		httpRestServer.send(400, F("text/html"), F("Missing required parameter!"));
		DEBUG_PRINTLN(F("No parameter"));
	}else{     //Parameter found
		DEBUG_PRINT(F("Read file: "));
		String filename = "product/"+httpRestServer.arg(F("day"))+"/"+httpRestServer.arg(F("type"))+".jso";

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


// ARRIVATO QUI
void getHistoricalValue(){
	DEBUG_PRINTLN(F("getHistoricalValue"));

	setCrossOrigin();
	String scopeDirectory = "states";

	if (httpRestServer.arg("frequence")== ""){     //Parameter not found
		httpRestServer.send(400, F("text/html"), F("Missing required parameter!"));
		DEBUG_PRINTLN(F("No frequence parameter"));
	}else{     //Parameter found
		String filename;
		if (httpRestServer.arg("frequence") == "years"){
			filename = scopeDirectory+'/'+httpRestServer.arg("frequence")+".jso";
		}else{
			if (httpRestServer.arg("year")== ""){
				httpRestServer.send(400, F("text/html"), F("Missing required parameter!"));
				DEBUG_PRINTLN(F("No year parameter"));
			}else{
				filename = scopeDirectory+'/'+httpRestServer.arg("frequence")+'/'+httpRestServer.arg("year")+".jso";
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

//            	  DEBUG_PRINTLN(F("Reset"));
//				  delay(15000);
//				  ESP.reset();
//				  delay(2000);
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

	String scopeDirectory = "static";

	DEBUG_PRINT(F("Read file: "));
	String filename = scopeDirectory+"/invinfo.jso";
	streamFileOnRest(filename);
}

void inverterDayWithProblem() {
	DEBUG_PRINTLN(F("inverterDayWithProblem"));

	setCrossOrigin();

	String scopeDirectory = "alarms";

	myFileSDCart = SD.open(scopeDirectory);


	DynamicJsonDocument doc;
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

	String scopeDirectory = "alarms";

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

	String scopeDirectory = "alarms";

	DEBUG_PRINT(F("Read file: "));

	String filename;
	String parameter = httpRestServer.arg("day");
	filename = scopeDirectory+"/alarStat.jso";

	streamFileOnRest(filename);
}

void getServerState(){
	DEBUG_PRINTLN(F("getServerState"));

	setCrossOrigin();

	DynamicJsonDocument doc;
	JsonObject rootObj = doc.to<JsonObject>();

	JsonObject net = rootObj.createNestedObject("network");
	rootObj["lastUpdate"] = getEpochStringByParams(now());

	net["ip"] = WiFi.localIP().toString();
	net["gw"] = WiFi.gatewayIP().toString();
	net["nm"] = WiFi.subnetMask().toString();

	net["dns1"] = WiFi.dnsIP(0).toString();
	net["dns2"] = WiFi.dnsIP(1).toString();

	net["signalStrengh"] = WiFi.RSSI();

	JsonObject chip = rootObj.createNestedObject("chip");
	chip["chipId"] = ESP.getChipId();
	chip["flashChipId"] = ESP.getFlashChipId();
	chip["flashChipSize"] = ESP.getFlashChipSize();
	chip["flashChipRealSize"] = ESP.getFlashChipRealSize();


	uint8_t raw = analogRead(A0);
	float volt=raw/1023.0;
	volt=volt*4.2;
	chip["batteryVoltage"] = volt;

	DEBUG_PRINT(F("Stream file..."));
	String buf;
	serializeJson(rootObj, buf);
	httpRestServer.send(200, "application/json", buf);
	DEBUG_PRINTLN(F("done."));
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

    httpRestServer.on("/historical", HTTP_GET, getHistoricalValue);

    httpRestServer.on("/config", HTTP_OPTIONS, sendCrossOriginHeader);
    httpRestServer.on("/config", HTTP_POST, postConfigFile);
    httpRestServer.on("/config", HTTP_GET, getConfigFile);

    httpRestServer.on("/inverterInfo", HTTP_GET, getInverterInfo);
    httpRestServer.on("/inverterState", HTTP_GET, getInverterLastState);
    httpRestServer.on("/inverterDayWithProblem", HTTP_GET, inverterDayWithProblem);
    httpRestServer.on("/inverterDayState", HTTP_GET, getInverterDayState);

    httpRestServer.on("/serverState", HTTP_GET, getServerState);

}

void serverRouting() {
	  httpServer.onNotFound([]() {                              // If the client requests any URI
		  DEBUG_PRINTLN(F("On not found"));
	    if (!handleFileRead(httpServer.uri())){                  // send it if it exists
	    	DEBUG_PRINTLN(F("Not found"));
	    	httpServer.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
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

	errorLed(!(fixedTime && sdStarted&& wifiConnected && isFileSaveOK));
//    digitalWrite(ERROR_PIN, !(fixedTime && sdStarted&& wifiConnected && isFileSaveOK));
}

float setPrecision(float val, byte precision){
	return ((int)(val*(10*precision)))/(float)(precision*10);
}

uint8_t sdWrongReadNumber = 0;
bool alreadySendNotification = false;

void errorLed(bool flag){
	if (flag){
		sdWrongReadNumber++;
	}else{
		sdWrongReadNumber = 0;
		alreadySendNotification = false;
	}

	digitalWrite(ERROR_PIN, flag);
	if (!alreadySendNotification && sdWrongReadNumber>=10){
		alreadySendNotification = true;
    	DEBUG_PRINT(F("Open config file..."));
		fs::File configFile = SPIFFS.open(F("/mc/config.txt"), "r");
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

			DEBUG_PRINT(F("After read config check serverSMTP and preferences "));
			DEBUG_PRINTLN(rootObj.containsKey("serverSMTP") && rootObj.containsKey("preferences"));

			if (rootObj.containsKey("serverSMTP") && rootObj.containsKey("preferences")){
				JsonObject serverSMTP = rootObj["serverSMTP"];
				JsonObject preferences = rootObj["preferences"];

				DEBUG_PRINT("(preferences.containsKey(adminEmail) && preferences[adminEmail]!="")");
				DEBUG_PRINTLN((preferences.containsKey("adminEmail") && preferences["adminEmail"]!=""));

				if (preferences.containsKey("adminEmail") && preferences["adminEmail"]!=""){
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

					const char* emailToSend = preferences["adminEmail"];
					EMailSender::Response resp = emailSend.send(emailToSend, message);

					DEBUG_PRINTLN(F("Sending status: "));
					DEBUG_PRINTLN(emailToSend);
					DEBUG_PRINTLN(resp.status);
					DEBUG_PRINTLN(resp.code);
					DEBUG_PRINTLN(resp.desc);
				}
			}
		}
	}
}
