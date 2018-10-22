#include "Arduino.h"
#include <Aurora.h>
#include <SD.h>
#define FS_NO_GLOBALS
#include "FS.h"
#include <Thread.h>

#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <SPI.h>

#include <ArduinoJson.h>

char hostname[] = "InverterCentraline";

// Interval get data
#define DAILY_INTERVAL 5
#define CUMULATIVE_INTERVAL 5

// SD
#define CS_PIN D8

// Inverte inizialization
Aurora inverter = Aurora(2, D2, D3, D1);

void leggiProduzioneCallback();
void updateLocalTimeWithNTPCallback();

bool saveJSonToAFile(DynamicJsonDocument doc, String filename);
JsonObject getJSonFromFile(DynamicJsonDocument doc, String filename);

Thread LeggiProduzione = Thread();

#define HTTP_REST_PORT 8080
ESP8266WebServer httpRestServer(HTTP_REST_PORT);

void restServerRouting();

WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 2*60*60, 60*60*1000);

Thread UpdateLocalTimeWithNTP = Thread();

bool fixedTime = false;

float setPrecision(float val, byte precision);

void setup() {
	// Inizilization of serial debug
	Serial.begin(19200);
	Serial.setTimeout(500);
	// Wait to finisc inizialization
	delay(600);

	Serial.print(F("Inizializing FS..."));
	if (SPIFFS.begin()){
		Serial.println(F("done."));
	}
	Serial.println(F("fail."));

	WiFi.hostname(hostname);
	wifi_station_set_hostname(hostname);
    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(10);

	Serial.print(F("Open config file..."));
	fs::File configFile = SPIFFS.open("/mc/config.txt", "r");
	if (configFile) {
	    Serial.println("done.");
		DynamicJsonDocument doc;
		DeserializationError error = deserializeJson(doc, configFile);
		if (error) {
			// if the file didn't open, print an error:
			Serial.print(F("Error parsing JSON "));
			Serial.println(error);
		}

		// close the file:
		configFile.close();

		JsonObject rootObj = doc.as<JsonObject>();
		JsonObject serverConfig = rootObj["server"];
		bool isStatic = serverConfig["isStatic"];
		if (isStatic==true){
			const char* address = serverConfig["address"];
			const char* gatway = serverConfig["gatway"];
			const char* netMask = serverConfig["netMask"];
		    //start-block2
		    IPAddress _ip;
			bool parseSuccess;
			parseSuccess = _ip.fromString(address);
			if (parseSuccess) {
				Serial.println("Address correctly parsed!");
			}

		    IPAddress _gw;
		    parseSuccess = _gw.fromString(gatway);
		    if (parseSuccess) {
				Serial.println("Gatway correctly parsed!");
			}

		    IPAddress _sn;
		    parseSuccess = _sn.fromString(netMask);
		    if (parseSuccess) {
				Serial.println("Subnet correctly parsed!");
			}
		    //end-block2

		    wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
		}
	}else{
	    Serial.println("fail.");
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
    Serial.println(F("WIFIManager connected!"));

	// Start inverter serial
	Serial.print(F("Initializing Inverter serial..."));
	inverter.begin();
	Serial.println(F("initialization done."));

	// Start inizialization of SD cart
	Serial.print(F("Initializing SD card..."));
	if (!SD.begin(CS_PIN)) {
		Serial.println(F("initialization failed!"));
		return;
	}
	Serial.println(F("Inizialization done."));

	// Start thread, and set it every 1 minutes
	LeggiProduzione.onRun(leggiProduzioneCallback);
	LeggiProduzione.setInterval(60 * 1000);

	int timeToRetry = 10;
	while ( WiFi.status() != WL_CONNECTED && timeToRetry>0 ) {
	    delay ( 500 );
	    Serial.print ( "." );
	    timeToRetry--;
	}

	timeClient.begin();
	updateLocalTimeWithNTPCallback();

	UpdateLocalTimeWithNTP.onRun(updateLocalTimeWithNTPCallback);
	UpdateLocalTimeWithNTP.setInterval(60*60 * 1000);

	restServerRouting();
	httpRestServer.begin();
    Serial.println(F("HTTP REST Server Started"));
}

void loop() {
	// Activate thread
	if (LeggiProduzione.shouldRun()) {
		LeggiProduzione.run();
	}

	if (UpdateLocalTimeWithNTP.shouldRun()) {
		UpdateLocalTimeWithNTP.run();
	}

	httpRestServer.handleClient();
	timeClient.update();
}

// We can read/write a file at time in the SD card
File myFileSDCart; // @suppress("Ambiguous problem")

void leggiProduzioneCallback() {
	Serial.print(F("Thread call (leggiProduzioneCallback) --> "));
	Serial.println(getEpochStringByParams(now()));

	tm nowDt = getDateTimeByParams(now());

	// Save cumulative data
	if (nowDt.tm_min % CUMULATIVE_INTERVAL == 0) {
		Aurora::DataCumulatedEnergy dce = inverter.readCumulatedEnergy(CUMULATED_DAILY_ENERGY);
		Serial.print(F("Read state --> "));
		Serial.println(dce.state.readState);

		if (dce.state.readState==1){
			unsigned long energy = dce.energy;

			Aurora::DataDSP dsp = inverter.readDSP(DSP_POWER_PEAK_TODAY_ALL);
			float powerPeak = dsp.value;

			if (energy && energy > 0) {
				String filename = getEpochStringByParams(now(), (char*) "%Y%m")
						+ ".jso";
				String tagName = getEpochStringByParams(now(), (char*) "%d");

				DynamicJsonDocument doc;
				JsonObject obj;

				obj = getJSonFromFile(&doc, filename);

				JsonObject dayData = obj.createNestedObject(tagName);
				dayData["pow"] = energy;
				dayData["powPeak"] = setPrecision(powerPeak, 1);

				Serial.print(F("Store cumulated energy --> "));
//				serializeJson(doc, Serial);
				Serial.print(doc.memoryUsage());
				Serial.println();

				saveJSonToAFile(&doc, filename);
			}
		}
	}

//	Serial.println("CHECK COND");
//	Serial.println(nowDt.tm_min);
//	Serial.println(DAILY_INTERVAL);
//	Serial.println(nowDt.tm_min % DAILY_INTERVAL);

	String scopeDirectory = F("product");
	if (!SD.exists(scopeDirectory)){
		SD.mkdir(scopeDirectory);
	}
	for (int i=0; i<3; i++){
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
		Serial.print(F("Read state --> "));
		Serial.println(dsp.state.readState);
		if (dsp.state.readState == 1){
			float val = dsp.value;

			if (val && val > 0) {
				String dataDirectory = getEpochStringByParams(now(), (char*) "%Y%m%d");
				if (i==0 && !SD.exists(scopeDirectory+'/'+dataDirectory)){
					SD.mkdir(scopeDirectory+'/'+dataDirectory);
				}

				String filenameDir = scopeDirectory+F("/")+dataDirectory+F("/")+filename;

				DynamicJsonDocument doc;
				JsonObject obj;

				obj = getJSonFromFile(&doc, filenameDir);

				JsonArray data;
				if (!obj.containsKey("data")) {
					data = obj.createNestedArray("data");
				} else {
					data = obj["data"];
				}

				JsonObject objArrayData = data.createNestedObject();
				objArrayData["h"] = getEpochStringByParams(now(), (char*) "%H%M");
//				printf("%.01f\n", power);
//				printf("%.01f\n", current);
//				printf("%.01f\n", voltage);
				objArrayData["val"] = setPrecision(val, 1);
//				objArrayData["A"] = setPrecision(current, 1);
//				objArrayData["V"] = setPrecision(voltage, 1);
				Serial.println(F("Store production --> "));
//				serializeJson(doc, Serial);
				Serial.print(doc.memoryUsage());
				Serial.println();

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
			Serial.print(F("Error parsing JSON "));
			Serial.println(error);
		}

		// close the file:
		myFileSDCart.close();

		return doc->as<JsonObject>();
	} else {
		// if the file didn't open, print an error:
		Serial.print(F("Error opening (or file not exists) "));
		Serial.println(filename);

		Serial.println(F("Empty json created"));
		return doc->to<JsonObject>();
	}

}

bool saveJSonToAFile(DynamicJsonDocument *doc, String filename) {
	SD.remove(filename);

	// open the file. note that only one file can be open at a time,
	// so you have to close this one before opening another.
	Serial.println(F("Open file in write mode"));
	myFileSDCart = SD.open(filename, FILE_WRITE);
	if (myFileSDCart) {
		Serial.print(F("Filename --> "));
		Serial.println(filename);

		Serial.print(F("Start write..."));

		serializeJson(*doc, myFileSDCart);

		Serial.print(F("..."));
		// close the file:
		myFileSDCart.close();
		Serial.println(F("done."));

		return true;
	} else {
		// if the file didn't open, print an error:
		Serial.print(F("Error opening "));
		Serial.println(filename);

		return false;
	}
}

void setCrossOrigin(){
	httpRestServer.sendHeader("Access-Control-Allow-Origin", "*");
	httpRestServer.sendHeader("Access-Control-Max-Age", "10000");
	httpRestServer.sendHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
	httpRestServer.sendHeader("Access-Control-Allow-Headers", "*");
};

void getProduction(){
	Serial.println(F("getProduction"));

	setCrossOrigin();

	if (httpRestServer.arg("day")== "" || httpRestServer.arg("type")== "" ){     //Parameter not found
		httpRestServer.send(400, "text/html", "Missing required parameter!");
		Serial.println(F("No parameter"));
	}else{     //Parameter found
		Serial.print(F("Read file: "));
		String filename = "product/"+httpRestServer.arg("day")+"/"+httpRestServer.arg("type")+".jso";

		Serial.println(filename);

		if (SD.exists(filename)){
			myFileSDCart = SD.open(filename);
			if (myFileSDCart){
				if (myFileSDCart.available()){
					Serial.print(F("Stream file..."));
					httpRestServer.streamFile(myFileSDCart, "application/json");
					Serial.println(F("done."));
				}else{
					Serial.println(F("File not found!"));
					httpRestServer.send(204, "text/html", "No content found!");
				}
				myFileSDCart.close();
			}
		}else{
			Serial.println(F("File not found!"));
			httpRestServer.send(204, "text/html", "No content found!");
		}
	}
}

void getStats(){
	Serial.println(F("getMontlyValue"));

	setCrossOrigin();

	if (httpRestServer.arg("month")== ""){     //Parameter not found
		httpRestServer.send(400, "text/html", "Missing required parameter!");
		Serial.println(F("No parameter"));
	}else{     //Parameter found
		Serial.print(F("Read file: "));
		String filename = httpRestServer.arg("month")+".jso";
		if (SD.exists(filename)){
			myFileSDCart = SD.open(filename);
			if (myFileSDCart){
				if (myFileSDCart.available()){
					Serial.print(F("Stream file..."));
					httpRestServer.streamFile(myFileSDCart, "application/json");
					Serial.println(F("done."));
				}else{
					Serial.println(F("File not found!"));
					httpRestServer.send(204, "text/html", "No content found!");
				}
				myFileSDCart.close();
			}
		}else{
			Serial.println(F("File not found!"));
			httpRestServer.send(204, "text/html", "No content found!");
		}
	}
}

void postConfigFile() {
	Serial.println(F("postConfigFile"));

    String postBody = httpRestServer.arg("plain");
    Serial.println(postBody);

	DynamicJsonDocument doc;
	DeserializationError error = deserializeJson(doc, postBody);
	if (error) {
		// if the file didn't open, print an error:
		Serial.print(F("Error parsing JSON "));
		Serial.println(error);

		String msg = error.c_str();

		httpRestServer.send(400, "text/html", "Error in parsin json body! <br>"+msg);

	}else{
		JsonObject postObj = doc.as<JsonObject>();

		Serial.print(F("HTTP Method: "));
		Serial.println(httpRestServer.method());

        if (httpRestServer.method() == HTTP_POST) {
            if ((postObj.containsKey("server"))) {

            	Serial.print(F("Open config file..."));
            	fs::File configFile = SPIFFS.open("/mc/config.txt", "w");
            	if (!configFile) {
            	    Serial.println("fail.");
            	    httpRestServer.send(304, "text/html", "Fail to store data, can't open file!");
            	}else{
            		Serial.println("done.");
            		serializeJson(doc, configFile);
            		httpRestServer.send(201, "text/html", "Find and stored!");
            	}
            }
            else {
            	httpRestServer.send(204, "text/html", "No data found, or incorrect!");
            }
        }
    }
}

void getConfigFile(){
	Serial.println(F("getConfigFile"));

	setCrossOrigin();

	Serial.print(F("Read file: "));
	if (SPIFFS.exists("/mc/config.txt")){
    	fs::File configFile = SPIFFS.open("/mc/config.txt", "r");
		if (configFile){
			if (configFile.available()){
				Serial.print(F("Stream file..."));
				httpRestServer.streamFile(configFile, "application/json");
				Serial.println(F("done."));
			}else{
				Serial.println(F("File not found!"));
				httpRestServer.send(204, "text/html", "No content found!");
			}
			configFile.close();
		}
	}else{
		Serial.println(F("File not found!"));
		httpRestServer.send(204, "text/html", "No content found!");
	}
}

void restServerRouting() {
    httpRestServer.on("/", HTTP_GET, []() {
    	httpRestServer.send(200, "text/html",
            "Welcome to the Inverter Centraline REST Web Server");
    });
    httpRestServer.on("/production", HTTP_GET, getProduction);
    httpRestServer.on("/stats", HTTP_GET, getStats);
    httpRestServer.on("/config", HTTP_POST, postConfigFile);
    httpRestServer.on("/config", HTTP_GET, getConfigFile);
}

void updateLocalTimeWithNTPCallback(){
	Serial.print(F("Thread call (updateLocalTimeWithNTPCallback) --> "));
	Serial.println(getEpochStringByParams(now()));

	if (!fixedTime){
		Serial.println(F("Update NTP Time."));
		if (timeClient.forceUpdate()){
			unsigned long epoch = timeClient.getEpochTime();
			Serial.println(epoch);

			Serial.println(F("NTP Time retrieved."));
			adjustTime(epoch);
			fixedTime = true;
		}else{
			Serial.println(F("NTP Time not retrieved."));

			Serial.println(F("Get Inverter Time."));
			// Get DateTime from inverter
			Aurora::DataTimeDate timeDate = inverter.readTimeDate();

			if (timeDate.state.readState){
				Serial.println(F("Inverter Time retrieved."));
				// Set correct time in Arduino Time librery
				adjustTime(timeDate.epochTime);
				fixedTime = true;
			}else{
				Serial.println(F("Inverter Time not retrieved."));
				Serial.println(F("DANGER, SYSTEM INCONCISTENCY"));
			}
		}

		Serial.println(getEpochStringByParams(now()));
	}
}


float setPrecision(float val, byte precision){
	return ((int)(val*(10*precision)))/(float)(precision*10);
}
