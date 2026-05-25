/**
 * Velocímetro de Bicicleta - ESP32
 *
 * Encoder rotativo → velocidad en RPM → brillo de LEDs
 * Protocolo: MQTT sobre WiFi
 *
 * Topics publicados:
 *   bici/rpm     → RPM actuales (float con 1 decimal)
 *   bici/estado  → "online" / "offline" (Last Will)
 *
 * Topics suscritos:
 *   bici/activado → "ON" = funcional | "OFF" = desactivado
 *                   Al desactivarse, todos los LEDs se apagan.
 *
 * LED de estado (PIN_LED_STATUS):
 *   Apagado          → Sin conexión WiFi
 *   Parpadeo rápido  → WiFi conectado, sin MQTT
 *   Encendido fijo   → WiFi + MQTT conectados
 *
 * Calibración:
 *   PULSOS_POR_VUELTA → pasos del encoder por revolución de la rueda.
 *   Depende del encoder y de la transmisión mecánica. Ajustar con
 *   prueba real (contar pulsos en una vuelta completa de la rueda).
 */

// ─────────────────────────────────────────────
//  LIBRERÍAS
// ─────────────────────────────────────────────
#include <RotaryEncoder.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE PINES
// ─────────────────────────────────────────────
constexpr uint8_t  ENCODER_CLK    = 4;
constexpr uint8_t  ENCODER_DT     = 5;
constexpr uint8_t  PIN_LED_STATUS = 2;   // LED de estado de conexión

// Pines de los LEDs de velocidad (PWM vía LEDC)
constexpr uint8_t  NUM_LEDS       = 5;
constexpr uint8_t  LED_PINS[NUM_LEDS] = {16, 17, 18, 19, 21};

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE LEDC (PWM ESP32)
// ─────────────────────────────────────────────
constexpr uint32_t LEDC_FREQ_HZ   = 5000;
constexpr uint8_t  LEDC_RES_BITS  = 8;     // Resolución 8 bits → duty 0–255
// ESP32 Arduino core 3.x: ledcAttach(pin, freq, res) — sin gestión manual de canales

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DEL ENCODER
// ─────────────────────────────────────────────
// Pasos del encoder por revolución completa de la rueda.
// Ajustar con prueba real: hacer girar la rueda una vez y
// contar cuántos pasos registra encoder->getPosition().
constexpr int      PULSOS_POR_VUELTA   = 20;
constexpr uint32_t INTERVALO_MUESTREO_MS = 150;  // Ventana de medición

// Rango de ticks por intervalo para mapear a brillo (0–255)
// Ajustar según la velocidad máxima esperada.
constexpr int      TICKS_MIN_BRILLO  = 2;   // Por debajo: LEDs apagados
constexpr int      TICKS_MAX_BRILLO  = 80;  // Desde aquí: brillo máximo

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
constexpr char     MQTT_CLIENT_ID[]  = "esp32-bici";

constexpr char     TOPIC_RPM[]       = "bici/rpm";
constexpr char     TOPIC_ESTADO[]    = "bici/estado";
constexpr char     TOPIC_ACTIVACION[]= "bici/activado";  // "ON" / "OFF"

// ─────────────────────────────────────────────
//  INTERVALOS
// ─────────────────────────────────────────────
constexpr uint32_t INTERVALO_MQTT_MS     = 500;   // Publicación MQTT
constexpr uint32_t INTERVALO_RECONEX_MS  = 10000;
constexpr uint32_t INTERVALO_PARPADEO_MS = 300;

// ─────────────────────────────────────────────
//  OBJETOS GLOBALES
// ─────────────────────────────────────────────
RotaryEncoder*  encoder = nullptr;
WiFiClient      wifiClient;
PubSubClient    mqttClient(wifiClient);

// ─────────────────────────────────────────────
//  ESTADO GLOBAL
// ─────────────────────────────────────────────
float    rpmActual          = 0.0f;
int      brightness         = 0;
long     ultimaPosEncoder   = 0;
uint32_t ultimoMuestreo     = 0;
bool     wifiConectado      = false;
bool     mqttConectado      = false;
bool     sistemaHabilitado  = true;
uint32_t ultimaMQTT         = 0;
uint32_t ultimaReconex      = 0;

// ─────────────────────────────────────────────
//  ISR: ENCODER
// ─────────────────────────────────────────────
void IRAM_ATTR checkEncoder() {
  encoder->tick();
}

// ─────────────────────────────────────────────
//  FUNCIONES: LEDS DE VELOCIDAD
// ─────────────────────────────────────────────

/**
 * Configura los 5 LEDs de velocidad con la API LEDC del core 3.x.
 * ledcAttach(pin, freq, resolution) reemplaza a ledcSetup + ledcAttachPin.
 */
void inicializarLEDs() {
  for (int i = 0; i < NUM_LEDS; i++) {
    ledcAttach(LED_PINS[i], LEDC_FREQ_HZ, LEDC_RES_BITS);
    ledcWrite(LED_PINS[i], 0);
  }
}

/**
 * Escribe el mismo brillo en todos los LEDs de velocidad.
 * Core 3.x: ledcWrite recibe el pin, no el canal.
 */
void setBrillo(int brillo) {
  for (int i = 0; i < NUM_LEDS; i++) {
    ledcWrite(LED_PINS[i], brillo);
  }
}

// ─────────────────────────────────────────────
//  FUNCIONES: LED DE ESTADO
// ─────────────────────────────────────────────
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
      if (!sistemaHabilitado) setBrillo(0);
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
//  FUNCIONES: VELOCÍMETRO
// ─────────────────────────────────────────────

/**
 * Mide el delta de posición del encoder en el intervalo de muestreo
 * y calcula RPM.
 *
 * RPM = (pasos / PULSOS_POR_VUELTA) × (60000 / INTERVALO_MS)
 *
 * Retorna también el brillo proporcional (0–255) para los LEDs.
 */
void actualizarVelocidad() {
  long posActual  = encoder->getPosition();
  int  delta      = abs((int)(posActual - ultimaPosEncoder));
  ultimaPosEncoder = posActual;

  rpmActual = ((float)delta / PULSOS_POR_VUELTA) * (60000.0f / INTERVALO_MUESTREO_MS);

  brightness = map(delta, TICKS_MIN_BRILLO, TICKS_MAX_BRILLO, 0, 255);
  brightness = constrain(brightness, 0, 255);

  Serial.printf("[Bici] delta=%d  RPM=%.1f  brillo=%d\n", delta, rpmActual, brightness);
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Sistema] Iniciando velocímetro de bici ESP32...");

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, LOW);

  inicializarLEDs();

  encoder = new RotaryEncoder(ENCODER_CLK, ENCODER_DT, RotaryEncoder::LatchMode::TWO03);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), checkEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_DT),  checkEncoder, CHANGE);

  conectarWifi();
  if (wifiConectado) conectarMQTT();

  actualizarLEDEstado();
  Serial.println("[Sistema] Setup completo.");
}

// ─────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────
void loop() {
  encoder->tick();  // Complemento del ISR para máxima precisión
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

  // ── Medir velocidad y actualizar LEDs ────
  if (ahora - ultimoMuestreo >= INTERVALO_MUESTREO_MS) {
    ultimoMuestreo = ahora;
    if (sistemaHabilitado) {
      actualizarVelocidad();
      setBrillo(brightness);
    } else {
      setBrillo(0);
    }
  }

  // ── Publicar RPM por MQTT ─────────────────
  if (sistemaHabilitado && mqttConectado && ahora - ultimaMQTT >= INTERVALO_MQTT_MS) {
    ultimaMQTT = ahora;
    char buf[10];
    dtostrf(rpmActual, 1, 1, buf);
    mqttClient.publish(TOPIC_RPM, buf);
    Serial.printf("[MQTT] %s → %s\n", TOPIC_RPM, buf);
  }

  // ── LED de estado ─────────────────────────
  actualizarLEDEstado();
}
