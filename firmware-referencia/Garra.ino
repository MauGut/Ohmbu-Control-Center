/**
 * Garra Mecánica - ESP32
 *
 * Servos:
 *   Servo1 (Derecha)    → potenciómetro 2  /  garra/cmd/servo1
 *   Servo2 (Base)       → encoder rotativo  /  garra/cmd/servo2
 *   Servo3 (Izquierda)  → potenciómetro 1  /  garra/cmd/servo3
 *
 * Topics suscritos:
 *   garra/cmd/modo    → "REMOTO" activa control remoto | "LOCAL" lo libera
 *   garra/cmd/servo1  → posición 0–MAX_SERVO1 (solo en modo REMOTO)
 *   garra/cmd/servo2  → posición 0–MAX_SERVO2 (solo en modo REMOTO)
 *   garra/cmd/servo3  → posición 0–MAX_SERVO3 (solo en modo REMOTO)
 *   garra/activado    → "ON" = sistema funcional | "OFF" = desactivado
 *
 * Topics publicados:
 *   garra/estado/disp   → "online" / "offline" (Last Will)
 *   garra/estado/modo   → "REMOTO" / "LOCAL"
 *   garra/estado/servo1 → posición actual
 *   garra/estado/servo2 → posición actual
 *   garra/estado/servo3 → posición actual
 *
 * LED de estado (PIN_LED):
 *   Apagado          → Sin conexión WiFi
 *   Parpadeo rápido  → WiFi conectado, sin MQTT
 *   Encendido fijo   → WiFi + MQTT, control local activo
 *   Parpadeo lento   → WiFi + MQTT, control REMOTO activo
 *
 * Librería necesaria: ESP32Servo (Kevin Harrington)
 */

// ─────────────────────────────────────────────
//  LIBRERÍAS
// ─────────────────────────────────────────────
#include <ESP32Servo.h>
#include <RotaryEncoder.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE PINES
// ─────────────────────────────────────────────
constexpr uint8_t  ENCODER1_CLK  = 4;
constexpr uint8_t  ENCODER1_DT   = 5;
constexpr uint8_t  POT1_PIN      = 34;
constexpr uint8_t  POT2_PIN      = 35;
constexpr uint8_t  SERVO1_PIN    = 25;
constexpr uint8_t  SERVO2_PIN    = 26;
constexpr uint8_t  SERVO3_PIN    = 27;
constexpr uint8_t  PIN_LED       = 2;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE SERVOS
// ─────────────────────────────────────────────
constexpr int  MAX_SERVO1   = 110;
constexpr int  MAX_SERVO2   = 180;
constexpr int  MAX_SERVO3   = 60;
constexpr int  INIT_SERVO1  = 0;
constexpr int  INIT_SERVO2  = 90;
constexpr int  INIT_SERVO3  = 60;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE ENCODER
// ─────────────────────────────────────────────
constexpr int  ENCODER_STEPS = 30;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN WIFI
// ─────────────────────────────────────────────
constexpr char     WIFI_SSID[]           = "Ohmbu-2.4G";
constexpr char     WIFI_PASS[]           = "Nene.peludo";
// Tiempo máximo esperando WL_CONNECTED antes de reiniciar el intento.
// NO es un bloqueo: el loop sigue corriendo durante este tiempo.
constexpr uint32_t WIFI_TIMEOUT_MS       = 10000;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN MQTT
// ─────────────────────────────────────────────
constexpr char     MQTT_BROKER[]         = "192.168.88.100";
constexpr uint16_t MQTT_PORT             = 1883;
constexpr char     MQTT_USER[]           = "";
constexpr char     MQTT_PASS_[]          = "";
constexpr char     MQTT_CLIENT_ID[]      = "esp32-garra";
// Timeout del socket MQTT en segundos (limita el bloqueo de mqttClient.connect()).
// PubSubClient por defecto usa 15s; reducirlo a 3s minimiza el freeze al reconectar.
constexpr uint8_t  MQTT_CONN_TIMEOUT_S   = 3;

constexpr char     TOPIC_CMD_MODO[]      = "garra/cmd/modo";
constexpr char     TOPIC_CMD_S1[]        = "garra/cmd/servo1";
constexpr char     TOPIC_CMD_S2[]        = "garra/cmd/servo2";
constexpr char     TOPIC_CMD_S3[]        = "garra/cmd/servo3";
constexpr char     TOPIC_EST_DISP[]      = "garra/estado/disp";
constexpr char     TOPIC_EST_MODO[]      = "garra/estado/modo";
constexpr char     TOPIC_EST_S1[]        = "garra/estado/servo1";
constexpr char     TOPIC_EST_S2[]        = "garra/estado/servo2";
constexpr char     TOPIC_EST_S3[]        = "garra/estado/servo3";
constexpr char     TOPIC_ACTIVACION[]    = "/baliza/8/estado";

// ─────────────────────────────────────────────
//  INTERVALOS
// ─────────────────────────────────────────────
constexpr uint32_t INTERVALO_CONTROL_MS       = 20;
constexpr uint32_t INTERVALO_ESTADO_MS        = 5000;
constexpr uint32_t INTERVALO_RECONEX_WIFI_MS  = 10000; // Reintento WiFi (no bloqueante)
constexpr uint32_t INTERVALO_RECONEX_MQTT_MS  = 5000;  // Reintento MQTT (independiente)
constexpr uint32_t INTERVALO_SERIAL_MS        = 1000;
constexpr uint32_t INTERVALO_PARPADEO_RAPIDO  = 300;
constexpr uint32_t INTERVALO_PARPADEO_LENTO   = 1500;

// ─────────────────────────────────────────────
//  OBJETOS GLOBALES
// ─────────────────────────────────────────────
Servo             servo1, servo2, servo3;
RotaryEncoder*    encoder1 = nullptr;
WiFiClient        wifiClient;
PubSubClient      mqttClient(wifiClient);

// ─────────────────────────────────────────────
//  ESTADO GLOBAL
// ─────────────────────────────────────────────
int      servo1Pos          = INIT_SERVO1;
int      servo2Pos          = INIT_SERVO2;
int      servo3Pos          = INIT_SERVO3;
int      encoder1Pos        = 0;

bool     wifiConectado      = false;
bool     mqttConectado      = false;
bool     modoRemoto         = false;
bool     sistemaHabilitado  = true;

// Timers de reconexión (independientes entre sí y del loop principal)
uint32_t tInicioWifi        = 0;   // Cuándo se llamó WiFi.begin() por última vez
bool     wifiIniciado       = false;
uint32_t ultimaReconexMQTT  = 0;

uint32_t ultimoEstado       = 0;
uint32_t ultimoControl      = 0;
uint32_t ultimoSerial       = 0;

// ─────────────────────────────────────────────
//  ISR: ENCODER
// ─────────────────────────────────────────────
void IRAM_ATTR checkEncoder1() {
  encoder1->tick();
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
//  FUNCIONES: WIFI (NO BLOQUEANTE)
// ─────────────────────────────────────────────

/**
 * Gestiona la conexión WiFi sin bloquear el loop.
 *
 * Funcionamiento:
 *   - Llama a WiFi.begin() una vez y retorna inmediatamente.
 *   - En cada llamada posterior chequea WiFi.status().
 *   - Si pasa WIFI_TIMEOUT_MS sin conectarse, reinicia el intento.
 *   - El loop nunca se detiene esperando WiFi.
 */
void gestionarWifi(uint32_t ahora) {
  wl_status_t ws = WiFi.status();

  // ── Recién conectado ──────────────────────
  if (ws == WL_CONNECTED) {
    if (!wifiConectado) {
      wifiConectado = true;
      wifiIniciado  = false;
      Serial.printf("[WiFi] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());
    }
    return;
  }

  // ── Se perdió la conexión ─────────────────
  if (wifiConectado) {
    wifiConectado     = false;
    mqttConectado     = false;
    modoRemoto        = false;
    sistemaHabilitado = true;
    wifiIniciado      = false;
    Serial.println("[WiFi] Conexión perdida.");
  }

  // ── Iniciar o reiniciar intento ───────────
  bool timeout = wifiIniciado && (ahora - tInicioWifi >= WIFI_TIMEOUT_MS);

  if (!wifiIniciado || timeout) {
    if (timeout) Serial.println("[WiFi] Timeout. Reintentando...");
    else         Serial.printf("[WiFi] Conectando a %s...\n", WIFI_SSID);

    WiFi.disconnect(true);
    delay(50);  // Pausa mínima recomendada antes de reconectar
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    wifiIniciado = true;
    tInicioWifi  = ahora;
    // Retorna inmediatamente; el status se chequeará en la próxima llamada
  }
}

// ─────────────────────────────────────────────
//  FUNCIONES: MQTT
// ─────────────────────────────────────────────
void publicarInt(const char* topic, int valor, bool retain = false) {
  if (!mqttConectado) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", valor);
  mqttClient.publish(topic, buf, retain);
}

void publicarEstado() {
  mqttClient.publish(TOPIC_EST_MODO, modoRemoto ? "REMOTO" : "LOCAL", true);
  publicarInt(TOPIC_EST_S1, servo1Pos, true);
  publicarInt(TOPIC_EST_S2, servo2Pos, true);
  publicarInt(TOPIC_EST_S3, servo3Pos, true);
}

int calcularMinServo3(int s1Pos) {
  return constrain((int)(50.0f - 0.8f * s1Pos), 0, MAX_SERVO3);
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
        modoRemoto = false;
        mqttClient.publish(TOPIC_EST_MODO, "LOCAL", true);
      }
    }
    return;
  }

  if (!sistemaHabilitado) {
    Serial.println("[Sistema] Comando ignorado: sistema DESACTIVADO.");
    return;
  }

  if (strcmp(topic, TOPIC_CMD_MODO) == 0) {
    bool nuevoModo = (strcmp(msg, "REMOTO") == 0);
    if (nuevoModo != modoRemoto) {
      modoRemoto = nuevoModo;
      Serial.printf("[Garra] Modo: %s\n", modoRemoto ? "REMOTO" : "LOCAL");
      mqttClient.publish(TOPIC_EST_MODO, modoRemoto ? "REMOTO" : "LOCAL", true);
    }
    return;
  }

  if (!modoRemoto) {
    Serial.println("[Garra] Comando de posición ignorado: no está en modo REMOTO.");
    return;
  }

  if (strcmp(topic, TOPIC_CMD_S1) == 0) {
    servo1Pos = constrain(atoi(msg), 0, MAX_SERVO1);
    servo1.write(servo1Pos);
    publicarInt(TOPIC_EST_S1, servo1Pos, true);
  } else if (strcmp(topic, TOPIC_CMD_S2) == 0) {
    servo2Pos = constrain(atoi(msg), 0, MAX_SERVO2);
    servo2.write(servo2Pos);
    publicarInt(TOPIC_EST_S2, servo2Pos, true);
  } else if (strcmp(topic, TOPIC_CMD_S3) == 0) {
    int minS3 = calcularMinServo3(servo1Pos);
    servo3Pos = constrain(atoi(msg), minS3, MAX_SERVO3);
    servo3.write(servo3Pos);
    publicarInt(TOPIC_EST_S3, servo3Pos, true);
  }
}

/**
 * Intenta conectar al broker MQTT.
 * mqttClient.connect() puede bloquear hasta MQTT_SOCKET_TIMEOUT segundos
 * si el broker no responde. Se llama como máximo cada INTERVALO_RECONEX_MQTT_MS.
 */
bool conectarMQTT() {
  Serial.printf("[MQTT] Conectando a %s:%d...\n", MQTT_BROKER, MQTT_PORT);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(callbackMQTT);
  mqttClient.setSocketTimeout(MQTT_CONN_TIMEOUT_S);

  bool ok = mqttClient.connect(MQTT_CLIENT_ID, TOPIC_EST_DISP, 0, true, "offline");

  mqttConectado = ok;
  if (ok) {
    Serial.println("[MQTT] Conectado.");
    mqttClient.publish(TOPIC_EST_DISP, "online", true);
    mqttClient.subscribe(TOPIC_CMD_MODO);
    mqttClient.subscribe(TOPIC_CMD_S1);
    mqttClient.subscribe(TOPIC_CMD_S2);
    mqttClient.subscribe(TOPIC_CMD_S3);
    mqttClient.subscribe(TOPIC_ACTIVACION);
    publicarEstado();
  } else {
    Serial.printf("[MQTT] Falló (código %d). Próximo intento en %ds.\n",
      mqttClient.state(), INTERVALO_RECONEX_MQTT_MS / 1000);
  }
  return ok;
}

/**
 * Gestiona la conexión MQTT: mantiene el loop activo y reintenta
 * la conexión con su propio timer, independiente del WiFi.
 */
void gestionarMQTT(uint32_t ahora) {
  if (!wifiConectado) return;

  // Mantener conexión activa y procesar mensajes entrantes
  if (mqttConectado) {
    if (!mqttClient.loop()) {
      mqttConectado     = false;
      modoRemoto        = false;
      sistemaHabilitado = true;
      Serial.println("[MQTT] Conexión perdida. Volviendo a control local.");
    }
    return;
  }

  // Reintentar conexión con su propio intervalo
  if (ahora - ultimaReconexMQTT >= INTERVALO_RECONEX_MQTT_MS) {
    ultimaReconexMQTT = ahora;
    conectarMQTT();
  }
}

// ─────────────────────────────────────────────
//  FUNCIONES: CONTROL LOCAL
// ─────────────────────────────────────────────
void leerEncoder() {
  encoder1->tick();
  int newPos = encoder1->getPosition();
  if (encoder1Pos != newPos) {
    encoder1Pos = constrain(newPos, -ENCODER_STEPS, ENCODER_STEPS);
    encoder1->setPosition(encoder1Pos);
    servo2Pos = constrain(
      map(encoder1Pos, -ENCODER_STEPS, ENCODER_STEPS, 0, MAX_SERVO2), 0, MAX_SERVO2);
    servo2.write(servo2Pos);
  }
}

void leerPotenciometros() {
  int pot2Val = (analogRead(POT2_PIN) + analogRead(POT2_PIN) + analogRead(POT2_PIN)) / 3;
  servo1Pos   = constrain(map(pot2Val, 0, 4095, 0, MAX_SERVO1), 0, MAX_SERVO1);

  int minS3   = calcularMinServo3(servo1Pos);
  int pot1Val = (analogRead(POT1_PIN) + analogRead(POT1_PIN) + analogRead(POT1_PIN)) / 3;
  servo3Pos   = constrain(map(pot1Val, 0, 4095, minS3, MAX_SERVO3), 0, MAX_SERVO3);

  servo3.write(servo3Pos);
  servo1.write(servo1Pos);
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Sistema] Iniciando garra mecánica ESP32...");

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  encoder1 = new RotaryEncoder(ENCODER1_CLK, ENCODER1_DT, RotaryEncoder::LatchMode::TWO03);
  attachInterrupt(digitalPinToInterrupt(ENCODER1_CLK), checkEncoder1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER1_DT),  checkEncoder1, CHANGE);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo3.attach(SERVO3_PIN);

  servo3.write(servo3Pos);
  delay(200);
  servo1.write(servo1Pos);
  servo2.write(servo2Pos);

  // Iniciar WiFi de forma no bloqueante
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  wifiIniciado = true;
  tInicioWifi  = millis();
  Serial.printf("[WiFi] Conectando a %s (no bloqueante)...\n", WIFI_SSID);

  Serial.println("[Sistema] Setup completo. Esperando conexión en background...");
}

// ─────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────
void loop() {
  uint32_t ahora = millis();

  // ── Gestión de conexiones (no bloqueante) ─
  gestionarWifi(ahora);
  gestionarMQTT(ahora);

  // ── Control local ─────────────────────────
  if (sistemaHabilitado && !modoRemoto && ahora - ultimoControl >= INTERVALO_CONTROL_MS) {
    ultimoControl = ahora;
    leerEncoder();
    leerPotenciometros();
  }

  // ── Publicar estado periódicamente ────────
  if (sistemaHabilitado && mqttConectado && ahora - ultimoEstado >= INTERVALO_ESTADO_MS) {
    ultimoEstado = ahora;
    publicarEstado();
  }

  // ── Log serial ───────────────────────────
  if (ahora - ultimoSerial >= INTERVALO_SERIAL_MS) {
    ultimoSerial = ahora;
    Serial.printf(
      "[Servos] S1=%3d  S2=%3d  S3=%3d  | Modo: %s | Sistema: %s | WiFi: %s | MQTT: %s\n",
      servo1Pos, servo2Pos, servo3Pos,
      modoRemoto        ? "REMOTO" : "LOCAL",
      sistemaHabilitado ? "ON"     : "OFF",
      wifiConectado     ? "OK"     : "---",
      mqttConectado     ? "OK"     : "---"
    );
  }

  // ── LED de estado ─────────────────────────
  actualizarLED();
}
