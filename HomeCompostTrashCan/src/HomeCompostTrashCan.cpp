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
//functions for publishing weight
void MQTT_connect();
bool MQTT_ping();

//variables for load cell
HX711 myScale(A1, A2);
const int CALIFACTOR =1055;
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

    Serial.printf("Weight = %0.02f\r", weight);
    
    //setting up pub for weight
    if((millis()-lastTime)>6000){
        if(mqtt.Update()){
          pubWeight.publish(weight);
            Serial.printf("Weight Sent to Adafruit.io = %0.0f\n",weight);
        }
        lastTime = millis();
        
        if(weight <300){
        greenAlert = "<<< Ready for Compost >>>";
        pubAlert.publish(greenAlert);
        Serial.println(greenAlert);
        //set light color
        }
        if(weight>300 && weight <500){
          yellowAlert = "<<< Half Way Full >>>";
          pubAlert.publish(yellowAlert);
          Serial.println(yellowAlert);
        }
        if(weight>500 && weight<600){
          orangeAlert = "<<< Almost Full >>>";
          pubAlert.publish(orangeAlert);
          Serial.println(orangeAlert);
        }
        if(weight>600){
          redAlert = "<<<FULL!!! Empty Out Drawer Please>>>";
          pubAlert.publish(redAlert);
          Serial.println(redAlert);
        }
    }

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
