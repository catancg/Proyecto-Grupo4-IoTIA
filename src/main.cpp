#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>



// 1. Configuraciones de Red y MQTT
const char* ssid = "Wokwi-GUEST"; // Red WiFi simulada de Wokwi
const char* password = "";
const char* mqtt_server = "broker.hivemq.com"; // Nuestro Broker público y gratuito
const char* topic = "cursoiot/well1/sensor"; // ¡REEMPLAZA "tu_nombre"!
WiFiClient espClient;
PubSubClient client(espClient);
// -------------------------
// Pines
// -------------------------
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

#define DHT_PIN 4
#define DHT_TYPE DHT22

#define FLOW_PIN 34

#define ONE_WIRE_BUS 4  // GPIO donde conectaste DATA
// -------------------------
// Objetos de sensores
// -------------------------
Adafruit_BMP085 bmp;
Adafruit_MPU6050 mpu;
DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
// -------------------------
// Variables de lectura
// -------------------------
float pressure_hPa = 0.0;
float bmpTemperatureC = 0.0;
float dhtTemperatureC = 0.0;
float DS18B20TemperatureC = 0.0;
float humidity = 0.0;
float accelX = 0.0;
float accelY = 0.0;
float accelZ = 0.0;
int flowRaw = 0;
float flowPercent = 0.0;
unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL_MS = 1000;  // 1 segundo
// -------------------------
// Funciones auxiliares
// -------------------------
void readSensors() {
  // BMP180
  pressure_hPa = bmp.readPressure() / 100.0;   // Pa -> hPa

  // DHT22
  dhtTemperatureC = dht.readTemperature();

  // MPU6050
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  accelX = accel.acceleration.x;
  accelY = accel.acceleration.y;
  accelZ = accel.acceleration.z;

  // Potenciómetro como flow sensor
  flowRaw = analogRead(FLOW_PIN);              // 0 a 4095
  flowPercent = (flowRaw / 4095.0) * 100.0;    // porcentaje simple
  // Dallas Sensor temp
  
  ds18b20.requestTemperatures();
  DS18B20TemperatureC = ds18b20.getTempCByIndex(0);

}

void printReadings() {
  Serial.println("===== Initial Sensor Snapshot =====");

  Serial.print("BMP180 Pressure [hPa]: ");
  Serial.println(pressure_hPa, 2);


  Serial.print("DS18B20 Temperature [C]: ");
  if (isnan(DS18B20TemperatureC)) {
    Serial.println("ERROR");
  } else {
    Serial.println(DS18B20TemperatureC, 2);
  }


  Serial.print("MPU6050 Accel X [m/s^2]: ");
  Serial.println(accelX, 3);

  Serial.print("MPU6050 Accel Y [m/s^2]: ");
  Serial.println(accelY, 3);

  Serial.print("MPU6050 Accel Z [m/s^2]: ");
  Serial.println(accelZ, 3);

  Serial.print("Flow Raw [0-4095]: ");
  Serial.println(flowRaw);

  Serial.print("Flow [%]: ");
  Serial.println(flowPercent, 2);

  Serial.println("==================================");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("Starting ESP32 well sensor initialization...");

  // Inicialización I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // Configuración ADC del ESP32
  pinMode(FLOW_PIN, INPUT);

  // Inicialización DHT22
  dht.begin();

  // Inicialización BMP180
  if (!bmp.begin()) {
    Serial.println("ERROR: BMP180 not found. Check wiring.");
  } else {
    Serial.println("BMP180 initialized successfully.");
  }

  // Inicialización MPU6050
  if (!mpu.begin()) {
    Serial.println("ERROR: MPU6050 not found. Check wiring.");
  } else {
    Serial.println("MPU6050 initialized successfully.");
  }

  Serial.println("DHT22 initialized.");
  Serial.println("ADC flow input initialized.");
  Serial.println();
  ds18b20.begin();

  // 2. Conexión al WiFi
  Serial.print("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
  }
  Serial.println(" ¡Conectado!");
  // 3. Configuración del Broker
  client.setServer(mqtt_server, 1883);
  // Captura inicial
  readSensors();
  printReadings();
}


void loop() {
  unsigned long now = millis();

  if (now - lastReadTime >= READ_INTERVAL_MS) {
    lastReadTime = now;
    readSensors();
    printReadings();
  }

  // 4. Mantener la conexión MQTT viva
  if (!client.connected()) {
    while (!client.connected()) {
      Serial.print("Conectando a MQTT...");
      if (client.connect("ESP32Client_Wokwi_Unico")) { // Este nombre debe ser único
          Serial.println(" ¡Conectado!");
      } else {
          delay(5000);
      }
      }
  }
  client.loop();
  // ----------- Lectura de sensores -----------

// BMP180
//pressure_hPa = bmp.readPressure() / 100.0;

// DHT22
//dhtTemperatureC = dht.readTemperature();

// MPU6050
//sensors_event_t accel, gyro, temp;
//mpu.getEvent(&accel, &gyro, &temp);

//accelX = accel.acceleration.x;
//accelY = accel.acceleration.y;
//accelZ = accel.acceleration.z;

// Potenciómetro
//flowRaw = analogRead(FLOW_PIN);
//flowPercent = (flowRaw / 4095.0) * 100.0;


// ----------- Construcción JSON -----------

String mensaje = "{";
mensaje += "\"well_id\":\"WELL-01\",";
mensaje += "\"pressure\":" + String(pressure_hPa, 2) + ",";
mensaje += "\"temperature\":" + String(DS18B20TemperatureC, 2) + ",";
mensaje += "\"vibration\":{";
mensaje += "\"x\":" + String(accelX, 3) + ",";
mensaje += "\"y\":" + String(accelY, 3) + ",";
mensaje += "\"z\":" + String(accelZ, 3);
mensaje += "},";
mensaje += "\"flow\":" + String(flowPercent, 2);
mensaje += "}";

// ----------- Publish -----------

client.publish(topic, mensaje.c_str());

// Debug
Serial.println("Dato enviado:");
Serial.println(mensaje);

delay(2000);
}
