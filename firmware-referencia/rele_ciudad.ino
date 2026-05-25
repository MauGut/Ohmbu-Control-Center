/**
 * Controlador de 2 Relés por MQTT - ESP32
 *
 * Cada relé tiene su propio comando, su propio topic de estado
 * y su propio topic de activación independiente.
 *
 * Por defecto ambos relés permanecen en REPOSO (dejan pasar corriente).
 *
 * Topics suscritos:
 *   ciudad/cmd/rele1    → "ON" activa relé 1 | "OFF" lo desactiva
 *   ciudad/cmd/rele2    → "ON" activa relé 2 | "OFF" lo desactiva
 *   /baliza/5/estado    → activación relé 1: "ON" = funcional | "OFF" = desactivado
 *   /baliza/3/estado    → activación relé 2: "ON" = funcional | "OFF" = desactivado
 *
 *   ⚠ Comportamiento de activación PARTICULAR de este prototipo:
 *   Cuando un relé está DESACTIVADO, se fuerza a ACTIVO (corta corriente)
 *   y no acepta comandos hasta recibir "ON" en su topic de activación.
 *   Al reactivarse vuelve a REPOSO y espera nuevos comandos.
 *
 * Topics publicados:
 *   ciudad/estado/rele1 → estado relé 1 ("ON" / "OFF")
 *   ciudad/estado/rele2 → estado relé 2 ("ON" / "OFF")
 *   ciudad/estado/disp  → estado del dispositivo ("online" / "offline")
 *
 * LED de estado (PIN_LED):
 *   Apagado          → Sin conexión WiFi
 *   Parpadeo rápido  → WiFi conectado, sin MQTT
 *   Encendido fijo   → WiFi + MQTT, ambos relés en reposo
 *   Parpadeo lento   → WiFi + MQTT, algún relé activo por comando remoto
 */

// ─────────────────────────────────────────────
//  LIBRERÍAS
// ─────────────────────────────────────────────
#include <WiFi.h>
#include <PubSubClient.h>

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE PINES
// ─────────────────────────────────────────────
constexpr uint8_t  PIN_RELE1     = 26;   // Relé 1
constexpr uint8_t  PIN_RELE2     = 25;   // Relé 2
constexpr uint8_t  PIN_LED       = 2;

// ─────────────────────────────────────────────
//  LÓGICA DE RELÉS
//  La mayoría de módulos son activos en LOW:
//    LOW  → relé energizado
//    HIGH → relé en reposo
// ─────────────────────────────────────────────
constexpr uint8_t  RELE_ACTIVO   = LOW;
constexpr uint8_t  RELE_REPOSO   = HIGH;

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
constexpr char     MQTT_CLIENT_ID[]  = "esp32-rele-ciudad";

// Comandos
constexpr char     TOPIC_CMD_R1[]    = "ciudad/cmd/rele1";
constexpr char     TOPIC_CMD_R2[]    = "ciudad/cmd/rele2";

// Estado publicado
constexpr char     TOPIC_RELE1[]     = "ciudad/estado/rele1";
constexpr char     TOPIC_RELE2[]     = "ciudad/estado/rele2";
constexpr char     TOPIC_DISP[]      = "ciudad/estado/disp";

// Activación individual (desactivado = relé fuerza corte de corriente)
constexpr char     TOPIC_ACTIV_R1[]  = "/baliza/5/estado";
constexpr char     TOPIC_ACTIV_R2[]  = "/baliza/3/estado";

// ─────────────────────────────────────────────
//  INTERVALOS
// ─────────────────────────────────────────────
constexpr uint32_t INTERVALO_RECONEX_MS       = 10000;
constexpr uint32_t INTERVALO_PARPADEO_RAPIDO  = 300;
constexpr uint32_t INTERVALO_PARPADEO_LENTO   = 1500;

// ─────────────────────────────────────────────
//  OBJETOS GLOBALES
// ─────────────────────────────────────────────
WiFiClient    wifiClient;
PubSubClient  mqttClient(wifiClient);

// ─────────────────────────────────────────────
//  ESTADO GLOBAL
// ─────────────────────────────────────────────
bool     rele1Activo      = false;
bool     rele2Activo      = false;
bool     rele1Habilitado  = true;   // Controlado por /baliza/5/estado
bool     rele2Habilitado  = true;   // Controlado por /baliza/3/estado
bool     modoRemoto       = false;  // true si algún relé está ON por comando MQTT
bool     wifiConectado    = false;
bool     mqttConectado    = false;
uint32_t ultimaReconex    = 0;

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
//  FUNCIONES: RELÉS
// ─────────────────────────────────────────────

/**
 * Aplica el estado al relé 1, publica el cambio y actualiza modoRemoto.
 * @param activar      true = energizar relé (corta corriente)
 * @param desdeRemoto  true si viene de un comando MQTT (no de activación forzada)
 */
void aplicarRele1(bool activar, bool desdeRemoto = false) {
  rele1Activo = activar;
  digitalWrite(PIN_RELE1, activar ? RELE_ACTIVO : RELE_REPOSO);
  Serial.printf("[Relé1] %s%s\n",
    activar ? "ACTIVO (corta corriente)" : "REPOSO (deja pasar corriente)",
    desdeRemoto ? " [remoto]" : ""
  );
  if (mqttConectado) mqttClient.publish(TOPIC_RELE1, activar ? "ON" : "OFF", true);
  modoRemoto = (rele1Activo && rele1Habilitado) || (rele2Activo && rele2Habilitado);
}

/**
 * Aplica el estado al relé 2, publica el cambio y actualiza modoRemoto.
 */
void aplicarRele2(bool activar, bool desdeRemoto = false) {
  rele2Activo = activar;
  digitalWrite(PIN_RELE2, activar ? RELE_ACTIVO : RELE_REPOSO);
  Serial.printf("[Relé2] %s%s\n",
    activar ? "ACTIVO (corta corriente)" : "REPOSO (deja pasar corriente)",
    desdeRemoto ? " [remoto]" : ""
  );
  if (mqttConectado) mqttClient.publish(TOPIC_RELE2, activar ? "ON" : "OFF", true);
  modoRemoto = (rele1Activo && rele1Habilitado) || (rele2Activo && rele2Habilitado);
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
  memcpy(msg, payload, length);
  msg[length] = '\0';
  Serial.printf("[MQTT] %s → %s\n", topic, msg);

  // ── Activación relé 1 (/baliza/5/estado) ──
  if (strcmp(topic, TOPIC_ACTIV_R1) == 0) {
    bool nuevoEstado = (strcmp(msg, "ON") == 0);
    if (nuevoEstado != rele1Habilitado) {
      rele1Habilitado = nuevoEstado;
      Serial.printf("[Relé1] %s\n", rele1Habilitado ? "HABILITADO" : "DESHABILITADO → forzando corte");
      if (!rele1Habilitado) {
        // Desactivado: forzar corte de corriente
        aplicarRele1(true, false);
      } else {
        // Reactivado: volver a reposo y esperar comandos
        aplicarRele1(false, false);
      }
    }
    return;
  }

  // ── Activación relé 2 (/baliza/3/estado) ──
  if (strcmp(topic, TOPIC_ACTIV_R2) == 0) {
    bool nuevoEstado = (strcmp(msg, "ON") == 0);
    if (nuevoEstado != rele2Habilitado) {
      rele2Habilitado = nuevoEstado;
      Serial.printf("[Relé2] %s\n", rele2Habilitado ? "HABILITADO" : "DESHABILITADO → forzando corte");
      if (!rele2Habilitado) {
        aplicarRele2(true, false);
      } else {
        aplicarRele2(false, false);
      }
    }
    return;
  }

  // ── Comando relé 1 ────────────────────────
  if (strcmp(topic, TOPIC_CMD_R1) == 0) {
    if (!rele1Habilitado) {
      Serial.println("[Relé1] Comando ignorado: DESHABILITADO.");
      return;
    }
    if      (strcmp(msg, "ON")  == 0) aplicarRele1(true,  true);
    else if (strcmp(msg, "OFF") == 0) aplicarRele1(false, true);
    else Serial.printf("[Relé1] Comando desconocido: %s\n", msg);
    return;
  }

  // ── Comando relé 2 ────────────────────────
  if (strcmp(topic, TOPIC_CMD_R2) == 0) {
    if (!rele2Habilitado) {
      Serial.println("[Relé2] Comando ignorado: DESHABILITADO.");
      return;
    }
    if      (strcmp(msg, "ON")  == 0) aplicarRele2(true,  true);
    else if (strcmp(msg, "OFF") == 0) aplicarRele2(false, true);
    else Serial.printf("[Relé2] Comando desconocido: %s\n", msg);
  }
}

bool conectarMQTT() {
  if (!wifiConectado) return false;
  if (mqttClient.connected()) { mqttConectado = true; return true; }
  Serial.printf("[MQTT] Conectando a %s:%d...\n", MQTT_BROKER, MQTT_PORT);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(callbackMQTT);
  bool ok = mqttClient.connect(MQTT_CLIENT_ID, TOPIC_DISP, 0, true, "offline");
  mqttConectado = ok;
  if (ok) {
    Serial.println("[MQTT] Conectado.");
    mqttClient.publish(TOPIC_DISP, "online", true);
    mqttClient.subscribe(TOPIC_CMD_R1);
    mqttClient.subscribe(TOPIC_CMD_R2);
    mqttClient.subscribe(TOPIC_ACTIV_R1);
    mqttClient.subscribe(TOPIC_ACTIV_R2);
    // Publicar estado actual de ambos relés al reconectar
    mqttClient.publish(TOPIC_RELE1, rele1Activo ? "ON" : "OFF", true);
    mqttClient.publish(TOPIC_RELE2, rele2Activo ? "ON" : "OFF", true);
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
  Serial.println("\n[Sistema] Iniciando controlador de relés ESP32...");

  pinMode(PIN_RELE1, OUTPUT);
  pinMode(PIN_RELE2, OUTPUT);
  pinMode(PIN_LED,   OUTPUT);

  // Estado inicial: ambos en reposo (dejan pasar corriente)
  aplicarRele1(false, false);
  aplicarRele2(false, false);
  digitalWrite(PIN_LED, LOW);

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
      mqttConectado    = false;
      // Failsafe: al perder MQTT, ambos relés a reposo si estaban habilitados
      if (rele1Habilitado) aplicarRele1(false, false);
      if (rele2Habilitado) aplicarRele2(false, false);
      Serial.println("[MQTT] Conexión perdida.");
    }
  }

  // ── Reintentar conexiones ─────────────────
  if (ahora - ultimaReconex >= INTERVALO_RECONEX_MS) {
    ultimaReconex = ahora;
    if (!wifiConectado || WiFi.status() != WL_CONNECTED) {
      wifiConectado = false;
      mqttConectado = false;
      conectarWifi();
    }
    if (wifiConectado && !mqttConectado) conectarMQTT();
  }

  // ── LED de estado ─────────────────────────
  actualizarLED();
}
