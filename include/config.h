#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "Your SSID"
#define WIFI_PASSWORD "Your Password"

// MQTT Configuration
#define MQTT_SERVER "Your mqtt server"
#define MQTT_PORT 1883

// Optional MQTT Authentication (leave empty if not needed)
#define MQTT_USER ""
#define MQTT_PASS ""

// Stepper Motor Configuration
#define STEPPER_MAX_SPEED 600
#define STEPPER_ACCELERATION 300

// Curtain Configuration
#define CURTAIN_MAX_POSITION 550  // Maximum position in units
#define POSITION_SCALE_FACTOR 100  // Steps per unit

// Safety Configuration
#define WATCHDOG_TIMEOUT 10  // seconds
#define WIFI_TIMEOUT 20000   // milliseconds
#define MQTT_RETRY_DELAY 5000  // milliseconds

#endif // CONFIG_H
