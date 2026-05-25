/**
 * Vumetro - ESP32
 *
 * Micrófono analógico → tira NeoPixel de 8 LEDs (verde→rojo)
 * Protocolo: MQTT sobre WiFi
 *
 * Topics publicados:
 *   vumetro/nivel    → porcentaje de ruido (0–100)
 *   vumetro/estado   → "online" / "offline" (Last Will)
 *
 * Topics suscritos:
 *   vumetro/activado → "ON" = funcional | "OFF" = desactivado
 *                      Al desactivarse, la tira se apaga y se
 *                      deja de publicar hasta recibir "ON".
 *
 * LED de estado (PIN_LED_STATUS):
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
constexpr uint8_t  PIN_NEOPIXEL   = 16;   // GPIO tira NeoPixel
constexpr uint8_t  MIC_PIN        = 34;   // ADC solo-lectura, micrófono
constexpr uint8_t  PIN_LED_STATUS = 2;    // LED de estado (integrado en muchos ESP32)

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE TIRA Y MICRÓFONO
// ─────────────────────────────────────────────
constexpr uint8_t  NUM_LEDS   = 9;
constexpr uint8_t  BRILLO     = 128;    // 50% de brillo (0–255)

// Valor máximo del ADC por encima del cual se considera nivel máximo.
// El ADC del ESP32 es de 12 bits (0–4095).
// Ajustar con prueba real según el micrófono y el entorno.
constexpr int      MIC_MIN    = 0;
constexpr int      MIC_MAX    = 2100;   // ≈ equivalente al 515 original en 10 bits

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
constexpr char     MQTT_CLIENT_ID[]  = "esp32-vumetro";

constexpr char     TOPIC_NIVEL[]     = "vumetro/nivel";
constexpr char     TOPIC_ESTADO[]    = "vumetro/estado";
constexpr char     TOPIC_ACTIVACION[]= "/baliza/7/estado";  // "ON" / "OFF"

// ─────────────────────────────────────────────
//  INTERVALOS
// ─────────────────────────────────────────────
constexpr uint32_t INTERVALO_LEDS_MS      = 50;     // Actualización visual (~20 fps)
constexpr uint32_t INTERVALO_MQTT_MS      = 200;    // Publicación MQTT (5 veces/seg)
constexpr uint32_t INTERVALO_RECONEX_MS   = 10000;  // Reintento de conexión
constexpr uint32_t INTERVALO_PARPADEO_MS  = 300;    // LED estado: WiFi sin MQTT

// ─────────────────────────────────────────────
//  OBJETOS GLOBALES
// ─────────────────────────────────────────────
Adafruit_NeoPixel leds(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
WiFiClient        wifiClient;
PubSubClient      mqttClient(wifiClient);

// ─────────────────────────────────────────────
//  ESTADO GLOBAL
// ─────────────────────────────────────────────
int      nivelActual        = 0;
int      porcentajeActual   = 0;  // 0–100, publicado por MQTT
bool     wifiConectado      = false;
bool     mqttConectado      = false;
bool     sistemaHabilitado  = true;   // Activo por defecto
uint32_t ultimaLed          = 0;
uint32_t ultimaMQTT         = 0;
uint32_t ultimaReconex      = 0;

// ─────────────────────────────────────────────
//  FUNCIONES: LED DE ESTADO
// ─────────────────────────────────────────────

/**
 * Actualiza el LED físico de estado según conectividad:
 *   Sin WiFi      → apagado
 *   WiFi sin MQTT → parpadeo rápido (300ms)
 *   WiFi + MQTT   → encendido fijo
 */
void actualizarLEDEstado() {
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
    actualizarLEDEstado();
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
        // Apagar la tira inmediatamente al desactivarse
        leds.clear();
        leds.show();
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
//  FUNCIONES: VUMETRO
// ─────────────────────────────────────────────

/**
 * Genera el color de un pixel según su posición en la tira.
 * Gradiente verde (bajo) → rojo (alto).
 */
uint32_t getColorGradient(int index, int total) {
  float ratio = (float)index / (float)(total - 1);
  uint8_t red   = (uint8_t)(ratio * 255);
  uint8_t green = (uint8_t)((1.0f - ratio) * 255);
  return leds.Color(red, green, 0);
}

/**
 * Lee el micrófono, calcula el nivel visual (0–NUM_LEDS) y el
 * porcentaje de ruido (0–100) que se publica por MQTT.
 * Si el valor supera MIC_MAX o es 0, se fuerza nivel máximo.
 */
int leerMicrofono() {
  int micValue = analogRead(MIC_PIN);

  int level;
  if (micValue >= MIC_MAX || micValue == 0) {
    level            = NUM_LEDS;
    porcentajeActual = 100;
  } else {
    level            = map(micValue, MIC_MIN, MIC_MAX, 0, NUM_LEDS);
    porcentajeActual = map(micValue, MIC_MIN, MIC_MAX, 0, 100);
  }
  level            = constrain(level,            0, NUM_LEDS);
  porcentajeActual = constrain(porcentajeActual, 0, 100);
  return level;
}

/**
 * Actualiza la tira NeoPixel con el nivel actual.
 */
void actualizarTira(int level) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds.setPixelColor(i, i < level ? getColorGradient(i, NUM_LEDS) : 0);
  }
  leds.show();
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Sistema] Iniciando vumetro ESP32...");

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, LOW);

  leds.begin();
  leds.setBrightness(BRILLO);
  leds.clear();
  leds.show();

  conectarWifi();
  if (wifiConectado) conectarMQTT();

  actualizarLEDEstado();
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

  // ── Actualizar tira NeoPixel ──────────────
  if (ahora - ultimaLed >= INTERVALO_LEDS_MS) {
    ultimaLed = ahora;
    if (sistemaHabilitado) {
      nivelActual = leerMicrofono();
      actualizarTira(nivelActual);
      Serial.println(nivelActual);
    } else {
      leds.clear();
      leds.show();
    }
  }

  // ── Publicar porcentaje de ruido por MQTT ─
  if (sistemaHabilitado && mqttConectado && ahora - ultimaMQTT >= INTERVALO_MQTT_MS) {
    ultimaMQTT = ahora;
    char buf[5];
    snprintf(buf, sizeof(buf), "%d", porcentajeActual);
    mqttClient.publish(TOPIC_NIVEL, buf);
  }

  // ── LED de estado ─────────────────────────
  actualizarLEDEstado();
}
