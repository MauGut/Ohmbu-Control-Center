/*
 * Camion ESP32 — Ohmbu
 *
 * Hardware:
 *   - ESP-32S
 *   - L9110S-F: motores delanteros (izq + der en paralelo)
 *   - L9110S-B: motores traseros  (izq + der en paralelo)
 *   - Ambos L9110S comparten los mismos 4 pines del ESP32
 *   - 3x 18650 en serie → XL4015 → 5V
 *   - 2x Sensor de choque (frente y atrás)
 *   - 1x LDR
 *   - 2x par de LEDs (frente y atrás)
 *
 * Dependencias (instalar desde el Library Manager):
 *   - PubSubClient  (Nick O'Leary)
 *
 * Topics MQTT (solo en modo normal, se ignoran en modo manual):
 *   Suscribe → camion/CMD         payload: GO | STOP | FORWARD | BACK
 *   Publica  → camion/STE/DIR     payload: FORWARD | BACK | STOPPED
 *   Publica  → camion/STE/LDR     payload: 0–100 (%) o -1 si sin calibrar
 *   Publica  → camion/STE/MODO    payload: NORMAL | MANUAL
 *
 * Modo manual:
 *   Activación : mantener ambos sensores de choque 3 s con el camión detenido
 *   Control    : pulsar sensor frente → adelante | sensor atrás → atrás
 *   Desactivación: al llegar al final de carrera (choque mientras se mueve)
 *   Funciona con o sin WiFi/MQTT
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ================================================================
//  CONFIGURACIÓN — editá solo esta sección para tu entorno
// ================================================================

const char* WIFI_SSID      = "Ohmbu-2.4G";
const char* WIFI_PASSWORD  = "Nene.peludo";

const char* MQTT_BROKER    = "192.168.88.100";
const int   MQTT_PORT      = 1883;
const char* MQTT_CLIENT_ID = "camion_ohmbu";
const char* MQTT_USER      = "";
const char* MQTT_PASS      = "";

const char* TOPIC_CMD      = "camion/CMD";
const char* TOPIC_STE_DIR  = "camion/STE/DIR";
const char* TOPIC_STE_LDR  = "camion/STE/LDR";
const char* TOPIC_STE_MODO = "camion/STE/MODO";

// Publicación de estado
const unsigned long STE_PUBLISH_INTERVAL_MS  = 500;

// Conectividad — no bloqueante
const unsigned long WIFI_CONNECT_TIMEOUT_MS  = 10000;  // intento inicial en setup
const unsigned long MQTT_RETRY_INTERVAL_MS   = 5000;   // reintento en loop

// ================================================================
//  PINES
// ================================================================

// Motores — L9110S-F y L9110S-B cableados en paralelo a estos 4 pines
const int MOTOR_IA = 16;
const int MOTOR_IB = 17;
const int MOTOR_IC = 18;
const int MOTOR_ID = 19;

// Sensores de choque — GPIO con INPUT_PULLUP interno disponible
const int CHOQUE_FRENTE = 27;
const int CHOQUE_ATRAS  = 23;

// LDR — ADC1, compatible con WiFi activo
const int LDR_PIN = 32;

// LEDs
const int LEDS_FRENTE = 13;
const int LEDS_ATRAS  = 14;
// const int LEDS_EXTRA = 25;  // Tercer par — reservado

// ================================================================
//  PARÁMETROS AJUSTABLES
// ================================================================

// LDR
const int LDR_UMBRAL_OSCURO = 35;
const int LDR_UMBRAL_CLARO  = 55;
const int LDR_RANGO_MINIMO  = 200;

// LED show arranque (alternado frente/atrás)
const int           LED_SHOW_ARRANQUE_PASOS   = 10;
const unsigned long LED_SHOW_ARRANQUE_PASO_MS = 250;

// LED show entrada modo manual (ambos LEDs juntos, muy rápido)
const int           LED_SHOW_MANUAL_PASOS     = 20;   // 10 blinks × 2 semiciclos
const unsigned long LED_SHOW_MANUAL_PASO_MS   = 60;

// Tiempo de pulsación simultánea para activar modo manual
const unsigned long MANUAL_ACTIVACION_MS = 3000;

// ================================================================
//  ENUMS Y ESTADO
// ================================================================

enum Direccion { DETENIDO, ADELANTE, ATRAS };
enum Modo      { MODO_NORMAL, MODO_MANUAL };
enum TipoShow  { SHOW_ARRANQUE, SHOW_MANUAL_ENTRADA };

Direccion dirActual       = DETENIDO;
Direccion ultimaDireccion = ADELANTE;
Modo      modoActual      = MODO_NORMAL;

// LDR
int ldrValMax = 2048;
int ldrValMin = 2048;

// LED show
bool          ledShowActivo          = false;
TipoShow      ledShowTipo            = SHOW_ARRANQUE;
int           ledShowPaso            = 0;
unsigned long ledShowTimer           = 0;
Direccion     dirPendiente           = DETENIDO;
bool          ledEstadoPrevioFrente  = false;
bool          ledEstadoPrevioAtras   = false;

// Modo manual — detección de activación
bool          ambosPresionadosActivo = false;
unsigned long ambosPresionadosDesde  = 0;

// Modo manual — detección de flanco para control de movimiento
bool choqueFrenteAnterior = true;   // true = no presionado (HIGH)
bool choqueAtrasAnterior  = true;

// Conectividad
unsigned long ultimoIntentoMQTT = 0;
unsigned long ultimoSTE         = 0;

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

// ================================================================
//  SETUP
// ================================================================

void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_IA, OUTPUT);
  pinMode(MOTOR_IB, OUTPUT);
  pinMode(MOTOR_IC, OUTPUT);
  pinMode(MOTOR_ID, OUTPUT);
  detener();

  pinMode(CHOQUE_FRENTE, INPUT_PULLUP);
  pinMode(CHOQUE_ATRAS,  INPUT_PULLUP);
  pinMode(LDR_PIN, INPUT);

  pinMode(LEDS_FRENTE, OUTPUT);
  pinMode(LEDS_ATRAS,  OUTPUT);
  apagarLeds();

  int ldrInicial = analogRead(LDR_PIN);
  ldrValMax = ldrInicial;
  ldrValMin = ldrInicial;

  // Intento inicial de WiFi con timeout — si no hay red, continúa igual
  iniciarWifi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(callbackMQTT);
}

// ================================================================
//  LOOP
// ================================================================

void loop() {
  gestionarConectividad();

  if (modoActual == MODO_NORMAL) {
    detectarEntradaModoManual();
    leerSensoresModoNormal();
  } else {
    leerSensoresModoManual();
  }

  actualizarLedShow();
  actualizarLEDsLuz();

  if (millis() - ultimoSTE >= STE_PUBLISH_INTERVAL_MS) {
    publicarEstado();
    ultimoSTE = millis();
  }
}

// ================================================================
//  WiFi — no bloqueante
// ================================================================

/*
 * Intento inicial con timeout. Si no conecta en WIFI_CONNECT_TIMEOUT_MS
 * el setup continúa y el modo manual queda disponible igual.
 */
void iniciarWifi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < WIFI_CONNECT_TIMEOUT_MS) {
    delay(200);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi conectado. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi no disponible — modo offline activo.");
  }
}

// ================================================================
//  Conectividad — gestión no bloqueante en loop
// ================================================================

/*
 * Maneja reconexión WiFi y MQTT sin bloquear el loop.
 * Si no hay red, simplemente no publica ni recibe comandos MQTT,
 * pero todo lo demás (motores, sensores, LEDs) sigue funcionando.
 */
void gestionarConectividad() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!mqtt.connected()) {
    if (millis() - ultimoIntentoMQTT >= MQTT_RETRY_INTERVAL_MS) {
      ultimoIntentoMQTT = millis();
      Serial.print("Intentando MQTT... ");
      bool ok = (strlen(MQTT_USER) > 0)
        ? mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)
        : mqtt.connect(MQTT_CLIENT_ID);
      if (ok) {
        Serial.println("conectado.");
        mqtt.subscribe(TOPIC_CMD);
      } else {
        Serial.printf("Error %d\n", mqtt.state());
      }
    }
    return;
  }

  mqtt.loop();
}

// ================================================================
//  MQTT callback
// ================================================================

/*
 * Los comandos se ignoran en modo manual para evitar interferencias.
 * Comandos: GO | STOP | FORWARD | BACK
 */
void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  if (modoActual == MODO_MANUAL) {
    Serial.println("CMD ignorado (modo manual activo).");
    return;
  }

  String cmd = "";
  for (unsigned int i = 0; i < length; i++) cmd += (char)payload[i];
  cmd.trim();
  cmd.toUpperCase();
  Serial.println("CMD: " + cmd);

  if      (cmd == "GO")      cmdArrancar();
  else if (cmd == "STOP")    detener();
  else if (cmd == "FORWARD") iniciarLedShow(ADELANTE, SHOW_ARRANQUE);
  else if (cmd == "BACK")    iniciarLedShow(ATRAS, SHOW_ARRANQUE);
  else Serial.println("CMD desconocido: " + cmd);
}

// ================================================================
//  COMANDOS MQTT
// ================================================================

void cmdArrancar() {
  if (dirActual != DETENIDO) {
    Serial.println("Ya en movimiento, GO ignorado.");
    return;
  }
  iniciarLedShow(ultimaDireccion, SHOW_ARRANQUE);
}

// ================================================================
//  MODO MANUAL — detección de activación
// ================================================================

/*
 * Detecta que ambos sensores están presionados simultáneamente
 * durante MANUAL_ACTIVACION_MS con el camión detenido.
 * Al cumplirse arranca el led show de entrada al modo manual.
 */
void detectarEntradaModoManual() {
  if (dirActual != DETENIDO || ledShowActivo) return;

  bool f = (digitalRead(CHOQUE_FRENTE) == LOW);
  bool a = (digitalRead(CHOQUE_ATRAS)  == LOW);

  if (f && a) {
    if (!ambosPresionadosActivo) {
      ambosPresionadosActivo = true;
      ambosPresionadosDesde  = millis();
      Serial.println("Activación manual: manteniendo...");
    } else if (millis() - ambosPresionadosDesde >= MANUAL_ACTIVACION_MS) {
      ambosPresionadosActivo = false;
      Serial.println("Entrando a modo manual...");
      iniciarLedShow(DETENIDO, SHOW_MANUAL_ENTRADA);
    }
  } else {
    if (ambosPresionadosActivo) {
      Serial.println("Activación manual cancelada.");
    }
    ambosPresionadosActivo = false;
  }
}

/*
 * Llamada al terminar SHOW_MANUAL_ENTRADA.
 * Resetea el estado de flancos para evitar falsos arranques.
 */
void activarModoManual() {
  modoActual = MODO_MANUAL;
  choqueFrenteAnterior = (digitalRead(CHOQUE_FRENTE) == HIGH);
  choqueAtrasAnterior  = (digitalRead(CHOQUE_ATRAS)  == HIGH);
  Serial.println("** Modo manual activo — usá los sensores para mover **");
}

void salirModoManual() {
  modoActual = MODO_NORMAL;
  Serial.println("** Modo manual desactivado **");
}

// ================================================================
//  SENSORES — modo normal
// ================================================================

/*
 * En modo normal: frena al llegar al final y espera nuevo CMD.
 * No arranca solo en sentido contrario.
 */
void leerSensoresModoNormal() {
  if (ledShowActivo) return;

  bool golpeFrente = (digitalRead(CHOQUE_FRENTE) == LOW);
  bool golpeAtras  = (digitalRead(CHOQUE_ATRAS)  == LOW);

  if (golpeFrente && dirActual == ADELANTE) {
    detener();
    ultimaDireccion = ATRAS;
    Serial.println("Choque frente — esperando CMD.");
  } else if (golpeAtras && dirActual == ATRAS) {
    detener();
    ultimaDireccion = ADELANTE;
    Serial.println("Choque atrás — esperando CMD.");
  }
}

// ================================================================
//  SENSORES — modo manual
// ================================================================

/*
 * Detenido: detecta flanco descendente en cada sensor para arrancar.
 *   Sensor frente → adelante
 *   Sensor atrás  → atrás
 *
 * En movimiento: detecta activación del sensor correspondiente
 * al final de carrera → frena y sale del modo manual.
 */
void leerSensoresModoManual() {
  if (ledShowActivo) return;

  bool f = (digitalRead(CHOQUE_FRENTE) == LOW);
  bool a = (digitalRead(CHOQUE_ATRAS)  == LOW);

  if (dirActual == DETENIDO) {
    // Flanco descendente → arrancar
    if (f && choqueFrenteAnterior) {
      Serial.println("[Manual] Pulsado frente → atrás");
      moverAtras();
    } else if (a && choqueAtrasAnterior) {
      Serial.println("[Manual] Pulsado atrás → adelante");
      moverAdelante();
    }
  } else {
    // Final de carrera → frenar y salir
    if (f && dirActual == ADELANTE) {
      detener();
      salirModoManual();
    } else if (a && dirActual == ATRAS) {
      detener();
      salirModoManual();
    }
  }

  // Actualizar estado anterior para detección de flanco
  choqueFrenteAnterior = !f;  // true cuando NO está presionado
  choqueAtrasAnterior  = !a;
}

// ================================================================
//  CONTROL DE MOTORES
// ================================================================

void moverAdelante() {
  digitalWrite(MOTOR_IA, HIGH);
  digitalWrite(MOTOR_IB, LOW);
  digitalWrite(MOTOR_IC, HIGH);
  digitalWrite(MOTOR_ID, LOW);
  dirActual = ADELANTE;
  Serial.println("→ Adelante");
}

void moverAtras() {
  digitalWrite(MOTOR_IA, LOW);
  digitalWrite(MOTOR_IB, HIGH);
  digitalWrite(MOTOR_IC, LOW);
  digitalWrite(MOTOR_ID, HIGH);
  dirActual = ATRAS;
  Serial.println("→ Atrás");
}

void detener() {
  digitalWrite(MOTOR_IA, LOW);
  digitalWrite(MOTOR_IB, LOW);
  digitalWrite(MOTOR_IC, LOW);
  digitalWrite(MOTOR_ID, LOW);
  dirActual = DETENIDO;
  Serial.println("→ Detenido");
}

// ================================================================
//  LEDs — luz ambiental
// ================================================================

void apagarLeds() {
  digitalWrite(LEDS_FRENTE, LOW);
  digitalWrite(LEDS_ATRAS,  LOW);
}

void actualizarLEDsLuz() {
  if (ledShowActivo) return;

  int ldrVal = analogRead(LDR_PIN);
  if (ldrVal > ldrValMax) ldrValMax = ldrVal;
  if (ldrVal < ldrValMin) ldrValMin = ldrVal;
  if ((ldrValMax - ldrValMin) < LDR_RANGO_MINIMO) return;

  int pct = map(ldrVal, ldrValMin, ldrValMax, 0, 100);

  if (pct < LDR_UMBRAL_OSCURO) {
    digitalWrite(LEDS_FRENTE, HIGH);
    digitalWrite(LEDS_ATRAS,  HIGH);
  } else if (pct > LDR_UMBRAL_CLARO) {
    apagarLeds();
  }
}

// ================================================================
//  LED SHOW — no bloqueante
// ================================================================

/*
 * tipo SHOW_ARRANQUE    : frente y atrás alternados antes de mover
 * tipo SHOW_MANUAL_ENTRADA: ambos LEDs juntos, rápido, confirma entrada manual
 *
 * dirDestino se ignora en SHOW_MANUAL_ENTRADA (el show activa el modo manual).
 */
void iniciarLedShow(Direccion dirDestino, TipoShow tipo) {
  ledEstadoPrevioFrente = digitalRead(LEDS_FRENTE);
  ledEstadoPrevioAtras  = digitalRead(LEDS_ATRAS);
  ledShowTipo   = tipo;
  dirPendiente  = dirDestino;
  ledShowPaso   = 0;
  ledShowTimer  = millis();
  ledShowActivo = true;
}

void actualizarLedShow() {
  if (!ledShowActivo) return;

  unsigned long pasoMs   = (ledShowTipo == SHOW_ARRANQUE) ? LED_SHOW_ARRANQUE_PASO_MS
                                                           : LED_SHOW_MANUAL_PASO_MS;
  int           totalPasos = (ledShowTipo == SHOW_ARRANQUE) ? LED_SHOW_ARRANQUE_PASOS
                                                             : LED_SHOW_MANUAL_PASOS;

  if (millis() - ledShowTimer < pasoMs) return;
  ledShowTimer = millis();

  if (ledShowPaso >= totalPasos) {
    // Show terminado — restaurar LEDs y ejecutar acción pendiente
    ledShowActivo = false;
    digitalWrite(LEDS_FRENTE, ledEstadoPrevioFrente);
    digitalWrite(LEDS_ATRAS,  ledEstadoPrevioAtras);

    if (ledShowTipo == SHOW_ARRANQUE) {
      if      (dirPendiente == ADELANTE) moverAdelante();
      else if (dirPendiente == ATRAS)    moverAtras();
    } else {
      activarModoManual();
    }
    dirPendiente = DETENIDO;
    return;
  }

  if (ledShowTipo == SHOW_ARRANQUE) {
    // Alternado: frente encendido en pasos pares, atrás en impares
    digitalWrite(LEDS_FRENTE, (ledShowPaso % 2 == 0) ? HIGH : LOW);
    digitalWrite(LEDS_ATRAS,  (ledShowPaso % 2 == 0) ? LOW  : HIGH);
  } else {
    // Ambos juntos parpadeando
    bool encendido = (ledShowPaso % 2 == 0);
    digitalWrite(LEDS_FRENTE, encendido ? HIGH : LOW);
    digitalWrite(LEDS_ATRAS,  encendido ? HIGH : LOW);
  }

  ledShowPaso++;
}

// ================================================================
//  PUBLICAR ESTADO
// ================================================================

void publicarEstado() {
  if (!mqtt.connected()) return;

  // DIR
  const char* dirStr;
  switch (dirActual) {
    case ADELANTE: dirStr = "FORWARD"; break;
    case ATRAS:    dirStr = "BACK";    break;
    default:       dirStr = "STOPPED"; break;
  }
  mqtt.publish(TOPIC_STE_DIR, dirStr);

  // MODO
  mqtt.publish(TOPIC_STE_MODO, (modoActual == MODO_MANUAL) ? "MANUAL" : "NORMAL");

  // LDR
  int ldrVal = analogRead(LDR_PIN);
  int pct = -1;
  if ((ldrValMax - ldrValMin) >= LDR_RANGO_MINIMO) {
    pct = constrain(map(ldrVal, ldrValMin, ldrValMax, 0, 100), 0, 100);
  }
  char ldrBuf[8];
  itoa(pct, ldrBuf, 10);
  mqtt.publish(TOPIC_STE_LDR, ldrBuf);
}
