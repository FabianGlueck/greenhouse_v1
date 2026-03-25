#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <ezButton.h>
BH1750 lightMeter;
bool bh_present = false;

// Helper: read lux (returns negative on error)
float readLux() {
  // BH1750::readLightLevel() returns lux as float
  return lightMeter.readLightLevel();
}


// Bodenfeuchte-Sensoren: Links=A2, Mitte=A0, Rechts=A3
const int NUM_SOIL = 3;
const int PIN_SOIL[NUM_SOIL] = {A2, A0, A3};       // Links, Mitte, Rechts
const char* SOIL_LABEL[NUM_SOIL] = {"L", "M", "R"}; // Labels für Serial-Output

const int PIN_LED = LED_BUILTIN;  // eingebaute LED (Pin 13 beim Uno)
const int PIN_LED_EXT = 8;         // externe LED an D8
const int PIN_RELAY = 9;         // Relais-Input an D9
const int TASTER_1_PIN = 10;
const int TASTER_2_PIN = 11;
const int TASTER_3_PIN = 12;

ezButton BUTTON1(TASTER_1_PIN);
ezButton BUTTON2(TASTER_2_PIN);
ezButton BUTTON3(TASTER_3_PIN);
// Kalibrierwerte nachher anpassen!
int DRY_RAW = 500;  // trocken
int WET_RAW = 330;  // nass

const int N = 10;   // Fenstergröße für Glättung
int buf[NUM_SOIL][N];
int idx[NUM_SOIL] = {0, 0, 0};
long sum[NUM_SOIL] = {0, 0, 0};

int smooth(int sensor, int x) {
  sum[sensor] -= buf[sensor][idx[sensor]];
  buf[sensor][idx[sensor]] = x;
  sum[sensor] += x;
  idx[sensor] = (idx[sensor] + 1) % N;
  return sum[sensor] / N;
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

  //Button Setup
  BUTTON1.setDebounceTime(100);  // set debounce time to 100 milliseconds
  BUTTON2.setDebounceTime(100);  // set debounce time to 100 milliseconds
  BUTTON3.setDebounceTime(100);  // set debounce time to 100 milliseconds

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
  // Puffer vorfüllen für alle 3 Sensoren
  for (int s = 0; s < NUM_SOIL; s++) {
    for (int i = 0; i < N; i++) {
      buf[s][i] = analogRead(PIN_SOIL[s]);
      sum[s] += buf[s][i];
      delay(5);
    }
  }
  
  Serial.println("=== STARTUP COMPLETE ===\n");
}

void loop() {
  BUTTON1.loop();  // Update button state
  BUTTON2.loop();  // Update button state
  BUTTON3.loop();  // Update button state

  // Alle 3 Bodenfeuchte-Sensoren auslesen
  int raw[NUM_SOIL], sm[NUM_SOIL], moistPct[NUM_SOIL];
  for (int i = 0; i < NUM_SOIL; i++) {
    raw[i] = analogRead(PIN_SOIL[i]);
    sm[i] = smooth(i, raw[i]);
    moistPct[i] = map(clamp(sm[i], WET_RAW, DRY_RAW), DRY_RAW, WET_RAW, 0, 100);
    moistPct[i] = clamp(moistPct[i], 0, 100);
  }

static bool relayOn = false;  // Merkt sich aktuellen Relais-Zustand
static bool buttonToggled = false;

  if (BUTTON1.isPressed()){
      digitalWrite(PIN_LED_EXT, HIGH);
      Serial.println("The button 1 is pressed");
    }
    if (BUTTON1.isReleased()){
      digitalWrite(PIN_LED_EXT, LOW);
      Serial.println("The button 1 is released");
    }

    if (BUTTON2.isPressed()){
      digitalWrite(PIN_LED_EXT, HIGH);
      Serial.println("The button 2 is pressed");
    }

    if (BUTTON2.isReleased()){
      digitalWrite(PIN_LED_EXT, LOW);
      Serial.println("The button 2 is released");
    }

    if (BUTTON3.isPressed()){
      digitalWrite(PIN_LED_EXT, HIGH);
      Serial.println("The button 3 is pressed");
    }
    if (BUTTON3.isReleased()){
      digitalWrite(PIN_LED_EXT, LOW);
      Serial.println("The button 3 is released");
    }


  // Periodic unified logging every LOG_INTERVAL ms
  // Format: Soil_L RAW:x SMOOTH:x Moist%:x | Soil_M RAW:x ... | Soil_R RAW:x ... | Temp:... | Lux:...
  if (millis() - lastLogTime >= LOG_INTERVAL) {
    for (int i = 0; i < NUM_SOIL; i++) {
      if (i > 0) Serial.print(" | ");
      Serial.print("Soil_");
      Serial.print(SOIL_LABEL[i]);
      Serial.print(" RAW:");
      Serial.print(raw[i]);
      Serial.print(" SMOOTH:");
      Serial.print(sm[i]);
      Serial.print(" Moist%:");
      Serial.print(moistPct[i]);
    }

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
