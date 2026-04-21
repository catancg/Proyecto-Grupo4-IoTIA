#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

// 1. Configuraciones de Red y MQTT
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";
const char* topic = "cursoiot/well1/sensor";
WiFiClient espClient;
PubSubClient client(espClient);

// ------------------ Pines ------------------
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define FLOW_PIN 34
#define ONE_WIRE_BUS 4

// ------------------ Objetos sensores ------------------
Adafruit_BMP085   bmp;
Adafruit_MPU6050  mpu;
OneWire           oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// ------------------ Variables de sensores ------------------
float pressure_hPa        = 0.0;
float DS18B20TemperatureC = 0.0;
int   flowRaw             = 0;
float flowPercent         = 0.0;

float accelX = 0.0, accelY = 0.0, accelZ = 0.0;
float vibration_mms = 0.0;

// ------------------ Temporización ------------------
const uint32_t READ_INTERVAL_MS = 1000;
const uint32_t PUB_INTERVAL_MS  = 1000;
uint32_t lastReadMs = 0;
uint32_t lastPubMs  = 0;

// ------------------ Vibración simplificada ------------------
// Toma (ax, ay, az) en m/s² y devuelve un valor estilo mm/s.
// No es física real, imita lo que publicaría un transmisor industrial.
float computeVibration(float ax, float ay, float az) {
  float mag = sqrt(ax*ax + ay*ay + az*az);

  // Baseline automático: se aprende del propio sensor, sirve tanto en
  // Wokwi (sin gravedad) como en hardware real (con gravedad en un eje).
  static float baseline = -1.0f;
  if (baseline < 0) baseline = mag;
  baseline = baseline * 0.98f + mag * 0.02f;

  float deviation = fabs(mag - baseline);
  float base      = deviation * 30.0f;

  return mag ;
}

// ------------------ Sensores ------------------
void readSensors() {
  pressure_hPa = bmp.readPressure() / 100.0;
  flowRaw      = analogRead(FLOW_PIN);
  flowPercent  = (flowRaw / 4095.0) * 100.0;

  ds18b20.requestTemperatures();
  DS18B20TemperatureC = ds18b20.getTempCByIndex(0);

  // MPU6050
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);
  accelX = accel.acceleration.x;
  accelY = accel.acceleration.y;
  accelZ = accel.acceleration.z;
}

void printReadings() {
  Serial.println("===== Sensor Snapshot =====");
  Serial.print("BMP180 Pressure [hPa]: "); Serial.println(pressure_hPa, 2);

  Serial.print("DS18B20 Temp [C]:      ");
  if (isnan(DS18B20TemperatureC)) Serial.println("ERROR");
  else Serial.println(DS18B20TemperatureC, 2);

  Serial.print("Accel X [m/s^2]:       "); Serial.println(accelX, 3);
  Serial.print("Accel Y [m/s^2]:       "); Serial.println(accelY, 3);
  Serial.print("Accel Z [m/s^2]:       "); Serial.println(accelZ, 3);
  Serial.print("Vibration [mm/s]:      "); Serial.println(vibration_mms, 3);

  Serial.print("Flow Raw [0-4095]:     "); Serial.println(flowRaw);
  Serial.print("Flow [%]:              "); Serial.println(flowPercent, 2);
  Serial.println("===========================");
}

// ------------------ Setup ------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("Starting ESP32 well sensor initialization...");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  pinMode(FLOW_PIN, INPUT);

  if (!bmp.begin())  Serial.println("ERROR: BMP180 not found.");
  else               Serial.println("BMP180 initialized successfully.");

  if (!mpu.begin())  Serial.println("ERROR: MPU6050 not found.");
  else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_260_HZ);
    Serial.println("MPU6050 initialized successfully.");
  }

  ds18b20.begin();

  Serial.print("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" ¡Conectado!");

  client.setServer(mqtt_server, 1883);

  readSensors();
  printReadings();

  lastReadMs = millis();
  lastPubMs  = millis();
}

// ------------------ Loop ------------------
void loop() {
  // Reconexión MQTT
  if (!client.connected()) {
    while (!client.connected()) {
      Serial.print("Conectando a MQTT...");
      if (client.connect("ESP32Client_Wokwi_Unico")) { Serial.println(" ¡Conectado!"); }
      else { delay(5000); }
    }
  }
  client.loop();

  uint32_t now = millis();

  // Lectura de sensores cada 1 s
  if (now - lastReadMs >= READ_INTERVAL_MS) {
    lastReadMs = now;
    readSensors();
  }

  // Publicación cada 1 s
  if (now - lastPubMs >= PUB_INTERVAL_MS) {
    lastPubMs = now;

    vibration_mms = computeVibration(accelX, accelY, accelZ);

    String mensaje = "{";
    mensaje += "\"well_id\":\"WELL-01\",";
    mensaje += "\"pressure\":"    + String(pressure_hPa, 2)        + ",";
    mensaje += "\"temperature\":" + String(DS18B20TemperatureC, 2) + ",";
    mensaje += "\"vibration\":"   + String(vibration_mms, 3)       + ",";
    mensaje += "\"flow\":"        + String(flowPercent, 2);
    mensaje += "}";

    client.publish(topic, mensaje.c_str());
    printReadings();
    Serial.print("Dato enviado: "); Serial.println(mensaje);
  }
}