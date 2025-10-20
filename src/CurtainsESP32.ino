#include <PubSubClient.h>
#include <AccelStepper.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>

// Configuration Constants
#define MQTT_VERSION MQTT_VERSION_3_1_1
#define WATCHDOG_TIMEOUT_S 10
#define WIFI_CONNECT_TIMEOUT_MS 20000
#define MQTT_RECONNECT_DELAY_MS 5000
#define MQTT_RECONNECT_MAX_DELAY_MS 30000
#define WEB_SERVER_PORT 80
#define MDNS_HOSTNAME "curtains"

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
WebServer server(WEB_SERVER_PORT);

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

// Web Server HTML Interface
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Curtain Controller</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 600px;
            width: 100%;
        }
        h1 {
            color: #333;
            text-align: center;
            margin-bottom: 10px;
            font-size: 28px;
        }
        .status {
            text-align: center;
            padding: 10px;
            border-radius: 10px;
            margin-bottom: 30px;
            font-weight: 500;
        }
        .status.online { background: #d4edda; color: #155724; }
        .status.offline { background: #f8d7da; color: #721c24; }
        .curtain-card {
            background: #f8f9fa;
            border-radius: 15px;
            padding: 25px;
            margin-bottom: 20px;
        }
        .curtain-title {
            font-size: 20px;
            font-weight: 600;
            color: #495057;
            margin-bottom: 15px;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .curtain-icon {
            font-size: 24px;
        }
        .position-display {
            text-align: center;
            font-size: 48px;
            font-weight: 700;
            color: #667eea;
            margin: 20px 0;
        }
        .position-label {
            text-align: center;
            color: #6c757d;
            font-size: 14px;
            margin-bottom: 20px;
        }
        .slider {
            width: 100%;
            height: 8px;
            border-radius: 5px;
            background: #e9ecef;
            outline: none;
            -webkit-appearance: none;
            margin: 20px 0;
        }
        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 24px;
            height: 24px;
            border-radius: 50%;
            background: #667eea;
            cursor: pointer;
            box-shadow: 0 2px 5px rgba(0,0,0,0.2);
        }
        .slider::-moz-range-thumb {
            width: 24px;
            height: 24px;
            border-radius: 50%;
            background: #667eea;
            cursor: pointer;
            border: none;
            box-shadow: 0 2px 5px rgba(0,0,0,0.2);
        }
        .button-group {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 10px;
            margin-top: 15px;
        }
        .btn {
            padding: 12px;
            border: none;
            border-radius: 10px;
            font-size: 14px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
            color: white;
        }
        .btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.2); }
        .btn-open { background: #28a745; }
        .btn-half { background: #ffc107; }
        .btn-close { background: #dc3545; }
        .btn-stop { background: #6c757d; grid-column: 1 / -1; }
        .info {
            text-align: center;
            color: #6c757d;
            font-size: 12px;
            margin-top: 20px;
            padding-top: 20px;
            border-top: 1px solid #e9ecef;
        }
        @media (max-width: 480px) {
            .container { padding: 20px; }
            h1 { font-size: 24px; }
            .position-display { font-size: 36px; }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🏠 Curtain Controller</h1>
        <div class="status online" id="status">● Connected</div>
        
        <!-- Curtain 1 -->
        <div class="curtain-card">
            <div class="curtain-title">
                <span class="curtain-icon">🪟</span>
                Curtain 1
            </div>
            <div class="position-display" id="pos1">0</div>
            <div class="position-label">Position (0 = Open, 550 = Closed)</div>
            <input type="range" min="0" max="550" value="0" class="slider" id="slider1">
            <div class="button-group">
                <button class="btn btn-open" onclick="setPosition(1, 0)">Open</button>
                <button class="btn btn-half" onclick="setPosition(1, 275)">Half</button>
                <button class="btn btn-close" onclick="setPosition(1, 550)">Close</button>
                <button class="btn btn-stop" onclick="stopCurtain(1)">⏹ Stop</button>
            </div>
        </div>
        
        <!-- Curtain 2 -->
        <div class="curtain-card">
            <div class="curtain-title">
                <span class="curtain-icon">🪟</span>
                Curtain 2
            </div>
            <div class="position-display" id="pos2">0</div>
            <div class="position-label">Position (0 = Open, 550 = Closed)</div>
            <input type="range" min="0" max="550" value="0" class="slider" id="slider2">
            <div class="button-group">
                <button class="btn btn-open" onclick="setPosition(2, 0)">Open</button>
                <button class="btn btn-half" onclick="setPosition(2, 275)">Half</button>
                <button class="btn btn-close" onclick="setPosition(2, 550)">Close</button>
                <button class="btn btn-stop" onclick="stopCurtain(2)">⏹ Stop</button>
            </div>
        </div>
        
        <div class="info">
            ESP32 Curtain Controller | IP: <span id="ip">--</span>
        </div>
    </div>
    
    <script>
        const slider1 = document.getElementById('slider1');
        const slider2 = document.getElementById('slider2');
        const pos1Display = document.getElementById('pos1');
        const pos2Display = document.getElementById('pos2');
        
        // Update display when slider moves
        slider1.oninput = function() {
            pos1Display.textContent = this.value;
        }
        slider2.oninput = function() {
            pos2Display.textContent = this.value;
        }
        
        // Send position when slider is released
        slider1.onchange = function() {
            setPosition(1, this.value);
        }
        slider2.onchange = function() {
            setPosition(2, this.value);
        }
        
        function setPosition(curtain, position) {
            fetch(`/set?curtain=${curtain}&position=${position}`)
                .then(response => response.json())
                .then(data => {
                    if (data.status === 'ok') {
                        console.log(`Curtain ${curtain} set to ${position}`);
                        if (curtain === 1) {
                            slider1.value = position;
                            pos1Display.textContent = position;
                        } else {
                            slider2.value = position;
                            pos2Display.textContent = position;
                        }
                    }
                })
                .catch(err => console.error('Error:', err));
        }
        
        function stopCurtain(curtain) {
            fetch(`/stop?curtain=${curtain}`)
                .then(response => response.json())
                .then(data => console.log('Stopped:', data))
                .catch(err => console.error('Error:', err));
        }
        
        function updateStatus() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    pos1Display.textContent = data.position1;
                    pos2Display.textContent = data.position2;
                    slider1.value = data.position1;
                    slider2.value = data.position2;
                    document.getElementById('ip').textContent = data.ip;
                    document.getElementById('status').className = 'status online';
                    document.getElementById('status').textContent = '● Connected';
                })
                .catch(err => {
                    document.getElementById('status').className = 'status offline';
                    document.getElementById('status').textContent = '● Disconnected';
                });
        }
        
        // Update status every 2 seconds
        updateStatus();
        setInterval(updateStatus, 2000);
    </script>
</body>
</html>
)rawliteral";

// Web Server Handlers
void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleSet() {
  if (server.hasArg("curtain") && server.hasArg("position")) {
    int curtain = server.arg("curtain").toInt();
    int position = server.arg("position").toInt();
    
    // Validate
    if (curtain < 1 || curtain > 2 || position < 0 || position > CURTAIN_MAXIMUM) {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid parameters\"}");
      return;
    }
    
    // Set position
    int32_t targetSteps = position * POSITION_SCALE;
    controllers[curtain - 1].targetPosition = targetSteps;
    controllers[curtain - 1].stepper->moveTo(targetSteps);
    
    Serial.print("Web: Curtain ");
    Serial.print(curtain);
    Serial.print(" -> ");
    Serial.println(position);
    
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
  }
}

void handleStop() {
  if (server.hasArg("curtain")) {
    int curtain = server.arg("curtain").toInt();
    
    if (curtain < 1 || curtain > 2) {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid curtain\"}");
      return;
    }
    
    // Stop the curtain
    controllers[curtain - 1].stepper->stop();
    controllers[curtain - 1].targetPosition = controllers[curtain - 1].stepper->currentPosition();
    
    Serial.print("Web: Stopped curtain ");
    Serial.println(curtain);
    
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing curtain parameter\"}");
  }
}

void handleStatus() {
  int32_t pos1 = controllers[0].stepper->currentPosition() / POSITION_SCALE;
  int32_t pos2 = controllers[1].stepper->currentPosition() / POSITION_SCALE;
  
  String json = "{";
  json += "\"position1\":" + String(pos1) + ",";
  json += "\"position2\":" + String(pos2) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

void setupWebServer() {
  // Setup routes
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/stop", handleStop);
  server.on("/status", handleStatus);
  
  // Start server
  server.begin();
  Serial.println("Web server started");
  
  // Setup mDNS
  if (MDNS.begin(MDNS_HOSTNAME)) {
    Serial.print("mDNS responder started: http://");
    Serial.print(MDNS_HOSTNAME);
    Serial.println(".local");
    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
  } else {
    Serial.println("Error setting up mDNS responder!");
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
  
  // Setup Web Server
  setupWebServer();
  Serial.print("Web interface available at: http://");
  Serial.println(WiFi.localIP());
  
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
  
  // Handle web server requests
  server.handleClient();
  
  // Process both steppers
  processStepperController(controllers[0]);
  processStepperController(controllers[1]);
  
  // Small delay to prevent tight loop
  yield();
}



