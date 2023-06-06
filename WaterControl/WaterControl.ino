/*This code is made out from RUC's fablab plantmonitoring project https://fablab.ruc.dk/plantmonitoring/*/
#include "DHTesp.h"
#include "Ticker.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <Preferences.h>
#include "credentials.h"
#include <NTPClient.h>
#include <Arduino_JSON.h>
#include "time.h"

//Defining the unit name and what interval that sends data to google sheet
#define UNITNAME "Water%20Sensor%201"
#define UPDATEINTERVAL 60000 
#define SERVERNAME "Hallo" // Don't have a server need to change name if there is one 

//Soil humidity threshold and makes then gobal
int AlarmDryThreshold  = 2400;
int AlarmWetThreshold = 1700;
int VeryMoistThreshold = 1900;
int MoistThreshold  = 2100;
int SlightMoisture  = 2300;

//Pins
const int LEDPIN = 16;
const int SOILPIN = 32;
const int SOLARPIN = 35;
const int dhtPin = 22;

// Setting up gobal variables
int waterlevel = 0; // soil humidity
unsigned long Timer = 0; // the over all timer
float temp = 0;  // temperature
float rh = 0;    // Relative humidity
unsigned int alarmstatus = 0;
unsigned long updateTimer = 0;
unsigned long pumpUpdateTimer = 0;
unsigned long tempAlarmTimer = 0;
long last_display_update_millis = 0;
boolean toggle = 0;
int disconnected_seconds = 0;
int solarvalue = 0;
float dewPoint = 0;
static unsigned int Last_Event_Time;
String UNIT = "Plant%20Butterfly";
String payload;

// // character buffer for storing URL, text and network status in.
char dataText[1024];
char text[1024];
char netStatus[512];

// Making the HTTpClient and DHTesp
HTTPClient http;
DHTesp dht;
WiFiMulti wifiMulti;
Preferences preferences;
Ticker tinkerTemp;
WiFiClient client;

// getting the newvalues for the temperature and humidity from the dht sensor
TempAndHumidity newValues = dht.getTempAndHumidity();

//Url for google sheet
String url = "https://script.google.com/macros/s/AKfycbwd-TUcT2GC7sMph0IS8Agh96oFIDPS1O8ZzWr5VNprH6Y9os7XakDSTnHVMYjeVe4hwQ/exec?";

//Making the final url that is send up all in it can change 
String finalurl; 
// Making the temptask, getTemperature and triggerGetTemp global 
void tempTask(void *pvParameters);
bool getTemperature();
void triggerGetTemp();

//Starting temperature before reading as null
TaskHandle_t tempTaskHande = NULL;

//Starts the task as false
bool taskEnabled = false;

//initilize the tempratur sensor
bool initTemp() {
  byte resultValue = 0;
  dht.setup(dhtPin, DHTesp::DHT11);
  Serial.println("DHT is running"); 

  xTaskCreatePinnedToCore(
    tempTask,
    "tempTask",
    4000,
    NULL,
    5,
    &tempTaskHande,
    1);
  // If the temperatur sensor fails or it can't finde the sensor
  if (tempTaskHande == NULL) {
    Serial.println("Failed to start temperatur sensor ");
    return false;
  } else {
    // Set the update interval for the temperatur sensor
    tinkerTemp.attach(10, triggerGetTemp);
  }
  return true;
}
//If taskhander is on then get temperature
void triggerGetTemp() {
  if (tempTaskHande != NULL) {
    xTaskResumeFromISR(tempTaskHande);
  }
}
//Getting the temeperature if the sensor is on
void getTemp() {
  if (tempTaskHande != NULL) {
    xTaskResumeFromISR(tempTaskHande);
  }
}
//Looks after temptask id on if yes then get temperature if not set temperature to null
void tempTask(void *pvParameters) {
  while (1) {
    if (taskEnabled) {
      getTemperatur();
    }
    vTaskSuspend(NULL);
  }
}
//prints and gets temperatur, humidity, dewpoint, soil humidity, solarvalue and windspped and addes latitude and longitude to the print out 
bool getTemperatur() {
  TempAndHumidity newValues = dht.getTempAndHumidity();

  if (dht.getStatus() != 0) {
    Serial.println("DHT11 Error :" + String(dht.getStatusString()));
    return false;
  }

  temp = newValues.temperature; // read a new value of the sensor
  rh = newValues.humidity;// read a new value of the sensor
  dewPoint = dht.computeDewPoint(newValues.temperature, newValues.humidity); // calculate the dewpoint
  waterlevel = analogRead(SOILPIN);  // reads a new value of the sensor
  solarvalue = analogRead(SOLARPIN);// reads a new value of the sensor

  // prints all the values from the higrow and solarpanel 
  Serial.println("Soil humidty: "+ String(waterlevel) +" Temperatur is " + String(temp ) + " Humidity is " + String(rh ) + " DewPoint is " + String(dewPoint ) + " SolarValue is " + String(solarvalue ));
  
  //prints the temperature, humidity, dewpoint and solarvalue it did not work
  sprintf(dataText, "temp%%20%.1f%%20Humidity%%20RH%%20%.1f%%dewPoint%%20%.1f.1f%%20solarvalue%%20%.1f",
          temp,
          rh,
          dewPoint,
          solarvalue);
  return true;
}
// Is used to get the network status and allows for a macAddress so it can interact with mac computers.
void displayUpdate() {
  netStatus[0] = '\0';
  sprintf(netStatus, "%s %s %s %ddBm %i %s",
          WIFISSID,
          WiFi.macAddress().c_str(),
          WiFi.localIP().toString().c_str(),
          WiFi.RSSI(),
          WiFi.status(),
          UNITNAME);

  Serial.println(netStatus);
}

//Setup the the device
void setup() {
  Serial.begin(115200);
  Serial.println("Boot");
  initTemp(); // begins the temperature taking 
  wifiMulti.addAP(WIFISSID, WIFIPASS); // connects with the wifi ssid and password
  preferences.begin("WaterSensor1", false); 
  if (wifiMulti.run() == WL_CONNECTED) { // check wifi connection

    Serial.println("Wifi connneted");

  }
  // Signal end of setup() to tasks
  taskEnabled = true;
}
//the loop
void loop() {
  if (!taskEnabled) {
    delay(2000);
    taskEnabled = true;
    if (tempTaskHande != NULL) {
      vTaskResume(tempTaskHande);
    }
  }
  yield(); 
  // Check if there is wifi conmection very 1000 milli sec 
  if (millis() > last_display_update_millis > 1000) {

    last_display_update_millis = millis();

    displayUpdate();

    if (WiFi.status() == WL_CONNECTED) {
      toggle = !toggle;
      disconnected_seconds = 0;
    } else {
      toggle = 0;
      disconnected_seconds++;
    }
  }
  // if the device is disconnect for more then 60 sec then restart the device 
  if (disconnected_seconds > 60) {
    ESP.restart();
  }

  //Runs the programes and input the soil humidity, relative humidity, temperature, windspeed and solarvalue then runs the pump if need
  if (millis() - updateTimer > UPDATEINTERVAL || updateTimer == 0) {
     //If alarmstatus is 2 all wronge either need more water or less water if alarmstatus is 1 need to check up on plant
  if (waterlevel > AlarmDryThreshold) {
    sprintf(text, "%s%s",
            "Warning%20soil%20dry%20",
            dataText);
    getTemperatur(); // check temperature, humidity and solarvalue
    alarmstatus = 2;
    Serial.println("Water Level is "+ waterlevel);
  } else if (waterlevel < AlarmWetThreshold) {
    sprintf(text, "%s%s",
            "Warning%20soil%20too%20wet%20",
            dataText);
    getTemperatur();    alarmstatus = 2;
    Serial.println("Water Level is " + waterlevel);
  } else if(waterlevel < SlightMoisture && waterlevel > AlarmDryThreshold){
    sprintf(text, "%s%s",
            "Warning%20soil%20slight%20dry%20",
            dataText);
    getTemperatur();
    alarmstatus = 1;
    Serial.println("Water Level is " + waterlevel);
  }else if(waterlevel < VeryMoistThreshold && waterlevel > MoistThreshold){
    sprintf(text, "%s%s",
            "Warning%20soil%20Very%moist%20",
            dataText);
    getTemperatur();
    alarmstatus = 1;
    Serial.println("Water Level is " + waterlevel);
  }else if(waterlevel > VeryMoistThreshold && waterlevel < AlarmWetThreshold){
    sprintf(text, "%s%s",
            "Warning%20soil%20Very%20moist%20",
            dataText);
    getTemperatur();
    alarmstatus = 1;
    Serial.println("Water Level is " + waterlevel);
  } else {
    sprintf(text, "%s%s",
            "Soil%20probably%20ok%20",
            dataText);
    getTemperatur();
    alarmstatus = 0;
    Serial.println("Water Level is " + waterlevel);
  }
    statusUpdate(); // sends data to google sheet

    updateTimer = millis(); // reset timer 
    
  }
}
//sends update to the preperdt google sheet
String statusUpdate() {
  Serial.print("[HTTP] begin...\n");
  
 finalurl = url +"unit="+ UNIT + "&soil=" +  waterlevel  + "&temp=" + temp + "&rh=" + rh + "&dewPoint=" + dewPoint + "&solarvalue=" + solarvalue + "&alarmstatus="+ alarmstatus;

  http.begin(finalurl);

  Serial.print("[HTTP] GET...\n");
  Serial.println(finalurl);
  // start connection and send HTTP header
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  // No longer looks for response
  http.end(); //Free's the resources

  return payload;
}