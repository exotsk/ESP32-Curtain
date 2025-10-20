#include <PubSubClient.h>
#include <AccelStepper.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

// Configuration Constants
#define MQTT_VERSION MQTT_VERSION_3_1_1
#define WATCHDOG_TIMEOUT_S 10
#define WIFI_CONNECT_TIMEOUT_MS 20000
#define MQTT_RECONNECT_DELAY_MS 5000
#define MQTT_RECONNECT_MAX_DELAY_MS 30000

// MQTT Topics
const char* MQTT_CLIENT_ID = "CURTAINS";
const char* MQTT_STEP1 = "/CURTAINS/ROLL1/";
const char* MQTT_STEP2 = "/CURTAINS/ROLL2/";
const char* PUB_STEPS1 = "/CURTAINS/ROLL1_step/";
const char* PUB_STEPS2 = "/CURTAINS/ROLL2_step/";

// Curtain Settings
#define CURTAIN_MAXIMUM 550
#define STOP_HYSTERESIS 5
#define MSG_BUFFER_SIZE 20
#define POSITION_SCALE 100

// Pin Configuration
const int SWITCH_1_PIN = 17;  // Stepper 1 - Upper limit
const int SWITCH_2_PIN = 16;  // Stepper 1 - Lower limit
const int SWITCH_3_PIN = 14;  // Stepper 2 - Upper limit
const int SWITCH_4_PIN = 12;  // Stepper 2 - Lower limit

// Stepper Motor Pins
const int STEPPER1_PINS[4] = {32, 25, 33, 26};
const int STEPPER2_PINS[4] = {23, 21, 22, 19};

// Stepper Configuration
struct StepperController {
  AccelStepper* stepper;
  int32_t targetPosition;
  int32_t lastPublishedPosition;
  bool hysteresisActive;
  int upperLimitPin;
  int lowerLimitPin;
  const char* positionTopic;
};

AccelStepper stepper1(AccelStepper::HALF4WIRE, STEPPER1_PINS[0], STEPPER1_PINS[1], STEPPER1_PINS[2], STEPPER1_PINS[3]);
AccelStepper stepper2(AccelStepper::HALF4WIRE, STEPPER2_PINS[0], STEPPER2_PINS[1], STEPPER2_PINS[2], STEPPER2_PINS[3]);

StepperController controllers[2] = {
  {&stepper1, 0, 0, false, SWITCH_1_PIN, SWITCH_2_PIN, PUB_STEPS1},
  {&stepper2, 0, 0, false, SWITCH_3_PIN, SWITCH_4_PIN, PUB_STEPS2}
};

char msgBuffer[MSG_BUFFER_SIZE];
uint32_t lastReconnectAttempt = 0;
uint32_t reconnectDelay = MQTT_RECONNECT_DELAY_MS;

// WiFi and MQTT Configuration - CHANGE THESE!
const char* WIFI_SSID = "Your SSID";
const char* WIFI_PASSWORD = "Your Password";
const char* MQTT_SERVER = "Your mqtt server";
const int MQTT_PORT = 1883;
// Optional MQTT authentication
const char* MQTT_USER = "";  // Leave empty if not needed
const char* MQTT_PASS = "";  // Leave empty if not needed
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  uint32_t startAttemptTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && 
         millis() - startAttemptTime < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset();  // Reset watchdog during connection
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed! Restarting...");
    delay(1000);
    ESP.restart();
  }
  
  randomSeed(micros());
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Validate payload length
  if (length >= MSG_BUFFER_SIZE) {
    Serial.println("Error: Payload too large");
    return;
  }
  
  // Copy payload to buffer
  for (unsigned int i = 0; i < length; i++) {
    msgBuffer[i] = payload[i];
  }
  msgBuffer[length] = '\0';
  
  // Parse position value
  float position = atof(msgBuffer);
  int32_t targetSteps = (int32_t)(position * POSITION_SCALE);
  
  // Validate range
  if (targetSteps < 0 || targetSteps > CURTAIN_MAXIMUM * POSITION_SCALE) {
    Serial.print("Error: Position out of range: ");
    Serial.println(targetSteps);
    return;
  }
  
  // Route to correct stepper
  if (strcmp(topic, MQTT_STEP1) == 0) {
    controllers[0].targetPosition = targetSteps;
    controllers[0].stepper->moveTo(targetSteps);
    Serial.print("Stepper 1 -> ");
    Serial.println(position);
  } else if (strcmp(topic, MQTT_STEP2) == 0) {
    controllers[1].targetPosition = targetSteps;
    controllers[1].stepper->moveTo(targetSteps);
    Serial.print("Stepper 2 -> ");
    Serial.println(position);
  }
}

bool reconnect() {
  uint32_t now = millis();
  
  // Non-blocking reconnect with exponential backoff
  if (now - lastReconnectAttempt < reconnectDelay) {
    return false;
  }
  
  lastReconnectAttempt = now;
  Serial.print("Attempting MQTT connection...");
  
  // Create unique client ID
  String clientId = String(MQTT_CLIENT_ID) + "-" + String(random(0xffff), HEX);
  
  bool connected = false;
  if (strlen(MQTT_USER) > 0) {
    connected = client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS);
  } else {
    connected = client.connect(clientId.c_str());
  }
  
  if (connected) {
    Serial.println("connected!");
    
    // Subscribe to topics
    if (client.subscribe(MQTT_STEP1)) {
      Serial.print("Subscribed to: ");
      Serial.println(MQTT_STEP1);
    }
    if (client.subscribe(MQTT_STEP2)) {
      Serial.print("Subscribed to: ");
      Serial.println(MQTT_STEP2);
    }
    
    // Reset reconnect delay on success
    reconnectDelay = MQTT_RECONNECT_DELAY_MS;
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
    
    // Exponential backoff (max 30 seconds)
    reconnectDelay = min(reconnectDelay * 2, (uint32_t)MQTT_RECONNECT_MAX_DELAY_MS);
    Serial.print("Next retry in: ");
    Serial.print(reconnectDelay / 1000);
    Serial.println("s");
    return false;
  }
}

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\nESP32 Curtain Controller Starting...");
  
  // Configure watchdog timer
  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);
  Serial.println("Watchdog timer enabled");
  
  // Configure limit switches
  pinMode(SWITCH_1_PIN, INPUT_PULLUP);
  pinMode(SWITCH_2_PIN, INPUT_PULLUP);
  pinMode(SWITCH_3_PIN, INPUT_PULLUP);
  pinMode(SWITCH_4_PIN, INPUT_PULLUP);
  Serial.println("Limit switches configured");
  
  // Setup WiFi
  setup_wifi();
  
  // Configure MQTT
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  client.setKeepAlive(60);
  Serial.println("MQTT configured");
  
  // Configure steppers
  stepper1.setMaxSpeed(600);
  stepper1.setAcceleration(300);
  stepper2.setMaxSpeed(600);
  stepper2.setAcceleration(300);
  Serial.println("Steppers configured");
  
  Serial.println("Setup complete!\n");
}
// Unified stepper control function - eliminates code duplication
void processStepperController(StepperController& ctrl) {
  int32_t currentPos = ctrl.stepper->currentPosition() / POSITION_SCALE;
  
  // Check if hysteresis is active
  if (ctrl.hysteresisActive) {
    // Disable hysteresis if position is outside dead zone
    if (((currentPos > STOP_HYSTERESIS) && (currentPos < CURTAIN_MAXIMUM - STOP_HYSTERESIS)) ||
        (currentPos < -STOP_HYSTERESIS) || (currentPos > CURTAIN_MAXIMUM + STOP_HYSTERESIS)) {
      ctrl.hysteresisActive = false;
    }
  } else {
    // Check upper limit switch
    if (digitalRead(ctrl.upperLimitPin) == LOW) {
      ctrl.stepper->stop();
      ctrl.stepper->disableOutputs();
      ctrl.stepper->setCurrentPosition(CURTAIN_MAXIMUM * POSITION_SCALE);
      ctrl.hysteresisActive = true;
      Serial.println("Upper limit reached");
    }
    
    // Check lower limit switch
    if (digitalRead(ctrl.lowerLimitPin) == LOW) {
      ctrl.stepper->stop();
      ctrl.stepper->disableOutputs();
      ctrl.stepper->setCurrentPosition(0);
      ctrl.hysteresisActive = true;
      Serial.println("Lower limit reached");
    }
  }
  
  // Publish position if changed
  if (currentPos != ctrl.lastPublishedPosition) {
    ctrl.lastPublishedPosition = currentPos;
    snprintf(msgBuffer, MSG_BUFFER_SIZE, "%d", currentPos);
    if (client.publish(ctrl.positionTopic, msgBuffer, true)) {
      // Position published successfully
    }
  }
  
  // Run stepper if not at target
  if (ctrl.targetPosition != ctrl.stepper->currentPosition()) {
    ctrl.stepper->run();
  }
}

void loop() {
  // Reset watchdog timer
  esp_task_wdt_reset();
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    setup_wifi();
  }
  
  // Handle MQTT connection (non-blocking)
  if (!client.connected()) {
    reconnect();
  } else {
    client.loop();
  }
  
  // Process both steppers
  processStepperController(controllers[0]);
  processStepperController(controllers[1]);
  
  // Small delay to prevent tight loop
  yield();
}



