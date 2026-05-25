/**
 * Monitor de Humedad de Suelo - ESP32
 *
 * Sensores: 2× sensor capacitivo/resistivo de humedad de suelo
 * Display:  2× tira NeoPixel (rojo=seco → verde=húmedo)
 * Protocolo: MQTT sobre WiFi
 *
 * Topics publicados:
 *   campo/humedad1  → humedad sensor 1 (0–100 %)
 *   campo/humedad2  → humedad sensor 2 (0–100 %)
 *   campo/estado    → estado online/offline del dispositivo
 *
 * Topics suscritos:
 *   campo/activado  → "ON" = funcional | "OFF" = desactivado
 *                     Al desactivarse, las tiras se apagan y se
 *                     deja de publicar hasta recibir "ON".
 *
 * LED de estado (PIN_LED):
 *   Apagado          → Sin conexión WiFi
 *   Parpadeo rápido  → WiFi conectado, sin MQTT
 *   Encendido fijo   → WiFi + MQTT conectados
 */

// ─────────────────────────────────────────────
//  LIBRERÍAS
// ─────────────────────────────────────────────
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE PINES
// ─────────────────────────────────────────────
constexpr uint8_t  PIN_LED1     = 16;
constexpr uint8_t  PIN_LED2     = 17;
constexpr uint8_t  SOIL_PIN1    = 34;
constexpr uint8_t  SOIL_PIN2    = 35;
constexpr uint8_t  PIN_LED      = 2;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE TIRAS LED
// ─────────────────────────────────────────────
constexpr uint8_t  NUM_LEDS     = 14;
constexpr uint8_t  BRILLO       = 100;

// ─────────────────────────────────────────────
//  CALIBRACIÓN DE SENSORES (ADC 12 bits: 0–4095)
// ─────────────────────────────────────────────
constexpr int DRY_INIT_1  = 3200;
constexpr int WET_INIT_1  = 1500;
constexpr int DRY_INIT_2  = 3200;
constexpr int WET_INIT_2  = 1500;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN WIFI
// ─────────────────────────────────────────────
constexpr char     WIFI_SSID[]       = "Ohmbu-2.4G";
constexpr char     WIFI_PASS[]       = "Nene.peludo";
constexpr uint32_t WIFI_TIMEOUT_MS   = 10000;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN MQTT
// ─────────────────────────────────────────────
constexpr char     MQTT_BROKER[]     = "192.168.88.100";
constexpr uint16_t MQTT_PORT         = 1883;
constexpr char     MQTT_USER[]       = "";
constexpr char     MQTT_PASS_[]      = "";
constexpr char     MQTT_CLIENT_ID[]  = "esp32-humedad-campo";

constexpr char     TOPIC_HUM1[]      = "campo/humedad1";
constexpr char     TOPIC_HUM2[]      = "campo/humedad2";
constexpr char     TOPIC_ESTADO[]    = "campo/estado";
constexpr char     TOPIC_ACTIVACION[]= "/baliza/4/estado";  // "ON" / "OFF"

// ─────────────────────────────────────────────
//  INTERVALOS
// ─────────────────────────────────────────────
constexpr uint32_t INTERVALO_LEDS_MS      = 150;
constexpr uint32_t INTERVALO_MQTT_MS      = 5000;
constexpr uint32_t INTERVALO_RECONEX_MS   = 10000;
constexpr uint32_t INTERVALO_PARPADEO_MS  = 300;

// ─────────────────────────────────────────────
//  OBJETOS GLOBALES
// ─────────────────────────────────────────────
Adafruit_NeoPixel strip1(NUM_LEDS, PIN_LED1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_LEDS, PIN_LED2, NEO_GRB + NEO_KHZ800);
WiFiClient        wifiClient;
PubSubClient      mqttClient(wifiClient);

// ─────────────────────────────────────────────
//  ESTADO GLOBAL
// ─────────────────────────────────────────────
struct SensorHumedad {
  int rawValue   = 0;
  int dryValue;
  int wetValue;
  int porcentaje = 0;
  int ledCount   = 0;
};

SensorHumedad s1, s2;
bool     wifiConectado      = false;
bool     mqttConectado      = false;
bool     sistemaHabilitado  = true;   // Activo por defecto
uint32_t ultimaLed          = 0;
uint32_t ultimaMQTT         = 0;
uint32_t ultimaReconex      = 0;

// ─────────────────────────────────────────────
//  FUNCIONES: LED DE ESTADO
// ─────────────────────────────────────────────
void actualizarLED() {
  if (!wifiConectado) {
    digitalWrite(PIN_LED, LOW);
  } else if (!mqttConectado) {
    bool on = (millis() / INTERVALO_PARPADEO_MS) % 2 == 0;
    digitalWrite(PIN_LED, on ? HIGH : LOW);
  } else {
    digitalWrite(PIN_LED, HIGH);
  }
}

// ─────────────────────────────────────────────
//  FUNCIONES: WIFI
// ─────────────────────────────────────────────
bool conectarWifi() {
  if (WiFi.status() == WL_CONNECTED) { wifiConectado = true; return true; }

  Serial.printf("[WiFi] Conectando a %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < WIFI_TIMEOUT_MS) {
    delay(200);
    actualizarLED();
  }

  wifiConectado = (WiFi.status() == WL_CONNECTED);
  if (wifiConectado) {
    Serial.printf("[WiFi] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WiFi] Sin conexión. Se reintentará.");
  }
  return wifiConectado;
}

// ─────────────────────────────────────────────
//  FUNCIONES: MQTT
// ─────────────────────────────────────────────
void publicarInt(const char* topic, int valor) {
  if (!mqttConectado) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", valor);
  mqttClient.publish(topic, buf);
  Serial.printf("[MQTT] %s → %s\n", topic, buf);
}

void publicarDatos() {
  publicarInt(TOPIC_HUM1, s1.porcentaje);
  publicarInt(TOPIC_HUM2, s2.porcentaje);
}

void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  Serial.printf("[MQTT] %s → %s\n", topic, msg);

  if (strcmp(topic, TOPIC_ACTIVACION) == 0) {
    bool nuevoEstado = (strcmp(msg, "ON") == 0);
    if (nuevoEstado != sistemaHabilitado) {
      sistemaHabilitado = nuevoEstado;
      Serial.printf("[Sistema] %s\n", sistemaHabilitado ? "ACTIVADO" : "DESACTIVADO");
      if (!sistemaHabilitado) {
        // Al desactivar: apagar las tiras inmediatamente
        strip1.clear(); strip1.show();
        strip2.clear(); strip2.show();
      }
    }
  }
}

bool conectarMQTT() {
  if (!wifiConectado) return false;
  if (mqttClient.connected()) { mqttConectado = true; return true; }

  Serial.printf("[MQTT] Conectando a %s:%d...\n", MQTT_BROKER, MQTT_PORT);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(callbackMQTT);

  bool ok = mqttClient.connect(MQTT_CLIENT_ID);

  mqttConectado = ok;
  if (ok) {
    Serial.println("[MQTT] Conectado.");
    mqttClient.publish(TOPIC_ESTADO, "online", true);
    mqttClient.subscribe(TOPIC_ACTIVACION);
  } else {
    Serial.printf("[MQTT] Falló (código %d). Se reintentará.\n", mqttClient.state());
  }
  return ok;
}

// ─────────────────────────────────────────────
//  FUNCIONES: SENSORES
// ─────────────────────────────────────────────
void leerSensor(SensorHumedad& s, uint8_t pin) {
  s.rawValue = analogRead(pin);

  if (s.rawValue > s.dryValue) s.dryValue = s.rawValue;
  if (s.rawValue < s.wetValue) s.wetValue = s.rawValue;

  s.porcentaje = constrain(map(s.rawValue, s.dryValue, s.wetValue, 0, 100), 0, 100);
  s.ledCount   = constrain(map(s.porcentaje, 0, 100, 0, NUM_LEDS), 0, NUM_LEDS);
}

void leerSensores() {
  leerSensor(s1, SOIL_PIN1);
  leerSensor(s2, SOIL_PIN2);

  Serial.printf(
    "[S1] raw=%4d  hum=%3d%%  leds=%2d  (dry=%4d wet=%4d) | "
    "[S2] raw=%4d  hum=%3d%%  leds=%2d  (dry=%4d wet=%4d)\n",
    s1.rawValue, s1.porcentaje, s1.ledCount, s1.dryValue, s1.wetValue,
    s2.rawValue, s2.porcentaje, s2.ledCount, s2.dryValue, s2.wetValue
  );
}

// ─────────────────────────────────────────────
//  FUNCIONES: TIRAS NEOPIXEL
// ─────────────────────────────────────────────
uint32_t colorPixel(Adafruit_NeoPixel& strip, int index, int ledCount) {
  if (index >= ledCount) return 0;
  uint8_t r = map(index, 0, NUM_LEDS - 1, 255, 0);
  uint8_t g = map(index, 0, NUM_LEDS - 1, 0,   255);
  return strip.Color(r, g, 0);
}

void actualizarTiras() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip1.setPixelColor(i, colorPixel(strip1, i, s1.ledCount));
    strip2.setPixelColor(i, colorPixel(strip2, i, s2.ledCount));
  }
  strip1.show();
  strip2.show();
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Sistema] Iniciando monitor de humedad ESP32...");

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  s1.dryValue = DRY_INIT_1;  s1.wetValue = WET_INIT_1;
  s2.dryValue = DRY_INIT_2;  s2.wetValue = WET_INIT_2;

  strip1.begin(); strip1.setBrightness(BRILLO); strip1.show();
  strip2.begin(); strip2.setBrightness(BRILLO); strip2.show();

  conectarWifi();
  if (wifiConectado) conectarMQTT();

  actualizarLED();
  Serial.println("[Sistema] Setup completo.");
}

// ─────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────
void loop() {
  uint32_t ahora = millis();

  // ── Mantener MQTT activo ──────────────────
  if (mqttConectado) {
    if (!mqttClient.loop()) {
      mqttConectado     = false;
      sistemaHabilitado = true;   // Failsafe: sin MQTT, siempre funcional
      Serial.println("[MQTT] Conexión perdida.");
    }
  }

  // ── Reintentar conexiones ─────────────────
  if (ahora - ultimaReconex >= INTERVALO_RECONEX_MS) {
    ultimaReconex = ahora;
    if (!wifiConectado || WiFi.status() != WL_CONNECTED) {
      wifiConectado     = false;
      mqttConectado     = false;
      sistemaHabilitado = true;   // Failsafe
      conectarWifi();
    }
    if (wifiConectado && !mqttConectado) conectarMQTT();
  }

  // ── Leer sensores y actualizar tiras ──────
  if (ahora - ultimaLed >= INTERVALO_LEDS_MS) {
    ultimaLed = ahora;
    if (sistemaHabilitado) {
      leerSensores();
      actualizarTiras();
    } else {
      // Mantener las tiras apagadas mientras está desactivado
      strip1.clear(); strip1.show();
      strip2.clear(); strip2.show();
    }
  }

  // ── Publicar por MQTT ─────────────────────
  if (sistemaHabilitado && mqttConectado && ahora - ultimaMQTT >= INTERVALO_MQTT_MS) {
    ultimaMQTT = ahora;
    publicarDatos();
  }

  // ── LED de estado ─────────────────────────
  actualizarLED();
}
