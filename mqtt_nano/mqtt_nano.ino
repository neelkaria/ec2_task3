// ====================================================================
// Task 3: MQTT + JSON - Serial Command Gateway (Nano 33 IoT)
// Receives commands via Serial Monitor, sends them via MQTT to Pico
// ====================================================================

#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "SimpleJson.h"

// -- WiFi (UPDATE THESE!) --------------------------------------------
// const char* WIFI_SSID = "Vodafone-3DE8";
// const char* WIFI_PASS = "GyDvEgPkd6GdJECW";
const char* WIFI_SSID = "WLAN-Pi-1";
const char* WIFI_PASS = "raspberry";

// -- MQTT ------------------------------------------------------------
// const char* MQTT_BROKER = "broker.emqx.io";
const char* MQTT_BROKER = "141.69.95.10";
const int MQTT_PORT = 1883;
const char* TOPIC_DATA = "iem/task3/neelk/pico/data";  // Subscribe
const char* TOPIC_CMD = "iem/task3/neelk/pico/cmd";    // Publish

// -- Robustness ------------------------------------------------------
const char* SHARED_TOKEN = "iem2026";
const char* EXPECTED_SOURCE = "pico";
const unsigned long WATCHDOG_TIMEOUT = 10000;

// -- Last known Pico state -------------------------------------------
int remotePotValue = 0, remoteInterval = 0, remoteUptime = 0;
bool remoteBlink = false, remoteLedState = false, remoteSafeState = false;

// -- Robustness state ------------------------------------------------
unsigned long lastValidData = 0;
bool picoTimeout = false;
int expectedSeqNr = -1;
unsigned long seqOutgoing = 0;

// -- Statistics ------------------------------------------------------
unsigned long msgAccepted = 0, msgRejectedJson = 0;
unsigned long msgRejectedAuth = 0, msgSeqGaps = 0;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
SimpleJson jsonOut, jsonIn;


void setupWiFi() {
  // TODO: Connect to WiFi (WiFiNINA)
  Serial.print("Connecting to WiFi...");

  while (WiFi.begin(WIFI_SSID, WIFI_PASS) != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
}


bool validateMessage(const SimpleJson& msg) {
  // TODO: Check token + source
  const char* token = msg.getString("token");
  const char* source = msg.getString("source");

  if (strcmp(token, SHARED_TOKEN) != 0) {
    msgRejectedAuth++;
    Serial.println(" Token Check Failed ");
    return false;
  }

  if (strcmp(source, EXPECTED_SOURCE) != 0) {
    msgRejectedAuth++;
    Serial.println(" Source Check Failed ");
    return false;
  }

  return true;
}


bool checkSequence(const SimpleJson& msg) {
  // TODO: Check sequence number (same logic as Pico)
  int seq = msg.getInt("seq");

  if (seq == 0) {
    expectedSeqNr = 1;
    return true;
  }

  if (expectedSeqNr == -1) {
    expectedSeqNr = seq + 1;
    return true;
  }

  if (seq < expectedSeqNr) {
    Serial.println("Replay detected");
    return false;
  }

  if (seq > expectedSeqNr) {
    msgSeqGaps++;
    Serial.println("Sequence gap detected");
  }

  expectedSeqNr = seq + 1;

  return true;
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // TODO: Receive sensor data from Pico:
  char arr[length + 1];
  memcpy(arr, payload, length);
  arr[length] = '\0';

  // 1. Parse JSON (jsonIn.parse)
  jsonIn.clear();
  if (!jsonIn.parse(arr)) {
    msgRejectedJson++;
    Serial.println("Invalid JSON");
    return;
  }

  // 2. validateMessage()
  if (!validateMessage(jsonIn)) return;

  // 3. checkSequence()
  if (!checkSequence(jsonIn)) return;

  // 4. Store values (remotePotValue etc.)
  remotePotValue = jsonIn.getInt("potValue");
  remoteBlink = jsonIn.getBool("blinkEnabled");
  remoteInterval = jsonIn.getInt("interval");
  remoteLedState = jsonIn.getBool("ledOn");
  remoteSafeState = jsonIn.getBool("safeState");
  remoteUptime = jsonIn.getInt("uptime");

  lastValidData = millis();
  picoTimeout = false;

  msgAccepted++;

  // 5. Update mirror LED (LED_BUILTIN)
  digitalWrite(LED_BUILTIN, jsonIn.getBool("ledOn") ? HIGH : LOW);
}

void mqttReconnect() {

  String clientId = "nano_" + String(random(0xffff), HEX);

  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected");
      mqtt.subscribe(TOPIC_DATA);
      lastValidData = millis();
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 1 seconds");
      // Wait 1 second before retrying
      delay(1000);
    }
  }
}


// Centarlizing JSON creation to avoid repetitive blocks in processSerialInput()
void sendCommand(bool blinkEnabled, bool ledOn, int interval, bool useLocalPot) {
  // TODO: Add token, source, seq and publish

  jsonOut.clear();

  jsonOut.setString("token", SHARED_TOKEN);
  jsonOut.setString("source", "nano");
  jsonOut.setInt("seq", (int)seqOutgoing++);

  jsonOut.setBool("blinkEnabled", blinkEnabled);
  jsonOut.setBool("ledOn", ledOn);
  if (interval > 0) jsonOut.setInt("interval", interval);
  jsonOut.setBool("useLocalPot", useLocalPot);

  char buf[256];
  jsonOut.toCharArray(buf, sizeof(buf));
  if(mqtt.publish(TOPIC_CMD, buf)){
    Serial.println(" Command Sent ");
  }
  else{
    Serial.println(" MQTT Publish Failed ");
  }
}


void processSerialInput() {
  // TODO: Read serial input and process commands:

  if (!Serial.available()) return;

  String command = Serial.readStringUntil('\n');

  //Remove whitespaces, convert to uppercase
  command.trim();
  command.toUpperCase();

  // bool blinkEnabled, bool ledOn, int interval, bool useLocalPot
  // ON -> blinkEnabled=false, ledOn=true
  if (command == "ON") sendCommand(0, 1, -1, 0);

  // OFF -> blinkEnabled=false
  else if (command == "OFF") sendCommand(0, 0, -1, 0);

  // BLINK -> blinkEnabled=true
  else if (command == "BLINK") sendCommand(1, 0 , -1, 0);

  // NOBLINK -> blinkEnabled=false
  else if (command == "NOBLINK") sendCommand(0, 0, -1, 0);

  // INTERVAL <ms> -> interval=<ms> (50..2000)
  else if (command.startsWith("INTERVAL")) {
    int val = command.substring(9).toInt();
    if (val >= 50 && val <= 2000) {
      sendCommand(1, 0, val, 0);
    }
    else {
      Serial.println("Interval must be 50-2000 ms");
    }
  }
  // POT -> useLocalPot=true
  else if (command == "POT") sendCommand(1, 0, -1, 1);

  // STATUS -> Display remote state
  else if (command == "STATUS") {
    Serial.println("-- Pico Remote Status ------------------------------------------------------");
    
    if (remoteBlink)
    {
      Serial.println("LED Mode: Blinking");
      Serial.print("Blink Interval: ");Serial.println(remoteInterval);
    }
    else{
      Serial.println("LED Mode: Static");
      Serial.print("LED State: ");Serial.println(remoteLedState);
    }

    Serial.print("Pot Value: ");Serial.println(remotePotValue);
    Serial.print("Uptime: ");Serial.print(remoteUptime/1000);Serial.println("s");
    Serial.print("Safe State: ");Serial.println(remoteSafeState ? "YES" : "NO");
  }

  // STATS -> Display statistics
  else if (command == "STATS") {
    Serial.println("-- Nano Statstics ----------------------------------------------------------");
    Serial.print("Valid Messages: ");Serial.println(msgAccepted);
    Serial.print("Invalid Messages: ");Serial.println(msgRejectedJson);
    Serial.print("Source/Token Check Failed: ");Serial.println(msgRejectedAuth);
    Serial.print("Sequence Gaps: "); Serial.println(msgSeqGaps);
  }
  // HELP -> Show help
  else if (command == "HELP") {
  
  Serial.println("========================================");
  Serial.println("           AVAILABLE COMMANDS           ");
  Serial.println("========================================");

  Serial.print("ON");                       Serial.println("  -> LED permanently ON");
  Serial.print("OFF");                      Serial.println("  -> LED permanently OFF");
  Serial.print("BLINK");                    Serial.println("  -> Enable LED blinking");
  Serial.print("NOBLINK");                  Serial.println("  -> Disable blinking");
  Serial.print("INTERVAL <50-2000> ms");    Serial.println("  -> Set blink interval in milliseconds (Example: INTERVAL 500)");
  Serial.print("POT");                      Serial.println("  -> Return blink control to Pico potentiometer");
  Serial.print("STATUS");                   Serial.println("  -> Display latest remote Pico status");
  Serial.print("STATS");                    Serial.println("  -> Show MQTT communication statistics");
  Serial.print("HELP");                     Serial.println("  -> Show this menu");
  Serial.println();

  Serial.println("========================================");
}
  
  else Serial.println(" Invalid Commamd ");
}



void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  while (!Serial) { delay(100); }

  setupWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}


void loop() {
  // TODO: Check MQTT connection + mqtt.loop()

  if (!mqtt.connected()) {
    Serial.println("Connecting to MQTT Broker ");
    mqttReconnect();
  }

  mqtt.loop();

  // TODO: Check watchdog (warn if Pico not responding)
  if (millis() - lastValidData > WATCHDOG_TIMEOUT) {

    if (!picoTimeout) {

      picoTimeout = true;

      Serial.println("WARNING: Pico timeout!");
      digitalWrite(LED_BUILTIN,LOW);
    }
  }
  processSerialInput();
}