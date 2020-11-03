// Project located at http://cactusprojects.com/iot-geiger-counter-influxdb/

#include <ESP8266WiFi.h>
#include <InfluxDb.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "RunningAverage.h"

#define INFLUXDB_HOST "192.168.1.XXX"
#define WIFI_SSID "XXXXXXXXXXXX"
#define WIFI_PASS "XXXXXXXXXXXX"
#define DATABASE "XXXXXXXXXXXX"
#define MEASUREMENT "XXXXXXXXXXXX"
#define DEVICE "XXXXXXXXXXXX"
#define ID "Geiger_Counter"
#define LOG_PERIOD 60000

Influxdb influx(INFLUXDB_HOST);
Adafruit_PCD8544 display = Adafruit_PCD8544(2, 0, 4, 5, 16); //LCD 1.5 Inch (Nokia 5110)84×48 (x,y) pixels
RunningAverage raMinute(60);

//################
int debug = 1; //#
//################

int loopCount = 0;
int cpm = 0;
int cpm_max = 0;
int cpm_1hr_avg = 0;
int cpm_ravg = 0;
int counts = 0;
int cal_factor = 1;
int wifiStatus;

unsigned long currentMillis;
unsigned long previousMillis; //variable for time measurement

void setup(){                                               
  Serial.begin(9600);      // start serial monitor
  delay(1000);
  Serial.println("");
  Serial.println("");
  Serial.println("Setup Routine of ESP8266 Geiger Counter");

  display.begin();
  display.setContrast(55);
  display.display();        // show adafruid splashscreen
  delay(2000);
  display.clearDisplay();   // clears the screen and buffer

  pinMode(LED_BUILTIN, OUTPUT); //D4
  digitalWrite(LED_BUILTIN, HIGH); //Turns it off

  pinMode(14, INPUT_PULLUP);                  // set pin INT0 input for capturing GM Tube events / GPIO5 = D1
  attachInterrupt(14, tube_pulse, FALLING); //defines interrupts

  raMinute.clear();

  influx.setDb(DATABASE);

  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0); display.setTextSize(1);display.print("Up Hrs: ");display.setCursor(48,0);display.print("0");
  display.setCursor(0,9); display.setTextSize(1);display.print("CPMi 1m:");
  display.setCursor(0,17);display.setTextSize(1);display.print("CPM avg:");
  display.setCursor(0,25);display.setTextSize(1);display.print("CPM Max:");
  display.setCursor(0,33);display.setTextSize(1);display.print("uSv/hr: ");
  display.setCursor(0,41);display.setTextSize(1);display.print("Not Connected");
  display.display();

    wifiStatus = WiFi.status();
    if (wifiStatus != WL_CONNECTED) {   
      new_connection();
    }
    else {
        display.fillRect(0,41,84,48, WHITE);
        display.setCursor(0,41);
        display.print(WiFi.localIP());
        display.display();
    }
    

  if (debug == 1) {Serial.println("Setup Complete.");}
}

void loop(){                                              
  currentMillis = millis();
  if(currentMillis - previousMillis > LOG_PERIOD){
    cpm = counts * cal_factor;                        

    raMinute.addValue(cpm);
    cpm_ravg = raMinute.getAverage();

    if (cpm > cpm_max){
      cpm_max = cpm;
    }
    
    Serial.print("CPM: ");                         
    Serial.println(cpm);                          

    display.fillRect(48,0,40,40, WHITE);
    display.display();
    display.setCursor(48,0);display.print(loopCount*.0166);
    display.setCursor(48,9);display.print(cpm);
    display.setCursor(48,17);display.print(cpm_ravg);
    display.setCursor(48,25);display.print(cpm_max);
    display.setCursor(48,33);display.print(cpm_ravg*0.0057);
    display.display();

    Serial.println("Attempting to write to DB");   
    counts = 0;

    InfluxData row(MEASUREMENT);
    row.addTag("Device", DEVICE);
    row.addTag("ID", ID);
    row.addValue("CPM", cpm);  
    row.addValue("LoopCount", loopCount);
    row.addValue("RandomValue", random(0, 100));
  
    wifiStatus = WiFi.status();
    while ( wifiStatus != WL_CONNECTED )
        {
          new_connection();
        }
  
    influx.write(row);
    if (debug == 1) {Serial.println("Wrote Data.");}
  
    //WiFi.mode(WIFI_OFF); // Probably turn off Wifi if want to save battery
    //WiFi.forceSleepBegin();
    //delay( 1 );
  
    status_blink();
    previousMillis = currentMillis;
    loopCount++;
  }
}

ICACHE_RAM_ATTR        //Needed to fix ISR not in IRAM boot error
void tube_pulse(){     //procedure for capturing events from interrupt
  counts++;
}

void new_connection() {
  
    wifiStatus = WiFi.status();
    
    if (wifiStatus != WL_CONNECTED) {   
       
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        int loops = 0;
        int retries = 0;
        display.fillRect(0,41,84,48, WHITE);
        display.setCursor(0,41);
        display.print("Not Connected");
        display.display();
       
        while (wifiStatus != WL_CONNECTED)
        {
          retries++;
          if( retries == 300 )
          {
              if (debug == 1) {Serial.println( "No connection after 300 steps, powercycling the WiFi radio. I have seen this work when the connection is unstable" );}
              WiFi.disconnect();
              delay( 10 );
              WiFi.forceSleepBegin();
              delay( 10 );
              WiFi.forceSleepWake();
              delay( 10 );
              WiFi.begin( WIFI_SSID, WIFI_PASS );
          }
          if ( retries == 600 )
          {
              if (debug == 1) {Serial.println( "No connection after 600 steps. WiFi connection failed, disabled WiFi and waiting for a minute" );}
              WiFi.disconnect( true );
              delay( 1 );
              WiFi.mode( WIFI_OFF );
              WiFi.forceSleepBegin();
              delay( 10 );
              retries = 0;
              
              if( loops == 3 )
              {
                  if (debug == 1) {Serial.println( "That was 3 loops, still no connection so let's go to deep sleep for 2 minutes" );}
                  Serial.flush();
                  ESP.deepSleep( 120000000, WAKE_RF_DISABLED );
              }     
          }
          delay(50);
          wifiStatus = WiFi.status();
        }
        
        wifiStatus = WiFi.status();
        Serial.print("WiFi connected, IP address: ");Serial.println(WiFi.localIP());
        display.fillRect(0,41,84,48, WHITE);
        display.setCursor(0,41);
        display.print(WiFi.localIP());
        display.display();
    }
}

void status_blink() {
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level   
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED on (Note that LOW is the voltage level
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level   
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED on (Note that LOW is the voltage level
}
