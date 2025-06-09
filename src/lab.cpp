#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_MAX31865.h>
#include <Adafruit_INA219.h>
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <cmath>
#include <Update.h>
#include <updatePage.h>
#include <ESPmDNS.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>

#define EEPROM_SIZE 64       // Size in bytes (more than we need)
#define EEPROM_OFFSET_ADDR 0 // Address to store our offset
#define OTA_USER "admin"
#define OTA_PASS "admin!@#$123"

// NITJ-WiFi credentials (replace with your actual credentials)
#define NITJ_USERNAME "your_username"
#define NITJ_PASSWORD "your_password"

// Google Apps Script URL
#define GOOGLE_SCRIPT_URL "https://script.google.com/macros/s/Axxxx"

#define MEDIAN_WINDOW 5 // Odd number (3, 5, or 7 work well)

// LED and Button Pins
#define WifiLED1 12   // WiFi status LED
#define MosfetLED2 14 // MOSFET gate status LED
#define DataLED3 27   // Data transmission LED
#define BUTTON 26     // Push button pin (configured as pull-down)
#define MOSFET 25     // MOSFET gate control pin
#define SAMPLE_COUNT 10 // Average over 10 readings

// MAX31865 Setup (PT200)
#define CS1 5          // Chip Select for Max31865 sensor 1
#define CS2 4          // Chip Select for Max31865 sensor 2
#define RNOMINAL 200.0 // Nominal Resistance (200Ω for PT200)
#define RREF1 430      // Reference Resistor for Max31865 sensor 1
#define RREF2 430      // Reference Resistor for Max31865 sensor 2
// #define RREF1 427      // Reference Resistor for Max31865 sensor 1
// #define RREF2 429      // Reference Resistor for Max31865 sensor 2


// Structure for cloud data
typedef struct
{
    float temp1;
    float temp2;
    float busVoltage;
    float current_mA;
    float power_mW;
} CloudData_t;

// Wi-Fi definitions 
const int NUM_NETWORKS = 3;
const char *ssid[NUM_NETWORKS] = {"NITJ-WiFi", "OPTIMUS", "cryo"};
const char *password[NUM_NETWORKS] = {"", "qqwweeaaaa", "cryo@123"};

float tempBuffer1[MEDIAN_WINDOW] = {0};
float tempBuffer2[MEDIAN_WINDOW] = {0};
int bufferIndex1 = 0;
int bufferIndex2 = 0;


float temperature_offset = 0.00; // To calibrate readings of both pt200
float temp1 = 0.00;
float temp2 = 0.00;
float busVoltage = 0.00;
float current_mA = 0.00;
float power_mW = 0.00;

// Fourier's Law variables
float thermalConductivity = 0.0;
float sampleThickness = 5.0;   // Default value in mm  Δx
float crossSectionArea = 0.00; // Default value in mm²+
float diameter = 10.0;         // Default value in mm

float dT = 0.00;

// RTOS Handles
TaskHandle_t cloudTaskHandle = NULL;
TaskHandle_t wifiLedTaskHandle = NULL;
TaskHandle_t dataLedTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t mainTaskHandle = NULL;
QueueHandle_t cloudDataQueue = NULL;
QueueHandle_t dataLedQueue = NULL;
TimerHandle_t wifiLedTimer = NULL;

// Shared variables (use atomic operations or mutex for thread safety)
volatile bool wifiConnected = false;
volatile bool mosfetState = false;
volatile bool dataLedActive = false;
volatile bool buttonLongPress = false;

// Web Server on port 80
WebServer server(80);

// Create MAX31865 sensor objects
Adafruit_MAX31865 max1 = Adafruit_MAX31865(CS1);
Adafruit_MAX31865 max2 = Adafruit_MAX31865(CS2);

// INA219 Setup
Adafruit_INA219 ina219;

// Function prototypes
void cloudTask(void *pvParameters);
void wifiLedTask(void *pvParameters);
void dataLedTask(void *pvParameters);
void buttonTask(void *pvParameters);
void mainTask(void *pvParameters);
void myFunction();
void measureParameters();
void sendDataToCloud();
void sendDataToGoogleSheets(float t1, float t2, float voltage, float current, float power);
float readTemperature1(Adafruit_MAX31865 &sensor);
float readTemperature2(Adafruit_MAX31865 &sensor);
void handleRoot();
void handleGetData();
void calculateThermalconductivity();
void handleUpload();
void handleUpdate();
void handleUpdatePage();
bool handleNITJWifiCaptivePortal();


// Median Filter Implementation
float getMedian(float samples[], int size)
{
    // Sort the samples
    for (int i = 0; i < size - 1; i++)
    {
        for (int j = i + 1; j < size; j++)
        {
            if (samples[j] < samples[i])
            {
                float temp = samples[i];
                samples[i] = samples[j];
                samples[j] = temp;
            }
        }
    }
    // Return middle element
    return samples[size / 2];
}

void initMedianFilter()
{
    for (int i = 0; i < MEDIAN_WINDOW; i++)
    {
        tempBuffer1[i] = readTemperature1(max1);
        tempBuffer2[i] = readTemperature2(max2);
        vTaskDelay(100 / portTICK_PERIOD_MS); // Allow time between readings
    }
}

void saveOffsetToEEPROM(float offset)
{
    EEPROM.put(EEPROM_OFFSET_ADDR, offset);
    EEPROM.commit();
    Serial.printf("Saved offset to EEPROM: %.3f\n", offset);
}

float readOffsetFromEEPROM()
{
    float offset = 0;
    EEPROM.get(EEPROM_OFFSET_ADDR, offset);
    Serial.printf("Read offset from EEPROM: %.3f\n", offset);
    return offset;
    EEPROM.get(EEPROM_OFFSET_ADDR, offset);

    // Validate
    if (isnan(offset) || offset < -10.0 || offset > 10.0)
    {
        offset = 0.0;
    }

    return offset;
}

void setup()
{
    Serial.begin(115200);

    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);

    // Load saved offset
    temperature_offset = readOffsetFromEEPROM();

    // Initialize PWM for MOSFET control
    ledcSetup(0, 5000, 8);    // Channel 0, 5kHz, 8-bit resolution
    ledcAttachPin(MOSFET, 0); // Attach MOSFET pin to channel 0
    ledcWrite(0, 255);          // Start with 0% duty cycle

    // Initialize LED and button pins
    pinMode(WifiLED1, OUTPUT);
    pinMode(MosfetLED2, OUTPUT);
    pinMode(DataLED3, OUTPUT);
    pinMode(BUTTON, INPUT_PULLDOWN);
    // pinMode(MOSFET, OUTPUT);

    digitalWrite(WifiLED1, HIGH); // Start with WiFi LED on (solid)
    digitalWrite(MosfetLED2, LOW);
    digitalWrite(DataLED3, LOW);
    // digitalWrite(MOSFET, HIGH); // Active low - start with MOSFET off

    // Initialize median filter buffers
    initMedianFilter();

    // Create a queue for data LED control
    dataLedQueue = xQueueCreate(5, sizeof(bool));

    // Create cloud data queue (holds 5 entries)
    cloudDataQueue = xQueueCreate(5, sizeof(CloudData_t));

    // Create cloud task
    xTaskCreatePinnedToCore(
        cloudTask,        // Task function
        "CloudTask",      // Task name
        8192,             // Stack size
        NULL,             // Parameters
        2,                // Priority (lower than button task)
        &cloudTaskHandle, // Task handle
        1                 // Core (same as main task)
    );

    // Create tasks
    xTaskCreatePinnedToCore(
        wifiLedTask,        // Task function
        "WiFiLEDTask",      // Task name
        2048,               // Stack size
        NULL,               // Parameters
        2,                  // Priority
        &wifiLedTaskHandle, // Task handle
        0                   // Core (0 or 1)
    );

    xTaskCreatePinnedToCore(
        dataLedTask,
        "DataLEDTask",
        2048,
        NULL,
        2,
        &dataLedTaskHandle,
        0);

    xTaskCreatePinnedToCore(
        buttonTask,
        "ButtonTask",
        4096,
        NULL,
        3, // Higher priority for responsive button handling
        &buttonTaskHandle,
        0);

    xTaskCreatePinnedToCore(
        mainTask,
        "MainTask",
        12288, // 12KB stack
        NULL,
        1,
        &mainTaskHandle,
        1);
}

void loop()
{
    // Empty - using RTOS tasks instead
    vTaskDelete(NULL); // Delete the loop task
}

void mainTask(void *pvParameters)
{
    // Connect to Wi-Fi
    bool connected = false;
    for (int i = 0; i < NUM_NETWORKS; i++)
    {
        Serial.printf("Connecting to: %s\n", ssid[i]);

        // Handle open networks
        if (strlen(password[i]) == 0)
        {                        // Empty password
            WiFi.begin(ssid[i]); // No password parameter
            Serial.println("NITJ WIFI");
        }
        else
        {
            WiFi.begin(ssid[i], password[i]);
        }

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10)
        {
            Serial.print(".");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            // Special handling for NITJ-WiFi captive portal
            if (strcmp(ssid[i], "NITJ-WiFi") == 0)
            {
                Serial.println("\nAttempting captive portal login...");
                if (!handleNITJWifiCaptivePortal())
                {
                    WiFi.disconnect();
                    continue; // Try next network if portal login fails
                }
            }

            connected = true;
            break;
        }
    }

    if (!connected)
    {
        Serial.println("All networks failed!");
        while (1)
            ; // Halt
    }
    wifiConnected = true;
    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Initialize MAX31865 sensors
    max1.begin(MAX31865_2WIRE);
    max2.begin(MAX31865_2WIRE);

    // Initialize INA219
    if (!ina219.begin())
    {
        Serial.println("Failed to find INA219!");
        while (1)
            vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Define Web Server routes
    server.on("/", handleRoot);
    server.on("/getData", handleGetData);
    server.on("/setPWM", HTTP_GET, []()
              {
        if (server.hasArg("value")) {
            int pwm = server.arg("value").toInt();
            pwm = 255 - constrain(pwm, 0, 255); // Invert for active-low
            ledcWrite(0, pwm);
            mosfetState = (pwm < 255); // ON if not 255
            digitalWrite(MosfetLED2, mosfetState);
            server.send(200, "text/plain", "OK");
        } });
    server.on("/update", HTTP_GET, handleUpdatePage);

    server.on("/update", HTTP_POST, handleUpdate, handleUpload);

    // Enable mDNS
    if (!MDNS.begin("cryo"))
    {
        Serial.println("Error setting up MDNS responder!");
    }
    MDNS.addService("http", "tcp", 80);

    server.begin();

    unsigned long previousMeasureMillis = 0;
    const long measureInterval = 1000; // 1 second interval for measurements
    
    unsigned long previousSendMillis = 0;
    const long sendInterval = 30000; // 30 second interval for send data

    for (;;)
    {

        // Measurement every 1 seconds
        if (millis() - previousMeasureMillis >= measureInterval)
        {
            previousMeasureMillis = millis();
            measureParameters();
            calculateThermalconductivity();
        }


        if (millis() - previousSendMillis >= sendInterval)
        {
            previousSendMillis = millis();
            sendDataToCloud(); 
        }

        server.handleClient(); // Handle client requests

        // Handle long press function call
            if (buttonLongPress) {
              buttonLongPress = false;
              myFunction();
            }

        vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay to prevent watchdog trigger
    }
}

void cloudTask(void *pvParameters)
{
    CloudData_t data;

    for (;;)
    {
        // Wait for new data (blocking)
        if (xQueueReceive(cloudDataQueue, &data, portMAX_DELAY) == pdTRUE)
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                // Serial.printf("[Cloud] Sending: T1=%.2f, T2=%.2f, V=%.2f\n",
                //               data.temp1, data.temp2, data.busVoltage);
                Serial.println("[Cloud] Sending");

                sendDataToGoogleSheets(
                    data.temp1,
                    data.temp2,
                    data.busVoltage,
                    data.current_mA,
                    data.power_mW);
            }
            else
            {
                Serial.println("[Cloud] WiFi disconnected - data not sent");
            }
        }
    }
}

void wifiLedTask(void *pvParameters)
{
    bool ledState = HIGH;

    for (;;)
    {
        if (!wifiConnected)
        {
            digitalWrite(WifiLED1, HIGH); // Solid on while connecting
        }
        else
        {
            // Blink pattern: 100ms on, 4900ms off (5 second total period)
            if (ledState == HIGH)
            {
                digitalWrite(WifiLED1, HIGH);
                vTaskDelay(10 / portTICK_PERIOD_MS);
                digitalWrite(WifiLED1, LOW);
                ledState = LOW;
            }
            else
            {
                vTaskDelay(4900 / portTICK_PERIOD_MS);
                ledState = HIGH;
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay
    }
}

void dataLedTask(void *pvParameters)
{
    bool ledCommand;

    for (;;)
    {
        if (xQueueReceive(dataLedQueue, &ledCommand, portMAX_DELAY) == pdTRUE)
        {
            if (ledCommand)
            {
                digitalWrite(DataLED3, HIGH);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                digitalWrite(DataLED3, LOW);
            }
        }
    }
}

void buttonTask(void *pvParameters)
{
    TickType_t pressStartTime = 0;
    bool buttonPressed = false;
    const TickType_t longPressDuration = 3000 / portTICK_PERIOD_MS; // 3 seconds
    int pwmValue = 0;

    for (;;)
    {
        if (digitalRead(BUTTON) == HIGH)
        {
            if (!buttonPressed)
            {
                // Button just pressed
                buttonPressed = true;
                pressStartTime = xTaskGetTickCount();
            }
            else
            {
                // Button still pressed - check for long press
                if ((xTaskGetTickCount() - pressStartTime) >= longPressDuration)
                {
                    // Long press detected
                    buttonLongPress = true;
                    buttonPressed = false; // Reset to wait for release
                }
            }
        }
        else
        {
            if (buttonPressed)
            {
                // Button released - check if it was a short press
                if ((xTaskGetTickCount() - pressStartTime) < longPressDuration)
                {
                    // // Short press - toggle MOSFET
                    // if (mosfetState)
                    // {
                    //     pwmValue = 255;
                    // }
                    // else
                    // {
                    //     pwmValue = 0; // Default to full power when turning on
                    // }
                    // ledcWrite(0, pwmValue);
                    ledcWrite(0, mosfetState ? 255 : 0); // ON=0, OFF=255
                    mosfetState = !mosfetState;

                    digitalWrite(MosfetLED2, mosfetState);
                    Serial.println("MOSFET toggled: " + String(mosfetState ? "ON" : "OFF"));
                }
                buttonPressed = false;
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS); // Debounce delay
    }
}

// void buttonTask(void *pvParameters)
// {
//     TickType_t pressStartTime = 0;
//     bool buttonPressed = false;
//     const TickType_t longPressDuration = 3000 / portTICK_PERIOD_MS; // 3 seconds

//     for (;;)
//     {
//         if (digitalRead(BUTTON) == HIGH)
//         {
//             if (!buttonPressed)
//             {
//                 // Button just pressed
//                 buttonPressed = true;
//                 pressStartTime = xTaskGetTickCount();
//             }
//             else
//             {
//                 // Button still pressed - check for long press
//                 if ((xTaskGetTickCount() - pressStartTime) >= longPressDuration)
//                 {
//                     // Long press detected
//                     buttonLongPress = true;
//                     buttonPressed = false; // Reset to wait for release
//                 }
//             }
//         }
//         else
//         {
//             if (buttonPressed)
//             {
//                 // Button released - check if it was a short press
//                 if ((xTaskGetTickCount() - pressStartTime) < longPressDuration)
//                 {
//                     // Short press - toggle MOSFET
//                     digitalWrite(MOSFET, mosfetState);
//                     mosfetState = !mosfetState;
//                     digitalWrite(MosfetLED2, mosfetState);
//                     Serial.println("MOSFET toggled: " + String(mosfetState ? "ON" : "OFF"));
//                 }
//                 buttonPressed = false;
//             }
//         }

//         vTaskDelay(50 / portTICK_PERIOD_MS); // Debounce delay
//     }
// }

void myFunction()
{
    Serial.println("Long press detected - executing myFunction()");

    // Visual feedback
    digitalWrite(WifiLED1, HIGH);
    digitalWrite(MosfetLED2, HIGH);
    digitalWrite(DataLED3, HIGH);
    delay(200);
    digitalWrite(WifiLED1, LOW);
    digitalWrite(MosfetLED2, LOW);
    digitalWrite(DataLED3, LOW);

    // Only attempt logout if connected to NITJ-WiFi
    if (WiFi.SSID() == "NITJ-WiFi")
    {
        Serial.println("Attempting to logout from NITJ-WiFi captive portal...");

        WiFiClient client;
        HTTPClient http;

        // Configure logout URL - try both endpoints
        String logoutURL = "http://10.10.11.1:8090/logout.xml";
        // Alternative: "http://10.10.11.1:8090/httpclient.html"

        http.begin(client, logoutURL);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        // Prepare logout payload
        String postData = "mode=193&username=" + String(NITJ_USERNAME);

        int httpResponseCode = http.POST(postData);
        String response = http.getString();

        Serial.print("Logout response code: ");
        Serial.println(httpResponseCode);
        Serial.print("Response: ");
        Serial.println(response);

        http.end();

        if (response.indexOf("success") != -1 || response.indexOf("logout") != -1)
        {
            Serial.println("Successfully logged out from captive portal");
            // Optional: Disconnect WiFi after logout
            WiFi.disconnect();
        }
        else
        {
            Serial.println("Logout may have failed - check response");
        }
    }
    else
    {
        Serial.println("Not connected to NITJ-WiFi - logout not required");
    }
}

float readStableCurrent()
{
    float sum = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        sum += ina219.getCurrent_mA();
        delay(10); // Small delay between readings
    }
    return sum / SAMPLE_COUNT;
}

void measureParameters()
{
    // Read temperature
    temp1 = readTemperature1(max1);
    temp2 = readTemperature2(max2);
    // Serial.println(temp1);
    // Serial.println(temp2);

    // Read Bus Voltage, Current, and Power
    busVoltage = ina219.getBusVoltage_V();
    current_mA = readStableCurrent();
    // power_mW = ina219.getPower_mW();
    power_mW = busVoltage * current_mA;
}

void sendDataToCloud()
{
    CloudData_t cloudData = {
        .temp1 = temp1,
        .temp2 = temp2,
        .busVoltage = busVoltage,
        .current_mA = current_mA,
        .power_mW = power_mW};

    // Send to queue (non-blocking)
    if (xQueueSend(cloudDataQueue, &cloudData, 0) != pdTRUE)
    {
        Serial.println("[Cloud] Queue full - data dropped");
    }
}

void sendDataToGoogleSheets(float t1, float t2, float voltage, float current, float power)
{
    HTTPClient http;
    http.begin(GOOGLE_SCRIPT_URL);
    http.addHeader("Content-Type", "application/json");

    String ipAddress = WiFi.localIP().toString();

    String postData = "{\"temp1\":" + String(t1) +
                      ",\"temp2\":" + String(t2) +
                      ",\"voltage\":" + String(voltage) +
                      ",\"current\":" + String(current) +
                      ",\"power\":" + String(power) +
                      ",\"thickness\":" + String(sampleThickness) +
                      ",\"area\":" + String(crossSectionArea) +
                      ",\"conductivity\":" + String(thermalConductivity) +
                      ",\"ip\":\"" + ipAddress + "\"" + 
                      "}";

    int httpResponseCode = http.POST(postData);
    Serial.println("Response Code: " + String(httpResponseCode));

    if (httpResponseCode == 302)
    {
        bool ledCommand = true;
        xQueueSend(dataLedQueue, &ledCommand, portMAX_DELAY);
    }
    else{
        Serial.println("Data not sent");
    }

    http.end();
}

void handleRoot()
{
    if (server.hasArg("thickness"))
    {
        sampleThickness = server.arg("thickness").toFloat();
    }
    if (server.hasArg("sampleDiameter"))
    {
        diameter = server.arg("sampleDiameter").toFloat();
    }
    if (server.hasArg("temperatureoffset"))
    {
        temperature_offset += server.arg("temperatureoffset").toFloat();
        saveOffsetToEEPROM(temperature_offset);
    }
    if (server.hasArg("pwmValue"))
    {
        int pwm = server.arg("pwmValue").toInt();
        pwm = 255 - constrain(pwm, 0, 255); // Invert for active-low
        ledcWrite(0, pwm);
        mosfetState = (pwm < 255); // ON if not 255
        digitalWrite(MosfetLED2, mosfetState);
    }

    String html = R"=====(
     <!-- Designed and Developed by Rahul Morya https://in.linkedin.com/in/rahul-morya-456a3b233 -->

<!DOCTYPE html>
<html lang='en'>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>ESP32 Thermal Dashboard</title>
  <style>
    body { background-color: #121212; color: #ffffff; font-family: Arial, sans-serif; margin: 0; padding: 20px; }
    .container { max-width: 1000px; margin: 0 auto; text-align: center; }
    h1 { margin-bottom: 20px; color: #4CAF50; }
    h2 { color: #4CAF50; margin-top: 30px; }
    .form-container { background-color: #1f1f1f; border-radius: 15px; padding: 20px; margin: 20px auto; width: 80%; box-shadow: 0 4px 10px rgba(0, 0, 0, 0.5); }
    form { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; }
    .form-group { flex: 1; min-width: 200px; margin: 10px; }
    label { display: block; margin-bottom: 8px; color: #cccccc; }
    input { width: 50%; padding: 12px; border-radius: 8px; border: none; background-color: #2b2b2b; color: white; }
    button { background-color: #4CAF50; color: white; border: none; padding: 12px 24px; border-radius: 8px; cursor: pointer; font-weight: bold; margin-top: 15px; transition: background-color 0.3s; }
    button:hover { background-color: #45a049; }
    .dashboard { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; margin-top: 30px; }
    .card { background-color: #1f1f1f; border-radius: 15px; padding: 20px; width: 200px; box-shadow: 0 4px 10px rgba(0, 0, 0, 0.5); }
    .value { font-size: 25px; font-weight: bold; border-radius: 50%; background-color: #2b2b2b; width: 100px; height: 100px; display: flex; justify-content: center; align-items: center; margin: 0 auto; color: #4CAF50; }
    .label { margin-top: 15px; font-size: 16px; color: #cccccc; }
    .info { font-size: 14px; color: #999999; margin-top: 10px; }
    .conductivity-card { background-color: #1f1f1f; border-radius: 15px; padding: 20px; margin: 20px auto; width: 80%; box-shadow: 0 4px 10px rgba(0, 0, 0, 0.5); }
    .conductivity-value { font-size: 40px; font-weight: bold; color: #4CAF50; margin: 15px 0; }
    .formula { font-family: monospace; background-color: #2b2b2b; padding: 10px; border-radius: 5px; margin: 15px 0; }
    .slider-container { margin: 20px 0; }
    .slider { width: 80%; height: 25px; background: #2b2b2b; outline: none; opacity: 0.7; transition: opacity .2s; }
    .slider:hover { opacity: 1; }
    .slider-value { margin-top: 10px; font-size: 18px; }
  </style>
</head>
<body>
  <div class='container'>
    <h1>Thermal Conductivity at Cryogenic Temperatures Measurement Dashboard</h1>

    <!-- Input Form -->
    <div class='form-container'>
      <h2>Sample Parameters</h2>
      <form method='post'>
        <div class='form-group'>
          <label for='thickness'>Sample Thickness Δx (mm)</label>
          <input type='number' step='0.1' name='thickness' value='%THICKNESS%' required>
        </div>
        <div class='form-group'>
          <label for='area'>Sample Diameter (mm)</label>
          <input type='number' step='0.1' name='sampleDiameter' value='%DIAMETER%' required>
        </div>
        <div class='form-group'>
          <label for='temperatureoffset'>Temperature offset</label>
          <input type='number' step='0.001' name='temperatureoffset' value='%TEMP_OFFSET%' required>
        </div>
        <div class='form-container'>
      <h2>Heater Control</h2>
      <div class='slider-container'>
        <label for='pwmSlider'>Heater Power Level: <span id='pwmValue'>%PWM_VALUE%</span>%</label>
        <input type='range' min='0' max='255' value='%PWM_VALUE%' class='slider' id='pwmSlider' name='pwmSlider'>
      </div>
      <div class='info'>Current MOSFET State: <span id='mosfetState'>%MOSFET_STATE%</span></div>
    </div>
        <button type='submit'>Update Parameters</button>
      </form>
    </div>

    <!-- Measurement Dashboard -->
    <div class='dashboard'>
      <div class='card'>
        <div class='value' id='temp1'>%TEMP1%</div>
        <div class='label'>Kelvin</div>
        <div class='info'>PT200 Temperature T1</div>
      </div>
      <div class='card'>
        <div class='value' id='temp2'>%TEMP2%</div>
        <div class='label'>Kelvin</div>
        <div class='info'>PT200 Temperature T2</div>
      </div>
      <div class='card'>
        <div class='value' id='dT'>%DT%</div>
        <div class='label'>Kelvin</div>
        <div class='info'>ΔT Difference</div>
      </div>
      <div class='card'>
        <div class='value' id='power_mW'>%POWER%</div>
        <div class='label'>Heater Power(mW)</div>
        <div class='info' id='busVoltage'>Bus Voltage: %BUS_VOLTAGE% V</div>
        <div class='info' id='current_mA'>Current: %CURRENT% mA</div>
      </div>
    </div>

    <!-- Thermal Conductivity Card -->
    <div class='conductivity-card'>
      <h2>Thermal Conductivity</h2>
      <div class='conductivity-value' id='thermalConductivity'>%THERMAL_CONDUCTIVITY% W/m·K</div>
      <div class='formula'>k = (Q × dx) / (A × ΔT)</div>
      <div class='info'>Where: Q = Power (W), dx = Thickness (m)<br>
      A = Area (m²), ΔT = Temp Difference (K)</div>
    </div>
  </div>

  <!-- AJAX Script for Auto-Updating Values -->
  <script>
    function updateData() {
      fetch('/getData')
        .then(response => response.json())
        .then(data => {
          document.getElementById('temp1').innerHTML = data.temp1;
          document.getElementById('temp2').innerHTML = data.temp2;
          document.getElementById('dT').innerHTML = data.dT;
          document.getElementById('power_mW').innerHTML = data.power_mW;
          document.getElementById('busVoltage').innerHTML = "Bus Voltage: " + data.busVoltage + " V";
          document.getElementById('current_mA').innerHTML = "Current: " + data.current_mA + " mA";
          document.getElementById('thermalConductivity').innerHTML = data.thermalConductivity + " W/m·K";
            document.getElementById('mosfetState').textContent = data.mosfetState ? "ON" : "OFF";
        })
        .catch(error => console.error('Error fetching data:', error));
    }

    // Update PWM value when slider changes
    document.getElementById('pwmSlider').addEventListener('input', function() {
      var pwmValue = this.value;
    //   document.getElementById('pwmValue').textContent = pwmValue;
    let percentage = Math.round((pwmValue / 255) * 100);
document.getElementById('pwmValue').textContent = percentage ;
      
      // Send PWM value to server
      fetch('/', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: 'pwmValue=' + pwmValue
      });
    });

    // Update every 2 seconds
    setInterval(updateData, 2000);
    updateData(); // Initial load
  </script>
</body>
</html>
 <!-- Designed and Developed by Rahul Morya https://in.linkedin.com/in/rahul-morya-456a3b233 -->

)=====";

    // Replace placeholders with actual values
    html.replace("%THICKNESS%", String(sampleThickness));
    html.replace("%DIAMETER%", String(diameter));
    html.replace("%TEMP_OFFSET%", String(temperature_offset));
    html.replace("%TEMP1%", String(temp1));
    html.replace("%TEMP2%", String(temp2));
    html.replace("%DT%", String(dT));
    html.replace("%POWER%", String(power_mW));
    html.replace("%BUS_VOLTAGE%", String(busVoltage));
    html.replace("%CURRENT%", String(current_mA));
    html.replace("%THERMAL_CONDUCTIVITY%", String(thermalConductivity));
    int pwm_value = ledcRead(0);
    pwm_value = constrain(pwm_value, 0, 255); // Force valid range
    int percentage = (255 - pwm_value) * 100 / 255;
    html.replace("%PWM_VALUE%", String(percentage));
    // html.replace("%PWM_VALUE%", String(ledcRead(0) * 100 / 255)); // Convert to percentage
    html.replace("%MOSFET_STATE%", mosfetState ? "ON" : "OFF");

    server.send(200, "text/html", html);
}

void handleGetData()
{
    String json = "{";
    json += "\"temp1\":" + String(temp1) + ",";
    json += "\"temp2\":" + String(temp2) + ",";
    json += "\"dT\":" + String(dT) + ",";
    json += "\"power_mW\":" + String(power_mW) + ",";
    json += "\"busVoltage\":" + String(busVoltage) + ",";
    json += "\"current_mA\":" + String(current_mA) + ",";
    json += "\"thermalConductivity\":" + String(thermalConductivity) + ",";
    json += "\"mosfetState\":" + String(mosfetState);
    json += "}";

    server.send(200, "application/json", json);
}

// // Function to read temperature from MAX31865
// float readTemperature1(Adafruit_MAX31865 &sensor)
// {
//     // float rtd = sensor.readRTD();
//     // Serial.print(rtd);
//     // Serial.print("      ");
//     // Serial.print(RREF1);
//     float temperature = sensor.temperature(RNOMINAL, RREF1);
//     temperature += 273.15;
//     // Serial.print("   Temperature T1: ");
//     // Serial.println(temperature);
//     return temperature;
// }

// float readTemperature2(Adafruit_MAX31865 &sensor)
// {
//     // float rtd = sensor.readRTD();
//     // Serial.print(rtd);
//     float temperature = sensor.temperature(RNOMINAL, RREF2);
//     temperature += 273.15 + temperature_offset;
//     // temperature += 273.15 ;
//     // Serial.print("   Temperature T2: ");
//     // Serial.println(temperature);

//     return temperature;
// }

float readTemperature1(Adafruit_MAX31865 &sensor)
{
    float rawTemp = sensor.temperature(RNOMINAL, RREF1) + 273.15;

    // Store in circular buffer
    tempBuffer1[bufferIndex1] = rawTemp;
    bufferIndex1 = (bufferIndex1 + 1) % MEDIAN_WINDOW;

    // Return median value
    float tempCopy[MEDIAN_WINDOW];
    memcpy(tempCopy, tempBuffer1, sizeof(tempCopy));
    return getMedian(tempCopy, MEDIAN_WINDOW);
}

float readTemperature2(Adafruit_MAX31865 &sensor)
{
    float rawTemp = sensor.temperature(RNOMINAL, RREF2) + 273.15 + temperature_offset;

    // Store in circular buffer
    tempBuffer2[bufferIndex2] = rawTemp;
    bufferIndex2 = (bufferIndex2 + 1) % MEDIAN_WINDOW;

    // Return median value
    float tempCopy[MEDIAN_WINDOW];
    memcpy(tempCopy, tempBuffer2, sizeof(tempCopy));
    return getMedian(tempCopy, MEDIAN_WINDOW);
}

void calculateThermalconductivity()
{
    // Calculate thermal conductivity
    dT = temp1 - temp2;
    float absolute_dT = fabs(dT);
    crossSectionArea = M_PI * (diameter / 2.00) * (diameter / 2.00);
    if (absolute_dT > 0 && sampleThickness > 0 && crossSectionArea > 0)
    {
        thermalConductivity = (power_mW * sampleThickness) / (crossSectionArea * absolute_dT * 1000);
    }
    else
    {
        thermalConductivity = 0.0;
    }
}

void handleUpdate()
{
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
}

void handleUpload()
{
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        { // Start with max available size
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        // Flashing firmware to ESP
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true))
        { // true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        }
        else
        {
            Update.printError(Serial);
        }
    }
}

void handleUpdatePage()
{
    if (!server.authenticate(OTA_USER, OTA_PASS))
    {
        return server.requestAuthentication();
    }

    server.send(200, "text/html", updateHTML);
}

// Helper function to check internet access via HTTP
bool checkInternetAccess()
{
    WiFiClient client;
    HTTPClient http;

    http.begin(client, "http://www.google.com");
    int httpCode = http.GET();
    http.end();

    return (httpCode == HTTP_CODE_OK);
}



bool handleNITJWifiCaptivePortal()
{
    HTTPClient http;

    // First make a GET request to establish session (like the Python script)
    String portalUrl = "http://10.10.11.1:8090/httpclient.html";

    http.begin(portalUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("User-Agent", "Mozilla/5.0");
    
    String postData = "username=" + String(NITJ_USERNAME) +
                      "&password=" + String(NITJ_PASSWORD) +
                      "&mode=191";

    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        String response = http.getString();
        Serial.println("Response:");
        Serial.println(response);

        if (response.indexOf("success") != -1)
        {
            Serial.println("Login successful!");
        }
        else
        {
            Serial.println("Login may have failed - check response");
        }
    }
    else
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }

    http.end();
    // // Now make the login POST request
    // String loginUrl = "http://10.10.11.1:8090/httpclient.html";
    // http.begin(client, loginUrl);
    // http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // // Form data with proper encoding

    // Serial.println("Attempting portal login with POST data:");
    // Serial.println(postData);

    // int httpResponseCode = http.POST(postData);
    // String response = http.getString();

    // Serial.print("Login response code: ");
    // Serial.println(httpResponseCode);
    // Serial.print("Response: ");
    // Serial.println(response);

    // http.end();

    // // Check for success indicators
    // bool success = (httpResponseCode > 0) &&
    //                (response.indexOf("success") != -1 ||
    //                 response.indexOf("authenticated") != -1);

    // if (success)
    // {
    //     Serial.println("Captive portal login successful!");
    //     // Verify internet connectivity
    //     if ( checkInternetAccess())
    //     {
    //         return true;
    //     }
    //     Serial.println("Internet access verification failed");
    //     return false;
    // }

    // Serial.println("Captive portal login failed");
    // return false;
    return true;
}

