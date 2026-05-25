/**
 * Cintas Chatarra - ESP32
 *
 * 2 motores DC via L9110S, controlados por pulsadores (hold-to-run).
 * Sin relé (a diferencia de Cintas Fundición).
 *
 * L9110S — control sin pin EN separado:
 *   1A = PWM (VELOCIDAD_MOTOR)  →  sentido horario
 *   1B = LOW
 *   Ambos LOW = stop
 *
 * Topics suscritos:
 *   chatarra/cmd/modo   → "REMOTO" / "LOCAL"
 *   chatarra/cmd/motor1 → "ON" / "OFF"  (en modo REMOTO)
 *   chatarra/cmd/motor2 → "ON" / "OFF"  (en modo REMOTO)
 *   chatarra/activado   → "ON" / "OFF"
 *
 * Topics publicados:
 *   chatarra/estado/disp   → "online" / "offline"
 *   chatarra/estado/modo   → "REMOTO" / "LOCAL"
 *   chatarra/estado/motor1 → "ON" / "OFF"
 *   chatarra/estado/motor2 → "ON" / "OFF"
 *
 * LED de estado:
 *   Apagado         → Sin WiFi
 *   Parpadeo rápido → WiFi sin MQTT
 *   Encendido fijo  → WiFi + MQTT, modo LOCAL
 *   Parpadeo lento  → WiFi + MQTT, modo REMOTO
 */

// ─────────────────────────────────────────────
//  LIBRERÍAS
// ─────────────────────────────────────────────
#include <WiFi.h>
#include <PubSubClient.h>

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE PINES
// ─────────────────────────────────────────────
constexpr uint8_t  M1_1A   = 25;
constexpr uint8_t  M1_1B   = 26;
constexpr uint8_t  M2_1A   = 27;
constexpr uint8_t  M2_1B   = 32;
constexpr uint8_t  BTN1    = 18;   // INPUT_PULLUP, activo LOW
constexpr uint8_t  BTN2    = 19;   // INPUT_PULLUP, activo LOW
constexpr uint8_t  PIN_LED = 2;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE LEDC (PWM motores)
// ─────────────────────────────────────────────
constexpr uint32_t LEDC_FREQ_HZ  = 5000;
constexpr uint8_t  LEDC_RES_BITS = 8;
// ESP32 Arduino core 3.x: ledcAttach(pin, freq, res) — sin gestión manual de canales

constexpr uint8_t  VELOCIDAD_MOTOR1 = 230;  // 0–255 — ajustar según carga de cada cinta
constexpr uint8_t  VELOCIDAD_MOTOR2 = 230;

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
constexpr char     MQTT_CLIENT_ID[]  = "esp32-cintas-chatarra";

constexpr char     TOPIC_CMD_MODO[]  = "chatarra/cmd/modo";
constexpr char     TOPIC_CMD_M1[]    = "chatarra/cmd/motor1";
constexpr char     TOPIC_CMD_M2[]    = "chatarra/cmd/motor2";
constexpr char     TOPIC_ACTIVACION[]= "/baliza/1/estado";

constexpr char     TOPIC_EST_DISP[]  = "chatarra/estado/disp";
constexpr char     TOPIC_EST_MODO[]  = "chatarra/estado/modo";
constexpr char     TOPIC_EST_M1[]    = "chatarra/estado/motor1";
constexpr char     TOPIC_EST_M2[]    = "chatarra/estado/motor2";

// ─────────────────────────────────────────────
//  INTERVALOS
// ─────────────────────────────────────────────
constexpr uint32_t INTERVALO_RECONEX_MS      = 10000;
constexpr uint32_t INTERVALO_ESTADO_MS       = 5000;
constexpr uint32_t INTERVALO_PARPADEO_RAPIDO = 300;
constexpr uint32_t INTERVALO_PARPADEO_LENTO  = 1500;

// ─────────────────────────────────────────────
//  OBJETOS GLOBALES
// ─────────────────────────────────────────────
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// ─────────────────────────────────────────────
//  ESTADO GLOBAL
// ─────────────────────────────────────────────
bool     motor1Activo      = false;
bool     motor2Activo      = false;
bool     wifiConectado     = false;
bool     mqttConectado     = false;
bool     modoRemoto        = false;
bool     sistemaHabilitado = true;
uint32_t ultimaReconex     = 0;
uint32_t ultimoEstado      = 0;

// ─────────────────────────────────────────────
//  FUNCIONES: MOTORES
// ─────────────────────────────────────────────
void setMotor1(bool activo) {
  motor1Activo = activo;
  if (activo) { ledcWrite(M1_1A, VELOCIDAD_MOTOR1); digitalWrite(M1_1B, LOW); }
  else        { ledcWrite(M1_1A, 0);                digitalWrite(M1_1B, LOW); }
}

void setMotor2(bool activo) {
  motor2Activo = activo;
  if (activo) { ledcWrite(M2_1A, VELOCIDAD_MOTOR2); digitalWrite(M2_1B, LOW); }
  else        { ledcWrite(M2_1A, 0);                digitalWrite(M2_1B, LOW); }
}

void detenerTodo() { setMotor1(false); setMotor2(false); }

// ─────────────────────────────────────────────
//  FUNCIONES: PUBLICAR ESTADO
// ─────────────────────────────────────────────
void publicarEstado() {
  mqttClient.publish(TOPIC_EST_MODO, modoRemoto   ? "REMOTO" : "LOCAL", true);
  mqttClient.publish(TOPIC_EST_M1,  motor1Activo ? "ON"     : "OFF",   true);
  mqttClient.publish(TOPIC_EST_M2,  motor2Activo ? "ON"     : "OFF",   true);
}

// ─────────────────────────────────────────────
//  FUNCIONES: LED DE ESTADO
// ─────────────────────────────────────────────
void actualizarLED() {
  if (!wifiConectado) {
    digitalWrite(PIN_LED, LOW);
  } else if (!mqttConectado) {
    bool on = (millis() / INTERVALO_PARPADEO_RAPIDO) % 2 == 0;
    digitalWrite(PIN_LED, on ? HIGH : LOW);
  } else if (modoRemoto) {
    bool on = (millis() / INTERVALO_PARPADEO_LENTO) % 2 == 0;
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
    delay(200); actualizarLED();
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
  memcpy(msg, payload, length); msg[length] = '\0';
  Serial.printf("[MQTT] %s → %s\n", topic, msg);

  if (strcmp(topic, TOPIC_ACTIVACION) == 0) {
    bool nuevoEstado = (strcmp(msg, "ON") == 0);
    if (nuevoEstado != sistemaHabilitado) {
      sistemaHabilitado = nuevoEstado;
      Serial.printf("[Sistema] %s\n", sistemaHabilitado ? "ACTIVADO" : "DESACTIVADO");
      if (!sistemaHabilitado) { detenerTodo(); modoRemoto = false; }
    }
    return;
  }

  if (!sistemaHabilitado) { Serial.println("[Sistema] Comando ignorado: DESACTIVADO."); return; }

  if (strcmp(topic, TOPIC_CMD_MODO) == 0) {
    bool nuevoModo = (strcmp(msg, "REMOTO") == 0);
    if (nuevoModo != modoRemoto) {
      modoRemoto = nuevoModo;
      if (!modoRemoto) detenerTodo();
      Serial.printf("[Cintas] Modo: %s\n", modoRemoto ? "REMOTO" : "LOCAL");
      mqttClient.publish(TOPIC_EST_MODO, modoRemoto ? "REMOTO" : "LOCAL", true);
    }
    return;
  }

  if (!modoRemoto) { Serial.println("[Cintas] Comando ignorado: no está en modo REMOTO."); return; }

  if (strcmp(topic, TOPIC_CMD_M1) == 0) {
    setMotor1(strcmp(msg, "ON") == 0);
    mqttClient.publish(TOPIC_EST_M1, motor1Activo ? "ON" : "OFF", true);
  } else if (strcmp(topic, TOPIC_CMD_M2) == 0) {
    setMotor2(strcmp(msg, "ON") == 0);
    mqttClient.publish(TOPIC_EST_M2, motor2Activo ? "ON" : "OFF", true);
  }
}

bool conectarMQTT() {
  if (!wifiConectado) return false;
  if (mqttClient.connected()) { mqttConectado = true; return true; }
  Serial.printf("[MQTT] Conectando a %s:%d...\n", MQTT_BROKER, MQTT_PORT);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(callbackMQTT);
  bool ok = mqttClient.connect(MQTT_CLIENT_ID, TOPIC_EST_DISP, 0, true, "offline");
  mqttConectado = ok;
  if (ok) {
    Serial.println("[MQTT] Conectado.");
    mqttClient.publish(TOPIC_EST_DISP, "online", true);
    mqttClient.subscribe(TOPIC_CMD_MODO);
    mqttClient.subscribe(TOPIC_CMD_M1);
    mqttClient.subscribe(TOPIC_CMD_M2);
    mqttClient.subscribe(TOPIC_ACTIVACION);
    publicarEstado();
  } else {
    Serial.printf("[MQTT] Falló (código %d). Se reintentará.\n", mqttClient.state());
  }
  return ok;
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Sistema] Iniciando cintas chatarra ESP32...");

  pinMode(PIN_LED, OUTPUT); pinMode(M1_1B, OUTPUT); pinMode(M2_1B, OUTPUT);
  digitalWrite(PIN_LED, LOW); digitalWrite(M1_1B, LOW); digitalWrite(M2_1B, LOW);
  pinMode(BTN1, INPUT_PULLUP); pinMode(BTN2, INPUT_PULLUP);

  ledcAttach(M1_1A, LEDC_FREQ_HZ, LEDC_RES_BITS); ledcWrite(M1_1A, 0);

  ledcAttach(M2_1A, LEDC_FREQ_HZ, LEDC_RES_BITS); ledcWrite(M2_1A, 0);

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

  if (mqttConectado) {
    if (!mqttClient.loop()) {
      mqttConectado = false; modoRemoto = false; sistemaHabilitado = true;
      detenerTodo();
      Serial.println("[MQTT] Conexión perdida. Control local restaurado.");
    }
  }

  if (ahora - ultimaReconex >= INTERVALO_RECONEX_MS) {
    ultimaReconex = ahora;
    if (!wifiConectado || WiFi.status() != WL_CONNECTED) {
      wifiConectado = false; mqttConectado = false;
      modoRemoto = false; sistemaHabilitado = true;
      conectarWifi();
    }
    if (wifiConectado && !mqttConectado) conectarMQTT();
  }

  if (!sistemaHabilitado) { actualizarLED(); return; }

  if (!modoRemoto) {
    setMotor1(digitalRead(BTN1) == LOW);
    setMotor2(digitalRead(BTN2) == LOW);
  }

  if (mqttConectado && ahora - ultimoEstado >= INTERVALO_ESTADO_MS) {
    ultimoEstado = ahora;
    publicarEstado();
  }

  actualizarLED();
}
