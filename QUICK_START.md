# ESP32 Curtain Controller - Quick Start Guide

## ⚡ Quick Setup (5 Minutes)

### Step 1: Configure WiFi & MQTT
Open `src/CurtainsESP32.ino` and change these lines (around line 58):

```cpp
const char* WIFI_SSID = "Your SSID";           // ← Your WiFi network name
const char* WIFI_PASSWORD = "Your Password";    // ← Your WiFi password
const char* MQTT_SERVER = "Your mqtt server";   // ← Your MQTT broker IP (e.g., "192.168.1.100")
```

### Step 2: Upload to ESP32
```bash
platformio run --target upload
```

### Step 3: Monitor Serial Output
```bash
platformio device monitor
```

You should see:
```
ESP32 Curtain Controller Starting...
Watchdog timer enabled
Limit switches configured
Connecting to WiFi...
WiFi connected!
IP address: 192.168.1.XXX
MQTT configured
Steppers configured
Setup complete!
```

### Step 4: Test MQTT Control

#### Control Curtain Position
```bash
# Open curtain 1 (position 0)
mosquitto_pub -h YOUR_MQTT_IP -t /CURTAINS/ROLL1/ -m "0"

# Close curtain 1 (position 550)
mosquitto_pub -h YOUR_MQTT_IP -t /CURTAINS/ROLL1/ -m "550"

# Half open curtain 1 (position 275)
mosquitto_pub -h YOUR_MQTT_IP -t /CURTAINS/ROLL1/ -m "275"

# Control curtain 2
mosquitto_pub -h YOUR_MQTT_IP -t /CURTAINS/ROLL2/ -m "275"
```

#### Monitor Position Updates
```bash
mosquitto_sub -h YOUR_MQTT_IP -t /CURTAINS/#
```

---

## 🔌 Hardware Connections

### Stepper 1 (Curtain 1)
- **Motor Pins**: GPIO 32, 25, 33, 26
- **Upper Limit Switch**: GPIO 17 (active LOW)
- **Lower Limit Switch**: GPIO 16 (active LOW)

### Stepper 2 (Curtain 2)
- **Motor Pins**: GPIO 23, 21, 22, 19
- **Upper Limit Switch**: GPIO 14 (active LOW)
- **Lower Limit Switch**: GPIO 12 (active LOW)

### Limit Switch Wiring
```
Switch → GPIO Pin
Switch → GND
(Internal pullup enabled, switch pulls pin LOW when pressed)
```

---

## 📡 MQTT Topics

### Command Topics (Subscribe)
- `/CURTAINS/ROLL1/` - Control curtain 1 position
- `/CURTAINS/ROLL2/` - Control curtain 2 position

**Value Format**: Integer 0-550 (multiplied by 100 internally for steps)

### Status Topics (Publish)
- `/CURTAINS/ROLL1_step/` - Current position of curtain 1
- `/CURTAINS/ROLL2_step/` - Current position of curtain 2

**Value Format**: Integer 0-550 (retained message)

---

## 🏠 Home Assistant Integration

### configuration.yaml
```yaml
mqtt:
  cover:
    - name: "Living Room Curtain 1"
      command_topic: "/CURTAINS/ROLL1/"
      position_topic: "/CURTAINS/ROLL1_step/"
      set_position_topic: "/CURTAINS/ROLL1/"
      position_open: 0
      position_closed: 550
      payload_open: "0"
      payload_close: "550"
      optimistic: false
      qos: 0
      retain: true
      
    - name: "Living Room Curtain 2"
      command_topic: "/CURTAINS/ROLL2/"
      position_topic: "/CURTAINS/ROLL2_step/"
      set_position_topic: "/CURTAINS/ROLL2/"
      position_open: 0
      position_closed: 550
      payload_open: "0"
      payload_close: "550"
      optimistic: false
      qos: 0
      retain: true
```

---

## 🛠️ Troubleshooting

### WiFi Not Connecting
1. Check SSID and password are correct
2. Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
3. Watch serial monitor for error messages
4. Device will auto-restart after 20 seconds if WiFi fails

### MQTT Not Connecting
1. Verify MQTT broker IP address
2. Check if MQTT broker is running: `netstat -an | grep 1883`
3. If using authentication, set `MQTT_USER` and `MQTT_PASS`
4. Check firewall settings
5. Serial monitor shows connection attempts with error codes

### Stepper Not Moving
1. Check motor power supply
2. Verify GPIO pin connections
3. Send MQTT command and watch serial monitor
4. Check if limit switches are triggered
5. Motor will disable outputs when limit is reached

### Position Not Updating
1. Check MQTT connection (serial monitor shows "connected!")
2. Verify MQTT broker is receiving messages
3. Position only publishes when changed
4. Use `mosquitto_sub -h YOUR_IP -t /CURTAINS/#` to monitor

### System Hangs
- Watchdog timer will auto-restart after 10 seconds
- Check serial monitor for last message before hang
- Verify power supply is stable

---

## 🎛️ Calibration

### First-Time Setup
1. Power on ESP32
2. Manually move curtains to closed position
3. Press limit switch at closed position
4. Send MQTT command to move to position 550
5. If curtain doesn't reach fully closed, adjust `CURTAIN_MAXIMUM` constant
6. Send command to move to position 0
7. Press limit switch at open position

### Adjusting Maximum Position
In `CurtainsESP32.ino`, change:
```cpp
#define CURTAIN_MAXIMUM 550  // Adjust this value
```
Larger value = more steps = longer travel distance

### Adjusting Speed
```cpp
stepper1.setMaxSpeed(600);        // Increase for faster movement
stepper1.setAcceleration(300);    // Increase for quicker acceleration
```

---

## 🔐 Optional: MQTT Authentication

If your MQTT broker requires authentication:

```cpp
const char* MQTT_USER = "your_username";
const char* MQTT_PASS = "your_password";
```

---

## 📊 Serial Monitor Output Examples

### Normal Operation
```
ESP32 Curtain Controller Starting...
Watchdog timer enabled
Limit switches configured
Connecting to WiFi...
WiFi connected!
IP address: 192.168.1.50
Attempting MQTT connection...connected!
Subscribed to: /CURTAINS/ROLL1/
Subscribed to: /CURTAINS/ROLL2/
MQTT configured
Steppers configured
Setup complete!

Stepper 1 -> 275.00
Upper limit reached
Stepper 2 -> 100.00
```

### Connection Issues
```
Attempting MQTT connection...failed, rc=-2
Next retry in: 5s
Attempting MQTT connection...failed, rc=-2
Next retry in: 10s
```

**Error Codes**:
- `-2`: MQTT_CONNECT_FAILED (check broker IP)
- `-4`: MQTT_CONNECTION_TIMEOUT (network issue)
- `5`: MQTT_CONNECTION_LOST (check WiFi)

---

## ⚙️ Advanced Configuration

### Change MQTT Topics
```cpp
const char* MQTT_STEP1 = "/CURTAINS/ROLL1/";      // Your custom topic
const char* PUB_STEPS1 = "/CURTAINS/ROLL1_step/"; // Your custom status topic
```

### Adjust Safety Margins
```cpp
#define STOP_HYSTERESIS 5  // Dead zone around limit switches (prevents bouncing)
```

### Watchdog Timeout
```cpp
#define WATCHDOG_TIMEOUT_S 10  // Seconds before auto-restart
```

### WiFi Connection Timeout
```cpp
#define WIFI_CONNECT_TIMEOUT_MS 20000  // Milliseconds before giving up
```

---

## 📈 Performance Notes

- **Response Time**: < 100ms from MQTT command to motor start
- **Position Updates**: Published only on change (reduces MQTT traffic)
- **Reconnection**: Exponential backoff (5s → 10s → 20s → 30s max)
- **Watchdog Protection**: Auto-restart if system hangs > 10 seconds
- **Memory Usage**: ~45KB program / ~10KB RAM

---

## 🆘 Support

1. Check `OPTIMIZATION_NOTES.md` for detailed technical information
2. Review serial monitor output for error messages
3. Verify hardware connections
4. Test MQTT broker independently

---

## 📝 Key Improvements vs Original

✅ **Fixed critical bug** in stepper 1 position tracking  
✅ **Non-blocking MQTT** reconnection  
✅ **Watchdog timer** for automatic recovery  
✅ **Input validation** prevents invalid commands  
✅ **Better error handling** with serial debugging  
✅ **Exponential backoff** for network reliability  
✅ **Code deduplication** - easier to maintain  
✅ **WiFi auto-recovery** - reconnects automatically  

---

**Ready to automate? Happy curtain controlling! 🎉**
