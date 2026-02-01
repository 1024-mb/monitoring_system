#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"
#include <Fonts/TomThumb.h>
#include <Fonts/FreeSans9pt7b.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

#define OLED_ADDR 0x3C
#define DHTPIN    4
#define DHTTYPE   DHT11
#define WARNING_LIGHT    19
#define MAX_POINTS 128
#define BUTTON 5
#define SCREEN_HEIGHT 63
#define LONG_PRESS_MS 2000  // 2 seconds threshold


static int displayToggle = 0;
int arrIndex = 0;
static int lastState = 1;
static int cycles = 0;
bool warningEnabled = true;
int count = 0;
int readings_count = 0;
float avg_temp = 0;
float avg_humidity = 0;

const char *ssid = "EnvironmentMonitor";
const char *password = "Monitor2025123";

// 16x16 Thermometer with bulb and tick marks
const unsigned char ICON_TEMP_16[] PROGMEM = {
  0x03, 0x80, //      ###
  0x02, 0x40, //      # #
  0x02, 0x40, //      # #
  0x02, 0x60, //      # ## (Tick)
  0x02, 0x40, //      # #
  0x02, 0x60, //      # ## (Tick)
  0x02, 0x40, //      # #
  0x03, 0xC0, //      ####
  0x07, 0xE0, //     ######
  0x0F, 0xF0, //    ########
  0x1F, 0xF8, //   ##########
  0x1F, 0xF8, //   ##########
  0x1F, 0xF8, //   ##########
  0x0F, 0xF0, //    ########
  0x07, 0xE0, //     ######
  0x00, 0x00  //
};

// 16x16 Water Drop with reflection highlight
const unsigned char ICON_HUM_16[] PROGMEM = {
  0x00, 0x00, 
  0x01, 0x80, //        XX
  0x03, 0xC0, //       XXXX
  0x06, 0x60, //      XX  XX   (Highlight)
  0x0C, 0x60, //     XX   XX
  0x0C, 0xF0, //     XX  XXXX
  0x19, 0xF8, //    XX  XXXXXX
  0x1F, 0xF8, //    XXXXXXXXXX
  0x1F, 0xF8, //    XXXXXXXXXX
  0x1F, 0xF8, //    XXXXXXXXXX
  0x1F, 0xF8, //    XXXXXXXXXX
  0x0F, 0xF0, //     XXXXXXXX
  0x07, 0xE0, //      XXXXXX
  0x03, 0xC0, //       XXXX
  0x00, 0x00, 
  0x00, 0x00
};



WiFiServer server(80);

Adafruit_SSD1306 display(128, 64, &Wire, -1);
DHT dht(DHTPIN, DHTTYPE);

typedef struct {
  float temp;
  float humidity;
} SensorData;

SensorData data;
SensorData data_array_120s[MAX_POINTS];
SensorData data_array_240m[MAX_POINTS];

SemaphoreHandle_t dataMutex;

void unavailableMsg() {
    display.setCursor(0, 30);
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(1.5);
    display.print("UNAVAILABLE");
    display.setFont(NULL);
}

void toggleState() {
  xSemaphoreTake(dataMutex, portMAX_DELAY);

  if(displayToggle == 5) {
    displayToggle = 0;
  }
  else {
    displayToggle++;
  }
  xSemaphoreGive(dataMutex);

}

void addValue(SensorData dataIn) {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  data_array_120s[arrIndex] = dataIn;
  arrIndex = (arrIndex + 1) % MAX_POINTS; // wrap aroundx


  xSemaphoreGive(dataMutex);
}

void SensorTask(void *pvParameters) {
  while (1) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    avg_temp += t;
    avg_humidity += h;
    readings_count++;
    xSemaphoreGive(dataMutex);


    if (!isnan(t) && !isnan(h) && !(t==0 && h==0)) {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      data.temp = t;
      data.humidity = h;
      xSemaphoreGive(dataMutex);

      addValue(data);
    }

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    count++;

    if(count >= 127) {
      data_array_240m[cycles].temp = data_array_120s[120].temp;
      data_array_240m[cycles].humidity = data_array_120s[120].humidity;

      cycles++;
      count = 0;
    }
    xSemaphoreGive(dataMutex);
    vTaskDelay(pdMS_TO_TICKS(1000));

  }
}

void humidityGraph120s() {
    display.setCursor(0, 0);
    display.print("Humidity Graph - 120s");

    xSemaphoreTake(dataMutex, portMAX_DELAY);

    // 1. Find min and max temperature
    float minHumidityVal = data_array_120s[0].humidity;
    float maxHumidityVal = data_array_120s[0].humidity;
    for (int i = 1; i < MAX_POINTS; i++) {
        if (data_array_120s[i].humidity < minHumidityVal) minHumidityVal = data_array_120s[i].humidity;
        if (data_array_120s[i].humidity > maxHumidityVal) maxHumidityVal = data_array_120s[i].humidity;
    }
    if (maxHumidityVal - minHumidityVal < 0.1f) maxHumidityVal += 1.0f; // avoid zero range
      if (minHumidityVal == 0) {
        minHumidityVal = maxHumidityVal - 10;
      }
      else {
        minHumidityVal -= 5;
        maxHumidityVal += 5;
      }

    // 2. Clear only graph area
    display.fillRect(0, 16, 128, 64, SSD1306_BLACK);

    // 3. Draw axes
    drawAxes120s_humidity(minHumidityVal, maxHumidityVal);

    // 4. Plot points
    int lastX = 0;
    int lastY = mapToY(data_array_120s[arrIndex].humidity, minHumidityVal, maxHumidityVal); // adjust min/max as needed
    int idx = arrIndex;

    for (int i = 1; i < MAX_POINTS; i++) {
        idx = (idx + 1) % MAX_POINTS;
        int x = map(i, 0, MAX_POINTS - 1, 0, 127);
        int y = mapToY(data_array_120s[idx].humidity, minHumidityVal, maxHumidityVal);
        display.drawLine(lastX, lastY, x, y, SSD1306_WHITE);
        lastX = x;
        lastY = y;
    }



    xSemaphoreGive(dataMutex);
}

void humidityGraph240m() {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    int temp_cycle = cycles;  
    xSemaphoreGive(dataMutex);

    if(temp_cycle >= 5) {
      display.setCursor(0, 0);
      display.print("Temp Graph - 240m");
      xSemaphoreTake(dataMutex, portMAX_DELAY);

      // 1. Find min and max temperature
      float minHumidityVal = data_array_240m[0].humidity;
      float maxHumidityVal = data_array_240m[0].humidity;
      for (int i = 1; i < MAX_POINTS; i++) {
          if (data_array_240m[i].humidity < minHumidityVal) minHumidityVal = data_array_240m[i].humidity;
          if (data_array_240m[i].temp > maxHumidityVal) minHumidityVal = data_array_240m[i].humidity;
      }

      if (minHumidityVal == 0) {
        minHumidityVal = maxHumidityVal - 15;
      }
      else {
        maxHumidityVal += 7.5;
        minHumidityVal -= 7.5;
      }

      if (maxHumidityVal - minHumidityVal < 0.1f) maxHumidityVal += 1.0f; // avoid zero range

      // 2. Clear only graph area
      display.fillRect(0, 16, 128, 64, SSD1306_BLACK);

      // 3. Draw axes
      drawAxes240m_humidity(minHumidityVal, maxHumidityVal);

      // 4. Plot points
      int lastX = 0, lastY = mapToY(data_array_240m[0].humidity, minHumidityVal, maxHumidityVal);
      for (int i = 1; i < MAX_POINTS; i++) {
          int x = map(i, 0, MAX_POINTS - 1, 0, 127);   // X across the screen

          int y = mapToY(data_array_240m[i].humidity, minHumidityVal, maxHumidityVal);
          display.drawLine(lastX, lastY, x, y, SSD1306_WHITE);
          lastX = x;
          lastY = y;
      }


      xSemaphoreGive(dataMutex);
    }
    else {
      unavailableMsg();
    }
}

// ----------------------
// REAL-TIME TEMP GRAPH
// ----------------------
void tempGraph240m() {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    int temp_cycle = cycles;  
    xSemaphoreGive(dataMutex);

    if(temp_cycle >= 5) {
      display.setCursor(0, 0);
      display.print("Temp Graph - 240m");
      xSemaphoreTake(dataMutex, portMAX_DELAY);

      // 1. Find min and max temperature
      float minTempVal = data_array_240m[0].temp;
      float maxTempVal = data_array_240m[0].temp;
      for (int i = 1; i < MAX_POINTS; i++) {
          if (data_array_240m[i].temp < minTempVal) minTempVal = data_array_240m[i].temp;
          if (data_array_240m[i].temp > maxTempVal) maxTempVal = data_array_240m[i].temp;
      }

      if (minTempVal == 0) {
        minTempVal = maxTempVal - 10;
      }
      else {
        minTempVal -= 2.5;
        maxTempVal += 2.5;
      }

      if (maxTempVal - minTempVal < 0.1f) maxTempVal += 1.0f; // avoid zero range

      // 2. Clear only graph area
      display.fillRect(0, 16, 128, 64, SSD1306_BLACK);

      // 3. Draw axes
      drawAxes240m_temp(minTempVal, maxTempVal);

      // 4. Plot points
      int lastX = 0, lastY = mapToY(data_array_240m[0].temp, minTempVal, maxTempVal);
      for (int i = 1; i < MAX_POINTS; i++) {
          int x = map(i, 0, MAX_POINTS - 1, 0, 127);   // X across the screen

          int y = mapToY(data_array_240m[i].temp, minTempVal, maxTempVal);
          display.drawLine(lastX, lastY, x, y, SSD1306_WHITE);
          lastX = x;
          lastY = y;
      }


      xSemaphoreGive(dataMutex);
    }
    else {
      unavailableMsg();
    }
}


void tempGraph120s() {
    display.setCursor(0, 0);
    display.print("Temp Graph - 120s");

    xSemaphoreTake(dataMutex, portMAX_DELAY);

    // 1. Find min and max temperature
    float minTempVal = data_array_120s[0].temp;
    float maxTempVal = data_array_120s[0].temp;
    for (int i = 1; i < MAX_POINTS; i++) {
        if (data_array_120s[i].temp < minTempVal) minTempVal = data_array_120s[i].temp;
        if (data_array_120s[i].temp > maxTempVal) maxTempVal = data_array_120s[i].temp;
    }
    if (maxTempVal - minTempVal < 0.1f) maxTempVal += 1.0f; // avoid zero range
      if (minTempVal == 0) {
        minTempVal = maxTempVal - 10;
      }
      else {
        minTempVal -= 2.5;
        maxTempVal += 2.5;
      }

    // 2. Clear only graph area
    display.fillRect(0, 16, 128, 64, SSD1306_BLACK);

    // 3. Draw axes
    drawAxes120s_temp(minTempVal, maxTempVal);

    // 4. Plot points
    int lastX = 0;
    int lastY = mapToY(data_array_120s[arrIndex].temp, minTempVal, maxTempVal); // adjust min/max as needed
    int idx = arrIndex;

    for (int i = 1; i < MAX_POINTS; i++) {
        idx = (idx + 1) % MAX_POINTS;
        int x = map(i, 0, MAX_POINTS - 1, 0, 127);
        int y = mapToY(data_array_120s[idx].temp, minTempVal, maxTempVal);
        display.drawLine(lastX, lastY, x, y, SSD1306_WHITE);
        lastX = x;
        lastY = y;
    }



    xSemaphoreGive(dataMutex);
}

// ----------------------
// DRAW AXES
// -------------------


void drawAxes120s_temp(float minTempVal, float maxTempVal) {
  // X-axis
  display.drawFastHLine(0, 63, 128, SSD1306_WHITE);
  // Y-axis
  display.drawFastVLine(0, 16, 64, SSD1306_WHITE);

  // Y-axis labels
  display.setFont(&TomThumb);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  for (int i = 1; i <= 5; i++) {
      float temp = minTempVal + (maxTempVal - minTempVal) * i / 5;
      int y;

      y = mapToY(temp, minTempVal, maxTempVal);
      
      display.setCursor(2, y+4);
      display.print(temp, 1);
      display.print("C");
      display.drawFastHLine(0, y, 2, SSD1306_WHITE);
  }

  // X-axis labels
  for (int i = 1; i <= 5; i++) {
      int x = map(i, 0, 5, 0, 127);
      display.setCursor(x - 6, 64 - 2);

      display.print(-127+(i * (MAX_POINTS / 5)));
      display.print("s");

      display.drawFastVLine(x, 61, 2, SSD1306_WHITE);
  }
  display.setFont(NULL);

}

void drawAxes120s_humidity(float minTempVal, float maxTempVal) {
  // X-axis
  display.drawFastHLine(0, 63, 128, SSD1306_WHITE);
  // Y-axis
  display.drawFastVLine(0, 16, 64, SSD1306_WHITE);

  // Y-axis labels
  display.setFont(&TomThumb);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  for (int i = 1; i <= 5; i++) {
      float temp = minTempVal + (maxTempVal - minTempVal) * i / 5;
      int y;

      y = mapToY(temp, minTempVal, maxTempVal);
      
      display.setCursor(2, y+4);
      display.print(temp, 1);
      display.print("%");
      display.drawFastHLine(0, y, 2, SSD1306_WHITE);
  }

  // X-axis labels
  for (int i = 1; i <= 5; i++) {
      int x = map(i, 0, 5, 0, 127);
      display.setCursor(x - 6, 64 - 2);

      display.print(-127+(i * (MAX_POINTS / 5)));
      display.print("s");

      display.drawFastVLine(x, 61, 2, SSD1306_WHITE);
  }
  display.setFont(NULL);

}

void drawAxes240m_humidity(float minTempVal, float maxTempVal) {
  // X-axis
  display.drawFastHLine(0, 63, 128, SSD1306_WHITE);
  // Y-axis
  display.drawFastVLine(0, 16, 64, SSD1306_WHITE);

  // Y-axis labels
  display.setFont(&TomThumb);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  for (int i = 1; i <= 5; i++) {
      float temp = minTempVal + (maxTempVal - minTempVal) * i / 5;
      int y;

      y = mapToY(temp, minTempVal, maxTempVal);
      
      display.setCursor(2, y+4);
      display.print(temp, 1);
      display.print("%");
      display.drawFastHLine(0, y, 2, SSD1306_WHITE);
  }

  // X-axis labels
  for (int i = 1; i <= 5; i++) {
      int x = map(i, 0, 5, 0, 127);
      display.setCursor(x - 6, 64 - 2);

      display.print(-127+(i * (MAX_POINTS / 5)));
      display.print("m");

      display.drawFastVLine(x, 61, 2, SSD1306_WHITE);
  }
  display.setFont(NULL);

}

void drawAxes240m_temp(float minTempVal, float maxTempVal) {
  // X-axis
  display.drawFastHLine(0, 63, 128, SSD1306_WHITE);
  // Y-axis
  display.drawFastVLine(0, 16, 64, SSD1306_WHITE);

  // Y-axis labels
  display.setFont(&TomThumb);
  display.setTextSize(1);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  for (int i = 0; i <= 5; i++) {
      float temp = minTempVal + (maxTempVal - minTempVal) * i / 5;
      int y;

      y = mapToY(temp, minTempVal, maxTempVal);
      
      display.setCursor(2, y+4);
      display.print(temp, 1);
      display.print("C");
      display.drawFastHLine(0, y, 3, SSD1306_WHITE);
  }

  // X-axis labels
  for (int i = 0; i <= 5; i++) {
      int x = map(i, 0, 5, 0, 127);
      display.setCursor(x - 6, 64 - 2);
      display.print("-");
      display.print((cycles) + (i * (MAX_POINTS / 5)));
      display.print("m");
      display.drawFastVLine(x, 60, 4, SSD1306_WHITE);

  }
  display.setFont(NULL);

}


// ----------------------
// MAP TEMP TO OLED Y
// ----------------------
int mapToY(float value, float minTempVal, float maxTempVal) {
    // Bottom-left origin, y=64 is bottom, y=0 top
    return 64 - (int)((value - minTempVal) * 47 / (maxTempVal - minTempVal)); // 48 pixels graph height
}

void MainDisplay() {
  time_t curr_time;
  curr_time = time(NULL);

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  float t = data.temp;
  float h = data.humidity;
  xSemaphoreGive(dataMutex);

  tm *tm_local = localtime(&curr_time);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.print(tm_local->tm_hour);
  display.print(":");
  display.print(tm_local->tm_min);
  display.print(":");
  display.print(tm_local->tm_sec);
  display.print("\n\n");

  // TEMPERATURE
  display.drawBitmap(0, 28, ICON_TEMP_16, 16, 16, SSD1306_WHITE);
  display.setCursor(23, 28); // align right of icon
  display.print(t, 1);
  display.print(" C");


    // HUMIDITY
  display.drawBitmap(0, 46, ICON_HUM_16, 16, 16, SSD1306_WHITE);
  display.setCursor(23, 46);
  display.print(h, 1);
  display.print(" %");

}

void AveragesDisplay() {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  float t = avg_temp / readings_count;
  float h = avg_humidity / readings_count;
  xSemaphoreGive(dataMutex);

  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("AVERAGES");

  // TEMPERATURE
  display.drawBitmap(0, 28, ICON_TEMP_16, 16, 16, SSD1306_WHITE);
  display.setCursor(23, 28); // align right of icon
  display.print(t, 1);
  display.print(" C");


    // HUMIDITY
  display.drawBitmap(0, 46, ICON_HUM_16, 16, 16, SSD1306_WHITE);
  display.setCursor(23, 46);
  display.print(h, 1);
  display.print(" %");

}

void DisplayTask(void *pvParameters) {

  while (1) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    SensorData temp_data = data;
    xSemaphoreGive(dataMutex);  

    xSemaphoreTake(dataMutex, portMAX_DELAY);
    int toggle = displayToggle;
    xSemaphoreGive(dataMutex);  


    display.clearDisplay();

    switch(toggle) {
      case 0:
        MainDisplay();
        break;

      case 1:
        AveragesDisplay();
        break;

      case 2:
        tempGraph120s();
        break;

      case 3:
        tempGraph240m();
        break;

      case 4:
        humidityGraph120s();
        break;

      case 5:
        humidityGraph240m();
        break;
    }

    display.display();

    vTaskDelay(pdMS_TO_TICKS(1000));
    
  }
}

void AlertTask(void *pvParameters) {
  while (1) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    float t = data.temp;
    float h = data.humidity;
    xSemaphoreGive(dataMutex);

    if ((t > 50 || t < -20) && warningEnabled == true) {
      digitalWrite(WARNING_LIGHT, HIGH);
      vTaskDelay(pdMS_TO_TICKS(50));
      digitalWrite(WARNING_LIGHT, LOW);
    }
    else if ((t > 40 || (t > -20 && t < 0)) && warningEnabled == true){
      digitalWrite(WARNING_LIGHT, HIGH);
      vTaskDelay(pdMS_TO_TICKS(200));
      digitalWrite(WARNING_LIGHT, LOW);
    }
    else if ((t > 30 || t < 0) && warningEnabled == true) {
      digitalWrite(WARNING_LIGHT, HIGH);
      vTaskDelay(pdMS_TO_TICKS(300));
      digitalWrite(WARNING_LIGHT, LOW);
    }

    if ((h>=75) && warningEnabled == true) {
      digitalWrite(WARNING_LIGHT, HIGH);
      vTaskDelay(pdMS_TO_TICKS(50));
      digitalWrite(WARNING_LIGHT, LOW);
    }
    else if (h<=25 && warningEnabled == true){
      digitalWrite(WARNING_LIGHT, HIGH);
      vTaskDelay(pdMS_TO_TICKS(300));
      digitalWrite(WARNING_LIGHT, LOW);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void buttonRead(void *pvParameters) {
    int reading;
    int lastState = HIGH;
    unsigned long pressStart = 0;

    while (1) {
        reading = digitalRead(BUTTON);

        // Button pressed
        if (lastState == HIGH && reading == LOW) {
            pressStart = millis();  // start timing
        }

        // Button released
        if (lastState == LOW && reading == HIGH) {
            unsigned long pressDuration = millis() - pressStart;
            if (pressDuration >= LONG_PRESS_MS) {
                // Long press action
                xSemaphoreTake(dataMutex, portMAX_DELAY);
                warningEnabled = !warningEnabled;
                xSemaphoreGive(dataMutex);

                display.clearDisplay();
                display.setCursor(0, 0);
                display.setTextSize(1);
                if(warningEnabled == true) {
                  display.setCursor(0, 30);
                  display.setFont(&FreeSans9pt7b);
                  display.setTextSize(1);
                  display.print("FLASH ENABLED");
                  display.setFont(NULL);

                }
                else if(warningEnabled == false) {
                  display.setCursor(0, 30);
                  display.setFont(&FreeSans9pt7b);
                  display.setTextSize(1);
                  display.print("FLASH DISABLED");
                  display.setFont(NULL);
                }
                display.display();
                vTaskDelay(pdMS_TO_TICKS(5000));
                display.clearDisplay();
                

            } else {
                // Short press action
                toggleState();
            }
            pressStart = 0;  // reset
        }

        lastState = reading;
        vTaskDelay(pdMS_TO_TICKS(7));  // debounce + scheduling
    }
}

void server_website(void *pvParameters) {
  while(1) {
    WiFiClient client = server.available();   // listen for incoming clients

    if (client) {                             // if you get a client,
      Serial.println("New Client.");           // print a message out the serial port
      String currentLine = "";                // make a String to hold incoming data from the client
      while (client.connected()) {            // loop while the client's connected
        if (client.available()) {             // if there's bytes to read from the client,
          char c = client.read();             // read a byte, then
          Serial.write(c);                    // print it out the serial monitor
          if (c == '\n') {                    // if the byte is a newline character

            // if the current line is blank, you got two newline characters in a row.
            // that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0) {

            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type: text/html; charset=UTF-8");
              client.println();
              client.print(
              "<!DOCTYPE html>"
              "<html lang='en'>"
              "<head>"
              "  <meta charset='UTF-8'>"
              "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
              "  <title>DASHBOARD</title>"
              "  <style>"
              "/* ---- WARNINGS ---- */"
              "    .warning-box {"
              "      display:inline-block;"
              "      padding:4px 8px;"
              "      border:1.5px dotted currentColor;"
              "      border-radius:6px;"
              "      font-size:0.7em;"
              "      margin:4px 0;"
              "      background:rgba(255,255,255,0.04);"
              "    }"
              "    .warning-danger {"
              "      color:#ff6666;"
              "      font-weight:600;"
              "    }"
              "    .warning-caution {"
              "      color:#ffcc00;"
              "      font-weight:600;"
              "    }"
              "    .warning-normal {"
              "      color:#00ff99;"
              "      font-weight:600;"
              "    }"
              "    /* ---- CARD ---- */"
              "    .toggle-card {"
              "      background: #252525;"
              "      border-radius: 16px;"
              "      padding: 20px;"
              "      text-align: center;"
              "      box-shadow: 0 4px 15px rgba(0,0,0,0.3);"
              "      margin: 10px auto;"
              "      max-width: 320px;"
              "    }"
              "    /* ---- BUTTON ---- */"
              "    .toggle-btn {"
              "      display: inline-block;"
              "      padding: 12px 24px;"
              "      font-size: 0.9em;"
              "      font-weight: 700;"
              "      text-decoration: none;"
              "      border-radius: 50px;"
              "      transition: all 0.2s ease;"
              "      border: none;"
              "    }"
              "    .toggle-btn.true {"
              "      background: #00ff99;"
              "      color: #1a1a1a;"
              "      box-shadow: 0 0 15px rgba(0, 255, 153, 0.4);"
              "    }"
              "    .toggle-btn.false {"
              "      background: #cc3333;  "
              "      color: #ffffff;"
              "      border: 1px solid #990000;"
              "      box-shadow: 0 0 12px rgba(204, 51, 51, 0.3); /* Red glow */"
              "    }"
              "    .toggle-btn:active { transform: scale(0.95); }"
              "    body {"
              "      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;"
              "      background-color: #1e1e2f;"
              "      color: #ffffff;"
              "      text-align: center;"
              "      padding: 10px;"
              "    }"
              "    h1 {"
              "      font-size: 2em;"
              "      margin-bottom: 0.2em;"
              "      color: #00ffcc;"
              "    }"
              "    h3 {"
              "      margin-top: 5em;"
              "      color: #ffaa00;"
              "    }"
              "    p {"
              "      font-size: 1.2em;"
              "      margin: 0.3em 0;"
              "      background: rgba(255, 255, 255, 0.05);"
              "      padding: 8px;"
              "      border-radius: 8px;"
              "      display: inline-block;"
              "      min-width: 200px;"
              "    }"
              "    .temp { color: #ff5555; }"
              "    .humidity { color: #55aaff; }"
              "  </style>"
              "</head>"
              "<body>"
              "  <h1>DASHBOARD</h1>"
              "  <h3>CURRENT</h3>");

              xSemaphoreTake(dataMutex, portMAX_DELAY);


              if(data.temp > 50) {
                client.print("<div class = 'warning-box'><div class='warning-danger'>⚠️ EXTREMELY HIGH TEMPERATURE!</div></div>");
              }
              else if(data.temp > 40) {
                client.print("<div class = 'warning-box'><div class='warning-caution'>⚠️ HIGH TEMPERATURE!</div></div>");
              }
              else if(data.temp > 30) {
                client.print("<div class = 'warning-box'><div class='warning-normal'>⚠️ HIGH TEMPERATURE</div></div>");
              }

              if (data.humidity>=75) {
                client.print("<div class = 'warning-box'><div class='warning-danger'>⚠️ EXTREMELY HIGH HUMIDITY!</div></div>");

              }
              else if (data.humidity<=25 && warningEnabled == true){
                client.print("<div class = 'warning-box'><div class='warning-danger'>⚠️ EXTREMELY LOW HUMIDITY!</div></div>");

              }


              client.print("<p class='temp'>&#x1F321; TEMPERATURE: ");
              client.print(data.temp);
              client.print(" °C</p>"
                          "<p class='humidity'>&#x1F4A7; HUMIDITY: ");
              client.print(data.humidity);
              client.print(" %</p>"
                          "<h3>AVERAGES</h3>"
                          "<p class='temp'>&#x1F321; TEMPERATURE: ");
              float t = avg_temp / readings_count;
              client.print(t);
              client.print(" °C</p>"
                          "<p class='humidity'>&#x1F4A7; HUMIDITY: ");
              float h = avg_humidity / readings_count;
              client.print(h);
              client.print(" %</p>");
              client.print("<div style='margin-top: 5em;' class='toggle-card'>");
              client.print("<h3 style='margin-top: 0px;'>WARNING LIGHT</h3>");

              // Dynamic Button
              client.print("<a href='/warn_toggle' class='toggle-btn ");
              client.print(warningEnabled ? "false" : "true"); // 'true' class makes it glow green
              client.print("'>");

              if(warningEnabled) {
                client.print("DISABLE"); // Clearer state
              } else {
                client.print("ENABLE");
              }
              client.print("</a></div>");

              xSemaphoreGive(dataMutex);

              // The HTTP response ends with another blank line:
              client.print(" \
                  </body>\
                </html>");
              client.println();
              break;
                // break out of the while loop:
            // Check to see if the client request was "GET /H" or "GET /L":
            }
            else {    // if you got a newline, then clear currentLine:
              currentLine = "";
            }
          }
          else if (c != '\r') {  // if you got anything else but a carriage return character,
            currentLine += c;      // add it to the end of the currentLine
          }
          if (currentLine.endsWith("GET /warn_toggle")) {
            warningEnabled = !warningEnabled;         // GET /H turns the LED on
          }

        }
      }
      // close the connection:
      client.stop();
      Serial.println("Client Disconnected.");
    }

    vTaskDelay(pdMS_TO_TICKS(20));  // debounce + scheduling

  }
}

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();

  Serial.println("Server started");


  pinMode(WARNING_LIGHT, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  dht.begin();

  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.setTextColor(WHITE);

  dataMutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(SensorTask, "Sensor", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(DisplayTask, "Display", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(AlertTask,   "Alert",   2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(buttonRead, "Read Button State", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(server_website, "Setup wifi server", 2048, NULL, 3, NULL, 0);
}

void loop() {
  // EMPTY — FreeRTOS owns the CPU
}
