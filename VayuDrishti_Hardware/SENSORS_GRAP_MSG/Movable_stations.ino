#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <Wire.h>
#include "RTClib.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <TinyGPS++.h>

#define RXPin 16       
#define TXPin 17       

#define DHTPIN 4       
#define DHTTYPE DHT11  
#define MQ7PIN 34      
#define MQ135PIN 35    

RTC_DS1307 rtc;
DHT dht(DHTPIN, DHTTYPE); 

#define On_Board_LED_PIN 2  

// WiFi Credentials
const char* ssid = "Prajwal's HP";  // Your WiFi SSID
const char* password = "FG Prajwal";  // Your WiFi Password

// Google Apps Script URL
String Web_App_URL = "https://script.google.com/macros/s/AKfycbwREmp5-767TNo_3jpJ4eaZoVhr2o0TwVnI9_l9q_vv9ncuCxgEdDqTsNvc9Hj3HMWL/exec";  //Google Apps Script URL

// Sensor Data Variables
String Status_Read_Sensor = "";
float Temp;
int Humd;
int MQ7_Raw_Value;
int MQ135_Raw_Value;

// GPS and RTC Variables
char timestamp[25];
float latitude = 0.0, longitude = 0.0;

HardwareSerial GPS_Serial(1);  // Initialize serial for GPS

// VVM501 Module Pins and SMS
#define RXD2 27    // VVM501 Module RXD Pin
#define TXD2 26    // VVM501 Module TXD Pin
#define powerPin 4 // VVM501 Power Pin
#define SerialAT Serial1
String number = "+919967076090"; // Replace with target phone number
String _buffer;


void setup() {
  Serial.begin(115200);
  
  // Simulate GPRS connection
  sendATCommands();

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
  if (!SD.begin()) {
    Serial.println("SD card initialization failed!");
    while (1);
  }

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

  // VVM501 initialization for SMS
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW);
  SerialAT.begin(115200, SERIAL_8N1, RXD2, TXD2);
  delay(10000);
  initializeModem();
}

void loop() {
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
  Serial.printf("Latitude: %.6f | Longitude: %.6f\n", latitude, longitude);
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
    String Send_Data_URL = Web_App_URL + "?sts=write";
    Send_Data_URL += "&srs=" + String("Success");
    Send_Data_URL += "&temp=" + String(temperature);
    Send_Data_URL += "&humd=" + String(humidity);
    Send_Data_URL += "&swtc1=" + String(MQ7_Raw_Value);
    Send_Data_URL += "&swtc2=" + String(MQ135_Raw_Value);

    Serial.println("Sending data to Google Spreadsheet...");
    Serial.println("URL: " + Send_Data_URL);

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

  // Simulate AQI value for SMS alerts
  int AQI = random(50, 501);  // Simulate AQI value
  if (AQI > 200) {
    String AQI_Status = getAQIStatus(AQI);
    sendSMS(AQI, AQI_Status);
  }

  // Wait for 30 seconds before the next reading
  delay(30000);  // 30 seconds
}

void initializeModem() {
  Serial.println("Modem Reset, please wait");
  SerialAT.println("AT+CRESET");
  delay(20000);

  SerialAT.println("ATE0");   // Echo off
  delay(1000);

  String response = SerialAT.readString();
  Serial.print("Modem Response: ");
  Serial.println(response);

  if (response.indexOf("OK") != -1) {
    Serial.println("Modem Ready");
  } else {
    Serial.println("Modem Not Ready");
  }
}

String getAQIStatus(int AQI) {
  if (AQI >= 200 && AQI <= 300) {
    return "GRAP Stage I: Poor air quality, AQI 201-300. Avoid outdoor activities.";
  } else if (AQI >= 301 && AQI <= 400) {
    return "GRAP Stage II: Very poor air quality, AQI 301-400. Wear a mask, avoid exertion.";
  } else if (AQI >= 401 && AQI <= 450) {
    return "GRAP Stage III: Severe air quality, AQI 401-450. Limit outdoor exposure.";
  } else if (AQI > 450) {
    return "GRAP Stage IV: Hazardous air quality, AQI > 450. Stay indoors, avoid breathing polluted air.";
  }
  return "AQI is safe, no precautions needed.";
}

void sendSMS(int AQI, String AQI_Status) {
  SerialAT.println("AT+CMGF=1");    // Text mode
  delay(1000);

  SerialAT.println("AT+CMGS=\"" + number + "\"");
  delay(1000);

  String message = "Alert: AQI is " + String(AQI) + "\n" + AQI_Status;
  SerialAT.println(message);
  delay(100);

  SerialAT.write(26);  
  delay(30000);
  Serial.println("SMS Sent: " + message);
}
