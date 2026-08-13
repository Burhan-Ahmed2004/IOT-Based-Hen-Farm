#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include "DHT.h"

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "asdf"
#define WIFI_PASSWORD "12345678"

// Insert Firebase project API Key
#define API_KEY " "  

// Insert RTDB URL
#define DATABASE_URL "https://hens-farm-default-rtdb.firebaseio.com/"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// DHT11 configuration
#define DHTPIN D6 // Pin connected to DHT11 sensor
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Soil moisture sensor configuration
#define SOIL_MOISTURE_PIN A0 // Pin connected to the soil moisture sensor

// Water pump and fan control pins
#define WATER_PUMP_PIN D5 // Pin connected to water pump relay
#define FAN_PIN D2        // Pin connected to fan relay

// Servo motor and ultrasonic sensor control pins
#include <Servo.h>
#define TRIG_PIN D7        // Pin connected to ultrasonic sensor trig
#define ECHO_PIN D8        // Pin connected to ultrasonic sensor echo
#define SERVO_PIN D4       // Pin connected to the servo motor

// Soil moisture threshold (below this value, the pump turns on)
#define MOISTURE_THRESHOLD 42  // Adjust based on your sensor range

// Temperature threshold (above this value, the fan turns on)
#define TEMP_THRESHOLD 17  // Temperature in Celsius

Servo grainGate; // Create a Servo object for the grain gate

void setup() {
  Serial.begin(115200);

  // Initialize DHT11 sensor
  dht.begin();

  // Set pins as output
  pinMode(WATER_PUMP_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  
  // Initialize water pump and fan to OFF
  digitalWrite(WATER_PUMP_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);

  // Set up ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Attach the servo motor to the specified pin
  grainGate.attach(SERVO_PIN);

  // Initialize WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Assign the Firebase configuration
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Sign up to Firebase
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase sign up successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase sign up failed: %s\n", config.signer.signupError.message.c_str());
  }

  // Assign the token status callback function
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  // Read temperature and humidity from DHT11
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check if readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
   // return;
  }

  // Read soil moisture value
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN); // Read analog value from soil moisture sensor

  // Map the soil moisture value to percentage (0-100)
  int soilMoisturePercentage = map(soilMoistureValue, 0, 1023, 0, 100);

  // Print values to Serial Monitoro
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println("°C");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
  Serial.print("Soil Moisture: ");
  Serial.print(soilMoisturePercentage);
  Serial.println("%");

  // Send data to Firebase every second
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // Update temperature, humidity, and soil moisture values in Firebase
    if (Firebase.RTDB.setFloat(&fbdo, "Temperature", temperature)) {
      Serial.println("Temperature updated successfully!");
    } else {
      Serial.println("Failed to update temperature.");
      Serial.println(fbdo.errorReason());
    }

    if (Firebase.RTDB.setFloat(&fbdo, "Humidity", humidity)) {
      Serial.println("Humidity updated successfully!");
    } else {
      Serial.println("Failed to update humidity.");
      Serial.println(fbdo.errorReason());
    }

    if (Firebase.RTDB.setInt(&fbdo, "SoilMoisture", soilMoisturePercentage)) {
      Serial.println("Soil Moisture updated successfully!");
    } else {
      Serial.println("Failed to update soil moisture.");
      Serial.println(fbdo.errorReason());
    }
  }
  Serial.println();
  Serial.println();

  // Control water pump based on soil moisture level
  if (soilMoistureValue > 350) {
    digitalWrite(WATER_PUMP_PIN, LOW);  // Turn on water pump
    Serial.println(soilMoistureValue);
    Serial.println("Water pump ON (low moisture)");
  } else {
    digitalWrite(WATER_PUMP_PIN, HIGH);   // Turn off water pump
    Serial.println("Water pump OFF (sufficient moisture)");
  }
  Serial.println();

  // Control fan based on temperature
  if (temperature > TEMP_THRESHOLD) {
    digitalWrite(FAN_PIN, LOW);  // Turn on fan
    Serial.println("Fan ON (temperature > 20°C)");
  } else {
    digitalWrite(FAN_PIN, HIGH);   // Turn off fan
    Serial.println("Fan OFF (temperature <= 20°C)");
  }

  // Measure distance using ultrasonic sensor
  long duration, distance;
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(4);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration * 0.0344 / 2; // Calculate distance in cm
    if (Firebase.RTDB.setInt(&fbdo, "distance",distance )) {
      Serial.println("distance updated successfully!");
    } else {
      Serial.println("Failed to update distance.");
      Serial.println(fbdo.errorReason());
    }
  Serial.print("Distance to grains: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Control the servo motor gate based on the distance
  if (distance > 4) {  // If distance is less than 10 cm
    grainGate.write(90);  // Open the gate
    Serial.println("Grain gate opened");
  } else {
    grainGate.write(0);   // Close the gate
    Serial.println("Grain gate closed");
  }

  delay(2000);
}
