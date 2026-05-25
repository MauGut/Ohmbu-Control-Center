# MQTT

La fuente canónica de topics MQTT es `referencias/mqtt_topics.md`.

El Control Center V2 se conecta a Mosquitto usando `config/mqtt.json`. La UI y el servidor no deben inventar rutas ni usar el mapa anterior `estaciones/...` / `maqueta/...` para comunicación con firmware.

## Topics que Node.js suscribe

Node.js se suscribe a los topics que los ESP32 publican:

- Sensores: `ambientales/temp`, `ambientales/humedad`, `ambientales/co2`, `ambientales/presion`, `anemometro/velocidad`, `bici/rpm`, `campo/humedad1`, `campo/humedad2`, `vumetro/nivel`
- Energia: `represa/voltaje`, `aerogenerador/voltaje`
- Health / Last Will: `ambientales/estado`, `aerogenerador/estado`, `anemometro/estado`, `bici/estado`, `campo/estado`, `represa/estado`, `vumetro/estado`
- Actuadores: `garra/estado/*`, `grua/estado/*`, `chatarra/estado/*`, `fundicion/estado/*`, `ciudad/estado/*`
- Balizas: `/baliza/1/estado` a `/baliza/8/estado`

Las 8 balizas se muestran en la fila de misiones del panel de sensores:

- `/baliza/1/estado`: Mision Industria
- `/baliza/2/estado`: Mision Movimiento
- `/baliza/3/estado`: Mision Navegacion
- `/baliza/4/estado`: Mision Suelo Vivo
- `/baliza/5/estado`: Mision Energia
- `/baliza/6/estado`: Mision Viento
- `/baliza/7/estado`: Mision Ambiente
- `/baliza/8/estado`: Mision Carga

Los payloads del firmware son escalares de texto: numeros, `online` / `offline`, `ON` / `OFF`, `REMOTO` / `LOCAL`.

## Topics que Node.js publica

Node.js publica solamente comandos de actuadores. No publica nunca en `/baliza/*`; esos topics se leen para cambiar el color/estado de las estaciones en la UI.

- Garra: `garra/cmd/modo`, `garra/cmd/servo1`, `garra/cmd/servo2`, `garra/cmd/servo3`
- Grua: `grua/cmd/modo`, `grua/cmd/x`, `grua/cmd/y`, `grua/cmd/iman`
- Cintas Chatarra: `chatarra/cmd/modo`, `chatarra/cmd/motor1`, `chatarra/cmd/motor2`
- Cintas Fundicion: `fundicion/cmd/modo`, `fundicion/cmd/motor1`, `fundicion/cmd/motor2`, `fundicion/cmd/motor3`
- Ciudad: `ciudad/cmd/rele1`, `ciudad/cmd/rele2`

## Estado interno

Los mensajes MQTT actualizan:

- `state.balizas`
- `state.stations`
- `state.sensors`
- `state.energy`
- `state.prototypes`
- `state.actuators`
- `state.deviceHealth`
- `state.mqtt.logs`
- `state.mqtt.publications`

Los mensajes malformados o no reconocidos se registran y no deben tirar el proceso.
