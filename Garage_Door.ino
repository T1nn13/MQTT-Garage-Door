#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "GarageDoor.h"

//Setting Door State variables to Zero to be aquired later. (digitalRead)
int doorstateopen = 0; 
int doorstateclosed = 0; 

//Counter for Error state calcs. Goes to error state if Door_Transition_Seconds is exceeded
int count = 0;

//Variable to know if a new message is in via HASS
boolean MQTT_Set_Request = false; // light is turned off by default


WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Sets all the door states to HASS and Serial Monitor
void Door_State(){
  doorstateopen = digitalRead(14);
  doorstateclosed = digitalRead(4);

  //OPEN STATE
  if ((doorstateopen == LOW && doorstateclosed == HIGH) && (DOOR_STATE_PREVIOUS != "OPEN")){
    Serial.println("It's OPEN");
    //Serial.println(doorstateopen);
    DOOR_STATE_PREVIOUS = "OPEN";
    client.publish(MQTT_SENSOR_STATE_TOPIC, DOOR_OPEN, true);
    count = 0; 
  }

  //CLOSED STATE
  else if ((doorstateclosed == LOW && doorstateopen == HIGH)&& (DOOR_STATE_PREVIOUS != "CLOSED")){
    Serial.println("It's CLOSED");
    //Serial.println(doorstateclosed);
    DOOR_STATE_PREVIOUS = "CLOSED";
    client.publish(MQTT_SENSOR_STATE_TOPIC, DOOR_CLOSED, true);
    count = 0;
  }

  //OTHER STATE - OPENING - CLOSING - ERROR
  else if ((doorstateclosed == LOW && doorstateopen == LOW) || (doorstateclosed == HIGH && doorstateopen == HIGH)){
      //if previous state was open, then closing.
      if (DOOR_STATE_PREVIOUS == "OPEN" || DOOR_STATE_PREVIOUS == "CLOSING"){
        Serial.println(DOOR_CLOSING);
        client.publish(MQTT_SENSOR_STATE_TOPIC, DOOR_CLOSING, true);
        DOOR_STATE_PREVIOUS = "CLOSING";
        // 1 second delay for timing and Error time out which is defined in Door_Transition_Seconds
        delay(1000);
        count ++;
        }
        //if previous state was closing, then opening.
      else if (DOOR_STATE_PREVIOUS == "CLOSED" || DOOR_STATE_PREVIOUS == "OPENING") {
        Serial.println(DOOR_OPENING);
        client.publish(MQTT_SENSOR_STATE_TOPIC, DOOR_OPENING, true);
        DOOR_STATE_PREVIOUS = "OPENING";
        // 1 second delay for timing and Error time out which is defined in Door_Transition_Seconds
        delay(1000);
        count ++;
        }
        //max transition time exceeded and error state
     if ((count > Door_Transition_Seconds) && (DOOR_STATE_PREVIOUS != "ERROR")) {
      DOOR_STATE_PREVIOUS = "ERROR";
      Serial.println(DOOR_ERROR);
      client.publish(MQTT_SENSOR_STATE_TOPIC, DOOR_ERROR, true);
     }
    }
 }

// function called to turn on/off the light to show that a request came in from HASS
void SetReset_Door_Pin() {
  //Only allows you to open/close when the door is fully open or closed or stuck in an error state
  if (MQTT_Set_Request && ((DOOR_STATE_PREVIOUS == "OPEN") || (DOOR_STATE_PREVIOUS == "CLOSED") || (DOOR_STATE_PREVIOUS == "ERROR"))) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("INFO: Turn light on...");
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("INFO: Turn light off...");
    }
  }


// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  // handle message topic
  if (String(MQTT_SENSOR_COMMAND_TOPIC).equals(p_topic)) {
    if (payload.equals(String(DOOR_OPEN))) {
      if (MQTT_Set_Request != true) {
        MQTT_Set_Request = true;
        SetReset_Door_Pin();     
      }
    } else if (payload.equals(String(DOOR_CLOSED))) {
      if (MQTT_Set_Request != false) {
        MQTT_Set_Request = false;
        SetReset_Door_Pin();
      }
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("INFO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("INFO: connected");
      // Once connected, publish an announcement...
      //publishLightState();
      // ... and resubscribe
      client.subscribe(MQTT_SENSOR_COMMAND_TOPIC);
     // client.subscribe(MQTT_Light_command_topic_2); 
    } 
    else {
      Serial.print("ERROR: failed, rc=");
      Serial.print(client.state());
      Serial.println("DEBUG: try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // init the serial
  Serial.begin(115200);
  //internal pullup resistors enables - switches pull to ground
  pinMode(openswitchPin,INPUT_PULLUP);
  pinMode(closeswitchPin,INPUT_PULLUP);

  // init the led
  pinMode(LED_PIN, OUTPUT);

  SetReset_Door_Pin();

  // init the WiFi connection
  Serial.println();
  Serial.println();
  Serial.print("INFO: Connecting to ");
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet, dns);
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("INFO: WiFi connected");
  Serial.print("INFO: IP address: ");
  Serial.println(WiFi.localIP());

  // init the MQTT connection
  client.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  Door_State();
  if (MQTT_Set_Request){
      delay(Door_Button_Press_Length);
      MQTT_Set_Request = false; 
      SetReset_Door_Pin(); 
  }

}
