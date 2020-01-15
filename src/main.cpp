/** kingswood-monitor-display-electricity
 * 
 * Gets Kingswood electrical power consumption and
 * and modifies the colour of status leds.
 * By: Richard Lyon
*/

#define FASTLED_ESP8266_D1_PIN_ORDER
#include <FastLED.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Timemark.h>
#include <math.h>
#include <stdio.h>

#define LED1 2  // Blue Onboard LED next to WIFI arial
#define LED2 16 // Red  Onboard LED next to USB port

#define FIRMWARE_VERSION 1.0
#define FIRMWARE_NAME "display-energy-NODEMCUDEVKIT-arduino"

char *deviceID();
void start_wifi();
void start_mqtt();
void startFastLED();
void callback(char *topic, byte *payload, unsigned int length);
void reconnect();
char *make_topic(const char *root, const char *subtopic);
char *get_subtopic(const char *topic);
String stringFromPayload(byte *payload, unsigned int length);
int intFromPayload(byte *payload, unsigned int length);
float floatFromPayload(byte *payload, unsigned int length);
int power;
// void setSensorLeds(int power, float ledBrightness);
CHSV setStatusLeds(int power, float ledBrightness);

//* Per sensor configuration *************************************************

// wifi credentials
const char *ssid = "Kingswood";
const char *password = "wbtc0rar";

// MQTT config
IPAddress mqtt_server(192, 168, 1, 30); // EmonHub server ip

char *powerTopic = "31/data/power";
char *powerMonitorStatusTopic = "emon/kingswood/monitor/status";
char *powerMonitorMaxPowerTopic = "emon/kingswood/monitor/maxpower";
char *powerMonitorMinPowerTopic = "emon/kingswood/monitor/minpower";
char *powerMonitorBrightnessTopic = "emon/kingswood/monitor/brightness";

// ***************************************************************************

// Addressable LEDs
#define DATA_PIN 5 // i.e D5
#define NUM_LEDS 3

#define HUE_MAX 160 // blue in the FastLED 'rainbow' colour map

CRGB leds[NUM_LEDS];

// Status light parameters
int powerMin = 200;
int powerMax = 1000;

// Status light brightness
float ledBrightness = 1.0; // 0.0 - 1.0

// MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Timers
#define HEARTBEAT_DURATION 100
#define KEEP_ALIVE_INTERVAL_SECS 10 // MQTT "online" update period: <= 15 secs
Timemark mqttKeepAliveHeartbeat(HEARTBEAT_DURATION);
Timemark mqttKeepAlive(KEEP_ALIVE_INTERVAL_SECS * 1000);

void setup()
{
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, HIGH);

  Serial.begin(115200);
  Serial.print("Device: ");
  Serial.println(deviceID());
  start_wifi();
  start_mqtt();
  startFastLED();

  mqttKeepAlive.start();
}

void loop()
{
  // Process MQTT
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  // mqtt keep alive and pulse
  if (mqttKeepAlive.expired())
  {
    // client.publish(statusTopic, "ONLINE");
    digitalWrite(LED1, LOW); // i.e. "ON" - onboard LED logic is inverted
    client.publish(powerMonitorStatusTopic, "ONLINE");
    mqttKeepAliveHeartbeat.start();
  }

  // setSensorLeds(power, ledBrightness);
  setStatusLeds(power, ledBrightness);

  // Turn off heartbeat LED if flash duration is over
  if (mqttKeepAliveHeartbeat.expired())
  {
    mqttKeepAliveHeartbeat.stop();
    digitalWrite(LED1, HIGH);
  }
}

////////////////////////////////////////////////////////////////////////////////

char *deviceID()
{
  char buf[7];
  sprintf(buf, "%x", ESP.getFlashChipId());
  return buf;
}

void start_wifi()
{
  delay(10);
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
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void start_mqtt()
{
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void startFastLED()
{
  FastLED.addLeds<WS2811, DATA_PIN, EOrder::RGB>(leds, NUM_LEDS);
  leds[0] = CRGB(0, 0, 0);
  leds[1] = CRGB(0, 0, 0);
  leds[2] = CRGB(0, 0, 0);
  FastLED.show();
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), powerMonitorStatusTopic, 1, true, "OFFLINE"))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // client.publish(statusTopic, "ONLINE");
      // ... and resubscribe
      client.subscribe(powerTopic);
      client.subscribe(powerMonitorMaxPowerTopic);
      client.subscribe(powerMonitorMinPowerTopic);
      client.subscribe(powerMonitorBrightnessTopic);
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

// Process MQTT callbacks
void callback(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, powerTopic) == 0)
  {
    power = intFromPayload(payload, length);
    Serial.print("Power:");
    Serial.print(power);
    Serial.println("W");
  }
  else if (strcmp(topic, powerMonitorMaxPowerTopic) == 0)
  {
    powerMax = intFromPayload(payload, length);
    Serial.print("Max power:");
    Serial.print(powerMax);
    Serial.println("W");
  }
  else if (strcmp(topic, powerMonitorMinPowerTopic) == 0)
  {
    powerMin = intFromPayload(payload, length);
    Serial.print("Min power:");
    Serial.print(powerMin);
    Serial.println("W");
  }
  else if (strcmp(topic, powerMonitorBrightnessTopic) == 0)
  {
    ledBrightness = floatFromPayload(payload, length);
    ledBrightness = constrain(ledBrightness, 0.0, 0.1);
    Serial.print("LED max brightness:");
    Serial.println(ledBrightness);
  }
}

CHSV setStatusLeds(int power, float ledBrightness)
{
  CHSV pixelColour;

  power = constrain(power, powerMin, powerMax);
  pixelColour.hue = map(power, powerMin, powerMax, HUE_MAX, 0);

  // Serial.print("Hue: ");Serial.print(power); Serial.print("/");Serial.println(pixelColour.hue);

  pixelColour.sat = 255;
  // 'breathing' function https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  pixelColour.val = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
  pixelColour.val = map(pixelColour.val, 0, 255, 70, 255) * ledBrightness;

  leds[0] = pixelColour;
  leds[1] = pixelColour;
  leds[2] = pixelColour;
  FastLED.show();
}

int intFromPayload(byte *payload, unsigned int length)
{
  char buffer[50];
  int i;
  for (i = 0; i < length; ++i)
  {
    buffer[i] = payload[i];
  }
  buffer[i + 1] = '\0';
  return strtol(buffer, NULL, 10); //Serial.println(strtod(buffer,NULL));
}

float floatFromPayload(byte *payload, unsigned int length)
{
  char buffer[50];
  int i;
  for (i = 0; i < length; ++i)
  {
    buffer[i] = payload[i];
  }
  buffer[i + 1] = '\0';
  return strtod(buffer, NULL);
}
