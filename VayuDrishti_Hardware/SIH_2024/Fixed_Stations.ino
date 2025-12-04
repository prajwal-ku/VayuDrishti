#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <Wire.h>
#include "RTClib.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

//#define RXPin 16       // GPS RX Pin
//#define TXPin 17       // GPS TX Pin

#define DHTPIN 4       // Pin where DHT11 is connected
#define DHTTYPE DHT11  // DHT 11 sensor
#define MQ7PIN 34      // Analog Pin for MQ-7
#define MQ135PIN 35    // Analog Pin for MQ-135

RTC_DS1307 rtc;
DHT dht(DHTPIN, DHTTYPE); // Initialize DHT11 sensor
//TinyGPSPlus gps; // GPS object

#define On_Board_LED_PIN 2  // On-board LED pin for status indication

// WiFi Credentials
const char* ssid = "Prajwal's HP";  // Your WiFi SSID
const char* password = "FG Prajwal";  // Your WiFi Password

// Google Apps Script URL
String Web_App_URL = "https://script.google.com/macros/s/AKfycbwREmp5-767TNo_3jpJ4eaZoVhr2o0TwVnI9_l9q_vv9ncuCxgEdDqTsNvc9Hj3HMWL/exec";  // Replace with your Google Apps Script URL

// Sensor Data Variables
String Status_Read_Sensor = "";
float Temp;
int Humd;
int MQ7_Raw_Value;
int MQ135_Raw_Value;

// GPS and RTC Variables
char timestamp[25];

void setup() {
  Serial.begin(115200);
  // Serial2.begin(9600, SERIAL_8N1, RXPin, TXPin); // Initialize GPS

  // Initialize DHT sensor
  dht.begin();

  // Initialize MQ sensor pins (as analog inputs)
  pinMode(MQ7PIN, INPUT);
  pinMode(MQ135PIN, INPUT);

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // Check if RTC lost power
  if (!rtc.isrunning()) {
    Serial.println("RTC is not running. Setting time...");
    DateTime now = DateTime(F(__DATE__), F(__TIME__));
    rtc.adjust(now + TimeSpan(0, 0, 3, 0)); // Add 3 minutes
  }

  // Initialize SD card
 /* if (!SD.begin()) {
    Serial.println("SD card initialization failed!");
    while (1);
  }*/

  // Create the CSV file with a header if it doesn't exist
  if (!SD.exists("/AQI.csv")) {
    File file = SD.open("/AQI.csv", FILE_WRITE);
    if (file) {
      file.println("Timestamp,Latitude,Longitude,Temperature (°C),Humidity (%),MQ-7 Raw,MQ-135 Raw");
      file.close();
      Serial.println("CSV file created: AQI.csv");
    } else {
      Serial.println("Failed to create AQI.csv");
    }
  }

  // Wi-Fi Connection Setup
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("Connected to Wi-Fi");
}

void loop() {
  // Commented out GPS part
  // Wait for GPS to get a valid fix
  /*
  Serial.println("Waiting for GPS fix...");
  while (true) {
    while (Serial2.available() > 0) {
      if (gps.encode(Serial2.read())) {
        if (gps.location.isValid()) {
          latitude = gps.location.lat();
          longitude = gps.location.lng();
          Serial.println("GPS fix acquired!");
          break;
        }
      }
    }
    if (gps.location.isValid()) break;
    delay(500); // Check every 500 ms
  }
  */

  // Get current time from RTC
  DateTime now = rtc.now();
  sprintf(timestamp, "%04d/%02d/%02d %02d:%02d:%02d",
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second());

  // Read DHT11 sensor values
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check if DHT sensor reading is valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Read analog values from MQ-7 and MQ-135 (raw sensor values)
  MQ7_Raw_Value = analogRead(MQ7PIN);
  MQ135_Raw_Value = analogRead(MQ135PIN);

  // Display data on Serial Monitor
  Serial.println("________________________________________________");
  Serial.println("                    AQI SQUAD DATA");
  Serial.println("________________________________________________");
  Serial.print("Timestamp: ");
  Serial.println(timestamp);
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  Serial.print("MQ-7 Raw: ");
  Serial.println(MQ7_Raw_Value);
  Serial.print("MQ-135 Raw: ");
  Serial.println(MQ135_Raw_Value);

  // Send data to Google Sheets
  if (WiFi.status() == WL_CONNECTED) {
    // Construct URL to send data to Google Sheets
    String Send_Data_URL = Web_App_URL + "?sts=write";
    Send_Data_URL += "&srs=" + String("Success");  // Assuming sensor reading is always successful for now
    Send_Data_URL += "&temp=" + String(temperature);
    Send_Data_URL += "&humd=" + String(humidity);
    Send_Data_URL += "&swtc1=" + String(MQ7_Raw_Value);
    Send_Data_URL += "&swtc2=" + String(MQ135_Raw_Value);

    Serial.println("Sending data to Google Spreadsheet...");
    Serial.println("URL: " + Send_Data_URL);

    // Send HTTP GET request
    HTTPClient http;
    http.begin(Send_Data_URL.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET();
    Serial.print("HTTP Status Code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Payload: " + payload);
    }

    http.end();
  }

  // Append data to SD card
  File file = SD.open("/AQI.csv", FILE_APPEND);
  if (file) {
    file.printf("%s,%.2f,%.2f,%d,%d\n",
                timestamp, temperature, humidity, MQ7_Raw_Value, MQ135_Raw_Value);
    file.close();
    Serial.println("Data written to AQI.csv");
  } else {
    Serial.println("Failed to open AQI.csv for appending");
  }

  // Wait for 30 seconds before the next reading
  delay(30000);  // 30 seconds
}
