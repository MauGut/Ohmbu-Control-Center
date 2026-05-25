/**
 * Anemómetro - ESP32
 *
 * Sensor: generador/motor analógico → voltaje proporcional al viento
 * Display: LCD I2C 16x2
 * Protocolo: MQTT sobre WiFi
 *
 * Topics publicados:
 *   anemometro/velocidad → velocidad del viento en km/h
 *   anemometro/estado    → "online" / "offline" (Last Will)
 *
 * Topics suscritos:
 *   anemometro/activado  → "ON" = funcional | "OFF" = desactivado
 *
 * LED de estado (PIN_LED_STATUS):
 *   Apagado          → Sin conexión WiFi
 *   Parpadeo rápido  → WiFi conectado, sin MQTT
 *   Encendido fijo   → WiFi + MQTT conectados
 *
 * Nota: el ADC del ESP32 es de 3.3V y 12 bits (0–4095).
 * Ajustar VOLTAJE_MAX con una prueba real del sensor.
 */

// ─────────────────────────────────────────────
//  LIBRERÍAS
// ─────────────────────────────────────────────
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE PINES
// ─────────────────────────────────────────────
constexpr uint8_t  SENSOR_PIN      = 34;   // ADC solo-lectura
constexpr uint8_t  LCD_SDA_PIN     = 21;
constexpr uint8_t  LCD_SCL_PIN     = 22;
constexpr uint8_t  PIN_LED_STATUS  = 2;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DEL SENSOR Y PANTALLA
// ─────────────────────────────────────────────
constexpr uint8_t  LCD_ADDR        = 0x27;
constexpr uint8_t  LCD_COLS        = 16;
constexpr uint8_t  LCD_ROWS        = 2;

// ADC del ESP32: referencia 3.3V, 12 bits (0–4095)
constexpr float    VOLTAJE_REF     = 3.3f;
constexpr float    VOLTAJE_MAX     = 0.2f;   // ⚠ Calibrar con prueba real
constexpr float    VELOCIDAD_MAX   = 50.0f;  // km/h correspondientes a VOLTAJE_MAX

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
constexpr char     MQTT_CLIENT_ID[]  = "esp32-anemometro";

constexpr char     TOPIC_VELOCIDAD[] = "anemometro/velocidad";
constexpr char     TOPIC_ESTADO[]    = "anemometro/estado";
constexpr char     TOPIC_ACTIVACION[]= "/baliza/6/estado";  // "ON" / "OFF"

// ─────────────────────────────────────────────
//  INTERVALOS
// ─────────────────────────────────────────────
constexpr uint32_t INTERVALO_LCD_MS       = 700;    // Actualización pantalla
constexpr uint32_t INTERVALO_MQTT_MS      = 1000;   // Publicación MQTT
constexpr uint32_t INTERVALO_RECONEX_MS   = 10000;
constexpr uint32_t INTERVALO_PARPADEO_MS  = 300;

// ─────────────────────────────────────────────
//  OBJETOS GLOBALES
// ─────────────────────────────────────────────
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
WiFiClient        wifiClient;
PubSubClient      mqttClient(wifiClient);

// ─────────────────────────────────────────────
//  ESTADO GLOBAL
// ─────────────────────────────────────────────
float    velocidadActual    = 0.0f;
bool     wifiConectado      = false;
bool     mqttConectado      = false;
bool     sistemaHabilitado  = true;
uint32_t ultimaLCD          = 0;
uint32_t ultimaMQTT         = 0;
uint32_t ultimaReconex      = 0;

// ─────────────────────────────────────────────
//  FUNCIONES: LED DE ESTADO
// ─────────────────────────────────────────────
void actualizarLED() {
  if (!wifiConectado) {
    digitalWrite(PIN_LED_STATUS, LOW);
  } else if (!mqttConectado) {
    bool on = (millis() / INTERVALO_PARPADEO_MS) % 2 == 0;
    digitalWrite(PIN_LED_STATUS, on ? HIGH : LOW);
  } else {
    digitalWrite(PIN_LED_STATUS, HIGH);
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
  if (wifiConectado) Serial.printf("[WiFi] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());
  else               Serial.println("[WiFi] Sin conexión. Se reintentará.");
  return wifiConectado;
}

// ─────────────────────────────────────────────
//  FUNCIONES: MQTT
// ─────────────────────────────────────────────
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
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("** DESACTIVADO **");
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

  bool ok = mqttClient.connect(MQTT_CLIENT_ID, TOPIC_ESTADO, 0, true, "offline");

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
//  FUNCIONES: SENSOR
// ─────────────────────────────────────────────

/**
 * Función map para valores float (igual que map() pero con decimales).
 */
float mapFloat(float x, float inMin, float inMax, float outMin, float outMax) {
  return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

/**
 * Lee el sensor y devuelve la velocidad del viento en km/h.
 * ADC 12 bits (0–4095), referencia 3.3V.
 */
float leerVelocidad() {
  int lectura  = analogRead(SENSOR_PIN);
  float voltaje = lectura * (VOLTAJE_REF / 4095.0f);
  float vel     = mapFloat(voltaje, 0.0f, VOLTAJE_MAX, 0.0f, VELOCIDAD_MAX);
  return max(vel, 0.0f);
}

// ─────────────────────────────────────────────
//  FUNCIONES: PANTALLA LCD
// ─────────────────────────────────────────────
void mostrarVelocidad(float velocidad) {
  lcd.setCursor(0, 0);
  lcd.print("Vel. del viento ");

  lcd.setCursor(0, 1);
  lcd.print(" >>> ");
  lcd.print(velocidad, 1);
  lcd.print(" km/h   ");
}

void mostrarDesactivado() {
  lcd.setCursor(0, 0);
  lcd.print("** DESACTIVADO **");
  lcd.setCursor(0, 1);
  lcd.print("                ");
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Sistema] Iniciando anemómetro ESP32...");

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, LOW);

  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(3, 0);
  lcd.print("Iniciando");
  lcd.setCursor(3, 1);
  lcd.print("Anemometro");

  delay(1500);
  lcd.clear();

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
      sistemaHabilitado = true;
      Serial.println("[MQTT] Conexión perdida.");
    }
  }

  // ── Reintentar conexiones ─────────────────
  if (ahora - ultimaReconex >= INTERVALO_RECONEX_MS) {
    ultimaReconex = ahora;
    if (!wifiConectado || WiFi.status() != WL_CONNECTED) {
      wifiConectado = false; mqttConectado = false; sistemaHabilitado = true;
      conectarWifi();
    }
    if (wifiConectado && !mqttConectado) conectarMQTT();
  }

  // ── Leer sensor y actualizar LCD ─────────
  if (ahora - ultimaLCD >= INTERVALO_LCD_MS) {
    ultimaLCD = ahora;
    if (sistemaHabilitado) {
      velocidadActual = leerVelocidad();
      mostrarVelocidad(velocidadActual);
      Serial.printf("[Sensor] %.1f km/h\n", velocidadActual);
    } else {
      mostrarDesactivado();
    }
  }

  // ── Publicar por MQTT ─────────────────────
  if (sistemaHabilitado && mqttConectado && ahora - ultimaMQTT >= INTERVALO_MQTT_MS) {
    ultimaMQTT = ahora;
    char buf[8];
    dtostrf(velocidadActual, 1, 1, buf);
    mqttClient.publish(TOPIC_VELOCIDAD, buf);
    Serial.printf("[MQTT] %s → %s\n", TOPIC_VELOCIDAD, buf);
  }

  // ── LED de estado ─────────────────────────
  actualizarLED();
}
