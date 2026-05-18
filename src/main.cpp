#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid        = "Wokwi-GUEST";
const char* password    = "";
const char* mqtt_server = "broker.hivemq.com";
const char* topic       = "cursoiot/well1/sensor";
const char* WELL_TOKEN = "well01-secret-2024";  // debe coincidir con env WELL_TOKEN en Node-RED
WiFiClient    espClient;
PubSubClient  client(espClient);

const uint32_t PUB_INTERVAL_MS = 1000;
uint32_t lastPubMs = 0;
uint32_t t = 0;

// Hardware RNG → float in [0.0, 1.0)
float randF() {
  return (float)(esp_random() & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.print("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" ¡Conectado!");

  client.setServer(mqtt_server, 1883);
  lastPubMs = millis();
}

void loop() {
  if (!client.connected()) {
    while (!client.connected()) {
      Serial.print("Conectando a MQTT...");
      if (client.connect("ESP32Client_Wokwi_Unico")) Serial.println(" ¡Conectado!");
      else delay(5000);
    }
  }
  client.loop();

  uint32_t now = millis();
  if (now - lastPubMs < PUB_INTERVAL_MS) return;
  lastPubMs = now;

  t++;
  uint32_t phase     = t % 70;
  bool     isAnomaly = (phase >= 20) && (phase < 40);

  float pressure, temperature, floww;
  int   vibration;

  if (!isAnomaly) {
    pressure    = 1000.0f + randF() * 50.0f;
    temperature =   80.0f + randF() * 10.0f;
    vibration   = (int)(2.0f + randF() * 2.0f);
    floww       =   50.0f + randF() * 10.0f;
  } else {
    pressure    = 690.0f + randF() * 40.0f;
    temperature = 100.0f + randF() * 15.0f;
    vibration   = (int)(8.0f + randF() * 4.0f);
    floww       =  15.0f + randF() * 15.0f;
  }

  Serial.println("===== Datos a publicar =====");
  Serial.printf("Tick: %lu | Anomalia: %s\n", t, isAnomaly ? "SI" : "NO");
  Serial.printf("Pressure    [hPa]: %.2f\n", pressure);
  Serial.printf("Temperature  [°C]: %.2f\n", temperature);
  Serial.printf("Vibration        : %d\n",   vibration);
  Serial.printf("Flow        [%%]  : %.2f\n", floww);
  Serial.println("============================");

  char payload[300];
  snprintf(payload, sizeof(payload),
           "{\"well_id\":\"WELL-01\",\"token\":\"%s\",\"pressure\":%.2f,\"temperature\":%.2f,\"vibration\":%d,\"floww\":%.2f}",
           WELL_TOKEN, pressure, temperature, vibration, floww);

  client.publish(topic, payload);
  Serial.print("Dato enviado: "); Serial.println(payload);
}
