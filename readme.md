# ESP32 Environment Dashboard

⚠️ 20-second demo: https://youtu.be/D_WVxfMUiTU

ESP32 Environment Dashboard is a full-featured monitoring system for temperature and humidity, with an OLED display, alerts, and a web dashboard hosted directly from the ESP32.

It features real-time graphs, averages, warnings, and a toggleable warning light. The project is designed using FreeRTOS tasks for sensors, display, alerts, button handling, and the web server.

---

## Table of Contents

- [Key Features](#key-features)  
- [Project Structure](#project-structure)  
- [Technologies Used](#technologies-used)  
- [Setup Instructions](#setup-instructions)  
- [Environment Variables](#environment-variables)  
- [Accessing the Dashboard](#accessing-the-dashboard)  
- [How It Works](#how-it-works)  
- [Notes](#notes)  

---

## Key Features

- Real-time temperature and humidity monitoring  
- OLED display with multiple views:
  - Current readings
  - 120-second and 240-minute graphs
  - Average temperature and humidity
- Alerts via warning light with configurable thresholds  
- Toggle warning light on/off with long button press  
- Web dashboard hosted on ESP32:
  - Displays current readings and averages
  - Warning alerts for extreme conditions
  - Toggle button to enable/disable alerts
- WiFi AP mode for easy access without external network  

---

## Project Structure

```
├── main.cpp             # Main ESP32 program
├── fonts/               # Custom fonts for OLED display
├── icons/               # Bitmaps for temperature and humidity
└── README.md
```

---

## Technologies Used

**Hardware:**  
- ESP32  
- DHT11 sensor  
- 0.96" OLED Display (SSD1306)  
- Passive buzzer for alerts  
- Push button for control  

**Libraries:**  
- Adafruit SSD1306  
- Adafruit GFX  
- DHT sensor library  
- FreeRTOS (built into ESP32 Arduino framework)  
- WiFi (ESP32 native library)  

**Frontend (Web Dashboard):**  
- HTML, CSS  
- JavaScript for button toggle  
- Hosted directly on ESP32  

---

## Setup Instructions

### Prerequisites

- Arduino IDE installed  
- ESP32 board added to Arduino IDE  
- USB cable to program ESP32  

### Local Setup

1. Clone or copy the repository  
```bash
git clone <your-repo-link>
cd ESP32-Dashboard
```

2. Open `main.cpp` in Arduino IDE  

3. Set WiFi credentials (AP mode optional):  
```cpp
const char *ssid = "EnvironmentMonitor";
const char *password = "Monitor2025123";
```

4. Install required libraries:
   - Adafruit SSD1306  
   - Adafruit GFX  
   - DHT sensor library  

5. Compile and upload to ESP32  

6. Open Serial Monitor to see ESP32 AP IP address  

---

## Accessing the Dashboard

- Connect your PC or phone to the ESP32 WiFi Access Point  
- Navigate to the IP printed on Serial Monitor, e.g.:  
```
http://192.168.4.1
```

---

## How It Works

**Tasks (FreeRTOS):**  

- `SensorTask` – Reads temperature and humidity every second  
- `DisplayTask` – Updates OLED display views  
- `AlertTask` – Controls warning light based on thresholds  
- `buttonRead` – Detects short/long presses for display toggle and warning toggle  
- `server_website` – Hosts HTML dashboard  

**Dashboard Features:**  

- Shows current readings  
- Shows average readings  
- Warnings displayed for extreme conditions  
- Toggle button to enable/disable warning light  

---

## Notes

- Only works with DHT11 sensor and SSD1306 OLED  
- WiFi AP mode does not require an external network  
- Warning thresholds can be customized in `AlertTask`  
- Graphs update dynamically using 120s or 240m data arrays  

---

## License

MIT License  

---

