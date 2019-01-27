#include <ESP8266WiFi.h>  // NodeMCU Library
#include <PubSubClient.h> // MQTT Client
#include <Wire.h>         // I2C library
#include "iAQcore.h"      // iAQ-Core driver
#include "config.h"       // contains all defines

//#define wifi_ssid
//#define wifi_password

//#define mqtt_server
//#define mqtt_port
//#define mqtt_access_token

//#define co2_topic
//#define tvoc_topic
//#define resist_topic


WiFiClient espClient;
PubSubClient client(espClient);
iAQcore iaqcore;


void setup() {

  // Enable serial
  Serial.begin(115200);
  Serial.println("");
  Serial.println("Starting iAQ-Core");

  // Enable I2C for ESP8266 NodeMCU boards [VDD to 3V3, GND to GND, SDA to D2, SCL to D1]
  Wire.begin(SDA,SCL);
  Wire.setClockStretchLimit(1000); // Default is 230us, see line78 of https://github.com/esp8266/Arduino/blob/master/cores/esp8266/core_esp8266_si2c.c

  // Enable iAQ-Core
  bool ok= iaqcore.begin();
  Serial.println(ok ? "iAQ-Core initialized" : "ERROR initializing iAQ-Core");

  // Setup WiFi
  setup_wifi();

  // Connect to MQTT
  client.setServer(mqtt_server, mqtt_port);

}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA); //make sure the ESP doen't become a access point
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_access_token, NULL)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/*
 * ----------------------------------------------------------------------------------------------------
 */

long lastMsg = 0;

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  // Set send frequency
  long now = millis();
  if (now - lastMsg > 1 * 1000) {
    lastMsg = now;

    // Read
    uint16_t eco2;
    uint16_t stat;
    uint32_t resist;
    uint16_t etvoc;
    iaqcore.read(&eco2,&stat,&resist,&etvoc);

    // Print
    if( stat & IAQCORE_STAT_I2CERR ) {
      Serial.println("iAQcore: I2C error");
    } else if( stat & IAQCORE_STAT_ERROR ) {
      Serial.println("iAQcore: chip broken");
    } else if( stat & IAQCORE_STAT_BUSY ) {
      Serial.println("iAQcore: chip busy");
    } else {
      if( stat&IAQCORE_STAT_RUNIN ) Serial.println(" RUNIN");

      // Create Thingsboard Compatible Payload -- a json message
      String payload = "{";
      payload += "\"CO2\":"; payload += String(eco2).c_str(); payload += ",";
      payload += "\"VOC\":"; payload += String(etvoc).c_str(); payload += ",";
      payload += "\"RESIST\":"; payload += String(resist).c_str();
      payload += "}";

      // Convert and send
      char attributes[100];
      payload.toCharArray(attributes, 100);
      Serial.println(attributes);
      client.publish("v1/devices/me/telemetry", attributes);

    }
  }
}
