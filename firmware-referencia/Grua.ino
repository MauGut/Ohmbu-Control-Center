/**
 * Grúa con Motores Paso a Paso y Electroimán - ESP32
 *
 * Motores:
 *   MotorX (gancho/horizontal) → joystick eje X  /  grua/cmd/x
 *   MotorY (base/vertical)     → joystick eje Y  /  grua/cmd/y
 *
 * Electroimán:
 *   Control físico:  pulsador (mientras presionado → ON, al soltar → OFF)
 *   Control remoto:  grua/cmd/iman → "ON" / "OFF"
 *
 * Topics suscritos:
 *   grua/cmd/modo   → "REMOTO" activa control remoto | "LOCAL" lo libera
 *   grua/cmd/x      → "IZQUIERDA" / "DERECHA" / "STOP"
 *   grua/cmd/y      → "ADELANTE" / "ATRAS" / "STOP"
 *   grua/cmd/iman   → "ON" / "OFF"
 *   grua/activado   → "ON" = funcional | "OFF" = desactivado
 *
 * Topics publicados:
 *   grua/estado/disp  → "online" / "offline" (Last Will)
 *   grua/estado/modo  → "REMOTO" / "LOCAL"
 *   grua/estado/iman  → estado del electroimán ("ON" / "OFF")
 *
 * Control remoto con watchdog:
 *   El centro de control debe reenviar el comando cada ≤ TIMEOUT_CMD_MS.
 *   Si no llega ningún comando dentro del timeout, los motores se detienen
 *   automáticamente (seguridad ante caída de red).
 *
 * LED de estado (PIN_LED):
 *   Apagado          → Sin conexión WiFi
 *   Parpadeo rápido  → WiFi conectado, sin MQTT
 *   Encendido fijo   → WiFi + MQTT, control local
 *   Parpadeo lento   → WiFi + MQTT, control REMOTO activo
 *
 * Librería necesaria: Stepper (incluida en Arduino/ESP32 core)
 */

// ─────────────────────────────────────────────
//  LIBRERÍAS
// ─────────────────────────────────────────────
#include <Stepper.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE PINES
// ─────────────────────────────────────────────
// MotorX (gancho) — IN1–IN4
constexpr uint8_t  MOTOR_X_IN1   = 25;
constexpr uint8_t  MOTOR_X_IN2   = 26;
constexpr uint8_t  MOTOR_X_IN3   = 27;
constexpr uint8_t  MOTOR_X_IN4   = 32;

// MotorY (base) — IN1–IN4
constexpr uint8_t  MOTOR_Y_IN1   = 12;
constexpr uint8_t  MOTOR_Y_IN2   = 13;
constexpr uint8_t  MOTOR_Y_IN3   = 14;
constexpr uint8_t  MOTOR_Y_IN4   = 15;

// Joystick — ADC 12 bits (0–4095)
constexpr uint8_t  JOYSTICK_X    = 34;   // Gancho (motorX)
constexpr uint8_t  JOYSTICK_Y    = 35;   // Base (motorY)

// Electroimán
constexpr uint8_t  RELE_IMAN     = 33;
constexpr uint8_t  BTN_IMAN      = 36;   // INPUT_PULLUP → LOW cuando presionado

// LED de estado
constexpr uint8_t  PIN_LED       = 2;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE MOTORES
// ─────────────────────────────────────────────
constexpr int      PASOS_POR_VUELTA  = 2048;  // 28BYJ-48

// Velocidad máxima en RPM (se pasa al objeto Stepper).
// Con valor alto, la biblioteca no agrega delay propio y el timing
// queda controlado por los intervalos INTERVALO_PASO_*.
constexpr int      VEL_MOTOR_RPM     = 1000;  // RPM internos (solo desactiva el throttle)

// Intervalo entre pasos para cada motor (controla la velocidad real).
// Aumentar para ir más lento; reducir para ir más rápido.
constexpr uint32_t INTERVALO_PASO_X_MS = 25;   // ~40 pasos/seg
constexpr uint32_t INTERVALO_PASO_Y_MS = 200;  // ~5 pasos/seg

// Zona muerta del joystick (ADC 12 bits, centro ≈ 2048)
constexpr int      JOY_CENTER      = 2048;
constexpr int      JOY_DEAD_ZONE   = 400;

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DEL RELÉ DEL ELECTROIMÁN
// ─────────────────────────────────────────────
constexpr uint8_t  IMAN_ACTIVO  = LOW;    // La mayoría de módulos relé son activos en LOW
constexpr uint8_t  IMAN_REPOSO  = HIGH;

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
constexpr char     MQTT_CLIENT_ID[]  = "esp32-grua";

constexpr char     TOPIC_CMD_MODO[]  = "grua/cmd/modo";
constexpr char     TOPIC_CMD_X[]     = "grua/cmd/x";       // IZQUIERDA / DERECHA / STOP
constexpr char     TOPIC_CMD_Y[]     = "grua/cmd/y";       // ADELANTE / ATRAS / STOP
constexpr char     TOPIC_CMD_IMAN[]  = "grua/cmd/iman";    // ON / OFF
constexpr char     TOPIC_EST_DISP[]  = "grua/estado/disp";
constexpr char     TOPIC_EST_MODO[]  = "grua/estado/modo";
constexpr char     TOPIC_EST_IMAN[]  = "grua/estado/iman";
constexpr char     TOPIC_ACTIVACION[]= "grua/activado";    // ON / OFF

// ─────────────────────────────────────────────
//  INTERVALOS
// ─────────────────────────────────────────────
constexpr uint32_t INTERVALO_ESTADO_MS       = 5000;   // Publicación periódica
constexpr uint32_t INTERVALO_RECONEX_MS      = 10000;
constexpr uint32_t INTERVALO_SERIAL_MS       = 1000;
constexpr uint32_t INTERVALO_PARPADEO_RAPIDO = 300;
constexpr uint32_t INTERVALO_PARPADEO_LENTO  = 1500;

// Watchdog de comandos remotos:
// Si no llega ningún comando en este tiempo, los motores paran.
constexpr uint32_t TIMEOUT_CMD_MS = 500;

// ─────────────────────────────────────────────
//  OBJETOS GLOBALES
// ─────────────────────────────────────────────
Stepper      motorX(PASOS_POR_VUELTA, MOTOR_X_IN1, MOTOR_X_IN2, MOTOR_X_IN3, MOTOR_X_IN4);
Stepper      motorY(PASOS_POR_VUELTA, MOTOR_Y_IN1, MOTOR_Y_IN2, MOTOR_Y_IN3, MOTOR_Y_IN4);
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// ─────────────────────────────────────────────
//  ESTADO GLOBAL
// ─────────────────────────────────────────────
int      dirX               = 0;    // -1 = izquierda, 0 = stop, +1 = derecha
int      dirY               = 0;    // -1 = atras,     0 = stop, +1 = adelante
bool     imanActivo         = false;
bool     wifiConectado      = false;
bool     mqttConectado      = false;
bool     modoRemoto         = false;
bool     sistemaHabilitado  = true;

uint32_t ultimoPasoX        = 0;
uint32_t ultimoPasoY        = 0;
uint32_t ultimoCmdX         = 0;   // Timestamp del último comando X remoto
uint32_t ultimoCmdY         = 0;   // Timestamp del último comando Y remoto
uint32_t ultimoEstado       = 0;
uint32_t ultimaReconex      = 0;
uint32_t ultimoSerial       = 0;

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
//  FUNCIONES: ELECTROIMÁN
// ─────────────────────────────────────────────

/**
 * Activa o desactiva el relé del electroimán y publica el nuevo estado.
 */
void aplicarIman(bool activar) {
  imanActivo = activar;
  digitalWrite(RELE_IMAN, activar ? IMAN_ACTIVO : IMAN_REPOSO);
  Serial.printf("[Imán] %s\n", activar ? "ON" : "OFF");
  if (mqttConectado) mqttClient.publish(TOPIC_EST_IMAN, activar ? "ON" : "OFF", true);
}

// ─────────────────────────────────────────────
//  FUNCIONES: MOTORES
// ─────────────────────────────────────────────

/**
 * Detiene ambos motores y resetea las direcciones.
 */
void detenerMotores() {
  dirX = 0;
  dirY = 0;
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
void publicarEstado() {
  mqttClient.publish(TOPIC_EST_MODO, modoRemoto ? "REMOTO" : "LOCAL", true);
  mqttClient.publish(TOPIC_EST_IMAN, imanActivo ? "ON" : "OFF", true);
}

void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  Serial.printf("[MQTT] %s → %s\n", topic, msg);

  // ── Activación / desactivación ──────────
  if (strcmp(topic, TOPIC_ACTIVACION) == 0) {
    bool nuevoEstado = (strcmp(msg, "ON") == 0);
    if (nuevoEstado != sistemaHabilitado) {
      sistemaHabilitado = nuevoEstado;
      Serial.printf("[Sistema] %s\n", sistemaHabilitado ? "ACTIVADO" : "DESACTIVADO");
      if (!sistemaHabilitado) {
        detenerMotores();
        aplicarIman(false);
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

  // ── Cambio de modo ──────────────────────
  if (strcmp(topic, TOPIC_CMD_MODO) == 0) {
    bool nuevoModo = (strcmp(msg, "REMOTO") == 0);
    if (nuevoModo != modoRemoto) {
      modoRemoto = nuevoModo;
      if (!modoRemoto) {
        // Al volver a LOCAL: parar motores y apagar imán por seguridad
        detenerMotores();
        aplicarIman(false);
      }
      Serial.printf("[Grúa] Modo: %s\n", modoRemoto ? "REMOTO" : "LOCAL");
      mqttClient.publish(TOPIC_EST_MODO, modoRemoto ? "REMOTO" : "LOCAL", true);
    }
    return;
  }

  if (!modoRemoto) {
    Serial.println("[Grúa] Comando ignorado: no está en modo REMOTO.");
    return;
  }

  // ── Comandos de movimiento (solo en REMOTO) ──
  if (strcmp(topic, TOPIC_CMD_X) == 0) {
    ultimoCmdX = millis();
    if      (strcmp(msg, "IZQUIERDA") == 0) dirX = -1;
    else if (strcmp(msg, "DERECHA")   == 0) dirX =  1;
    else                                    dirX =  0;  // "STOP" u otro

  } else if (strcmp(topic, TOPIC_CMD_Y) == 0) {
    ultimoCmdY = millis();
    if      (strcmp(msg, "ADELANTE") == 0) dirY =  1;
    else if (strcmp(msg, "ATRAS")    == 0) dirY = -1;
    else                                   dirY =  0;

  } else if (strcmp(topic, TOPIC_CMD_IMAN) == 0) {
    aplicarIman(strcmp(msg, "ON") == 0);
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
    mqttClient.subscribe(TOPIC_CMD_X);
    mqttClient.subscribe(TOPIC_CMD_Y);
    mqttClient.subscribe(TOPIC_CMD_IMAN);
    mqttClient.subscribe(TOPIC_ACTIVACION);
    publicarEstado();
  } else {
    Serial.printf("[MQTT] Falló (código %d). Se reintentará.\n", mqttClient.state());
  }
  return ok;
}

// ─────────────────────────────────────────────
//  FUNCIONES: CONTROL LOCAL
// ─────────────────────────────────────────────

/**
 * Lee el joystick y actualiza dirX/dirY.
 * ADC 12 bits → rango 0–4095, centro ≈ 2048.
 */
void leerJoystick() {
  int xVal = analogRead(JOYSTICK_X);
  int yVal = analogRead(JOYSTICK_Y);

  int xDiff = xVal - JOY_CENTER;
  int yDiff = yVal - JOY_CENTER;

  dirX = (abs(xDiff) > JOY_DEAD_ZONE) ? (xDiff > 0 ? 1 : -1) : 0;
  dirY = (abs(yDiff) > JOY_DEAD_ZONE) ? (yDiff > 0 ? 1 : -1) : 0;
}

/**
 * Lee el pulsador del electroimán.
 * Pulsador con INPUT_PULLUP: LOW cuando está presionado.
 * El imán está ON mientras el botón esté presionado.
 */
void leerPulsadorIman() {
  bool presionado = (digitalRead(BTN_IMAN) == LOW);
  if (presionado != imanActivo) {
    aplicarIman(presionado);
  }
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Sistema] Iniciando grúa ESP32...");

  pinMode(PIN_LED,   OUTPUT);
  pinMode(RELE_IMAN, OUTPUT);
  pinMode(BTN_IMAN,  INPUT_PULLUP);

  digitalWrite(PIN_LED,   LOW);
  digitalWrite(RELE_IMAN, IMAN_REPOSO);

  // Velocidad alta para que la librería no agregue delay propio.
  // El timing real lo controlan INTERVALO_PASO_X_MS / INTERVALO_PASO_Y_MS.
  motorX.setSpeed(VEL_MOTOR_RPM);
  motorY.setSpeed(VEL_MOTOR_RPM);

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
      modoRemoto        = false;
      sistemaHabilitado = true;
      detenerMotores();
      aplicarIman(false);
      Serial.println("[MQTT] Conexión perdida. Control local restaurado.");
    }
  }

  // ── Reintentar conexiones ─────────────────
  if (ahora - ultimaReconex >= INTERVALO_RECONEX_MS) {
    ultimaReconex = ahora;
    if (!wifiConectado || WiFi.status() != WL_CONNECTED) {
      wifiConectado = false; mqttConectado = false;
      modoRemoto = false;    sistemaHabilitado = true;
      detenerMotores();
      conectarWifi();
    }
    if (wifiConectado && !mqttConectado) conectarMQTT();
  }

  if (!sistemaHabilitado) {
    actualizarLED();
    return;  // Sistema desactivado: no hace nada más
  }

  // ── Watchdog de comandos remotos ─────────
  // Si el centro de control deja de enviar comandos, para los motores.
  if (modoRemoto) {
    if (ahora - ultimoCmdX > TIMEOUT_CMD_MS) dirX = 0;
    if (ahora - ultimoCmdY > TIMEOUT_CMD_MS) dirY = 0;
  }

  // ── Control local (bloqueado en modo REMOTO) ──
  if (!modoRemoto) {
    leerJoystick();
    leerPulsadorIman();
  }

  // ── Mover motores ─────────────────────────
  if (dirX != 0 && ahora - ultimoPasoX >= INTERVALO_PASO_X_MS) {
    ultimoPasoX = ahora;
    motorX.step(dirX);
  }
  if (dirY != 0 && ahora - ultimoPasoY >= INTERVALO_PASO_Y_MS) {
    ultimoPasoY = ahora;
    motorY.step(dirY);
  }

  // ── Publicar estado periódicamente ────────
  if (mqttConectado && ahora - ultimoEstado >= INTERVALO_ESTADO_MS) {
    ultimoEstado = ahora;
    publicarEstado();
  }

  // ── Log serial ────────────────────────────
  if (ahora - ultimoSerial >= INTERVALO_SERIAL_MS) {
    ultimoSerial = ahora;
    Serial.printf(
      "[Grúa] X=%+d  Y=%+d  Imán=%s  | Modo: %s\n",
      dirX, dirY,
      imanActivo ? "ON" : "OFF",
      modoRemoto ? "REMOTO" : "LOCAL"
    );
  }

  // ── LED de estado ─────────────────────────
  actualizarLED();
}
