#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
BH1750 lightMeter;
bool bh_present = false;

// Helper: read lux (returns negative on error)
float readLux() {
  // BH1750::readLightLevel() returns lux as float
  return lightMeter.readLightLevel();
}


const int PIN_SOIL = A0;
const int PIN_LED = LED_BUILTIN;  // eingebaute LED (Pin 13 beim Uno)
const int PIN_LED_EXT = 8;         // externe LED an D8
const int PIN_RELAY = 9;         // Relais-Input an D8 (aktiviert bei hoher Feuchte)

// Kalibrierwerte nachher anpassen!
int DRY_RAW = 500;  // trocken
int WET_RAW = 330;  // nass

const int N = 10;   // Fenstergröße für Glättung
int buf[N];
int idx = 0;
long sum = 0;

int smooth(int x) {
  sum -= buf[idx];
  buf[idx] = x;
  sum += buf[idx];
  idx = (idx + 1) % N;
  return sum / N;
}

int clamp(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// Simple one-shot I2C scanner (prints devices found on bus)
void scanI2COnce() {
  Serial.println("I2C scan start...");
  bool any = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      any = true;
      Serial.print("  Found device at 0x");
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
    }
    delay(1);
  }
  if (!any) Serial.println("  No I2C devices found on bus");
  Serial.println("I2C scan done");
}

// BME280
Adafruit_BME280 bme;
bool bme_present = false;

// Unified logging interval for all sensors
unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 2000UL; // ms (2 seconds)

void setup() {
  Serial.begin(9600);
  delay(500);  // Give Serial time to stabilize
  Wire.begin();
  delay(100);  // Give Wire time to initialize

  Serial.println("\n=== GREENHOUSE STARTUP ===");

  // LEDs
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_LED_EXT, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_LED_EXT, LOW);
  digitalWrite(PIN_RELAY, HIGH);

  // Scan I2C bus first to see what's actually present
  Serial.println("Scanning I2C bus...");
  scanI2COnce();

  // Try both common I2C addresses for BME280 (0x76 and 0x77)
  Serial.println("Initializing BME280...");
  if (bme.begin(0x76)) {
    bme_present = true;
    Serial.println("✓ BME280 found at 0x76");
  } else if (bme.begin(0x77)) {
    bme_present = true;
    Serial.println("✓ BME280 found at 0x77");
  } else {
    Serial.println("✗ BME280 not found (check wiring & I2C address)");
  }
  // BH1750 (light sensor) initialization
  Serial.println("Initializing BH1750...");
  if (lightMeter.begin()) {
    bh_present = true;
    Serial.println("✓ BH1750 light sensor found");
  } else {
    Serial.println("✗ BH1750 not found");
  }
  // Puffer vorfüllen
  for (int i = 0; i < N; i++) {
    buf[i] = analogRead(PIN_SOIL);
    sum += buf[i];
    delay(5);
  }
  
  Serial.println("=== STARTUP COMPLETE ===\n");
}

void loop() {
  int raw = analogRead(PIN_SOIL);
  int s = smooth(raw);

  // Hinweis: Beim kapazitiven Sensor gilt oft: kleiner RAW = nasser
  // Auf Prozent mappen (0% = trocken, 100% = nass) anhand deiner Kalibrierung
  int mapped = map(clamp(s, WET_RAW, DRY_RAW), DRY_RAW, WET_RAW, 0, 100);
  mapped = clamp(mapped, 0, 100);

static bool ledOn = false;  // Merkt sich aktuellen LED-Zustand

if (!ledOn && mapped >= 50) {
  ledOn = true;
  digitalWrite(PIN_LED, HIGH);
  digitalWrite(PIN_LED_EXT, HIGH);
  digitalWrite(PIN_RELAY, LOW);
}

if (ledOn && mapped < 40) {
  ledOn = false;
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_LED_EXT, LOW);
  digitalWrite(PIN_RELAY, HIGH);
}

  // Periodic unified logging every LOG_INTERVAL ms
  if (millis() - lastLogTime >= LOG_INTERVAL) {
    Serial.print("Soil RAW:");
    Serial.print(raw);
    Serial.print(" SMOOTH:");
    Serial.print(s);
    Serial.print(" Moisture%:");
    Serial.print(mapped);
    
    if (bme_present) {
      float t = bme.readTemperature(); // degC
      float h = bme.readHumidity();    // %
      float p = bme.readPressure() / 100.0F; // hPa
      Serial.print(" | Temp:");
      Serial.print(t, 1);
      Serial.print("C Humid:");
      Serial.print(h, 1);
      Serial.print("% Press:");
      Serial.print(p, 0);
      Serial.print("hPa");
    }
    
    if (bh_present) {
      float lux = readLux();
      Serial.print(" | Lux:");
      if (lux < 0) {
        Serial.print("ERROR");
      } else {
        Serial.print(lux, 0);
      }
    }
    
    Serial.println();
    lastLogTime = millis();
  }
}
