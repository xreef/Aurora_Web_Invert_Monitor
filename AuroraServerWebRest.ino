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

void restServerRouting();

WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 2*60*60, 60*60*1000);

Thread UpdateLocalTimeWithNTP = Thread();

bool fixedTime = false;

float setPrecision(float val, byte precision);

#define DSP_GRID_POWER_ALL_FILENAME									"power.jso"		/* Global */
#define DSP_GRID_CURRENT_ALL_FILENAME								"current.jso"
#define DSP_GRID_VOLTAGE_ALL_FILENAME								"voltage.jso"

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

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(10);
	WiFi.hostname(hostname);
	wifi_station_set_hostname(hostname);

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

    Serial.print("IP --> ");
    Serial.println(WiFi.localIP());
    Serial.print("GW --> ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("SM --> ");
    Serial.println(WiFi.subnetMask());

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

	if (LeggiStatoInverter.shouldRun()) {
		LeggiStatoInverter.run();
	}

	if (ManageStaticData.shouldRun()) {
		ManageStaticData.run();
	}

	if (UpdateLocalTimeWithNTP.shouldRun()) {
		UpdateLocalTimeWithNTP.run();
	}

	httpRestServer.handleClient();
	timeClient.update();
}

// We can read/write a file at time in the SD card
File myFileSDCart; // @suppress("Ambiguous problem")

void leggiStatoInverterCallback() {
	Serial.print(F("Thread call (LeggiStatoInverterCallback) --> "));
	Serial.println(getEpochStringByParams(now()));

	Aurora::DataState dataState = inverter.readState();
	if (dataState.state.readState == true) {
		Serial.println(F("done."));

		Serial.print(F("Create json..."));

		String scopeDirectory = F("states");
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

		Serial.println(F("done."));

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

		bool inverterProblem = dataState.alarmState > 0
				|| dataState.inverterState != 2;
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

			Serial.print(F("Inverter problem --> "));
			Serial.println(inverterProblem);

			Serial.print(F("firstElement --> "));
			Serial.println(firstElement);

			Serial.print(F("Inverter problem --> "));
			Serial.println(inverterProblem);

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

				Serial.println(F("Store alarms --> "));
				//				serializeJson(doc, Serial);
				Serial.print(docAS.memoryUsage());
				Serial.println();

				if (!SD.exists(scopeDirectory + '/' + dayDirectory)) {
					SD.mkdir(scopeDirectory + '/' + dayDirectory);
				}

				saveJSonToAFile(&docAS, filenameAL);
			}

		}
//		}
	}

}

void manageStaticDataCallback () {
	Serial.print(F("Thread call (manageStaticDataCallback) --> "));
	Serial.println(getEpochStringByParams(now()));

	Serial.print(F("Data version read... "));
	Aurora::DataVersion dataVersion = inverter.readVersion();

	Serial.print(F("Firmware release read... "));
	Aurora::DataFirmwareRelease firmwareRelease = inverter.readFirmwareRelease();

	Serial.print(F("System SN read... "));
	Aurora::DataSystemSerialNumber systemSN = inverter.readSystemSerialNumber();

	Serial.print(F("Manufactoru Week Year read... "));
	Aurora::DataManufacturingWeekYear manufactoryWeekYear = inverter.readManufacturingWeekYear();

	Serial.print(F("systemPN read... "));
	Aurora::DataSystemPN systemPN = inverter.readSystemPN();

	Serial.print(F("configStatus read... "));
	Aurora::DataConfigStatus configStatus = inverter.readConfig();

	if (dataVersion.state.readState == true){
    	Serial.println(F("done."));

    	Serial.print(F("Create json..."));

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

		Serial.println(F("done."));

		saveJSonToAFile(&doc, filename);
	}

}

void leggiProduzioneCallback() {
	Serial.print(F("Thread call (leggiProduzioneCallback) --> "));
	Serial.println(getEpochStringByParams(now()));

	tm nowDt = getDateTimeByParams(now());

	// Save cumulative data
	if (nowDt.tm_min % CUMULATIVE_INTERVAL == 0) {
		Aurora::DataCumulatedEnergy dce = inverter.readCumulatedEnergy(
		CUMULATED_DAILY_ENERGY);
		Serial.print(F("Read state --> "));
		Serial.println(dce.state.readState);

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

				Serial.print(F("Store cumulated energy --> "));
//				serializeJson(doc, Serial);
				Serial.print(doc.memoryUsage());
				Serial.println();

				saveJSonToAFile(&doc, filename);
			}

		}
	}

	// Save cumulative data
	if (nowDt.tm_min % CUMULATIVE_TOTAL_INTERVAL == 0) {

		Serial.println("Get all totals.");
		Aurora::DataCumulatedEnergy dce = inverter.readCumulatedEnergy(
		CUMULATED_TOTAL_ENERGY_LIFETIME);
		unsigned long energyLifetime = dce.energy;
		Serial.println(energyLifetime);

		if (dce.state.readState == 1) {

			dce = inverter.readCumulatedEnergy(CUMULATED_YEARLY_ENERGY);
			unsigned long energyYearly = dce.energy;
			Serial.println(energyYearly);

			dce = inverter.readCumulatedEnergy(CUMULATED_MONTHLY_ENERGY);
			unsigned long energyMonthly = dce.energy;
			Serial.println(energyMonthly);

			dce = inverter.readCumulatedEnergy(CUMULATED_WEEKLY_ENERGY);
			unsigned long energyWeekly = dce.energy;
			Serial.println(energyWeekly);

			dce = inverter.readCumulatedEnergy(CUMULATED_DAILY_ENERGY);
			unsigned long energyDaily = dce.energy;
			Serial.println(energyDaily);

			String scopeDirectory = F("states");
			if (!SD.exists(scopeDirectory)) {
				SD.mkdir(scopeDirectory);
			}
			if (energyLifetime) {
				String filename = scopeDirectory + F("/lastStat.jso");
				String tagName = getEpochStringByParams(now(), (char*) "%d");

				DynamicJsonDocument doc;
				JsonObject obj;

				obj = getJSonFromFile(&doc, filename);

				if (obj.containsKey("energyMonthly")){
					if (obj["energyMonthly"]>energyMonthly){
						String dir = scopeDirectory +"/months";
						if (!SD.exists(dir)) {
							SD.mkdir(dir);
						}

						obj.remove("energyWeekly");
						obj.remove("energyDaily");

						String filenamem = dir +'/'+getEpochStringByParams(now(), (char*) "%Y")+ F(".jso");
						DynamicJsonDocument docM;
						JsonObject objM = getJSonFromFile(&docM, filenamem);

						JsonArray dataM;
						if (!objM.containsKey("data")) {
							dataM = objM.createNestedArray("data");
						} else {
							dataM = objM["data"];
						}

						dataM.add(obj);

						saveJSonToAFile(&doc, filenamem);
					}
				}
				if (obj.containsKey("energyYearly")){
					if (obj["energyYearly"]>energyYearly){
						obj.remove("energyWeekly");
						obj.remove("energyMonthly");
						obj.remove("energyDaily");

						String filenamem = scopeDirectory + F("/years.jso");
						DynamicJsonDocument docM;
						JsonObject objM = getJSonFromFile(&docM, filenamem);

						JsonArray dataM;
						if (!objM.containsKey("data")) {
							dataM = objM.createNestedArray("data");
						} else {
							dataM = objM["data"];
						}

						dataM.add(obj);

						saveJSonToAFile(&doc, filenamem);
					}
				}

				obj["lastUpdate"] = getEpochStringByParams(now());

//			JsonObject dayData = obj.createNestedObject(tagName);
				obj["energyLifetime"] = energyLifetime;
				obj["energyYearly"] = energyYearly;
				obj["energyMonthly"] = energyMonthly;
				obj["energyWeekly"] = energyWeekly;
				obj["energyDaily"] = energyDaily;

				Serial.print(F("Store energyLifetime energy --> "));
				//				serializeJson(doc, Serial);
				Serial.print(doc.memoryUsage());
				Serial.println();

				saveJSonToAFile(&doc, filename);
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
			Serial.print(F("Read state --> "));
			Serial.println(dsp.state.readState);
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
					Serial.println(F("Store production --> "));
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

void streamFileOnRest(String filename){
	if (SD.exists(filename)){
		myFileSDCart = SD.open(filename);
		if (myFileSDCart){
			if (myFileSDCart.available()){
				Serial.print(F("Stream file..."));
				httpRestServer.streamFile(myFileSDCart, "application/json");
				Serial.println(F("done."));
			}else{
				Serial.println(F("Data not available!"));
				httpRestServer.send(204, "text/html", "Data not available!");
			}
			myFileSDCart.close();
		}else{
			Serial.println(F("File not found!"));
			httpRestServer.send(204, "text/html", "No content found!");
		}
	}else{
		Serial.println(F("File not found!"));
		httpRestServer.send(204, "text/html", "File not exist!");
	}
}

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

		streamFileOnRest(filename);
	}
}

void getProductionTotal(){
	Serial.println(F("getProduction"));

	setCrossOrigin();


	Serial.print(F("Read file: "));
	String scopeDirectory = F("states");
	String filename =  scopeDirectory+F("/lastStat.jso");


	Serial.println(filename);

	streamFileOnRest(filename);

}

void getMontlyValue(){
	Serial.println(F("getMontlyValue"));

	setCrossOrigin();
	String scopeDirectory = F("monthly");

	if (httpRestServer.arg("month")== ""){     //Parameter not found
		httpRestServer.send(400, "text/html", "Missing required parameter!");
		Serial.println(F("No parameter"));
	}else{     //Parameter found
		Serial.print(F("Read file: "));
		String filename = scopeDirectory+'/'+httpRestServer.arg("month")+".jso";
		streamFileOnRest(filename);
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

void getInverterInfo(){
	Serial.println(F("getInverterInfo"));

	setCrossOrigin();

	String scopeDirectory = F("static");

	Serial.print(F("Read file: "));
	String filename = scopeDirectory+"/invinfo.jso";
	streamFileOnRest(filename);
}

void inverterDayWithProblem() {
	Serial.println(F("inverterDayWithProblem"));

	setCrossOrigin();

	String scopeDirectory = F("states");

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
		Serial.println(entry.name());
		if (entry.isDirectory()) {
			data.add(entry.name());
		}
		entry.close();
	}

	myFileSDCart.close();

	if (data.size() > 0) {
		Serial.print(F("Stream file..."));
		String buf;
		serializeJson(rootObj, buf);
		httpRestServer.send(200, "application/json", buf);
		Serial.println(F("done."));
	} else {
		Serial.println(F("No content found!"));
		httpRestServer.send(204, "text/html", "No content found!");
	}
}

void getInverterDayState() {
	Serial.println(F("getInverterDayState"));

	setCrossOrigin();

	String scopeDirectory = F("states");

	Serial.print(F("Read file: "));

	if (httpRestServer.arg("day") == "") {     //Parameter not found
		httpRestServer.send(400, "text/html", "Missing required parameter!");
		Serial.println(F("No parameter"));
	} else {     //Parameter found

		String filename;
		String parameter = httpRestServer.arg("day");
		filename = scopeDirectory + '/' + parameter + "/alarms.jso";

		Serial.println(filename);

		streamFileOnRest(filename);
	}
}

void getInverterLastState(){
	Serial.println(F("getInverterLastState"));

	setCrossOrigin();

	String scopeDirectory = F("states");

	Serial.print(F("Read file: "));

	String filename;
	String parameter = httpRestServer.arg("day");
	filename = scopeDirectory+"/alarStat.jso";

	streamFileOnRest(filename);
}

void restServerRouting() {
    httpRestServer.on("/", HTTP_GET, []() {
    	httpRestServer.send(200, "text/html",
            "Welcome to the Inverter Centraline REST Web Server");
    });
    httpRestServer.on("/production", HTTP_GET, getProduction);
    httpRestServer.on("/productionTotal", HTTP_GET, getProductionTotal);
    httpRestServer.on("/monthly", HTTP_GET, getMontlyValue);

    httpRestServer.on("/config", HTTP_POST, postConfigFile);
    httpRestServer.on("/config", HTTP_GET, getConfigFile);

    httpRestServer.on("/inverterInfo", HTTP_GET, getInverterInfo);
    httpRestServer.on("/inverterState", HTTP_GET, getInverterLastState);
    httpRestServer.on("/inverterDayWithProblem", HTTP_GET, inverterDayWithProblem);
    httpRestServer.on("/inverterDayState", HTTP_GET, getInverterDayState);
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
