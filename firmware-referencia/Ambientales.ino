/**
 * Estación de Sensores Ambientales - ESP32
 *
 * Sensores: DHT22 (temp/humedad), BMP085 (presión), MQ135 (CO2)
 * Display:  LCD I2C 20x4
 * Protocolo: MQTT sobre WiFi
 *
 * Topics publicados:
 *   ambientales/temp     → temperatura en °C
 *   ambientales/humedad  → humedad relativa en %
 *   ambientales/co2      → nivel de CO2 estimado en %
 *   ambientales/presion  → presión atmosférica en hPa
 *   ambientales/estado   → estado de conexión del dispositivo
 *
 * Topics suscritos:
 *   ambientales/activado → "ON" = funcional | "OFF" = desactivado
 *
 * LED de estado (PIN_LED):
 *   Apagado          → Sin conexión WiFi
 *   Parpadeo rápido  → WiFi conectado, sin MQTT
 *   Encendido fijo   → WiFi + MQTT conectados y sistema activo
 */

// ─────────────────────────────────────────────
//  LIBRERÍAS
// ─────────────────────────────────────────────
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE PINES
// ─────────────────────────────────────────────
constexpr uint8_t  DHT_PIN        = 4;
constexpr uint8_t  MQ135_PIN      = 34;
constexpr uint8_t  LCD_SDA_PIN    = 21;
constexpr uint8_t  LCD_SCL_PIN    = 22;
constexpr uint8_t  PIN_LED        = 2;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE SENSORES Y PANTALLA
// ─────────────────────────────────────────────
constexpr uint8_t  DHT_TYPE       = DHT22;
constexpr uint8_t  LCD_ADDR       = 0x27;
constexpr uint8_t  LCD_COLS       = 20;
constexpr uint8_t  LCD_ROWS       = 4;

constexpr int      CO2_MIN        = 5;    // Valor mínimo del CO2 simulado (%)
constexpr int      CO2_MAX        = 45;   // Valor máximo del CO2 simulado (%)

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
constexpr char     MQTT_PASS[]       = "";
constexpr char     MQTT_CLIENT_ID[]  = "esp32-estacion-ambiental";

constexpr char     TOPIC_TEMP[]      = "ambientales/temp";
constexpr char     TOPIC_HUMEDAD[]   = "ambientales/humedad";
constexpr char     TOPIC_CO2[]       = "ambientales/co2";
constexpr char     TOPIC_PRESION[]   = "ambientales/presion";
constexpr char     TOPIC_ESTADO[]    = "ambientales/estado";
constexpr char     TOPIC_ACTIVACION[]= "/baliza/7/estado";  // "ON" / "OFF"

// ─────────────────────────────────────────────
//  INTERVALOS DE TIEMPO
// ─────────────────────────────────────────────
constexpr uint32_t INTERVALO_LECTURA_MS      = 2000;
constexpr uint32_t INTERVALO_CO2_MS          = 10000; // Actualización del CO2 simulado
constexpr uint32_t INTERVALO_MQTT_MS         = 5000;
constexpr uint32_t INTERVALO_RECONEX_MS      = 10000;
constexpr uint32_t INTERVALO_PARPADEO_MS     = 300;

// ─────────────────────────────────────────────
//  OBJETOS GLOBALES
// ─────────────────────────────────────────────
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
DHT               dht(DHT_PIN, DHT_TYPE);
Adafruit_BMP085   bmp;
WiFiClient        wifiClient;
PubSubClient      mqttClient(wifiClient);

// ─────────────────────────────────────────────
//  ESTADO GLOBAL
// ─────────────────────────────────────────────
struct SensorData {
  float temperatura = NAN;
  float humedad     = NAN;
  int   co2         = -1;
  float presion     = NAN;
  bool  bmpOk       = false;
};

SensorData datos;
bool       wifiConectado      = false;
bool       mqttConectado      = false;
bool       sistemaHabilitado  = true;
uint32_t   ultimaLectura      = 0;
uint32_t   ultimaPublicacion  = 0;
uint32_t   ultimaReconexion   = 0;
uint32_t   ultimoCO2          = 0;

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
void publicarFloat(const char* topic, float valor, uint8_t decimales = 2) {
  if (!mqttConectado || !sistemaHabilitado) return;
  char buffer[16];
  dtostrf(valor, 1, decimales, buffer);
  mqttClient.publish(topic, buffer);
  Serial.printf("[MQTT] %s → %s\n", topic, buffer);
}

void publicarInt(const char* topic, int valor) {
  if (!mqttConectado || !sistemaHabilitado) return;
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%d", valor);
  mqttClient.publish(topic, buffer);
  Serial.printf("[MQTT] %s → %s\n", topic, buffer);
}

void publicarDatos() {
  if (!isnan(datos.temperatura)) publicarFloat(TOPIC_TEMP,    datos.temperatura, 1);
  if (!isnan(datos.humedad))     publicarFloat(TOPIC_HUMEDAD, datos.humedad,     1);
  if (datos.co2 >= 0)            publicarInt  (TOPIC_CO2,     datos.co2);
  if (datos.bmpOk)               publicarFloat(TOPIC_PRESION, datos.presion / 100.0f, 2);
}

/**
 * Callback MQTT.
 * Gestiona el topic de activación; ignora cualquier otro mensaje
 * ya que este prototipo solo publica datos y no recibe comandos de control.
 */
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
    Serial.println("[MQTT] Conectado al broker.");
    mqttClient.publish(TOPIC_ESTADO, "online", true);
    mqttClient.subscribe(TOPIC_ACTIVACION);
  } else {
    Serial.printf("[MQTT] Falló. Código: %d. Se reintentará.\n", mqttClient.state());
  }
  return ok;
}

// ─────────────────────────────────────────────
//  FUNCIONES: SENSORES
// ─────────────────────────────────────────────
void leerSensores() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(h) && !isnan(t)) {
    datos.humedad     = h;
    datos.temperatura = t;
  } else {
    Serial.println("[Sensor] Error leyendo DHT22.");
    datos.humedad     = NAN;
    datos.temperatura = NAN;
  }

  // CO2 simulado: valor aleatorio entre CO2_MIN y CO2_MAX, se actualiza cada INTERVALO_CO2_MS
  if (datos.co2 < 0 || millis() - ultimoCO2 >= INTERVALO_CO2_MS) {
    datos.co2  = random(CO2_MIN, CO2_MAX + 1);
    ultimoCO2  = millis();
  }

  if (datos.bmpOk) {
    datos.presion = bmp.readPressure();
  }
}

// ─────────────────────────────────────────────
//  FUNCIONES: PANTALLA LCD
// ─────────────────────────────────────────────
void mostrarEnLCD() {
  lcd.clear();

  lcd.setCursor(0, 0);
  if (!isnan(datos.temperatura)) {
    lcd.print("Temp: "); lcd.print(datos.temperatura, 1); lcd.print(" \xDFC");
  } else {
    lcd.print("Temp: ERROR");
  }

  lcd.setCursor(0, 1);
  if (!isnan(datos.humedad)) {
    lcd.print("Hum:  "); lcd.print(datos.humedad, 1); lcd.print(" %");
  } else {
    lcd.print("Hum:  ERROR");
  }

  lcd.setCursor(0, 2);
  lcd.print("CO2:  "); lcd.print(datos.co2); lcd.print(" %");

  lcd.setCursor(0, 3);
  if (datos.bmpOk && !isnan(datos.presion)) {
    lcd.print("P:"); lcd.print(datos.presion / 100.0f, 1); lcd.print("hPa ");
  } else {
    lcd.print("P: N/A      ");
  }

  // lcd.setCursor(16, 3);
  // if (mqttConectado)    lcd.print("MQTT");
  // else if (wifiConectado) lcd.print("WiFi");
  // else                  lcd.print("----");
}

void mostrarDesactivado() {
  lcd.clear();
  lcd.setCursor(3, 1);
  lcd.print("** DESACTIVADO **");
  lcd.setCursor(16, 3);
  lcd.print("MQTT");
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Sistema] Iniciando estación ambiental ESP32...");

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Estacion Ambiental");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  dht.begin();
  randomSeed(esp_random());  // Semilla de hardware para el CO2 simulado

  datos.bmpOk = bmp.begin();
  if (!datos.bmpOk) {
    Serial.println("[Sensor] BMP085 no encontrado.");
    lcd.setCursor(0, 2);
    lcd.print("BMP085: ERROR");
  }

  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi...");
  conectarWifi();

  if (wifiConectado) {
    lcd.setCursor(0, 1);
    lcd.print("Conectando MQTT...");
    conectarMQTT();
  }

  actualizarLED();
  delay(1000);
  lcd.clear();
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
      mqttConectado        = false;
      sistemaHabilitado    = true;  // Failsafe: sin MQTT, siempre funcional
      Serial.println("[MQTT] Conexión perdida.");
    }
  }

  // ── Reintentar conexiones ─────────────────
  if (ahora - ultimaReconexion >= INTERVALO_RECONEX_MS) {
    ultimaReconexion = ahora;
    if (!wifiConectado || WiFi.status() != WL_CONNECTED) {
      wifiConectado = false;
      mqttConectado = false;
      sistemaHabilitado = true;  // Failsafe
      conectarWifi();
    }
    if (wifiConectado && !mqttConectado) conectarMQTT();
  }

  // ── Leer sensores y actualizar LCD ────────
  if (ahora - ultimaLectura >= INTERVALO_LECTURA_MS) {
    ultimaLectura = ahora;
    if (sistemaHabilitado) {
      leerSensores();
      mostrarEnLCD();
    } else {
      mostrarDesactivado();
    }
  }

  // ── Publicar por MQTT ─────────────────────
  if (sistemaHabilitado && mqttConectado && ahora - ultimaPublicacion >= INTERVALO_MQTT_MS) {
    ultimaPublicacion = ahora;
    publicarDatos();
  }

  // ── LED de estado ─────────────────────────
  actualizarLED();
}
