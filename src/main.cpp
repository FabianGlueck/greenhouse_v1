#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

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
unsigned long lastBmeLog = 0;
const unsigned long BME_INTERVAL = 5000UL; // ms

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // LEDs
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_LED_EXT, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_LED_EXT, LOW);
  digitalWrite(PIN_RELAY, HIGH);

  // Try both common I2C addresses for BME280 (0x76 and 0x77)
  if (bme.begin(0x76)) {
    bme_present = true;
    Serial.println("BME280 found at 0x76");
  } else if (bme.begin(0x77)) {
    bme_present = true;
    Serial.println("BME280 found at 0x77");
  } else {
    Serial.println("BME280 not found");
    // Print what else is on the I2C bus to help debugging
    scanI2COnce();
  }
  // Puffer vorfüllen
  for (int i = 0; i < N; i++) {
    buf[i] = analogRead(PIN_SOIL);
    sum += buf[i];
    delay(5);
  }
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

  Serial.print("RAW:");
  Serial.print(raw);
  Serial.print("  SMOOTH:");
  Serial.print(s);
  Serial.print("  Moisture%:");
  Serial.println(mapped);

  // Periodic BME280 logging every BME_INTERVAL ms
  if (bme_present && (millis() - lastBmeLog >= BME_INTERVAL)) {
    float t = bme.readTemperature(); // degC
    float h = bme.readHumidity();    // %
    float p = bme.readPressure() / 100.0F; // hPa

    Serial.print("BME Temp:C ");
    Serial.print(t, 2);
    Serial.print("  Humidity:% ");
    Serial.print(h, 2);
    Serial.print("  Pressure:hPa ");
    Serial.println(p, 2);

    lastBmeLog = millis();
  }

  delay(250);
}
