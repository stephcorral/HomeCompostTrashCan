/* 
 * Home Compost Trash Can
 * Author: Stephanie Corral
 * Date: 4-8-2025
 * 
 * Air Quality
 * Load Cell
 * motors
 * fans
 * 
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

// Include Particle Device OS APIs
#include "Particle.h"
#include <Adafruit_MQTT.h>
#include "HX711.h"
#include "Adafruit_MQTT_SPARK.h"
#include "credentials.h"
#include "Air_Quality_Sensor.h"
#include "neopixel.h"
#include "Colors.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_GFX.h"
#include "Adafruit_BME280.h"


//create TCP Client
TCPClient TheClient;
//setup MQTT Server and login
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER, AIO_SERVERPORT,AIO_USERNAME, AIO_KEY);
//Feeds
Adafruit_MQTT_Publish pubWeight = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/trashdrawer");
Adafruit_MQTT_Publish pubAlert = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/alertdrawer");
//variables for publish weight feed
unsigned int last, lastTime;
float pubValue;
String greenAlert, yellowAlert, orangeAlert, redAlert; 

//air quality sensor variables
AirQualitySensor sensor(A0);
int quality, sensorValue;

//neopixel variables
const int PIXELCOUNT = 12;
Adafruit_NeoPixel pixel ( PIXELCOUNT , SPI1 , WS2812B ); 

//OLED variables
const int OLED_RESET =-1;
Adafruit_SSD1306 display(OLED_RESET);
String date, dateTime, timeOnly;

//covered if anything should happen to OLED
#if (SSD1306_LCDHEIGHT !=64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!!!");
#endif

//BME SETUP
Adafruit_BME280 bme;
bool status;
const int hexAddress = 0x76;
float temp, humidity;
const int DEGREE_SYM = 0xB0;

//functions for publishing weight
void MQTT_connect();
bool MQTT_ping();

//variables for load cell DT, SCK
HX711 myScale(A1, A2);
const int CALIFACTOR =1045;
const int SAMPLES = 10;
float weight, rawData, calibration;
int offset;

SYSTEM_MODE(SEMI_AUTOMATIC);

SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler(LOG_LEVEL_INFO);

void setup() {
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);
    
    WiFi.on();
    WiFi.connect();
    while (WiFi.connecting())
    {
        Serial.printf(".");
    }  

    //load cell weight set up
    myScale.set_scale();
    delay(5000);
    myScale.tare();
    myScale.set_scale(CALIFACTOR);

    //neopixel set up
    pixel.begin();
    pixel.setBrightness(20);
    pixel.show();

    //setup bme
    status = bme.begin(hexAddress);
    if(status == false){
      Serial.printf("Failed to Start!!!");
    }
    //OLED Display Startup
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.display();
    display.clearDisplay();
    display.setTextSize(1);
    display.setRotation(2);
    display.setCursor(0,0);
    display.setTextColor(WHITE);
    display.display();
    delay(5000);
    display.clearDisplay();

    //setup Time and Date
    Time.zone(-6);
    Particle.syncTime();
}

void loop() {
    MQTT_connect();
    MQTT_ping();
    //setting up pub for weight

    //setting up weight load cell for drainage drawer
    //flow will be if drawer is at a certain weight 
    //alert will be sent to phone and after a certain time
    //a light will illuminate
    weight = myScale.get_units(SAMPLES);
    offset = myScale.get_offset();
    calibration =myScale.get_scale();

    //air quality sensor code
    quality = sensor.slope();
    sensorValue = sensor.getValue();
  
    Serial.printf("Sensor value: %i\n", sensorValue);
  
    if(quality == AirQualitySensor::FORCE_SIGNAL){
      Serial.println(" HIGH POLLUTION! Force Signal Active!!!!");
    } else if(quality == AirQualitySensor::HIGH_POLLUTION){
      Serial.println("High Polution!");
    } else if(quality == AirQualitySensor::LOW_POLLUTION){
      Serial.println("Low Pollution!");
    }else if(quality == AirQualitySensor::FRESH_AIR){
      Serial.println("Fresh too Def Air!");
    }
    delay(5000);

    Serial.printf("Weight = %0.02f\r", weight);
    
    //setting up pub for weight
    if((millis()-lastTime)>6000){
        if(mqtt.Update()){
          pubWeight.publish(weight);
            Serial.printf("Weight Sent to Adafruit.io = %0.0f\n",weight);
        }
        lastTime = millis();
        if(weight <=5){
          weight=0;
        }
        if(weight <300){
        greenAlert = "<<< Ready for Compost >>>";
        pubAlert.publish(greenAlert);
        Serial.println(greenAlert);
        pixel.setPixelColor(PIXELCOUNT, green);
        pixel.show();
        }
        if(weight>300 && weight <500){
          yellowAlert = "<<< Half Way Full >>>";
          pubAlert.publish(yellowAlert);
          Serial.println(yellowAlert);
          pixel.setPixelColor(PIXELCOUNT, yellow);
          pixel.show();
        }
        if(weight>500 && weight<600){
          orangeAlert = "<<< Almost Full >>>";
          pubAlert.publish(orangeAlert);
          Serial.println(orangeAlert);
          pixel.setPixelColor(PIXELCOUNT, orange);
          pixel.show();
        }
        if(weight>600){
          redAlert = "<<<FULL!!! Empty Out Drawer Please>>>";
          pubAlert.publish(redAlert);
          Serial.println(redAlert);
          pixel.setPixelColor(PIXELCOUNT, red);
          pixel.show();
        }
        pixel.clear();
        pixel.show();
    }
    //setting up display
    dateTime =Time.timeStr();
    date = dateTime.substring(0,11);
    timeOnly =dateTime.substring(11,19);
  
    //BME set up
    temp = bme.readTemperature();
    humidity = bme.readHumidity();
    temp = (temp * 9/5.0)+32;
    
    //OLED displaying
    display.printf("Date: %s\n",date.c_str());
    display.printf("Time: %s\n",timeOnly.c_str());
    display.printf("Temp = %0.0f%c\n",temp, DEGREE_SYM);
    display.printf("Humidity = %0.0f\n",humidity);
    display.setCursor(0,0);
    display.display();
    display.clearDisplay();

}
// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
    int8_t ret;
   
    // Return if already connected.
    if (mqtt.connected()) {
      return;
    }
   
    Serial.print("Connecting to MQTT... ");
   
    while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
         Serial.printf("Error Code %s\n",mqtt.connectErrorString(ret));
         Serial.printf("Retrying MQTT connection in 5 seconds...\n");
         mqtt.disconnect();
         delay(5000);  // wait 5 seconds and try again
    }
    Serial.printf("MQTT Connected!\n");
  }
  
  bool MQTT_ping() {
    static unsigned int last;
    bool pingStatus;
  
    if ((millis()-last)>120000) {
        Serial.printf("Pinging MQTT \n");
        pingStatus = mqtt.ping();
        if(!pingStatus) {
          Serial.printf("Disconnecting \n");
          mqtt.disconnect();
        }
        last = millis();
    }
    return pingStatus;
  }
