// Connections to Wemos D1 Mini
//
// All sensors running at 3.3 V
//
// HX711 DOUT  = pin D6 (GPIO 12)
// HX711 SCK = pin D5 (GPIO 14)

// Load cell connection to HX711:
//
// RED = E+
// BLACK = E-
// WHITE = A-
// GREEN = A+

// PIR sensor = D2
// Light sensor = A0
// Temp/Humidity DHT22 = D4
// Rain Sensor digital out = D3

#include "ThingSpeak.h"  // https://github.com/mathworks/thingspeak-arduino
#include <ESP8266WiFi.h>
#include "dht.h";  // http://playground.arduino.cc/Main/DHTLib
#include <ArduinoOTA.h>
#include "HX711.h"  // https://github.com/bogde/HX711

const char* ssid = "<your SSID>"; // Your WLAN SSID
const char* password = "<your PWD>"; // Your WLAN Password

// OTA instructions: https://github.com/esp8266/Arduino/blob/master/doc/ota_updates/ota_updates.md#arduinoota-configuration
// Wifimanager for wifi-AP if no network is found: https://github.com/tzapu/WiFiManager
const char* OTA_hostname = "WeMosWeather2"; // The hostname that you will find under network ports

int status = WL_IDLE_STATUS;
WiFiClient  client;

dht DHT;
#define DHT22_PIN D7

int a;  // activity
int asum;  // activity total
int rain; // Rain
int rainsum; // Rain total
int weight; // Weight
int lastweight;
int newweight;
unsigned long last_check; // Timer for writing to thingspeak
unsigned long last_activity_check;  // Timer for checking activity and rain
const int ledPin = BUILTIN_LED; // WeMos D1 Mini internal LED for indication (ledpin on Wemos D1 mini is D4)
int tempNow; // Temperature from DHT22
int humNow;  // Humidity from DHT22

/*
  *****************************************************************************************
  **** Visit https://www.thingspeak.com to sign up for a free account and create
  **** a channel.  The video tutorial http://community.thingspeak.com/tutorials/thingspeak-channels/ 
  **** has more information. You need to change this to your channel, and your write API key
  **** IF YOU SHARE YOUR CODE WITH OTHERS, MAKE SURE YOU REMOVE YOUR WRITE API KEY!!
  *****************************************************************************************/
unsigned long myChannelNumber = <your channel number>;
const char * myWriteAPIKey = "<your API KEY>";

HX711 scale(12, 14);    // parameter "gain" is ommited; the default value 128 is used by the library

void setup() {

  pinMode(D2, INPUT); // prepare digital input for PIR sensor
  pinMode(D3, INPUT); // prepare digital input for rain sensor
  pinMode(ledPin, OUTPUT); // prepare internal LED for indication (ledpin on Wemos D1 mini is D4)

  Serial.begin(115200);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    // Connecting to WLAN and blinking slow while doing it.
    digitalWrite(ledPin, LOW); // turn LED on
    delay(250);
    digitalWrite(ledPin, HIGH); // turn LED off
    Serial.print(".");
    delay(250);
  } 
  
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());  

 // Confirmation blinks. Yeah! Connected. Fast blinks.
   for (int i=0; i <= 20; i++){
    digitalWrite(ledPin, LOW); // turn LED on
    delay (40);
    digitalWrite(ledPin, HIGH); // turn LED off
    delay (40);
  } // End for loop
  
  ThingSpeak.begin(client);

  //////////////////////////
  //  Code to set up OTA  //
  //////////////////////////
  
  Serial.println();
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(OTA_hostname);

  // No authentication by default
  // ArduinoOTA.setPassword(OTA_password);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  ///////////////////////////////
  //  END: Code to set up OTA  //
  ///////////////////////////////

  scale.set_scale(221.f);      // this value is obtained by calibrating the scale with known weights; see the README for details:

  /*
   * 1. Call set_scale() with no parameter.
     2. Call tare() with no parameter.
     3. Place a known weight on the scale and call get_units(10).
     4. Divide the result in step 3 to your known weight. You should get about the parameter you need to pass to set_scale.
     5. Adjust the parameter in step 4 until you get an accurate reading.

     Example code to run for calibration:
     Serial.begin(9600);
     Serial.println("Starting");
     scale.set_scale();
     scale.tare();
     Serial.println("You now have 30 seconds to place a known weight on tha scale.");
     delay (30 * 1000);
     Serial.print("Result: ");
     Serial.println(scale.get_units(10););
   */
  
  scale.tare();                // reset the scale to 0

}

void loop() {

  ArduinoOTA.handle(); // Needs to be here for OTA

  // Sensors and ThingSpeak every xx seconds
  if(last_check <  millis()){

    // lastweight = newweight;
    
    // read the light sensor input on analog pin 0:
    int light = analogRead(A0);
    
    // 1024 = dark, 0=light, so reverse this. More logical to have a high value for more light and 0 for darkness
    light = 1024 - light; 
  
    // READ temp and humidity fro DHT22
    int chk = DHT.read22(DHT22_PIN);
    tempNow = DHT.temperature;
    humNow = DHT.humidity;

    // Read weight from load cell
    delay(100);
    scale.power_up();
    delay(100);
    weight = scale.get_units(20);
    // weight = newweight;

    // Write the stuff to serial for debug purposes
    Serial.print("Temp: ");
    Serial.println(tempNow);
    Serial.print("Humidity: ");
    Serial.println(humNow);
    Serial.print("Light: ");
    Serial.println(light);
    Serial.print("Motion: ");
    Serial.println(asum);
    Serial.print("Weight: ");
    Serial.println(weight, 1);
    Serial.print("Rain: ");
    Serial.println(rainsum);
    Serial.println();

    // Prepare values for ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
    // pieces of information in a channel.
    ThingSpeak.setField(1,light);
    ThingSpeak.setField(2,tempNow);
    ThingSpeak.setField(3,humNow);
    ThingSpeak.setField(4,asum);
    ThingSpeak.setField(5,weight);
    ThingSpeak.setField(6,rainsum);
  
    // Write all the fields to the channel at Thingspeak.
    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);  
  
    // Set last check so that this runs every xx minutes
    last_check = millis() + 5*60*1000 ;  

    // Zero the counters for activity and rain
    asum = 0;
    rainsum = 0;
    
  } // end check sensors and send to ThingSpeak

  scale.power_down();  // put the ADC in sleep mode

  // Check and add for activity and rain every half second
  if(last_activity_check <  millis()){
    a = digitalRead(D2);
    rain = !digitalRead(D3);
    asum = asum + a;
    rainsum = rainsum + rain;
    // Set last check so that this runs every 500 milliseconds
    last_activity_check = millis() + 500 ;  
  } // End last activity check
  
}
