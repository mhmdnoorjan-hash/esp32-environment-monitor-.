#define BLYNK_TEMPLATE_ID "TMPL4fMoTAX8k"
#define BLYNK_TEMPLATE_NAME "ESP32 Environment Monitor"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT22

#define LED_PIN 2
#define BUZZER_PIN 5

char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

const float TEMP_THRESHOLD = 30.0;
const float HUM_THRESHOLD = 70.0;

const unsigned long READ_INTERVAL_MS = 2000;
const unsigned long BUZZER_TOGGLE_MS = 200;
const unsigned long NOTIFICATION_COOLDOWN_MS = 60000;

DHT dht(DHTPIN, DHTTYPE);

unsigned long lastReadTime = 0;
unsigned long lastBuzzerToggle = 0;
unsigned long lastNotificationTime = 0;

bool buzzerState = false;
bool alertActive = false;
bool alertSent = false;

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 40) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed");
  }
}

void connectBlynk() {
  Serial.println("Connecting to Blynk...");
  Blynk.config(BLYNK_AUTH_TOKEN, "blynk.cloud", 80);

  if (Blynk.connect(10000)) {
    Serial.println("Blynk connected");
  } else {
    Serial.println("Blynk connection failed");
  }
}

void printReadings(float temperature, float humidity, bool isAlert) {
  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.print(" C | Humidity: ");
  Serial.print(humidity, 1);
  Serial.print(" % | Status: ");
  Serial.println(isAlert ? "ALERT" : "NORMAL");
}

void updateAlertOutputs(bool isAlert) {
  digitalWrite(LED_PIN, isAlert ? HIGH : LOW);

  if (!isAlert) {
    buzzerState = false;
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  unsigned long currentMillis = millis();

  if (currentMillis - lastBuzzerToggle >= BUZZER_TOGGLE_MS) {
    lastBuzzerToggle = currentMillis;
    buzzerState = !buzzerState;
    digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
  }
}

void sendAlertNotification(float temperature, float humidity) {
  unsigned long currentMillis = millis();

  if (!alertSent || (currentMillis - lastNotificationTime >= NOTIFICATION_COOLDOWN_MS)) {
    String message = "Temperature: " + String(temperature, 1) +
                     " C, Humidity: " + String(humidity, 1) + " %";

    Blynk.logEvent("high_temp_alert", message);

    lastNotificationTime = currentMillis;
    alertSent = true;
  }
}

void sendDataToBlynk(float temperature, float humidity, bool isAlert) {
  if (!Blynk.connected()) {
    return;
  }

  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Blynk.virtualWrite(V2, isAlert ? 1 : 0);
  Blynk.virtualWrite(V3, isAlert ? "ALERT" : "NORMAL");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();
  delay(2000);

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    connectBlynk();
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) {
    connectBlynk();
  }

  Blynk.run();

  unsigned long currentMillis = millis();

  if (currentMillis - lastReadTime >= READ_INTERVAL_MS) {
    lastReadTime = currentMillis;

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor");
      alertActive = false;
      updateAlertOutputs(alertActive);
      return;
    }

    bool tempAlert = temperature >= TEMP_THRESHOLD;
    bool humAlert = humidity >= HUM_THRESHOLD;
    alertActive = tempAlert || humAlert;

    printReadings(temperature, humidity, alertActive);
    sendDataToBlynk(temperature, humidity, alertActive);

    if (alertActive && Blynk.connected()) {
      sendAlertNotification(temperature, humidity);
    } else if (!alertActive) {
      alertSent = false;
    }
  }

  updateAlertOutputs(alertActive);
}
