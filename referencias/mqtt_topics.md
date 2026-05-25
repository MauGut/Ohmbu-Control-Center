# MQTT Topics — Maqueta ESP32
> Referencia completa de todos los topics, mensajes y prototipos del sistema.

---

## Convenciones

- **→ Publica**: el dispositivo envía este topic al broker.
- **← Recibe**: el dispositivo escucha este topic y reacciona.
- **Last Will**: el broker publica `offline` automáticamente si el dispositivo se desconecta.
- **retain=true**: el broker recuerda el último valor y lo reenvía a nuevos suscriptores.
- **Activación estándar**: `ON` = funcional, `OFF` = desactivado (motores/leds/salidas apagadas).
- **Activación invertida** (solo Relé Ciudad): `OFF` fuerza el relé a cortar corriente.

---

## AMBIENTALES
**Client ID:** `esp32-estacion-ambiental`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `ambientales/temp` | float ej: `23.5` | Temperatura en °C |
| → Publica | `ambientales/humedad` | float ej: `65.2` | Humedad relativa en % |
| → Publica | `ambientales/co2` | int ej: `22` | Nivel CO2 simulado (rango 5–45) |
| → Publica | `ambientales/presion` | float ej: `1013.25` | Presión atmosférica en hPa |
| → Publica | `ambientales/estado` | `online` / `offline` | Last Will |
| ← Recibe | `/baliza/7/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |

---

## AEROGENERADOR
**Client ID:** `esp32-aerogenerador`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `aerogenerador/voltaje` | float ej: `7.4` | Voltaje ficticio proporcional a RPM (rango 3–11.6 V) |
| → Publica | `aerogenerador/estado` | `online` / `offline` | Last Will |
| ← Recibe | `/baliza/6/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |

---

## ANEMÓMETRO
**Client ID:** `esp32-anemometro`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `anemometro/velocidad` | float ej: `12.3` | Velocidad del viento en km/h |
| → Publica | `anemometro/estado` | `online` / `offline` | Last Will |
| ← Recibe | `/baliza/6/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |

---

## BICI
**Client ID:** `esp32-bici`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `bici/rpm` | float ej: `45.2` | RPM de la rueda |
| → Publica | `bici/estado` | `online` / `offline` | Last Will |
| ← Recibe | `/baliza/2/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |

---

## CINTAS CHATARRA
**Client ID:** `esp32-cintas-chatarra`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `chatarra/estado/disp` | `online` / `offline` | Last Will |
| → Publica | `chatarra/estado/modo` | `REMOTO` / `LOCAL` | Modo de control activo |
| → Publica | `chatarra/estado/motor1` | `ON` / `OFF` | Estado motor 1 |
| → Publica | `chatarra/estado/motor2` | `ON` / `OFF` | Estado motor 2 |
| ← Recibe | `/baliza/1/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |
| ← Recibe | `chatarra/cmd/modo` | `REMOTO` / `LOCAL` | Toma o libera control remoto |
| ← Recibe | `chatarra/cmd/motor1` | `ON` / `OFF` | Enciende/apaga motor 1 (solo en modo REMOTO) |
| ← Recibe | `chatarra/cmd/motor2` | `ON` / `OFF` | Enciende/apaga motor 2 (solo en modo REMOTO) |

---

## CINTAS FUNDICIÓN
**Client ID:** `esp32-cintas-fundicion`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `fundicion/estado/disp` | `online` / `offline` | Last Will |
| → Publica | `fundicion/estado/modo` | `REMOTO` / `LOCAL` | Modo de control activo |
| → Publica | `fundicion/estado/motor1` | `ON` / `OFF` | Estado motor 1 |
| → Publica | `fundicion/estado/motor2` | `ON` / `OFF` | Estado motor 2 |
| → Publica | `fundicion/estado/motor3` | `ON` / `OFF` | Estado motor 3 |
| → Publica | `fundicion/estado/rele` | `ON` / `OFF` | Estado del relé (sigue siempre a motor 3) |
| ← Recibe | `/baliza/1/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |
| ← Recibe | `fundicion/cmd/modo` | `REMOTO` / `LOCAL` | Toma o libera control remoto |
| ← Recibe | `fundicion/cmd/motor1` | `ON` / `OFF` | Enciende/apaga motor 1 (solo en modo REMOTO) |
| ← Recibe | `fundicion/cmd/motor2` | `ON` / `OFF` | Enciende/apaga motor 2 (solo en modo REMOTO) |
| ← Recibe | `fundicion/cmd/motor3` | `ON` / `OFF` | Enciende/apaga motor 3 + relé (solo en modo REMOTO) |

---

## GARRA
**Client ID:** `esp32-garra`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `garra/estado/disp` | `online` / `offline` | Last Will |
| → Publica | `garra/estado/modo` | `REMOTO` / `LOCAL` | Modo de control activo |
| → Publica | `garra/estado/servo1` | int ej: `45` | Posición servo Derecha en grados (0–110) |
| → Publica | `garra/estado/servo2` | int ej: `90` | Posición servo Base en grados (0–180) |
| → Publica | `garra/estado/servo3` | int ej: `30` | Posición servo Izquierda en grados (0–60) |
| ← Recibe | `/baliza/8/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |
| ← Recibe | `garra/cmd/modo` | `REMOTO` / `LOCAL` | Toma o libera control remoto |
| ← Recibe | `garra/cmd/servo1` | int `0`–`110` | Posición servo Derecha (solo en modo REMOTO) |
| ← Recibe | `garra/cmd/servo2` | int `0`–`180` | Posición servo Base (solo en modo REMOTO) |
| ← Recibe | `garra/cmd/servo3` | int `0`–`60` | Posición servo Izquierda (solo en modo REMOTO) |

---

## GRÚA
**Client ID:** `esp32-grua`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `grua/estado/disp` | `online` / `offline` | Last Will |
| → Publica | `grua/estado/modo` | `REMOTO` / `LOCAL` | Modo de control activo |
| → Publica | `grua/estado/iman` | `ON` / `OFF` | Estado del electroimán |
| ← Recibe | `/baliza/1/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |
| ← Recibe | `grua/cmd/modo` | `REMOTO` / `LOCAL` | Toma o libera control remoto |
| ← Recibe | `grua/cmd/x` | `IZQUIERDA` / `DERECHA` / `STOP` | Mueve gancho horizontal — requiere reenvío continuo cada ≤500ms |
| ← Recibe | `grua/cmd/y` | `ADELANTE` / `ATRAS` / `STOP` | Mueve base vertical — requiere reenvío continuo cada ≤500ms |
| ← Recibe | `grua/cmd/iman` | `ON` / `OFF` | Activa/desactiva electroimán (solo en modo REMOTO) |

---

## HUMEDAD EN SUELO
**Client ID:** `esp32-humedad-campo`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `campo/humedad1` | int `0`–`100` | Humedad sensor 1 en % |
| → Publica | `campo/humedad2` | int `0`–`100` | Humedad sensor 2 en % |
| → Publica | `campo/estado` | `online` / `offline` | Last Will |
| ← Recibe | `/baliza/4/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |

---

## RELÉ CIUDAD
**Client ID:** `esp32-rele-ciudad`

> ⚠ Lógica de activación INVERTIDA respecto al resto:
> `OFF` en el topic de activación fuerza el relé a cortar corriente (ACTIVO).
> `ON` devuelve el relé a reposo y habilita comandos.

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `ciudad/estado/disp` | `online` / `offline` | Last Will |
| → Publica | `ciudad/estado/rele1` | `ON` / `OFF` | Estado relé 1 |
| → Publica | `ciudad/estado/rele2` | `ON` / `OFF` | Estado relé 2 |
| ← Recibe | `/baliza/5/estado` | `ON` / `OFF` | Habilita/deshabilita relé 1 (OFF = fuerza corte) |
| ← Recibe | `/baliza/3/estado` | `ON` / `OFF` | Habilita/deshabilita relé 2 (OFF = fuerza corte) |
| ← Recibe | `ciudad/cmd/rele1` | `ON` / `OFF` | Controla relé 1 (solo si habilitado) |
| ← Recibe | `ciudad/cmd/rele2` | `ON` / `OFF` | Controla relé 2 (solo si habilitado) |

---

## REPRESA
**Client ID:** `esp32-represa`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `represa/voltaje` | float ej: `8.2` | Voltaje ficticio según posición del pot (rango 4.6–11.6 V) |
| → Publica | `represa/estado` | `online` / `offline` | Last Will |
| ← Recibe | `/baliza/5/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |

---

## VUMETRO
**Client ID:** `esp32-vumetro`

| Dirección | Topic | Mensajes | Descripción |
|---|---|---|---|
| → Publica | `vumetro/nivel` | int `0`–`100` | Porcentaje de nivel de ruido |
| → Publica | `vumetro/estado` | `online` / `offline` | Last Will |
| ← Recibe | `/baliza/7/estado` | `ON` / `OFF` | Activa o desactiva el prototipo |

---

## Comportamiento general del control remoto

Los prototipos con control remoto (Garra, Grúa, Cintas Chatarra, Cintas Fundición)
siguen este flujo:

1. Enviar `REMOTO` al topic `[prototipo]/cmd/modo` → bloquea controles físicos, habilita comandos MQTT.
2. Enviar comandos de control al topic correspondiente.
3. Enviar `LOCAL` al topic `[prototipo]/cmd/modo` → devuelve control físico, detiene salidas por seguridad.

Si se pierde la conexión MQTT estando en modo REMOTO, el prototipo vuelve
automáticamente a modo LOCAL y detiene todas las salidas.

La Grúa además implementa un **watchdog de 500ms**: si no llega un nuevo
comando de movimiento dentro de ese tiempo, los motores se detienen solos.
