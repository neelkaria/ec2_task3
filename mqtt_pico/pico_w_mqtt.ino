// ====================================================================
// Task 3: MQTT + JSON (Pico W, Wokwi)
// Receives control commands via MQTT, publishes sensor data
// ====================================================================
#include <WiFi.h>
#include <PubSubClient.h>
#include "SimpleJson.h"

// -- WiFi (Wokwi) ---------------------------------------------------
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// -- MQTT ------------------------------------------------------------
// const char* MQTT_BROKER = "broker.emqx.io";
const char* MQTT_BROKER = "141.69.95.10";

const int MQTT_PORT = 1883;
const char* TOPIC_DATA = "iem/task3/neelk/pico/data";
const char* TOPIC_CMD = "iem/task3/neelk/pico/cmd";

// -- Robustness ------------------------------------------------------
const char* SHARED_TOKEN = "iem2026";
const char* EXPECTED_SOURCE = "nano";
const unsigned long WATCHDOG_TIMEOUT = 10000;
const int INTERVAL_MIN = 50;
const int INTERVAL_MAX = 2000;

// -- Hardware --------------------------------------------------------
const int LED_PIN = 28;
const int BUTTON_PIN = 2;
const int POT_PIN = 26;
const int HEARTBEAT_PIN = LED_BUILTIN;

// -- State -----------------------------------------------------------
volatile bool blinkEnabled = true;
bool ledState = false;
int overrideInterval = -1; // -1 = poti controls
unsigned long lastValidCmd = 0;
bool inSafeState = false;
int expectedSeqNr = -1;
unsigned long seqOutgoing = 0;
bool wifiStatus;
unsigned long interval = 0;
unsigned long lastInterval = 0;
unsigned long watchdogTimer = 0;
bool toggleBlink = false;
bool outputState = false;
bool hbOutputState = false;

//-- Variables --------------------------------------------------------
unsigned long hbCurrentTime = 0;
unsigned long hbPreviousTime = 0;
unsigned long currentTime = 0;
unsigned long previousTime = 0;

// Statistics
unsigned long msgAccepted = 0, msgRejectedJson = 0;
unsigned long msgRejectedAuth = 0, msgSeqGaps = 0;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
SimpleJson jsonOut, jsonIn;

void setupWiFi() {
  // TODO: Connect to WiFi (Wokwi-GUEST, same as Task 1/2)
  Serial1.println("\nConnecting to WiFi Network ..");
  WiFi.mode(WIFI_STA);    // WiFi Station Mode
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial1.print(".");
    delay(1000);
  }

  Serial1.println("\nConnected to the WiFi network");
}


void enterSafeState() {
  // TODO: Activate safe state
  // - Set overrideInterval = -1 (back to poti)
  // - Print warning message
  if (!inSafeState)
  {
    inSafeState = true;
    overrideInterval = -1;
    Serial1.println(" Entering Safe State ");
  }

}


void leaveSafeState() {
  // TODO: Leave safe state when valid message arrives
  if (inSafeState)
  {
    inSafeState = false;
    Serial1.println(" Leaving Safe STate ");
  }
}


bool validateMessage(const SimpleJson& msg) {
  // TODO: Check token (must match SHARED_TOKEN)
  // TODO: Check source (must match EXPECTED_SOURCE)

  const char* token = msg.getString("token");
  const char* source = msg.getString("source");

  if(strcmp(token, SHARED_TOKEN) != 0)
  {
    msgRejectedAuth++;
    Serial1.println(" Token Check Failed ");
    return false;
  }

  if(strcmp(source, EXPECTED_SOURCE) != 0)
  {
    msgRejectedAuth++;
    Serial1.println(" Source Check Failed ");
    return false;
  }

  return true;
}


bool checkSequence(const SimpleJson& msg) {
  // TODO: Check sequence number
  // - Detect gaps (warn but accept)
  // - Detect replays (discard)
  // - Detect restart (seq == 0 -> resync)

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
    Serial1.println("Replay detected");
    return false;
  }

  if (seq > expectedSeqNr) {
    msgSeqGaps++;
    Serial1.println("Sequence gap detected");
  }

  expectedSeqNr = seq + 1;

  return true;
}


void processCommand(const SimpleJson& cmd) {
  // TODO: Process received commands:

  // - blinkEnabled (bool)
  if (cmd.hasKey("blinkEnabled")) {
    blinkEnabled = cmd.getBool("blinkEnabled");
  }

  // - ledOn (bool) -> LED permanently on
  if (cmd.hasKey("ledOn")) {
    bool ledOn = cmd.getBool("ledOn");
    ledState = ledOn;
    digitalWrite(LED_PIN, ledState);
  }

  // - interval (int) -> override with range check
  if (cmd.hasKey("interval")) {
    int val = cmd.getInt("interval");

    if (val >= INTERVAL_MIN && val <= INTERVAL_MAX) {
      overrideInterval = val;
      Serial1.println("Interval override set");
    }
    else {
      Serial1.println(" Invalid Interval....Falling back to local pot ");
      overrideInterval = -1;
    }
  }

  // - useLocalPot (bool) -> reset override
  if (cmd.hasKey("useLocalPot")) {
    if (cmd.getBool("useLocalPot")) {
      overrideInterval = -1;
      Serial1.println("Using local potentiometer");
    }
  }
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // TODO: Receive and process message:
  // 1. Copy payload into char array (null-terminate!)
  char arr[length + 1];
  memcpy(arr, payload, length);
  arr[length] = '\0';

  // 2. Parse JSON (jsonIn.parse)
  if (!jsonIn.parse(arr))
  {
    msgRejectedJson++;
    Serial1.println("Invalid JSON");
    return;
  }
  
  // 3. Call validateMessage()
  if (!validateMessage(jsonIn)) return;

  // 4. Call checkSequence()
  if (!checkSequence(jsonIn)) return;

  // 5. Reset watchdog timer
  watchdogTimer = millis();
  
  // 6. Call processCommand()
  processCommand(jsonIn);

  leaveSafeState();
}


void mqttReconnect() {
  // TODO: Connect to MQTT + subscribe to TOPIC_CMD

  String clientId = "pico_" + String(random(0xffff), HEX);

  while(!mqtt.connected()){
    Serial1.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(clientId.c_str())) {
      Serial1.println("connected");
      mqtt.subscribe(TOPIC_CMD);
      watchdogTimer = millis();
    } else {
      Serial1.print("failed, rc=");
      Serial1.print(mqtt.state());
      Serial1.println(" try again in 5 seconds");
      // Wait 1 second before retrying
      delay(1000);
    }
  }
}


void publishSensorData(int potValue, unsigned long blinkInterval) {
  // TODO: Build JSON with SimpleJson:

  char buffer[256];

  // Clear previous JSON object
  jsonOut.clear();

  // Build JSON payload
  jsonOut.setString("token", SHARED_TOKEN);
  jsonOut.setString("source", "pico");
  jsonOut.setInt("seq", seqOutgoing++);
  jsonOut.setInt("potValue", potValue);
  jsonOut.setBool("blinkEnabled", blinkEnabled);
  jsonOut.setInt("interval", blinkInterval);
  jsonOut.setBool("ledOn", ledState);
  jsonOut.setBool("safeState", inSafeState);
  jsonOut.setInt("uptime", (int) millis());

  // Convert JSON object to char array
  jsonOut.toCharArray(buffer, sizeof(buffer));

  // Publish via MQTT
  if (mqtt.publish(TOPIC_DATA, buffer)) {
    Serial1.println("Data published:");
    // Serial1.println(buffer);
    msgAccepted++;
  } else {
    Serial1.println("MQTT publish failed");
  }

}


void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(HEARTBEAT_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), stateChange, FALLING);

  // init watchdog 
  // watchdogTimer = millis();
  Serial1.begin(115200);
  delay(1000);
  setupWiFi();

  // TODO: mqtt.setServer + mqtt.setCallback
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}


void loop() {

  // Heartbeat
  hbCurrentTime = millis();
  if ((hbCurrentTime - hbPreviousTime) >= 300) 
  {
    hbPreviousTime = hbCurrentTime;
    hbOutputState ^= 1;
    digitalWrite(HEARTBEAT_PIN, hbOutputState);
  }


  // TODO: Check MQTT connection + mqtt.loop()
  if (!mqtt.connected())
  {
    Serial1.println("Connecting to MQTT Broker ");    
    mqttReconnect();
  }

  mqtt.loop();

  // TODO: Check watchdog (enterSafeState if needed)
  if (millis() - watchdogTimer >= WATCHDOG_TIMEOUT) 
  {
    enterSafeState();
  }

  // TODO: Read button (blink toggle, same as Task 1)
  /* Handled by intterupt */

  // TODO: Read poti + determine interval
  if (overrideInterval == -1)
  {
    interval = map(analogRead(POT_PIN), 0, 1023, INTERVAL_MIN, INTERVAL_MAX);
  } else {
    interval = overrideInterval;
  }

  // TODO: Blink with variable interval (same as Task 1)
  if (blinkEnabled) {
    blinkLED();
  }

  // TODO: Publish sensor data (every 500ms)
  static unsigned long lastPublish = 0;

  if (millis() - lastPublish >= 500) {
    lastPublish = millis();
    publishSensorData(analogRead(POT_PIN), interval);
  }

  delay(5);
}


// toggle blink via button, independent of command
void stateChange() {
  blinkEnabled ^= 1;

  if (blinkEnabled) {
    previousTime = millis();
  } else {
    digitalWrite(LED_PIN, LOW);
    ledState = LOW;
  }
}

void blinkLED() {
  currentTime = millis();

  if ((currentTime - previousTime) >= interval) {
    previousTime = currentTime;

    ledState ^= 1;
    digitalWrite(LED_PIN, ledState);
  }
}