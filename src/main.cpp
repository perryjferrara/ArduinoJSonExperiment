// example code based on: https://iotdesignpro.com/projects/how-to-connect-esp32-mqtt-broker
#include <Arduino.h>
#include <WiFi.h>
#include "DHT.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- Pin Configurations ---
#define LED_PIN 32    // GPIO pin for LED
#define VOUT_PIN 25   // GPIO pin for voltage output
#define DHTPIN 22     // GPIO pin for DHT sensor
#define DHTTYPE DHT11 // DHT sensor type

// Update these with values suitable for your network.
// const char* ssid = "your wifi sid";
// const char* password = "your wifi passwd";
// Replace with your network credentials
const char *ssid = "iPhone";
const char *password = "himomha123";
// const char* mqtt_server = "test.mosquitto.org";
const char *mqtt_server = "broker.hivemq.com";
#define mqtt_port 1883
#define MQTT_SERIAL_PUBLISH_CH "csc113/ambientenv/publish" // MQTT topic for publishing sensor data
#define MQTT_SERIAL_SUBSCRIBE_CH "csc113/ambientenv/listen" // MQTT topic for subscribing to commands

const char *DEVICE_ID = "PF"; // Device ID for MQTT commands
bool publishingEnabled = true; // Enable or disable sensor publishing

unsigned long lastPublishTime = 0; // Timestamp of the last publish
const unsigned long publishInterval = 1000; // Interval between publishes in milliseconds

DHT dht(DHTPIN, DHTTYPE); // DHT sensor instance

WiFiClient wifiClient; // WiFi client instance

PubSubClient client(wifiClient); // MQTT client instance

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "PF";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    // if (client.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD))
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // subscribe
      client.subscribe(MQTT_SERIAL_SUBSCRIBE_CH);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.println("-------new message from broker-----"); // Print a message when a new message is received
  Serial.print("channel: "); // Print the MQTT topic
  Serial.println(topic);
  Serial.print("data: "); // Print the received data
  Serial.write(payload, length);
  Serial.println();

  JsonDocument doc; // JSON document for parsing the received message
  DeserializationError error = // Error object for JSON deserialization
      deserializeJson(doc, (const char *)payload, length); // Deserialize the JSON payload into the document

  if (error)
  {
    Serial.print("Invalid JSON: "); // Print the JSON deserialization error
    Serial.println(error.c_str()); // Print the error message
    return;
  }

  // Both fields must be strings.
  if (!doc["ID"].is<const char *>() ||
      !doc["cmd"].is<const char *>())
  {
    Serial.println("Message requires ID and cmd as strings."); // Print the required message format
    return;
  }

  String id = doc["ID"].as<String>(); // Extract the device ID from the JSON message
  String cmd = doc["cmd"].as<String>(); // Extract the command from the JSON message 

  Serial.println();
  Serial.print("ID: ");
  Serial.println(id);
  Serial.print("Command: ");
  Serial.println(cmd);
  Serial.println();
  
  // Ignore commands intended for another device.
  // Accept commands for this device OR all devices.
if (id != DEVICE_ID && id != "*") { // Ignore commands intended for another device
  return; // Exit the callback if the command is not for this device
}

  if (cmd == "STOP")
  {
    publishingEnabled = false; // Disable sensor publishing
    Serial.println("Sensor publishing STOPPED."); // Print a message when sensor publishing is stopped
  }
  else if (cmd == "START")
  {
    publishingEnabled = true; // Enable sensor publishing
    Serial.println("Sensor publishing STARTED."); // Print a message when sensor publishing is started
  }
  else
  {
    Serial.println("Unknown command. Use STOP or START."); // Print a message for unknown commands
  }
}

void setup()
{
  Serial.begin(9600);
  Serial.setTimeout(500); // Set timeout for serial input
  setup_wifi(); // Connect to WiFi
  client.setServer(mqtt_server, mqtt_port); // Set the MQTT server address and port
  client.setCallback(callback); // Set the MQTT callback function
  reconnect(); // Connect to the MQTT server
  Serial.setTimeout(5000); // Set timeout for serial input
  dht.begin(); // Initialize the DHT sensor
}

void publishSerialData(const char *serialData) // Publish serial data over MQTT
{
  if (!client.connected())
  {
    reconnect(); // Reconnect to the MQTT server if disconnected
  }
  Serial.println(String("publishing: ") + serialData); // Print the data being published

  client.publish(MQTT_SERIAL_PUBLISH_CH, serialData); // Publish the serial data to the MQTT channel
}
void loop()
{
  if (!client.connected())
  {
    reconnect(); // Reconnect to the MQTT server if disconnected
  }

  // Always process incoming messages, including while stopped.
  client.loop(); // Process incoming MQTT messages
  delay(1); // Delay to prevent flooding the MQTT server

  if (!publishingEnabled)
  {
    return; // Exit the loop if sensor publishing is disabled
  }

  unsigned long now = millis(); // Get the current time in milliseconds

  if (now - lastPublishTime < publishInterval)
  {
    return; // Exit the loop if the publish interval has not elapsed
  }

  lastPublishTime = now; // Update the last publish time

  float h = dht.readHumidity(); // Read the humidity from the DHT sensor
  float t = dht.readTemperature(); // Read the temperature in Celsius from the DHT sensor
  float f = dht.readTemperature(true); // Read the temperature in Fahrenheit from the DHT sensor

  JsonDocument doc; // Create a JSON document to store sensor data

  doc["ID"] = DEVICE_ID; // Add the device ID to the JSON document
  doc["humidity"] = h; // Add the humidity to the JSON document
  doc["temperatureC"] = t; // Add the temperature in Celsius to the JSON document
  doc["temperatureF"] = f; // Add the temperature in Fahrenheit to the JSON document
  doc["temperatureK"] = t + 273.15; // Add the temperature in Kelvin to the JSON document


  JsonObject location = doc["location"].to<JsonObject>(); // Create a location object in the JSON document
  location["lat"] = 48.8584; // Add the latitude to the location object
  location["lon"] = 2.2945; // Add the longitude to the location object

  doc["time"] = time(nullptr); // Add the current time to the JSON document

  String str; // Create a string to store the serialized JSON data
  serializeJson(doc, str); // Serialize the JSON document to a string
  publishSerialData(str.c_str()); // Publish the JSON data over MQTT
}