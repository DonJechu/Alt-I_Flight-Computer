#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>

// --------------------------------------------------
//   Atl-1 FLIGHT COMPUTER  —  v2.1  LITTLEFS ONBOARD LOGGING
//   - Saves each flight into /flight_N.csv in non-volatile memory
//   - Retrieves data via Serial: LIST | DUMP | DUMP:flight_1.csv | FORMAT
// --------------------------------------------------

// ------------- 1. Config -------------
const char* ssid     = "Atl_FC";
const char* password = "12345678";
#define PIN_SDA    21
#define PIN_SCL    22
#define PIN_SERVO  13
#define MPU_ADDR   0x68

// ------------- Flight Thresholds -------------
#define LIFTOFF_G            2.5f
#define LIFTOFF_CONFIRMS     4
#define APOGEE_VEL_THRESHOLD 0.0f
#define APOGEE_CONFIRMS      5
#define APOGEE_ALT_DROP      1.0f
#define APOGEE_ALT_CONFIRMS  4
#define MIN_ALTITUDE_LAUNCH  3.0f
#define LANDING_ALT_M        0.75f
#define ASCENT_TIMEOUT_MS    12000

// ------------- 2. Objects -------------
Adafruit_MPU6050 mpu;
Adafruit_BMP280  bmp;
Servo            parachute;
WebSocketsServer webSocket = WebSocketsServer(81);

// ------------- 3. Globals -------------
float         basePressure     = 1013.25f;
float         groundAltitude   = 0.0f;
int           currentState     = 0;
float         currentAltitude  = 0.0f;
float         filteredAltitude = 0.0f;
float         gForce           = 0.0f;
float         maxAltitude      = 0.0f;
float         lastAltitude     = 0.0f;
float         velocity         = 0.0f;
float         pitch            = 0.0f;
float         roll             = 0.0f;
float         cachedTemp       = 0.0f;
float         cachedPressHPa   = 0.0f;

unsigned long lastTelemetryTime = 0;
unsigned long lastTime          = 0;
unsigned long ascentStartTime   = 0;
unsigned long lastBmpRead       = 0;
const int     telemetryInterval = 100;
const int     bmpInterval       = 40;

// Confirmation counters
int liftoffCounter   = 0;
int apogeeVelCounter = 0;
int apogeeAltCounter = 0;

// ------------- 4. LittleFS -------------
bool   fsReady       = false;
File   logFile;
int    logWriteCount = 0;
String logFileName   = "";
bool   logClosed     = false;
String serialBuffer  = "";

// --------------------------------------------------
//  Flight States
// --------------------------------------------------
enum FlightStates {
  STANDBY  = 0, ARMED    = 1, IGNITION = 2,
  ASCENT   = 3, APOGEE   = 4, DESCENT  = 5, LANDING = 6
};

// --------------------------------------------------
//  FORWARD DECLARATIONS
// --------------------------------------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void initLittleFS();
void logData();
void closeLog();
void handleSerialCommands();
void listFiles();
void dumpFileToSerial(String path);

// --------------------------------------------------
//  SETUP
// --------------------------------------------------
void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);

  WiFi.softAP(ssid, password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("=== HYDRO-1 FLIGHT COMPUTER v2.1 BOOT ===");

  // MPU6050
  if (!mpu.begin(MPU_ADDR)) Serial.println("[ERROR] MPU6050 Init Failed");
  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  delay(100);

  // BMP280
  if (!bmp.begin(0x76)) Serial.println("[ERROR] BMP280 Init Failed");
  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X2,
    Adafruit_BMP280::SAMPLING_X16,
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_1
  );

  // 200 readings: first 100 warm up the IIR filter, second 100 calculate basePressure
  // The IIR x16 filter needs ~70 samples to stabilize — first ones are discarded
  Serial.println("Calibrando presion de suelo...");
  float sumP = 0;
  for (int i = 0; i < 200; i++) {
    float p = bmp.readPressure();
    if (i >= 100) sumP += p;  // only average with a stable IIR filter
    delay(13);
  }
  basePressure = (sumP / 100.0f) / 100.0f;
  Serial.print("Base Pressure: "); Serial.print(basePressure); Serial.println(" hPa");

  // Ground altitude offset — 50 samples averaged
  float sumA = 0;
  for (int i = 0; i < 50; i++) { sumA += bmp.readAltitude(basePressure); delay(13); }
  groundAltitude = sumA / 50.0f;
  Serial.print("Ground Offset: "); Serial.println(groundAltitude);

  // Initial sensor cache
  cachedTemp     = bmp.readTemperature();
  cachedPressHPa = bmp.readPressure() / 100.0f;

  // LittleFS — must be initialized BEFORE attaching the servo
  initLittleFS();

  // Servo
  parachute.attach(PIN_SERVO);
  parachute.write(0);

  lastTime         = millis();
  lastBmpRead      = millis();
  filteredAltitude = 0.0f;
  lastAltitude     = 0.0f;
  currentAltitude  = 0.0f;

  Serial.println("=== READY FOR FLIGHT ===");
  Serial.println("Comandos: LIST | DUMP | DUMP:<archivo> | FORMAT");
}

// --------------------------------------------------
//  LOOP
// --------------------------------------------------
void loop() {
  webSocket.loop();
  handleSerialCommands();
  readSensors();
  updateLogic();

  if (millis() - lastTelemetryTime >= telemetryInterval) {
    sendData();
    logData();
    lastTelemetryTime = millis();
  }
}

// --------------------------------------------------
//  LITTLEFS — initialization and flight file creation
// --------------------------------------------------
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("[ERROR] LittleFS mount failed");
    return;
  }
  fsReady = true;

  // Search for the next available flight number
  int n = 1;
  while (LittleFS.exists("/flight_" + String(n) + ".csv")) n++;
  logFileName = "/flight_" + String(n) + ".csv";

  logFile = LittleFS.open(logFileName, FILE_WRITE);
  if (!logFile) {
    Serial.println("[ERROR] No se pudo abrir el archivo de log");
    fsReady = false;
    return;
  }

  logFile.println("t_ms,alt_m,vel_ms,accel_G,pitch_deg,roll_deg,yaw_deg,temp_C,pressure_hPa,phase");
  logFile.flush();

  Serial.println("[FS] Archivo: " + logFileName);
  Serial.print("[FS] Libre: ");
  Serial.print(LittleFS.totalBytes() - LittleFS.usedBytes());
  Serial.println(" bytes");
}

// --------------------------------------------------
//  LOG — writes a CSV row to flash, flushes every 10 writes
// --------------------------------------------------
void logData() {
  if (!fsReady || !logFile || logClosed) return;

  logFile.print(millis());            logFile.print(',');
  logFile.print(currentAltitude, 1);  logFile.print(',');
  logFile.print(velocity, 1);         logFile.print(',');
  logFile.print(gForce, 2);           logFile.print(',');
  logFile.print(pitch, 1);            logFile.print(',');
  logFile.print(roll, 1);             logFile.print(',');
  logFile.print("0.0,");
  logFile.print(cachedTemp, 1);       logFile.print(',');
  logFile.print(cachedPressHPa, 1);   logFile.print(',');
  logFile.println(currentState);

  if (++logWriteCount >= 10) {
    logFile.flush();
    logWriteCount = 0;
  }
}

void closeLog() {
  if (!fsReady || logClosed) return;
  logFile.flush();
  logFile.close();
  logClosed = true;
  Serial.println("[FS] Log cerrado: " + logFileName);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("[PWR] WiFi apagado — recupera datos por Serial (DUMP)");
}

// --------------------------------------------------
//  SERIAL — non-blocking commands to retrieve data
// --------------------------------------------------
void handleSerialCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      serialBuffer.trim();
      if (serialBuffer.length() > 0) {
        if (serialBuffer == "LIST") {
          listFiles();
        } else if (serialBuffer == "DUMP") {
          listFiles();
          File root = LittleFS.open("/");
          File f = root.openNextFile();
          while (f) {
            dumpFileToSerial("/" + String(f.name()));
            f.close();
            f = root.openNextFile();
          }
        } else if (serialBuffer.startsWith("DUMP:")) {
          dumpFileToSerial("/" + serialBuffer.substring(5));
        } else if (serialBuffer == "FORMAT") {
          closeLog();
          LittleFS.format();
          Serial.println("[FS] Flash formateado. Reinicia para nuevo log.");
        }
      }
      serialBuffer = "";
    } else {
      serialBuffer += c;
    }
  }
}

void listFiles() {
  Serial.println("--- Archivos LittleFS ---");
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  while (f) {
    Serial.print("  /"); Serial.print(f.name());
    Serial.print("  "); Serial.print(f.size()); Serial.println(" bytes");
    f.close();
    f = root.openNextFile();
  }
  Serial.print("Usado: "); Serial.print(LittleFS.usedBytes());
  Serial.print(" / "); Serial.print(LittleFS.totalBytes()); Serial.println(" bytes");
  Serial.println("-------------------------");
}

void dumpFileToSerial(String path) {
  File f = LittleFS.open(path, FILE_READ);
  if (!f) { Serial.println("[FS] No encontrado: " + path); return; }
  Serial.println("=== " + path + " (" + String(f.size()) + " bytes) ===");
  while (f.available()) Serial.write(f.read());
  f.close();
  Serial.println("\n=== FIN ===");
}

// --------------------------------------------------
//  WEBSOCKET — responds to LIST command so Ground Station can see files
// --------------------------------------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type != WStype_TEXT) return;
  String cmd = String((char*)payload).substring(0, length);
  cmd.trim();

  if (cmd == "LIST") {
    String msg = "FILES:";
    File root = LittleFS.open("/");
    File f = root.openNextFile();
    while (f) {
      msg += String(f.name()) + ":" + String(f.size()) + ";";
      f.close();
      f = root.openNextFile();
    }
    webSocket.sendTXT(num, msg);
  }
}

// --------------------------------------------------
//  FLIGHT LOGIC
// --------------------------------------------------
void updateLogic() {
  switch (currentState) {

    case STANDBY:
      setCpuFrequencyMhz(80);
      if (gForce >= LIFTOFF_G) {
        liftoffCounter++;
        if (liftoffCounter >= LIFTOFF_CONFIRMS) {
          setCpuFrequencyMhz(240);
          currentState    = ASCENT;
          ascentStartTime = millis();
          maxAltitude     = currentAltitude;
          liftoffCounter  = 0;
          Serial.println("[EVENT] LIFTOFF CONFIRMED");
        }
      } else {
        liftoffCounter = 0;
      }
      break;

    case ASCENT:
      if (currentAltitude > maxAltitude) maxAltitude = currentAltitude;

      if (maxAltitude < MIN_ALTITUDE_LAUNCH) break;

      if (velocity <= APOGEE_VEL_THRESHOLD) {
        apogeeVelCounter++;
        if (apogeeVelCounter >= APOGEE_CONFIRMS) {
          Serial.println("[EVENT] APOGEE — velocity zero crossing");
          openParachute();
          break;
        }
      } else {
        apogeeVelCounter = 0;
      }

      if (currentAltitude < (maxAltitude - APOGEE_ALT_DROP)) {
        apogeeAltCounter++;
        if (apogeeAltCounter >= APOGEE_ALT_CONFIRMS) {
          Serial.println("[EVENT] APOGEE — altitude drop backup");
          openParachute();
          break;
        }
      } else {
        apogeeAltCounter = 0;
      }

      if (millis() - ascentStartTime > ASCENT_TIMEOUT_MS) {
        Serial.println("[SAFETY] TIMEOUT — forcing parachute");
        openParachute();
        break;
      }
      break;

    case DESCENT:
      if (currentAltitude < LANDING_ALT_M) {
        currentState = LANDING;
        closeLog();  // closes and flushes the file upon landing
        Serial.println("[EVENT] LANDING DETECTED");
      }
      break;

    case LANDING:
      break;
  }
}

void openParachute() {
  parachute.write(90);
  currentState     = DESCENT;
  apogeeVelCounter = 0;
  apogeeAltCounter = 0;
}

// --------------------------------------------------
//  SENSORS
// --------------------------------------------------
void readSensors() {
  unsigned long currentTime = millis();
  lastTime = currentTime;

  if (millis() - lastBmpRead >= bmpInterval) {
    float rawAlt = bmp.readAltitude(basePressure) - groundAltitude;

    filteredAltitude = filteredAltitude * 0.8f + rawAlt * 0.2f;

    if (abs(filteredAltitude - currentAltitude) < 100.0f) {
      currentAltitude = filteredAltitude;
    }

    float bmpDt = (currentTime - lastBmpRead) / 1000.0f;
    if (bmpDt > 0.005f) {
      float rawVel = (currentAltitude - lastAltitude) / bmpDt;
      velocity = velocity * 0.85f + rawVel * 0.15f;
    }

    // Cache temperature and pressure — avoids double I2C read in sendData/logData
    cachedTemp     = bmp.readTemperature();
    cachedPressHPa = bmp.readPressure() / 100.0f;

    lastAltitude = currentAltitude;
    lastBmpRead  = millis();
  }

  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);
  float ax = a.acceleration.x / 9.81f;
  float ay = a.acceleration.y / 9.81f;
  float az = a.acceleration.z / 9.81f;
  gForce = sqrt(ax*ax + ay*ay + az*az);
  pitch  = atan2(ax, sqrt(ay*ay + az*az)) * 180.0f / 3.14159f;
  roll   = atan2(ay, az) * 180.0f / 3.14159f;
}

// --------------------------------------------------
//  TELEMETRY — JSON via WebSocket (uses cached values)
// --------------------------------------------------
void sendData() {
  String json = "{";
  json += "\"t\":"        + String(millis())          + ",";
  json += "\"alt\":"      + String(currentAltitude, 1) + ",";
  json += "\"vel\":"      + String(velocity, 1)        + ",";
  json += "\"accel\":"    + String(gForce, 2)           + ",";
  json += "\"pitch\":"    + String(pitch, 1)            + ",";
  json += "\"roll\":"     + String(roll, 1)             + ",";
  json += "\"yaw\":"      + String(0.0f, 1)             + ",";
  json += "\"temp\":"     + String(cachedTemp, 1)       + ",";
  json += "\"pressure\":" + String(cachedPressHPa, 1)   + ",";
  json += "\"phase\":"    + String(currentState);
  json += "}";

  webSocket.broadcastTXT(json);
}
