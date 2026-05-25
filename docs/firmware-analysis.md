# Firmware Analysis

Fuente canónica actual: `referencias/mqtt_topics.md`.

Los archivos en `firmware-referencia/` fueron revisados como referencia de comportamiento existente. No se modifica firmware en esta tarea.

## Resultado

El mapa canónico de `referencias/mqtt_topics.md` coincide con la familia legacy real observada en los sketches: topics escalares por prototipo, comandos por prototipo y activación por `/baliza/N/estado`.

## Resumen por firmware

| Firmware | Publica | Recibe |
| --- | --- | --- |
| `Ambientales.ino` | `ambientales/temp`, `ambientales/humedad`, `ambientales/co2`, `ambientales/presion`, `ambientales/estado` | `/baliza/7/estado` |
| `Anemometro.ino` | `anemometro/velocidad`, `anemometro/estado` | `/baliza/6/estado` |
| `Bici.ino` | `bici/rpm`, `bici/estado` | `bici/activado` en firmware actual; la referencia canónica indica `/baliza/2/estado` |
| `Vumetro.ino` | `vumetro/nivel`, `vumetro/estado` | `/baliza/7/estado` |
| `Humedad_en_suelo_x2.ino` | `campo/humedad1`, `campo/humedad2`, `campo/estado` | `/baliza/4/estado` |
| `baliza_mqtt-v2.ino` | `/baliza/{id}/estado` | `/baliza/{id}/cmd` |
| `Garra.ino` | `garra/estado/disp`, `garra/estado/modo`, `garra/estado/servo1`, `garra/estado/servo2`, `garra/estado/servo3` | `/baliza/8/estado`, `garra/cmd/modo`, `garra/cmd/servo1`, `garra/cmd/servo2`, `garra/cmd/servo3` |
| `Grua.ino` | `grua/estado/disp`, `grua/estado/modo`, `grua/estado/iman` | `grua/activado` en firmware actual; la referencia canónica indica `/baliza/1/estado`, mas `grua/cmd/*` |
| `camion_esp32_v2.ino` | `camion/STE/DIR`, `camion/STE/LDR`, `camion/STE/MODO` | `camion/CMD` |
| `Cintas_Chatarra.ino` | `chatarra/estado/disp`, `chatarra/estado/modo`, `chatarra/estado/motor1`, `chatarra/estado/motor2` | `/baliza/1/estado`, `chatarra/cmd/*` |
| `Cintas_Fundicion.ino` | `fundicion/estado/disp`, `fundicion/estado/modo`, `fundicion/estado/motor1`, `fundicion/estado/motor2`, `fundicion/estado/motor3`, `fundicion/estado/rele` | `/baliza/1/estado`, `fundicion/cmd/*` |
| `rele_ciudad.ino` | `ciudad/estado/disp`, `ciudad/estado/rele1`, `ciudad/estado/rele2` | `/baliza/5/estado`, `/baliza/3/estado`, `ciudad/cmd/rele1`, `ciudad/cmd/rele2` |

## Mismatches firmware vs referencia canónica

- `Bici.ino` usa `bici/activado`, mientras `referencias/mqtt_topics.md` define `/baliza/2/estado`.
- `Grua.ino` usa `grua/activado`, mientras `referencias/mqtt_topics.md` define `/baliza/1/estado`.
- `camion_esp32_v2.ino` tiene topics propios (`camion/CMD`, `camion/STE/*`) pero `referencias/mqtt_topics.md` no define una sección de Camión.
- `aerogenerador/voltaje` aparece en la referencia canónica, pero no hay sketch de aerogenerador en `firmware-referencia/`.

## Adaptación pendiente

La implementación Node.js sigue `referencias/mqtt_topics.md`, no el antiguo mapa normalizado `estaciones/...` / `maqueta/...`.

Pendientes de firmware, para una tarea futura:

- Alinear Bici a `/baliza/2/estado`.
- Alinear Grúa a `/baliza/1/estado`.
- Confirmar o agregar firmware de Aerogenerador.
- Definir si Camión queda fuera del mapa canónico actual o si se debe agregar a `referencias/mqtt_topics.md`.
