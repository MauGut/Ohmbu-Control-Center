# Manual de Control Center V2

Este manual describe el uso actual del Control Center V2 con las rutas MQTT reales ya migradas.

Fuente canónica de MQTT:

- `referencias/mqtt_topics.md`

Documentación base:

- `docs/MQTT_IMPLEMENTATION.md`
- `docs/MQTT_TEST_COMMANDS.md`

## Ejecutar el sistema

Instalar dependencias:

```powershell
npm install
```

Iniciar servidor:

```powershell
npm run dev
```

Abrir:

- Panel principal: `http://localhost:3000/`
- Debug: `http://localhost:3000/debug`
- Output: `http://localhost:3000/output`

El sistema debe iniciar aunque no haya hardware, OSC, go2rtc o broker MQTT disponible.

## Menu lateral

Las secciones visibles del menu lateral son:

- Sensores
- Camaras
- Garra
- Ciudad
- Grua
- Camion
- Cinta de minerales
- Cinta de fundicion

## Controles fisicos

El panel arcade se interpreta como teclado en el navegador y tambien puede enviarse por `POST /api/arcade/input`.

| Boton | Accion |
| --- | --- |
| 00 | Acceso directo a Camion |
| 01 | Acceso directo a Cinta de minerales |
| 02 | Acceso directo a Cinta de fundicion |
| 03 | Acceso directo a Garra |
| 04 | Acceso directo a Ciudad |
| 05 | Acceso directo a Grua |
| 06 | Boton contextual A |
| 07 | Boton contextual B |
| 08 | Enter / aceptar |
| 09 | Atras |
| Joystick arriba/abajo/izquierda/derecha | Mueve el foco dentro de la pagina |

En teclado comun, las teclas `0` a `9`, flechas, `Enter` y `Escape` replican esas acciones para pruebas.

## Modelo MQTT actual

El Control Center ya no usa el mapa viejo `estaciones/...` ni `maqueta/...` para comunicarse con los ESP32.

Ahora usa el mapa canónico de prototipos:

- Sensores y energía: `ambientales/*`, `aerogenerador/*`, `anemometro/*`, `bici/*`, `campo/*`, `represa/*`, `vumetro/*`
- Estados de actuadores: `garra/estado/*`, `grua/estado/*`, `chatarra/estado/*`, `fundicion/estado/*`, `ciudad/estado/*`
- Balizas leidas por Node.js: `/baliza/{id}/estado`
- Comandos publicados por Node.js: `garra/cmd/*`, `grua/cmd/*`, `chatarra/cmd/*`, `fundicion/cmd/*`, `ciudad/cmd/*`

## Topics que recibe Node.js

| Topic | Payload | Estado actualizado |
| --- | --- | --- |
| `ambientales/temp` | float | `sensors.temperature` |
| `ambientales/humedad` | float | `sensors.humidity` |
| `ambientales/co2` | int | `sensors.co2` |
| `ambientales/presion` | float | `sensors.pressure` |
| `aerogenerador/voltaje` | float | `energy.wind_voltage` |
| `anemometro/velocidad` | float | `sensors.wind` |
| `bici/rpm` | float | `sensors.crank_rpm` |
| `campo/humedad1` | int | `sensors.soil_moisture_1` |
| `campo/humedad2` | int | `sensors.soil_moisture_2` |
| `represa/voltaje` | float | `energy.dam_voltage` |
| `vumetro/nivel` | int | `sensors.noise` |
| `*/estado` | `online` / `offline` | `deviceHealth.*`, `prototypes.*` |
| `garra/estado/modo` | `REMOTO` / `LOCAL` | `prototypes.garra.mode` |
| `garra/estado/servo1` | int | `actuators.garra.servo1` |
| `garra/estado/servo2` | int | `actuators.garra.servo2` |
| `garra/estado/servo3` | int | `actuators.garra.servo3` |
| `grua/estado/modo` | `REMOTO` / `LOCAL` | `prototypes.grua.mode` |
| `grua/estado/iman` | `ON` / `OFF` | `actuators.grua.iman` |
| `chatarra/estado/modo` | `REMOTO` / `LOCAL` | `prototypes.chatarra.mode` |
| `chatarra/estado/motor1` | `ON` / `OFF` | `actuators.chatarra.motor1` |
| `chatarra/estado/motor2` | `ON` / `OFF` | `actuators.chatarra.motor2` |
| `fundicion/estado/modo` | `REMOTO` / `LOCAL` | `prototypes.fundicion.mode` |
| `fundicion/estado/motor1` | `ON` / `OFF` | `actuators.fundicion.motor1` |
| `fundicion/estado/motor2` | `ON` / `OFF` | `actuators.fundicion.motor2` |
| `fundicion/estado/motor3` | `ON` / `OFF` | `actuators.fundicion.motor3` |
| `fundicion/estado/rele` | `ON` / `OFF` | `actuators.fundicion.rele` |
| `ciudad/estado/rele1` | `ON` / `OFF` | `actuators.ciudad.rele1` |
| `ciudad/estado/rele2` | `ON` / `OFF` | `actuators.ciudad.rele2` |
| `/baliza/1/estado` a `/baliza/8/estado` | `ON` / `OFF` | `balizas.*`, `stations.*` |

Todo mensaje recibido se registra en `state.mqtt.logs`. Si el valor no cambia el estado, se mantiene el log pero se evita un broadcast de cambio de estado.

## Misiones en pantalla

La fila de circulos rojos/verdes del panel de sensores representa las 8 balizas, no las secciones del menu:

| Baliza | Topic | Nombre en pantalla |
| --- | --- | --- |
| 1 | `/baliza/1/estado` | Mision Industria |
| 2 | `/baliza/2/estado` | Mision Movimiento |
| 3 | `/baliza/3/estado` | Mision Navegacion |
| 4 | `/baliza/4/estado` | Mision Suelo Vivo |
| 5 | `/baliza/5/estado` | Mision Energia |
| 6 | `/baliza/6/estado` | Mision Viento |
| 7 | `/baliza/7/estado` | Mision Ambiente |
| 8 | `/baliza/8/estado` | Mision Carga |

Cada circulo pasa a verde cuando su topic correspondiente recibe `ON`, y vuelve a rojo cuando recibe `OFF`.

## Energia

La tarjeta `Voltaje de panel` es derivada:

- Si `Mision Energia` (`/baliza/5/estado`) esta en rojo/OFF: `0 V`
- Si `Mision Energia` (`/baliza/5/estado`) esta en verde/ON: `10.4 V`

## Topics que publica Node.js

Node.js no publica nunca en `/baliza/*`; solo lee esos topics para cambiar los circulos rojos/verdes de la pantalla de sensores.

### Comandos

| Topic | Payload |
| --- | --- |
| `garra/cmd/modo` | `REMOTO` / `LOCAL` |
| `garra/cmd/servo1` | int 0-110 |
| `garra/cmd/servo2` | int 0-180 |
| `garra/cmd/servo3` | int 0-60 |
| `grua/cmd/modo` | `REMOTO` / `LOCAL` |
| `grua/cmd/x` | `IZQUIERDA` / `DERECHA` / `STOP` |
| `grua/cmd/y` | `ADELANTE` / `ATRAS` / `STOP` |
| `grua/cmd/iman` | `ON` / `OFF` |
| `chatarra/cmd/modo` | `REMOTO` / `LOCAL` |
| `chatarra/cmd/motor1` | `ON` / `OFF` |
| `chatarra/cmd/motor2` | `ON` / `OFF` |
| `fundicion/cmd/modo` | `REMOTO` / `LOCAL` |
| `fundicion/cmd/motor1` | `ON` / `OFF` |
| `fundicion/cmd/motor2` | `ON` / `OFF` |
| `fundicion/cmd/motor3` | `ON` / `OFF` |
| `ciudad/cmd/rele1` | `ON` / `OFF` |
| `ciudad/cmd/rele2` | `ON` / `OFF` |

Las publicaciones realizadas por Node.js quedan registradas en `state.mqtt.publications`.

## API útil

Ver estado completo:

```powershell
Invoke-RestMethod http://localhost:3000/api/state
```

Simular telemetría canónica:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/simulate/telemetry/ambientales -ContentType application/json -Body '{"temp":23.5,"humedad":65.2,"co2":22,"presion":1013.25}'
```

Simular cualquier topic MQTT:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/simulate/mqtt -ContentType application/json -Body '{"topic":"garra/estado/servo1","payload":"45"}'
```

Simular baliza entrante:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/simulate/mqtt -ContentType application/json -Body '{"topic":"/baliza/8/estado","payload":"ON"}'
```

Publicar comando por MQTT:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/mqtt/command/garra/modo -ContentType application/json -Body '{"value":"REMOTO"}'
```

Resetear estado simulado:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/simulate/reset
```

## Comandos Mosquitto

Ruta recomendada en Git Bash:

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe"
```

También puede usarse:

```bash
"C:/Program Files/mosquitto/mosquitto_pub.exe"
```

### Monitoreo

Todo el tráfico:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "#"
```

Solo balizas:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "/baliza/+/estado"
```

Solo telemetría:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "ambientales/#" -t "aerogenerador/#" -t "anemometro/#" -t "bici/#" -t "campo/#" -t "represa/#" -t "vumetro/#"
```

Solo comandos:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "garra/cmd/#" -t "grua/cmd/#" -t "chatarra/cmd/#" -t "fundicion/cmd/#" -t "ciudad/cmd/#"
```

### Publicar datos de prueba

Sensores:

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/temp" -m "23.5"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/humedad" -m "65.2"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/co2" -m "22"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/presion" -m "1013.25"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "anemometro/velocidad" -m "12.3"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "bici/rpm" -m "45.2"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "campo/humedad1" -m "42"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "campo/humedad2" -m "51"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "vumetro/nivel" -m "73"
```

Energía:

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "aerogenerador/voltaje" -m "7.4"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "represa/voltaje" -m "8.2"
```

Health / Last Will:

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/estado" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/estado/disp" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "grua/estado/disp" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ciudad/estado/disp" -m "online" -r
```

Estados de actuadores:

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/estado/modo" -m "REMOTO"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/estado/servo1" -m "45"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "grua/estado/iman" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ciudad/estado/rele1" -m "ON"
```

Balizas entrantes:

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "/baliza/8/estado" -m "ON" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "/baliza/8/estado" -m "OFF" -r
```

Comandos:

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/cmd/modo" -m "REMOTO"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/cmd/servo1" -m "45"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "grua/cmd/x" -m "IZQUIERDA"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ciudad/cmd/rele1" -m "ON"
```

## Pendientes conocidos

- No se implementó firmware en esta etapa.
- `camion` no tiene topics definidos en `referencias/mqtt_topics.md`.
- No hay topics ACK canónicos en `referencias/mqtt_topics.md`; `state.acks` queda preparado para una convención futura.
- La UI todavía no tiene controles finos para todos los actuadores; los comandos MQTT están disponibles por API.
