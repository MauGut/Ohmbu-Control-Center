/*
 * ============================================================
 *  Baliza WS2812B con control MQTT — ESP32-C3
 *  Librerías: Adafruit NeoPixel, PubSubClient
 *
 *  Para cada unidad, cambiar únicamente BALIZA_ID (1-8).
 * ============================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

// ─── Configuración — editar aquí ─────────────────────────────
#define BALIZA_ID          8            // ← Cambiar por 1-8 en cada unidad

#define PIN_LEDS           8            // GPIO datos WS2812B 
#define NUM_LEDS           10
#define PIN_BTN_A          4            // Pulsador A — normalmente cerrado (NC)
#define PIN_BTN_B          5            // Pulsador B — normalmente abierto  (NO)
#define BRILLO             150          // 0-255

#define WIFI_SSID          "Ohmbu-2.4G"
#define WIFI_PASSWORD      "Nene.peludo"
#define MQTT_BROKER        "192.168.88.100"
#define MQTT_PORT          1883
#define MQTT_USER          ""
#define MQTT_PASS          ""

#define DURACION_NARANJA   3000         // ms de parpadeo naranja antes de ir a rojo
#define INTERVALO_PARPADEO 500          // ms entre cada destello
#define INTERVALO_RECONEX  5000         // ms entre intentos de reconexión MQTT
// ─────────────────────────────────────────────────────────────

// Colores (RGB)
const uint32_t ROJO    = Adafruit_NeoPixel::Color(180,   0,   0);
const uint32_t VERDE   = Adafruit_NeoPixel::Color(  0, 180,   0);
const uint32_t NARANJA = Adafruit_NeoPixel::Color(220,  80,   0);
const uint32_t AZUL    = Adafruit_NeoPixel::Color(  0,   0, 180);
const uint32_t APAGADO = Adafruit_NeoPixel::Color(  0,   0,   0);

// Máquina de estados
enum Estado { ROJO_OFF, VERDE_ON, NARANJA_BLINK };
Estado estadoActual;

// Control de tiempo (sin delay bloqueante)
unsigned long tiempoInicioNaranja  = 0;
unsigned long tiempoUltimoParpadeo = 0;
bool ledVisible = false;

// Debounce de pulsadores
unsigned long tiempoUltimoPulsoA = 0;
unsigned long tiempoUltimoPulsoB = 0;
bool btnAAnterior = LOW;   // NC: en reposo el pin está en LOW
bool btnBAnterior = HIGH;  // NO: en reposo el pin está en HIGH
const unsigned long DEBOUNCE_MS = 50;

// Publicación pendiente (para cuando no hay conexión MQTT)
bool  hayPublicacionPendiente = false;
char  payloadPendiente[4]     = "";   // "ON" o "OFF"

// Reconexión MQTT no bloqueante
unsigned long tiempoUltimoIntentoMQTT = 0;

// Topics MQTT (construidos en setup con BALIZA_ID)
char topicEstado[32];   // /baliza/N/estado  → publicar
char topicCmd[32];      // /baliza/N/cmd     → suscribir

Adafruit_NeoPixel tira(NUM_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

// ─── Tira de LEDs ─────────────────────────────────────────────

void setColor(uint32_t color) {
    tira.fill(color);
    tira.show();
}

// ─── Publicación con cola pendiente ──────────────────────────

void publicar(const char* payload) {
    if (mqtt.connected()) {
        mqtt.publish(topicEstado, payload, true);
        hayPublicacionPendiente = false;
    } else {
        // Guarda el estado para enviarlo al reconectar
        strncpy(payloadPendiente, payload, sizeof(payloadPendiente));
        hayPublicacionPendiente = true;
    }
}

// ─── Transiciones de estado ──────────────────────────────────

void irAVerde() {
    estadoActual = VERDE_ON;
    setColor(VERDE);
    publicar("ON");
}

void irARojo() {
    estadoActual = ROJO_OFF;
    setColor(ROJO);
    publicar("OFF");
}

void irANaranja() {
    estadoActual = NARANJA_BLINK;
    tiempoInicioNaranja  = millis();
    tiempoUltimoParpadeo = millis();
    ledVisible = true;
    setColor(NARANJA);
}

// ─── Callback MQTT ───────────────────────────────────────────

void onMensajeMQTT(char* topic, byte* payload, unsigned int len) {
    char msg[8] = {0};
    memcpy(msg, payload, min(len, (unsigned int)7));

    if (strcmp(msg, "ON") == 0) {
        irAVerde();
    } else if (strcmp(msg, "OFF") == 0) {
        irANaranja();   // Igual que el botón B: parpadeo naranja, cancelable con A
    }
}

// ─── Reconexión MQTT no bloqueante ───────────────────────────
//
//  Solo intenta conectar cada INTERVALO_RECONEX ms, sin bloquear
//  el loop ni cambiar el color de los LEDs.

void intentarReconectarMQTT() {
    if (WiFi.status() != WL_CONNECTED)    return;
    if (mqtt.connected())                 return;
    if (millis() - tiempoUltimoIntentoMQTT < INTERVALO_RECONEX) return;

    tiempoUltimoIntentoMQTT = millis();

    char clientId[20];
    snprintf(clientId, sizeof(clientId), "baliza-%d", BALIZA_ID);
    const char* user = strlen(MQTT_USER) > 0 ? MQTT_USER : nullptr;
    const char* pass = strlen(MQTT_PASS) > 0 ? MQTT_PASS : nullptr;

    if (mqtt.connect(clientId, user, pass)) {
        mqtt.subscribe(topicCmd);
        Serial.println("MQTT reconectado.");

        // Envía el estado que quedó pendiente durante la desconexión
        if (hayPublicacionPendiente) {
            mqtt.publish(topicEstado, payloadPendiente, true);
            hayPublicacionPendiente = false;
            Serial.print("Estado pendiente enviado: ");
            Serial.println(payloadPendiente);
        }
    }
}

// ─── Detección de flanco (pulsación) ─────────────────────────
//  invertido=true → pulsador NC (flanco LOW→HIGH = pulsado)

bool pulsado(int pin, bool& estadoAnterior, unsigned long& tiempoUltimo, bool invertido = false) {
    bool estadoActualPin = digitalRead(pin);
    bool flanco = invertido ? (estadoAnterior == LOW  && estadoActualPin == HIGH)
                            : (estadoAnterior == HIGH && estadoActualPin == LOW);
    if (flanco && millis() - tiempoUltimo > DEBOUNCE_MS) {
        tiempoUltimo   = millis();
        estadoAnterior = estadoActualPin;
        return true;
    }
    if (estadoActualPin != estadoAnterior) estadoAnterior = estadoActualPin;
    return false;
}

// ─── Setup ───────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    snprintf(topicEstado, sizeof(topicEstado), "/baliza/%d/estado", BALIZA_ID);
    snprintf(topicCmd,    sizeof(topicCmd),    "/baliza/%d/cmd",    BALIZA_ID);

    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);

    tira.begin();
    tira.setBrightness(BRILLO);
    tira.show();

    // Configurar MQTT siempre, independientemente de si hay conexión
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(onMensajeMQTT);

    // ── Primera conexión WiFi: parpadeo azul bloqueante ───────
    //    (solo ocurre aquí; las reconexiones son silenciosas)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);  // Fix antena ESP32-C3 Super Mini
    Serial.print("Conectando a WiFi");
    unsigned long inicioConexion = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicioConexion < 20000) {
        ledVisible = !ledVisible;
        setColor(ledVisible ? AZUL : APAGADO);
        delay(INTERVALO_PARPADEO);
        Serial.print(".");
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(" timeout — continuando sin WiFi.");
    }else{
        Serial.println(" OK — IP: " + WiFi.localIP().toString());
        // ── Primera conexión MQTT ─────────────────────────────────
        char clientId[20];
        snprintf(clientId, sizeof(clientId), "baliza-%d", BALIZA_ID);
        const char* user = strlen(MQTT_USER) > 0 ? MQTT_USER : nullptr;
        const char* pass = strlen(MQTT_PASS) > 0 ? MQTT_PASS : nullptr;
        if (mqtt.connect(clientId, user, pass)) {
            mqtt.subscribe(topicCmd);
        }                                   
    }

    // Estado inicial: rojo
    irARojo();
}

// ─── Loop ────────────────────────────────────────────────────

void loop() {
    intentarReconectarMQTT();
    mqtt.loop();

    bool pulsoA = pulsado(PIN_BTN_A, btnAAnterior, tiempoUltimoPulsoA, true);
    bool pulsoB = pulsado(PIN_BTN_B, btnBAnterior, tiempoUltimoPulsoB);

    switch (estadoActual) {

        case ROJO_OFF:
            if (pulsoA) irAVerde();
            if (pulsoB) irANaranja();
            break;

        case VERDE_ON:
            if (pulsoB) irANaranja();
            break;

        case NARANJA_BLINK:
            // Pulsador A cancela el parpadeo y activa verde
            if (pulsoA) { irAVerde(); break; }

            // Parpadeo no bloqueante
            if (millis() - tiempoUltimoParpadeo >= INTERVALO_PARPADEO) {
                ledVisible = !ledVisible;
                setColor(ledVisible ? NARANJA : APAGADO);
                tiempoUltimoParpadeo = millis();
            }

            // Fin de los 3 segundos → ir a rojo
            if (millis() - tiempoInicioNaranja >= DURACION_NARANJA) {
                irARojo();
            }
            break;
    }
}
