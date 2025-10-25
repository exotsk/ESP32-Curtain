#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "IoT"          // WiFi network name
#define WIFI_PASSWORD "677380677380"      // WiFi password

// MQTT Configuration
#define MQTT_SERVER "192.168.1.110"     // MQTT broker IP address
#define MQTT_PORT 1883                  // MQTT broker port

// Optional MQTT Authentication (leave empty if not needed)
#define MQTT_USER ""                    // MQTT username
#define MQTT_PASS ""                    // MQTT password

// Web Server Configuration
#define WEB_SERVER_PORT 80              // HTTP server port

// Stepper Motor Configuration
#define STEPPER_MAX_SPEED 600
#define STEPPER_ACCELERATION 300

// Curtain Configuration
#define CURTAIN_MAX_POSITION 550  // Maximum position in units
#define POSITION_SCALE_FACTOR 100  // Steps per unit

// Safety Configuration
#define WATCHDOG_TIMEOUT 10      // seconds
#define WIFI_TIMEOUT 20000       // milliseconds
#define MQTT_RETRY_DELAY 5000    // milliseconds
#define MQTT_RECONNECT_MAX_DELAY_MS 30000  // Maximum MQTT reconnect delay

// Buffer Configuration
#define MSG_BUFFER_SIZE 64       // Size for message buffers

// Motor Control Configuration
#define STOP_HYSTERESIS 5        // Hysteresis zone for limit switches

#endif // CONFIG_H

// Backwards-compatible names expected by CurtainsESP32.ino
// These map old names used in the .ino file to the values above.
#ifndef CURTAIN_MAXIMUM
#define CURTAIN_MAXIMUM CURTAIN_MAX_POSITION
#endif

#ifndef POSITION_SCALE
#define POSITION_SCALE POSITION_SCALE_FACTOR
#endif

#ifndef WATCHDOG_TIMEOUT_S
#define WATCHDOG_TIMEOUT_S WATCHDOG_TIMEOUT
#endif

#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS WIFI_TIMEOUT
#endif

#ifndef MQTT_RECONNECT_DELAY_MS
#define MQTT_RECONNECT_DELAY_MS MQTT_RETRY_DELAY
#endif

#ifndef MQTT_RECONNECT_MAX_DELAY_MS
#define MQTT_RECONNECT_MAX_DELAY_MS 30000
#endif

#ifndef WEB_SERVER_PORT
#define WEB_SERVER_PORT 80
#endif

#ifndef MDNS_HOSTNAME
#define MDNS_HOSTNAME "curtains"
#endif

#ifndef MSG_BUFFER_SIZE
#define MSG_BUFFER_SIZE 64
#endif

#ifndef STOP_HYSTERESIS
#define STOP_HYSTERESIS 5
#endif
