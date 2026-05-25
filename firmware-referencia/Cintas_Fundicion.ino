/**
 * Cintas Fundición - ESP32
 *
 * 3 motores DC via L9110S, controlados por pulsadores (hold-to-run).
 * El pulsador del motor 3 también activa un relé.
 * El relé SIEMPRE sigue al motor 3, tanto en modo local como remoto.
 *
 * L9110S — control sin pin EN separado:
 *   1A = PWM (VELOCIDAD_MOTORn)  →  sentido horario
 *   1B = LOW
 *   Ambos LOW = stop
 *
 * Pulsadores (hold-to-run, todos INPUT_PULLUP internos):
 *   Botón 1 presionado → motor 1 ON
 *   Botón 2 presionado → motor 2 ON
 *   Botón 3 presionado → motor 3 ON + relé ON
 *   Al soltar          → motor OFF (+ relé OFF en caso del 3)
 *
 * Topics suscritos:
 *   fundicion/cmd/modo   → "REMOTO" / "LOCAL"
 *   fundicion/cmd/motor1 → "ON" / "OFF"  (en modo REMOTO)
 *   fundicion/cmd/motor2 → "ON" / "OFF"  (en modo REMOTO)
 *   fundicion/cmd/motor3 → "ON" / "OFF"  (en modo REMOTO, relé sigue a motor3)
 *   fundicion/activado   → "ON" / "OFF"
 *
 * Topics publicados:
 *   fundicion/estado/disp   → "online" / "offline"
 *   fundicion/estado/modo   → "REMOTO" / "LOCAL"
 *   fundicion/estado/motor1 → "ON" / "OFF"
 *   fundicion/estado/motor2 → "ON" / "OFF"
 *   fundicion/estado/motor3 → "ON" / "OFF"
 *   fundicion/estado/rele   → "ON" / "OFF"
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
constexpr uint8_t  M1_1A   = 25;   // Motor 1 — 1A (PWM velocidad)
constexpr uint8_t  M1_1B   = 26;   // Motor 1 — 1B (LOW)
constexpr uint8_t  M2_1A   = 27;   // Motor 2 — 1A (PWM velocidad)
constexpr uint8_t  M2_1B   = 32;   // Motor 2 — 1B (LOW)
constexpr uint8_t  M3_1A   = 21;   // Motor 3 — 1A (PWM velocidad)
constexpr uint8_t  M3_1B   = 22;   // Motor 3 — 1B (LOW)
constexpr uint8_t  RELE    = 33;   // Relé (activo en LOW, sigue a motor 3)
constexpr uint8_t  BTN1    = 18;   // Pulsador motor 1 (INPUT_PULLUP, activo LOW)
constexpr uint8_t  BTN2    = 19;   // Pulsador motor 2 (INPUT_PULLUP, activo LOW)
constexpr uint8_t  BTN3    = 23;   // Pulsador motor 3 (INPUT_PULLUP, activo LOW)
constexpr uint8_t  PIN_LED = 2;    // LED de estado

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE LEDC (PWM motores)
// ─────────────────────────────────────────────
constexpr uint32_t LEDC_FREQ_HZ  = 5000;
constexpr uint8_t  LEDC_RES_BITS = 8;
// ESP32 Arduino core 3.x: ledcAttach(pin, freq, res) — sin gestión manual de canales

// ─────────────────────────────────────────────
//  VELOCIDAD DE CADA MOTOR (0–255)
//  Ajustar de forma independiente según la carga mecánica de cada cinta.
// ─────────────────────────────────────────────
constexpr uint8_t  VELOCIDAD_MOTOR1 = 230;   // ≈ 59% de velocidad
constexpr uint8_t  VELOCIDAD_MOTOR2 = 225;
constexpr uint8_t  VELOCIDAD_MOTOR3 = 220;

// ─────────────────────────────────────────────
//  LÓGICA DEL RELÉ
// ─────────────────────────────────────────────
constexpr uint8_t  RELE_ACTIVO  = HIGH;
constexpr uint8_t  RELE_REPOSO  = LOW;

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
constexpr char     MQTT_CLIENT_ID[]  = "esp32-cintas-fundicion";

constexpr char     TOPIC_CMD_MODO[]  = "fundicion/cmd/modo";
constexpr char     TOPIC_CMD_M1[]    = "fundicion/cmd/motor1";
constexpr char     TOPIC_CMD_M2[]    = "fundicion/cmd/motor2";
constexpr char     TOPIC_CMD_M3[]    = "fundicion/cmd/motor3";
constexpr char     TOPIC_ACTIVACION[]= "/baliza/1/estado";

constexpr char     TOPIC_EST_DISP[]  = "fundicion/estado/disp";
constexpr char     TOPIC_EST_MODO[]  = "fundicion/estado/modo";
constexpr char     TOPIC_EST_M1[]    = "fundicion/estado/motor1";
constexpr char     TOPIC_EST_M2[]    = "fundicion/estado/motor2";
constexpr char     TOPIC_EST_M3[]    = "fundicion/estado/motor3";
constexpr char     TOPIC_EST_RELE[]  = "fundicion/estado/rele";

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
bool     motor3Activo      = false;
bool     wifiConectado     = false;
bool     mqttConectado     = false;
bool     modoRemoto        = false;
bool     sistemaHabilitado = true;
uint32_t ultimaReconex     = 0;
uint32_t ultimoEstado      = 0;

// ─────────────────────────────────────────────
//  FUNCIONES: MOTORES Y RELÉ
// ─────────────────────────────────────────────
void setMotor1(bool activo) {
  motor1Activo = activo;
  ledcWrite(M1_1A, activo ? VELOCIDAD_MOTOR1 : 0);
  digitalWrite(M1_1B, LOW);
}

void setMotor2(bool activo) {
  motor2Activo = activo;
  ledcWrite(M2_1A, activo ? VELOCIDAD_MOTOR2 : 0);
  digitalWrite(M2_1B, LOW);
}

/**
 * Activa/desactiva motor 3 y el relé (siempre en conjunto).
 */
void setMotor3(bool activo) {
  motor3Activo = activo;
  ledcWrite(M3_1A, activo ? VELOCIDAD_MOTOR3 : 0);
  digitalWrite(M3_1B, LOW);
  digitalWrite(RELE, activo ? RELE_ACTIVO : RELE_REPOSO);
}

void detenerTodo() {
  setMotor1(false);
  setMotor2(false);
  setMotor3(false);  // También apaga el relé
}

// ─────────────────────────────────────────────
//  FUNCIONES: PUBLICAR ESTADO
// ─────────────────────────────────────────────
void publicarEstado() {
  mqttClient.publish(TOPIC_EST_MODO,  modoRemoto    ? "REMOTO" : "LOCAL", true);
  mqttClient.publish(TOPIC_EST_M1,    motor1Activo  ? "ON"     : "OFF",   true);
  mqttClient.publish(TOPIC_EST_M2,    motor2Activo  ? "ON"     : "OFF",   true);
  mqttClient.publish(TOPIC_EST_M3,    motor3Activo  ? "ON"     : "OFF",   true);
  mqttClient.publish(TOPIC_EST_RELE,  motor3Activo  ? "ON"     : "OFF",   true);
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

  // ── Activación ──────────────────────────
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

  // ── Modo ────────────────────────────────
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

  // ── Comandos de motores (solo en REMOTO) ──
  if (strcmp(topic, TOPIC_CMD_M1) == 0) {
    setMotor1(strcmp(msg, "ON") == 0);
    mqttClient.publish(TOPIC_EST_M1, motor1Activo ? "ON" : "OFF", true);

  } else if (strcmp(topic, TOPIC_CMD_M2) == 0) {
    setMotor2(strcmp(msg, "ON") == 0);
    mqttClient.publish(TOPIC_EST_M2, motor2Activo ? "ON" : "OFF", true);

  } else if (strcmp(topic, TOPIC_CMD_M3) == 0) {
    setMotor3(strcmp(msg, "ON") == 0);  // Relé sigue a motor 3
    mqttClient.publish(TOPIC_EST_M3,   motor3Activo ? "ON" : "OFF", true);
    mqttClient.publish(TOPIC_EST_RELE, motor3Activo ? "ON" : "OFF", true);
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
    mqttClient.subscribe(TOPIC_CMD_M3);
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
  Serial.println("\n[Sistema] Iniciando cintas fundición ESP32...");

  pinMode(PIN_LED, OUTPUT);
  pinMode(M1_1B,  OUTPUT); pinMode(M2_1B, OUTPUT); pinMode(M3_1B, OUTPUT);
  pinMode(RELE,   OUTPUT);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);

  digitalWrite(PIN_LED, LOW);
  digitalWrite(M1_1B,   LOW); digitalWrite(M2_1B, LOW); digitalWrite(M3_1B, LOW);
  digitalWrite(RELE, RELE_REPOSO);

  ledcAttach(M1_1A, LEDC_FREQ_HZ, LEDC_RES_BITS); ledcWrite(M1_1A, 0);
  ledcAttach(M2_1A, LEDC_FREQ_HZ, LEDC_RES_BITS); ledcWrite(M2_1A, 0);
  ledcAttach(M3_1A, LEDC_FREQ_HZ, LEDC_RES_BITS); ledcWrite(M3_1A, 0);

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

  // ── Control local (bloqueado en modo REMOTO) ──
  if (!modoRemoto) {
    setMotor1(digitalRead(BTN1) == LOW);
    setMotor2(digitalRead(BTN2) == LOW);
    setMotor3(digitalRead(BTN3) == LOW);  // Relé sigue a motor 3
  }

  // ── Publicar estado periódicamente ────────
  if (mqttConectado && ahora - ultimoEstado >= INTERVALO_ESTADO_MS) {
    ultimoEstado = ahora;
    publicarEstado();
  }

  actualizarLED();
}
