# Architecture

OHMBU Interactive Control Center V2 es un orquestador local Node.js para una red cerrada de instalacion.

Componentes:

- Express sirve la UI estatica y la API.
- Socket.IO transmite el estado en vivo al navegador.
- MQTT.js conecta con Mosquitto.
- El adaptador OSC envia escenas a MagicQ y Resolume.
- go2rtc queda fuera del proceso Node.js; la UI solo consume URLs configuradas.
- El estado vive en memoria.

## Flujo MQTT canonico

La fuente de verdad MQTT es `referencias/mqtt_topics.md`.

Los ESP32 y balizas publican telemetria, estados, Last Will y habilitaciones en topics como `ambientales/temp`, `garra/estado/servo1`, `ciudad/estado/rele1` o `/baliza/8/estado`. Node.js se suscribe a esos topics exactos y actualiza el estado global.

Node.js publica comandos solo en topics de actuadores como `garra/cmd/modo` o `grua/cmd/x`. Node.js no publica en `/baliza/*`; solo lee esos topics para cambiar los circulos de estado en la pantalla.

## Flujo de navegador

1. El browser carga `/`, `/sensores`, `/debug` u otra ruta.
2. El servidor envia `state:snapshot`.
3. Cada cambio real de estado emite `state:changed` y un nuevo snapshot.
4. El debug muestra logs MQTT recibidos, publicaciones MQTT, health, ACKs y estado canonico.

El sistema debe seguir corriendo aunque MQTT, OSC, go2rtc o hardware no esten disponibles.
