// #include <WiFi.h>
// #include <HTTPClient.h>
// #include <Adafruit_MAX31865.h>
// #include <Adafruit_INA219.h>
// #include <WebServer.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <freertos/queue.h>
// #include <freertos/timers.h>

// // WiFi Credentials
// const char *ssid = "OPTIMUS";
// const char *password = "qqwweeaaaa";

// // Google Apps Script URL
// // #define GOOGLE_SCRIPT_URL "https://script.google.com/macros/s/AKfycbxril7oCvF6GLlrPi_Fqav3YSGsdOCXRMAZWqekEe9m3gWlV1FuI1ViCmHFuQzCiiAbhQ/exec"
// #define GOOGLE_SCRIPT_URL "https://script.google.com/macros/s/AKfycbwwrsw9v3StLrnlF-ybqiIHzmC4QfPpTR-4sC5Go55EabMVKxSj0ARoNnL70llefInTQw/exec"

// // LED and Button Pins
// #define WifiLED1 12   // WiFi status LED
// #define MosfetLED2 14 // MOSFET gate status LED
// #define DataLED3 27   // Data transmission LED
// #define BUTTON 26     // Push button pin (configured as pull-down)
// #define MOSFET 25     // MOSFET gate control pin

// // MAX31865 Setup (PT200)
// #define CS1 5          // Chip Select for Max31865 sensor 1
// #define CS2 4          // Chip Select for Max31865 sensor 2
// #define RREF1 427      // Reference Resistor for Max31865 sensor 1
// #define RREF2 429      // Reference Resistor for Max31865 sensor 2
// #define RNOMINAL 200.0 // Nominal Resistance (200Ω for PT200)

// float temperature_offset = 0.00; // To calibrate readings of both pt200
// float temp1 = 0.00;
// float temp2 = 0.00;
// float busVoltage = 0.00;
// float current_mA = 0.00;
// float power_mW = 0.00;

// // Fourier's Law variables
// float thermalConductivity = 0.0;
// float sampleThickness = 5.0;   // Default value in mm  Δx
// float crossSectionArea = 0.00; // Default value in mm²+
// float diameter = 10.0;         // Default value in mm

// float dT = 0.00;

// // RTOS Handles
// TaskHandle_t wifiLedTaskHandle = NULL;
// TaskHandle_t dataLedTaskHandle = NULL;
// TaskHandle_t buttonTaskHandle = NULL;
// TaskHandle_t mainTaskHandle = NULL;
// QueueHandle_t dataLedQueue = NULL;
// TimerHandle_t wifiLedTimer = NULL;

// // Shared variables (use atomic operations or mutex for thread safety)
// volatile bool wifiConnected = false;
// volatile bool mosfetState = false;
// volatile bool dataLedActive = false;
// volatile bool buttonLongPress = false;

// // Web Server on port 80
// WebServer server(80);

// // Create MAX31865 sensor objects
// Adafruit_MAX31865 max1 = Adafruit_MAX31865(CS1);
// Adafruit_MAX31865 max2 = Adafruit_MAX31865(CS2);

// // INA219 Setup
// Adafruit_INA219 ina219;

// // Function prototypes
// void wifiLedTask(void *pvParameters);
// void dataLedTask(void *pvParameters);
// void buttonTask(void *pvParameters);
// void mainTask(void *pvParameters);
// void myFunction();
// void measureParameters();
// void sendDataToCloud();
// void sendDataToGoogleSheets(float t1, float t2, float voltage, float current, float power);
// float readTemperature1(Adafruit_MAX31865 &sensor);
// float readTemperature2(Adafruit_MAX31865 &sensor);
// void handleRoot();
// void handleGetData();
// void calculateThermalconductivity();
// void handleUpload();
// void handleUpdate();

// void setup()
// {
//     Serial.begin(115200);

//     // Initialize LED and button pins
//     pinMode(WifiLED1, OUTPUT);
//     pinMode(MosfetLED2, OUTPUT);
//     pinMode(DataLED3, OUTPUT);
//     pinMode(BUTTON, INPUT_PULLDOWN);
//     pinMode(MOSFET, OUTPUT);

//     digitalWrite(WifiLED1, HIGH); // Start with WiFi LED on (solid)
//     digitalWrite(MosfetLED2, LOW);
//     digitalWrite(DataLED3, LOW);
//     digitalWrite(MOSFET, HIGH); // Active low - start with MOSFET off

//     // Create a queue for data LED control
//     dataLedQueue = xQueueCreate(5, sizeof(bool));

//     // Create tasks
//     xTaskCreatePinnedToCore(
//         wifiLedTask,        // Task function
//         "WiFiLEDTask",      // Task name
//         2048,               // Stack size
//         NULL,               // Parameters
//         2,                  // Priority
//         &wifiLedTaskHandle, // Task handle
//         0                   // Core (0 or 1)
//     );

//     xTaskCreatePinnedToCore(
//         dataLedTask,
//         "DataLEDTask",
//         2048,
//         NULL,
//         2,
//         &dataLedTaskHandle,
//         0);

//     xTaskCreatePinnedToCore(
//         buttonTask,
//         "ButtonTask",
//         2048,
//         NULL,
//         3, // Higher priority for responsive button handling
//         &buttonTaskHandle,
//         0);

//     xTaskCreatePinnedToCore(
//         mainTask,
//         "MainTask",
//         4096,
//         NULL,
//         1,
//         &mainTaskHandle,
//         1 // Use core 1 for main task
//     );

//     // Connect to Wi-Fi (moved to main task)
// }

// void loop()
// {
//     // Empty - using RTOS tasks instead
//     vTaskDelete(NULL); // Delete the loop task
// }

// void mainTask(void *pvParameters)
// {
//     // Connect to Wi-Fi
//     WiFi.begin(ssid, password);
//     Serial.print("Connecting to Wi-Fi...");

//     while (WiFi.status() != WL_CONNECTED)
//     {
//         Serial.print(".");
//         vTaskDelay(1000 / portTICK_PERIOD_MS);
//     }

//     wifiConnected = true;
//     Serial.println("\nConnected to Wi-Fi!");
//     Serial.print("ESP32 IP Address: ");
//     Serial.println(WiFi.localIP());

//     // Initialize MAX31865 sensors
//     max1.begin(MAX31865_2WIRE);
//     max2.begin(MAX31865_2WIRE);

//     // Initialize INA219
//     if (!ina219.begin())
//     {
//         Serial.println("Failed to find INA219!");
//         while (1)
//             vTaskDelay(1000 / portTICK_PERIOD_MS);
//     }

//     // Define Web Server routes
//     server.on("/", handleRoot);
//     server.on("/getData", handleGetData);
//     server.begin();

//     unsigned long previousMeasureMillis = 0;
//     const long measureInterval = 2000; // 2 second interval for measurements

//     unsigned long previousSendMillis = 0;
//     const long sendInterval = 5000; // 5 second interval to send data

//     for (;;)
//     {
//         if (Serial.available() > 0)
//         {
//             String input = Serial.readStringUntil('\n');
//             input.trim();
//             float incomingValue = input.toFloat();
//             Serial.print("Received Offset : ");
//             Serial.println(incomingValue, 6);
//             if (incomingValue != RREF1)
//             {
//                 temperature_offset = incomingValue;
//                 Serial.print("Offset updated to: ");
//                 Serial.println(temperature_offset, 6);
//             }
//         }

//         // Check if 2 seconds have passed for measurements
//         unsigned long currentMillis = millis();
//         if (currentMillis - previousMeasureMillis >= measureInterval)
//         {
//             previousMeasureMillis = currentMillis;
//             measureParameters();
//             calculateThermalconductivity();
//         }

//         server.handleClient(); // Handle client requests

//         // Check if 5 seconds have passed for data sending
//         if (currentMillis - previousSendMillis >= sendInterval)
//         {
//             previousSendMillis = currentMillis;
//             sendDataToCloud();
//         }

//         // Handle long press function call
//         if (buttonLongPress)
//         {
//             buttonLongPress = false;
//             myFunction();
//         }

//         vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay to prevent watchdog trigger
//     }
// }
// void wifiLedTask(void *pvParameters)
// {
//     bool ledState = HIGH;

//     for (;;)
//     {
//         if (!wifiConnected)
//         {
//             digitalWrite(WifiLED1, HIGH); // Solid on while connecting
//         }
//         else
//         {
//             // Blink pattern: 100ms on, 4900ms off (5 second total period)
//             if (ledState == HIGH)
//             {
//                 digitalWrite(WifiLED1, HIGH);
//                 vTaskDelay(100 / portTICK_PERIOD_MS);
//                 digitalWrite(WifiLED1, LOW);
//                 ledState = LOW;
//             }
//             else
//             {
//                 vTaskDelay(4900 / portTICK_PERIOD_MS);
//                 ledState = HIGH;
//             }
//         }

//         vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay
//     }
// }

// void dataLedTask(void *pvParameters)
// {
//     bool ledCommand;

//     for (;;)
//     {
//         if (xQueueReceive(dataLedQueue, &ledCommand, portMAX_DELAY) == pdTRUE)
//         {
//             if (ledCommand)
//             {
//                 digitalWrite(DataLED3, HIGH);
//                 vTaskDelay(100 / portTICK_PERIOD_MS);
//                 digitalWrite(DataLED3, LOW);
//             }
//         }
//     }
// }

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
//                     mosfetState = !mosfetState;
//                     digitalWrite(MOSFET, mosfetState);
//                     digitalWrite(MosfetLED2, mosfetState);
//                     Serial.println("MOSFET toggled: " + String(mosfetState ? "ON" : "OFF"));
//                 }
//                 buttonPressed = false;
//             }
//         }

//         vTaskDelay(50 / portTICK_PERIOD_MS); // Debounce delay
//     }
// }

// void myFunction()
// {
//     Serial.println("Long press detected - executing myFunction()");
//     // Add your custom function code here
// }

// void measureParameters()
// {
//     // Read temperature
//     temp1 = readTemperature1(max1);
//     temp2 = readTemperature2(max2);
//     // Read Bus Voltage, Current, and Power
//     busVoltage = ina219.getBusVoltage_V();
//     current_mA = ina219.getCurrent_mA();
//     power_mW = ina219.getPower_mW();
// }

// void sendDataToCloud()
// {
//     if (WiFi.status() == WL_CONNECTED)
//     {
//         Serial.printf("Temp1: %.2f°C, Temp2: %.2f°C, Voltage: %.2fV, Current: %.2fmA, Power: %.2fmW\n",
//                       temp1, temp2, busVoltage, current_mA, power_mW);
//         sendDataToGoogleSheets(temp1, temp2, busVoltage, current_mA, power_mW);
//     }
//     else
//     {
//         Serial.println("WiFi Disconnected! and Data not sent to Cloud");
//     }
// }

// void sendDataToGoogleSheets(float t1, float t2, float voltage, float current, float power)
// {
//     HTTPClient http;
//     http.begin(GOOGLE_SCRIPT_URL);
//     http.addHeader("Content-Type", "application/json");

//     String postData = "{\"temp1\":" + String(t1) + ",\"temp2\":" + String(t2) + ",\"voltage\":" + String(voltage) + ",\"current\":" + String(current) + ",\"power\":" + String(power) + ",\"thickness\":" + String(sampleThickness) + ",\"area\":" + String(crossSectionArea) + ",\"conductivity\":" + String(thermalConductivity) + "}";

//     int httpResponseCode = http.POST(postData);
//     Serial.println("Response Code: " + String(httpResponseCode));

//     if (httpResponseCode == 302)
//     {
//         bool ledCommand = true;
//         xQueueSend(dataLedQueue, &ledCommand, portMAX_DELAY);
//     }

//     http.end();
// }

// void handleRoot()
// {
//     if (server.hasArg("thickness") && server.hasArg("sampleDiameter") && server.hasArg("temperatureoffset"))
//     {
//         sampleThickness = server.arg("thickness").toFloat();
//         diameter = server.arg("sampleDiameter").toFloat();
//         temperature_offset = server.arg("temperatureoffset").toFloat();
//     }
//     String html = R"=====(
// <!DOCTYPE html>
// <html lang='en'>
// <head>
//   <meta charset='UTF-8'>
//   <meta name='viewport' content='width=device-width, initial-scale=1.0'>
//   <title>ESP32 Thermal Dashboard</title>
//   <style>
//     body { background-color: #121212; color: #ffffff; font-family: Arial, sans-serif; margin: 0; padding: 20px; }
//     .container { max-width: 1000px; margin: 0 auto; text-align: center; }
//     h1 { margin-bottom: 20px; color: #4CAF50; }
//     h2 { color: #4CAF50; margin-top: 30px; }
//     .form-container { background-color: #1f1f1f; border-radius: 15px; padding: 20px; margin: 20px auto; width: 80%; box-shadow: 0 4px 10px rgba(0, 0, 0, 0.5); }
//     form { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; }
//     .form-group { flex: 1; min-width: 200px; margin: 10px; }
//     label { display: block; margin-bottom: 8px; color: #cccccc; }
//     input { width: 50%; padding: 12px; border-radius: 8px; border: none; background-color: #2b2b2b; color: white; }
//     button { background-color: #4CAF50; color: white; border: none; padding: 12px 24px; border-radius: 8px; cursor: pointer; font-weight: bold; margin-top: 15px; transition: background-color 0.3s; }
//     button:hover { background-color: #45a049; }
//     .dashboard { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; margin-top: 30px; }
//     .card { background-color: #1f1f1f; border-radius: 15px; padding: 20px; width: 200px; box-shadow: 0 4px 10px rgba(0, 0, 0, 0.5); }
//     .value { font-size: 25px; font-weight: bold; border-radius: 50%; background-color: #2b2b2b; width: 100px; height: 100px; display: flex; justify-content: center; align-items: center; margin: 0 auto; color: #4CAF50; }
//     .label { margin-top: 15px; font-size: 16px; color: #cccccc; }
//     .info { font-size: 14px; color: #999999; margin-top: 10px; }
//     .conductivity-card { background-color: #1f1f1f; border-radius: 15px; padding: 20px; margin: 20px auto; width: 80%; box-shadow: 0 4px 10px rgba(0, 0, 0, 0.5); }
//     .conductivity-value { font-size: 40px; font-weight: bold; color: #4CAF50; margin: 15px 0; }
//     .formula { font-family: monospace; background-color: #2b2b2b; padding: 10px; border-radius: 5px; margin: 15px 0; }
//   </style>
// </head>
// <body>
//   <div class='container'>
//     <h1>Thermal Conductivity at Cryogenic Temperatures Measurement Dashboard</h1>

//     <!-- Input Form -->
//     <div class='form-container'>
//       <h2>Sample Parameters</h2>
//       <form method='post'>
//         <div class='form-group'>
//           <label for='thickness'>Sample Thickness Δx (mm)</label>
//           <input type='number' step='0.1' name='thickness' value='%THICKNESS%' required>
//         </div>
//         <div class='form-group'>
//           <label for='area'>Sample Diameter (mm)</label>
//           <input type='number' step='0.1' name='sampleDiameter' value='%DIAMETER%' required>
//         </div>
//         <div class='form-group'>
//           <label for='temperatureoffset'>Temperature offset</label>
//           <input type='number' step='0.001' name='temperatureoffset' value='%TEMP_OFFSET%' required>
//         </div>
//         <button type='submit'>Update Parameters</button>
//       </form>
//     </div>

//     <!-- Measurement Dashboard -->
//     <div class='dashboard'>
//       <div class='card'>
//         <div class='value' id='temp1'>%TEMP1%</div>
//         <div class='label'>Kelvin</div>
//         <div class='info'>PT200 Temperature T1</div>
//       </div>
//       <div class='card'>
//         <div class='value' id='temp2'>%TEMP2%</div>
//         <div class='label'>Kelvin</div>
//         <div class='info'>PT200 Temperature T2</div>
//       </div>
//       <div class='card'>
//         <div class='value' id='dT'>%DT%</div>
//         <div class='label'>Kelvin</div>
//         <div class='info'>ΔT Difference</div>
//       </div>
//       <div class='card'>
//         <div class='value' id='power_mW'>%POWER%</div>
//         <div class='label'>Heater Power(mW)</div>
//         <div class='info' id='busVoltage'>Bus Voltage: %BUS_VOLTAGE% V</div>
//         <div class='info' id='current_mA'>Current: %CURRENT% mA</div>
//       </div>
//     </div>

//     <!-- Thermal Conductivity Card -->
//     <div class='conductivity-card'>
//       <h2>Thermal Conductivity</h2>
//       <div class='conductivity-value' id='thermalConductivity'>%THERMAL_CONDUCTIVITY% W/m·K</div>
//       <div class='formula'>k = (Q × dx) / (A × ΔT)</div>
//       <div class='info'>Where: Q = Power (W), dx = Thickness (m)<br>
//       A = Area (m²), ΔT = Temp Difference (K)</div>
//     </div>
//   </div>

//   <!-- AJAX Script for Auto-Updating Values -->
//   <script>
//     function updateData() {
//       fetch('/getData')
//         .then(response => response.json())
//         .then(data => {
//           document.getElementById('temp1').innerHTML = data.temp1;
//           document.getElementById('temp2').innerHTML = data.temp2;
//           document.getElementById('dT').innerHTML = data.dT;
//           document.getElementById('power_mW').innerHTML = data.power_mW;
//           document.getElementById('busVoltage').innerHTML = "Bus Voltage: " + data.busVoltage + " V";
//           document.getElementById('current_mA').innerHTML = "Current: " + data.current_mA + " mA";
//           document.getElementById('thermalConductivity').innerHTML = data.thermalConductivity + " W/m·K";
//         })
//         .catch(error => console.error('Error fetching data:', error));
//     }

//     // Update every 2 seconds
//     setInterval(updateData, 2000);
//     updateData(); // Initial load
//   </script>
// </body>
// </html>
// )=====";

//     // Replace placeholders with actual values
//     html.replace("%THICKNESS%", String(sampleThickness));
//     html.replace("%DIAMETER%", String(diameter));
//     html.replace("%TEMP_OFFSET%", String(temperature_offset));
//     html.replace("%TEMP1%", String(temp1));
//     html.replace("%TEMP2%", String(temp2));
//     html.replace("%DT%", String(dT));
//     html.replace("%POWER%", String(power_mW));
//     html.replace("%BUS_VOLTAGE%", String(busVoltage));
//     html.replace("%CURRENT%", String(current_mA));
//     html.replace("%THERMAL_CONDUCTIVITY%", String(thermalConductivity));

//     server.send(200, "text/html", html);
// }

// void handleGetData()
// {
//     String json = "{";
//     json += "\"temp1\":" + String(temp1) + ",";
//     json += "\"temp2\":" + String(temp2) + ",";
//     json += "\"dT\":" + String(dT) + ",";
//     json += "\"power_mW\":" + String(power_mW) + ",";
//     json += "\"busVoltage\":" + String(busVoltage) + ",";
//     json += "\"current_mA\":" + String(current_mA) + ",";
//     json += "\"thermalConductivity\":" + String(thermalConductivity);
//     json += "}";

//     server.send(200, "application/json", json);
// }

// // Function to read temperature from MAX31865
// float readTemperature1(Adafruit_MAX31865 &sensor)
// {
//     float rtd = sensor.readRTD();
//     Serial.print(rtd);
//     Serial.print("      ");
//     Serial.print(RREF1);
//     float temperature = sensor.temperature(RNOMINAL, RREF1);
//     temperature += 273.15;
//     Serial.print("   Temperature T1: ");
//     Serial.println(temperature);
//     return temperature;
// }

// float readTemperature2(Adafruit_MAX31865 &sensor)
// {
//     float rtd = sensor.readRTD();
//     Serial.print(rtd);
//     float temperature = sensor.temperature(RNOMINAL, RREF2);
//     temperature += 273.15 + temperature_offset;
//     // temperature += 273.15 ;
//     Serial.print("   Temperature T2: ");
//     Serial.println(temperature);

//     return temperature;
// }

// void calculateThermalconductivity()
// {
//     // Calculate thermal conductivity
//     dT = temp1 - temp2;
//     float absolute_dT = fabs(dT);
//     crossSectionArea = 3.14159 * (diameter / 2.00) * (diameter / 2.00);
//     if (absolute_dT > 0 && sampleThickness > 0 && crossSectionArea > 0)
//     {
//         thermalConductivity = (power_mW * sampleThickness) / (crossSectionArea * absolute_dT * 1000);
//     }
//     else
//     {
//         thermalConductivity = 0.0;
//     }
// }