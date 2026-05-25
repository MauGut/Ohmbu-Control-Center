# MQTT Implementation

Fuente canónica: `referencias/mqtt_topics.md`.

## Implementación en código

- Suscripcion: `src/mqttAdapter.js`, `createMqttAdapter`, `client.subscribe(config.mqtt.subscriptions)`.
- Parseo: `src/mqttAdapter.js`, `parsePayload`.
- Aplicacion de mensajes: `src/mqttAdapter.js`, `applyCanonicalMessage`.
- Estado: `src/state.js`.
- API de simulacion y comandos: `src/server.js`.
- Configuracion de topics: `config/mqtt.json`.

## Topics recibidos

| Topic | Payload | Estado actualizado | Broadcast |
| --- | --- | --- | --- |
| `ambientales/temp` | float | `sensors.temperature` | `mqtt:message`, `state:changed`, `state:snapshot` si cambia |
| `ambientales/humedad` | float | `sensors.humidity` | idem |
| `ambientales/co2` | int | `sensors.co2` | idem |
| `ambientales/presion` | float | `sensors.pressure` | idem |
| `aerogenerador/voltaje` | float | `energy.wind_voltage` | idem |
| `anemometro/velocidad` | float | `sensors.wind` | idem |
| `bici/rpm` | float | `sensors.crank_rpm` | idem |
| `campo/humedad1` | int | `sensors.soil_moisture_1` | idem |
| `campo/humedad2` | int | `sensors.soil_moisture_2` | idem |
| `represa/voltaje` | float | `energy.dam_voltage` | idem |
| `vumetro/nivel` | int | `sensors.noise` | idem |
| `*/estado` health | `online` / `offline` | `deviceHealth.*`, `prototypes.*` | idem |
| `garra/estado/servo1` | int | `actuators.garra.servo1`, `prototypes.garra.state.servo1` | idem |
| `garra/estado/servo2` | int | `actuators.garra.servo2`, `prototypes.garra.state.servo2` | idem |
| `garra/estado/servo3` | int | `actuators.garra.servo3`, `prototypes.garra.state.servo3` | idem |
| `garra/estado/modo` | `REMOTO` / `LOCAL` | `prototypes.garra.mode` | idem |
| `grua/estado/modo` | `REMOTO` / `LOCAL` | `prototypes.grua.mode` | idem |
| `grua/estado/iman` | `ON` / `OFF` | `actuators.grua.iman` | idem |
| `chatarra/estado/modo` | `REMOTO` / `LOCAL` | `prototypes.chatarra.mode` | idem |
| `chatarra/estado/motor1` | `ON` / `OFF` | `actuators.chatarra.motor1` | idem |
| `chatarra/estado/motor2` | `ON` / `OFF` | `actuators.chatarra.motor2` | idem |
| `fundicion/estado/modo` | `REMOTO` / `LOCAL` | `prototypes.fundicion.mode` | idem |
| `fundicion/estado/motor1` | `ON` / `OFF` | `actuators.fundicion.motor1` | idem |
| `fundicion/estado/motor2` | `ON` / `OFF` | `actuators.fundicion.motor2` | idem |
| `fundicion/estado/motor3` | `ON` / `OFF` | `actuators.fundicion.motor3` | idem |
| `fundicion/estado/rele` | `ON` / `OFF` | `actuators.fundicion.rele` | idem |
| `ciudad/estado/rele1` | `ON` / `OFF` | `actuators.ciudad.rele1` | idem |
| `ciudad/estado/rele2` | `ON` / `OFF` | `actuators.ciudad.rele2` | idem |
| `/baliza/1/estado` a `/baliza/8/estado` | `ON` / `OFF` | `balizas.*`, `stations.*` | idem |

Las balizas se muestran en la UI como misiones:

| Baliza | Topic | Nombre |
| --- | --- | --- |
| 1 | `/baliza/1/estado` | Mision Industria |
| 2 | `/baliza/2/estado` | Mision Movimiento |
| 3 | `/baliza/3/estado` | Mision Navegacion |
| 4 | `/baliza/4/estado` | Mision Suelo Vivo |
| 5 | `/baliza/5/estado` | Mision Energia |
| 6 | `/baliza/6/estado` | Mision Viento |
| 7 | `/baliza/7/estado` | Mision Ambiente |
| 8 | `/baliza/8/estado` | Mision Carga |

Todos los topics recibidos se registran en `state.mqtt.logs`. Si el valor recibido no cambia el estado, se evita un broadcast de cambio de estado, pero se mantiene el log MQTT.

## Topics publicados

Node.js no publica nunca en `/baliza/*`; esos topics se leen para cambiar el color/estado de las estaciones en pantalla.

| Topic | Payload | Funcion |
| --- | --- | --- |
| `garra/cmd/modo` | `REMOTO` / `LOCAL` | `publishCommand("garra", "modo", value)` |
| `garra/cmd/servo1` | int 0-110 | `publishCommand("garra", "servo1", value)` |
| `garra/cmd/servo2` | int 0-180 | `publishCommand("garra", "servo2", value)` |
| `garra/cmd/servo3` | int 0-60 | `publishCommand("garra", "servo3", value)` |
| `grua/cmd/modo` | `REMOTO` / `LOCAL` | `publishCommand("grua", "modo", value)` |
| `grua/cmd/x` | `IZQUIERDA` / `DERECHA` / `STOP` | `publishCommand("grua", "x", value)` |
| `grua/cmd/y` | `ADELANTE` / `ATRAS` / `STOP` | `publishCommand("grua", "y", value)` |
| `grua/cmd/iman` | `ON` / `OFF` | `publishCommand("grua", "iman", value)` |
| `chatarra/cmd/modo` | `REMOTO` / `LOCAL` | `publishCommand("chatarra", "modo", value)` |
| `chatarra/cmd/motor1` | `ON` / `OFF` | `publishCommand("chatarra", "motor1", value)` |
| `chatarra/cmd/motor2` | `ON` / `OFF` | `publishCommand("chatarra", "motor2", value)` |
| `fundicion/cmd/modo` | `REMOTO` / `LOCAL` | `publishCommand("fundicion", "modo", value)` |
| `fundicion/cmd/motor1` | `ON` / `OFF` | `publishCommand("fundicion", "motor1", value)` |
| `fundicion/cmd/motor2` | `ON` / `OFF` | `publishCommand("fundicion", "motor2", value)` |
| `fundicion/cmd/motor3` | `ON` / `OFF` | `publishCommand("fundicion", "motor3", value)` |
| `ciudad/cmd/rele1` | `ON` / `OFF` | `publishCommand("ciudad", "rele1", value)` |
| `ciudad/cmd/rele2` | `ON` / `OFF` | `publishCommand("ciudad", "rele2", value)` |

## Tests con mosquitto_pub

Ejemplo:

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/temp" -m "23.5"
```

Ver `docs/MQTT_TEST_COMMANDS.md` para la lista completa.

## Pendientes conocidos

- No se implemento firmware.
- No hay UI con controles finos por actuador; los comandos MQTT estan disponibles por API.
- No hay ACK real definido en `referencias/mqtt_topics.md`; `state.acks` queda preparado para uso futuro.
- `camion` no tiene topics en la referencia canónica actual.
