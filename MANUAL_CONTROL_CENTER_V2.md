# Manual de instalación, ejecución y pruebas  
# OHMBU Control Center V2

Este documento reúne las instrucciones para instalar, configurar, ejecutar y probar el sistema **OHMBU Control Center V2** en una PC Windows 11 dentro de una red local cerrada.

El sistema está pensado para coordinar:

- interfaz web de control;
- panel físico arcade USB;
- nodos ESP32 por MQTT;
- cámaras desde go2rtc;
- envío de escenas por OSC a MagicQ y Resolume;
- páginas de debug y salida para proyección.

---

## 1. Arquitectura general

La arquitectura recomendada es híbrida:

```text
ESP32 / sensores / actuadores
        ↓ MQTT
Mosquitto MQTT Broker
        ↓
Node.js / Control Center V2
        ↓ Socket.IO
Interfaz web / Debug / Output

Node.js
        ↓ OSC
MagicQ / Resolume

ESP32-CAM
        ↓ HTTP MJPEG
go2rtc
        ↓
Interfaz web / Output / operador audiovisual
```

## 1.1 Rol de cada componente

### Node.js

Node.js es el **orquestador central** del sistema.

Sus funciones principales son:

- levantar el servidor web;
- servir las páginas del panel de control;
- conectarse a Mosquitto;
- recibir mensajes MQTT desde los ESP32;
- actualizar el estado global del sistema;
- emitir actualizaciones en tiempo real por Socket.IO;
- enviar comandos MQTT a nodos actuadores;
- enviar mensajes OSC a MagicQ y Resolume;
- mostrar logs y estado técnico en `/debug`.

### Mosquitto

Mosquitto es el **broker MQTT**.

No toma decisiones. Solo recibe y distribuye mensajes entre:

- ESP32;
- servidor Node.js;
- herramientas de prueba como `mosquitto_pub` y `mosquitto_sub`.

### MQTT

MQTT se usa para comunicación con ESP32:

- estados de estaciones;
- sensores;
- telemetría;
- health;
- comandos;
- confirmaciones o ACK.

### OSC

OSC se usa para comunicación audiovisual:

- selección de escenas en MagicQ;
- selección de escenas en Resolume.

OSC **no reemplaza MQTT** en los ESP32. MQTT queda como columna vertebral técnica de dispositivos. OSC queda como salida escénica/audiovisual.

### go2rtc

go2rtc se usa como relay/proxy de cámaras.

La idea es:

```text
ESP32-CAM → go2rtc → sistema web
```

Así evitamos que el front, el output y otras vistas llamen directamente muchas veces a la ESP32-CAM, lo que suele volver inestable el stream.

---

# 2. Requisitos de instalación

## 2.1 Software necesario

En la PC de control instalar:

- Windows 11;
- Git;
- Git Bash;
- Node.js LTS;
- Visual Studio Code;
- Mosquitto;
- go2rtc;
- navegador web actualizado.

Opcional para desarrollo:

- Arduino IDE;
- drivers USB CH340/CP210x según las placas;
- Codex en VS Code.

---

# 3. Instalación del proyecto

## 3.1 Copiar el proyecto

Ubicación recomendada:

```text
C:\ohmbu\control-center-v2
```

Desde Git Bash:

```bash
cd /c/ohmbu/control-center-v2
```

## 3.2 Instalar dependencias

```bash
npm install
```

Si en PowerShell aparece bloqueo por política de ejecución con `npm.ps1`, usar:

```powershell
npm.cmd install
```

## 3.3 Ejecutar el sistema

Desde Git Bash:

```bash
cd /c/ohmbu/control-center-v2
npm run dev
```

El sistema debería mostrar algo similar a:

```text
Control Center V2 started
http://localhost:3000
MQTT connected
```

Abrir en navegador:

```text
http://localhost:3000/
http://localhost:3000/sensores
http://localhost:3000/debug
http://localhost:3000/output
```

---

# 4. Instalación y configuración de Mosquitto

## 4.1 Descargar Mosquitto

Descargar desde:

```text
https://mosquitto.org/download/
```

Instalar la versión para Windows x64.

Ruta recomendada por defecto:

```text
C:\Program Files\mosquitto
```

## 4.2 Configurar Mosquitto para red local

Por defecto, Mosquitto puede escuchar solo en `127.0.0.1`. Para que los ESP32 se conecten desde la red, debe escuchar en `0.0.0.0`.

Abrir el Bloc de notas como administrador y editar:

```text
C:\Program Files\mosquitto\mosquitto.conf
```

Agregar al final:

```conf
listener 1883 0.0.0.0
allow_anonymous true
```

Guardar.

## 4.3 Reiniciar Mosquitto

Abrir PowerShell como administrador:

```powershell
net stop mosquitto
net start mosquitto
```

Si indica que el servicio ya estaba iniciado, está bien.

## 4.4 Verificar puerto MQTT

En PowerShell:

```powershell
netstat -ano | findstr :1883
```

Debe aparecer:

```text
0.0.0.0:1883    LISTENING
```

Si aparece solo:

```text
127.0.0.1:1883
```

los ESP32 no van a poder conectarse desde la red.

## 4.5 Habilitar firewall

En PowerShell como administrador:

```powershell
New-NetFirewallRule -DisplayName "Mosquitto MQTT 1883" -Direction Inbound -Protocol TCP -LocalPort 1883 -Action Allow
```

---

# 5. Configuraciones del proyecto

Todas las configuraciones principales están dentro de:

```text
config/
```

## 5.1 Configuración general

Archivo:

```text
config/system.json
```

Aquí se definen parámetros generales del servidor.

Ejemplo:

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 3000
  }
}
```

Si se cambia el puerto del sistema web, se modifica aquí.

## 5.2 Configuración MQTT

Archivo:

```text
config/mqtt.json
```

Aquí se configura la conexión al broker MQTT.

Ejemplo local:

```json
{
  "enabled": true,
  "brokerUrl": "mqtt://127.0.0.1:1883"
}
```

Para Node.js corriendo en la misma PC que Mosquitto, `127.0.0.1` está bien.

Para los ESP32 **no** se usa `127.0.0.1`. En los firmwares debe colocarse la IP real de la PC, por ejemplo:

```cpp
const char* MQTT_SERVER = "192.168.1.3";
```

o en red final:

```cpp
const char* MQTT_SERVER = "192.168.10.10";
```

## 5.3 Configuración de módulos

Archivo:

```text
config/modules.json
```

Define los módulos operativos:

- garra;
- ciudad;
- grua;
- camion;
- cintas;
- represa.

Estos módulos se relacionan con:

- paneles del front;
- botones arcade;
- escenas OSC;
- cámaras;
- sensores;
- tópicos MQTT.

## 5.4 Configuración del arcade

Archivo:

```text
config/arcadeMap.json
```

Mapeo base:

```text
1 → Garra
2 → Ciudad
3 → Grúa
4 → Camión
5 → Cintas / Chatarrera
6 → Represa
A → Acción contextual 1
B → Acción contextual 2
7 → Click / Menú
8 → Atrás
Flechas → Joystick
```

Si el panel arcade USB emite otras teclas, se corrige este archivo.

## 5.5 Configuración de escenas

Archivo:

```text
config/scenes.json
```

Escenas previstas:

```text
escena_0 → inicio del evento
escena_1 → Garra
escena_2 → Ciudad
escena_3 → Grúa
escena_4 → Camión
escena_5 → Cintas / Chatarrera
escena_6 → Represa
escena_7 → final evento
```

## 5.6 Configuración OSC

Archivo:

```text
config/osc.json
```

Aquí se configuran los destinos OSC para MagicQ y Resolume.

Ejemplo:

```json
{
  "enabled": true,
  "address": "/escena",
  "targets": {
    "magicq": {
      "enabled": true,
      "host": "192.168.10.20",
      "port": 8000
    },
    "resolume": {
      "enabled": true,
      "host": "192.168.10.21",
      "port": 7000
    }
  }
}
```

Si MagicQ y Resolume están en la misma PC, pueden compartir IP y usar distintos puertos.

Si todavía no se conectan los softwares, puede dejarse:

```json
"enabled": false
```

o deshabilitar cada target.

## 5.7 Configuración de video / go2rtc

Archivo:

```text
config/video.json
```

Define:

- modo de video;
- URL base de go2rtc;
- cámaras disponibles;
- cámaras asociadas a cada módulo/panel.

Ejemplo conceptual:

```json
{
  "mode": "go2rtc",
  "go2rtcBaseUrl": "http://192.168.10.10:1984",
  "cameras": {
    "cam_garra": {
      "name": "Cámara Garra",
      "stream": "cam_garra"
    },
    "cam_ciudad": {
      "name": "Cámara Ciudad",
      "stream": "cam_ciudad"
    }
  }
}
```

La IP y el puerto dependen de dónde esté corriendo go2rtc.

---

# 6. Rutas de comunicación MQTT

## 6.1 Estaciones / balizas ESP32-C3

Las balizas informan si una estación está habilitada o no.

### Estado

```text
estaciones/{moduleId}/state
```

Ejemplo:

```text
estaciones/garra/state
estaciones/ciudad/state
estaciones/grua/state
```

Payload:

```json
{
  "node_id": "est_garra",
  "module_id": "garra",
  "enabled": true,
  "state": "ENABLED",
  "rssi": -50,
  "uptime_ms": 12345
}
```

### Health

```text
estaciones/{moduleId}/health
```

Payload:

```json
{
  "node_id": "est_garra",
  "module_id": "garra",
  "online": true,
  "rssi": -51,
  "uptime_ms": 23456
}
```

### Evento

```text
estaciones/{moduleId}/event
```

Payload:

```json
{
  "node_id": "est_garra",
  "module_id": "garra",
  "type": "enabled",
  "enabled": true,
  "ts": 12345
}
```

## 6.2 Nodos de maqueta / sensores / actuadores

### Telemetría

```text
maqueta/{moduleId}/telemetry
```

Ejemplo:

```text
maqueta/garra/telemetry
maqueta/ciudad/telemetry
maqueta/represa/telemetry
```

Payload:

```json
{
  "node_id": "maq_represa",
  "module_id": "represa",
  "sensors": {
    "voltaje_represa": 110.5,
    "caudal": 60,
    "rpm": 70
  },
  "uptime_ms": 12345,
  "rssi": -48
}
```

### Estado

```text
maqueta/{moduleId}/state
```

Payload:

```json
{
  "node_id": "maq_ciudad",
  "module_id": "ciudad",
  "state": "ACTIVE",
  "relay": true,
  "uptime_ms": 12345
}
```

### Health

```text
maqueta/{moduleId}/health
```

Payload:

```json
{
  "node_id": "maq_ciudad",
  "module_id": "ciudad",
  "online": true,
  "rssi": -49,
  "uptime_ms": 45678
}
```

### Comandos hacia ESP32

```text
maqueta/{moduleId}/cmd
```

Ejemplo para Ciudad:

```text
maqueta/ciudad/cmd
```

Payload para encender relé:

```json
{
  "action": "set_relay",
  "value": true
}
```

Payload para apagar relé:

```json
{
  "action": "set_relay",
  "value": false
}
```

Ejemplo para Garra:

```text
maqueta/garra/cmd
```

Modo remoto:

```json
{
  "action": "set_mode",
  "mode": "REMOTE"
}
```

Mover servo:

```json
{
  "action": "set_servo",
  "servo": 1,
  "value": 80
}
```

### Confirmación / ACK

```text
maqueta/{moduleId}/ack
```

Payload:

```json
{
  "node_id": "maq_garra",
  "module_id": "garra",
  "action": "set_servo",
  "status": "ok",
  "uptime_ms": 34567
}
```

---

# 7. Rutas web del sistema

Con el sistema ejecutándose:

```text
http://localhost:3000/
http://localhost:3000/sensores
http://localhost:3000/camaras
http://localhost:3000/garra
http://localhost:3000/ciudad
http://localhost:3000/grua
http://localhost:3000/camion
http://localhost:3000/cintas
http://localhost:3000/represa
http://localhost:3000/debug
http://localhost:3000/output
```

## 7.1 Panel principal

```text
/
```

o:

```text
/sensores
```

Muestra:

- panel de control lateral;
- estaciones;
- sensores;
- eficiencia energética;
- cámaras desde go2rtc.

## 7.2 Debug

```text
/debug
```

Muestra:

- estado global;
- MQTT;
- OSC;
- último input arcade;
- módulo activo;
- escena activa;
- logs.

## 7.3 Output

```text
/output
```

Vista limpia para salida audiovisual o proyección.

---

# 8. Comandos de prueba desde consola

## 8.1 Verificar Mosquitto local

Terminal 1:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "test/demo" -v
```

Terminal 2:

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "test/demo" -m "hola mqtt"
```

Resultado esperado en Terminal 1:

```text
test/demo hola mqtt
```

## 8.2 Verificar Mosquitto por IP real

Buscar IP de la PC:

```bash
ipconfig
```

Ejemplo:

```text
192.168.1.3
```

Terminal 1:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 192.168.1.3 -t "test/demo" -v
```

Terminal 2:

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 192.168.1.3 -t "test/demo" -m "hola desde ip real"
```

## 8.3 Escuchar todo MQTT del sistema

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "#" -v
```

O solo estaciones:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "estaciones/#" -v
```

O solo maqueta:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "maqueta/#" -v
```

---

# 9. Pruebas MQTT contra el sistema

## 9.1 Habilitar estación Garra

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "estaciones/garra/state" -m "{\"node_id\":\"est_garra\",\"module_id\":\"garra\",\"enabled\":true,\"state\":\"ENABLED\",\"rssi\":-50,\"uptime_ms\":12345}"
```

Resultado esperado:

- icono de Garra pasa a verde;
- `/debug` registra el cambio.

## 9.2 Deshabilitar estación Garra

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "estaciones/garra/state" -m "{\"node_id\":\"est_garra\",\"module_id\":\"garra\",\"enabled\":false,\"state\":\"DISABLED\",\"rssi\":-52,\"uptime_ms\":22345}"
```

Resultado esperado:

- icono de Garra pasa a rojo.

## 9.3 Habilitar todas las estaciones

```bash
for m in garra ciudad grua camion cintas represa; do
  "/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "estaciones/$m/state" -m "{\"node_id\":\"est_$m\",\"module_id\":\"$m\",\"enabled\":true,\"state\":\"ENABLED\",\"rssi\":-50,\"uptime_ms\":12345}"
done
```

## 9.4 Deshabilitar todas las estaciones

```bash
for m in garra ciudad grua camion cintas represa; do
  "/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "estaciones/$m/state" -m "{\"node_id\":\"est_$m\",\"module_id\":\"$m\",\"enabled\":false,\"state\":\"DISABLED\",\"rssi\":-55,\"uptime_ms\":22345}"
done
```

---

# 10. Pruebas de telemetría

## 10.1 Telemetría Represa

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "maqueta/represa/telemetry" -m "{\"node_id\":\"maq_represa\",\"module_id\":\"represa\",\"sensors\":{\"voltaje_represa\":110.5,\"caudal\":60,\"rpm\":70},\"uptime_ms\":12345,\"rssi\":-48}"
```

## 10.2 Telemetría Garra

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "maqueta/garra/telemetry" -m "{\"node_id\":\"maq_garra\",\"module_id\":\"garra\",\"sensors\":{\"servo1\":80,\"servo2\":120,\"servo3\":40,\"mode\":\"REMOTE\",\"enabled\":true},\"uptime_ms\":12345,\"rssi\":-50}"
```

## 10.3 Telemetría Ciudad

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "maqueta/ciudad/telemetry" -m "{\"node_id\":\"maq_ciudad\",\"module_id\":\"ciudad\",\"sensors\":{\"relay\":true,\"temperatura\":24,\"humedad\":55},\"uptime_ms\":12345,\"rssi\":-49}"
```

---

# 11. Pruebas de comandos a actuadores

Estos comandos se publican hacia los ESP32. Si no hay ESP32 conectado, solo se verán si se está escuchando `maqueta/#`.

## 11.1 Ciudad: encender relé

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "maqueta/ciudad/cmd" -m "{\"action\":\"set_relay\",\"value\":true}"
```

## 11.2 Ciudad: apagar relé

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "maqueta/ciudad/cmd" -m "{\"action\":\"set_relay\",\"value\":false}"
```

## 11.3 Garra: modo remoto

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "maqueta/garra/cmd" -m "{\"action\":\"set_mode\",\"mode\":\"REMOTE\"}"
```

## 11.4 Garra: mover servo 1

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "maqueta/garra/cmd" -m "{\"action\":\"set_servo\",\"servo\":1,\"value\":80}"
```

## 11.5 Garra: mover servo 2

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "maqueta/garra/cmd" -m "{\"action\":\"set_servo\",\"servo\":2,\"value\":120}"
```

## 11.6 Garra: mover servo 3

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "maqueta/garra/cmd" -m "{\"action\":\"set_servo\",\"servo\":3,\"value\":40}"
```

---

# 12. Pruebas por API HTTP

## 12.1 Estado del sistema

```bash
curl.exe http://localhost:3000/api/state
```

## 12.2 Simular estación habilitada

```bash
curl.exe -X POST http://localhost:3000/api/simulate/station/garra/enabled
```

## 12.3 Simular estación deshabilitada

```bash
curl.exe -X POST http://localhost:3000/api/simulate/station/garra/disabled
```

## 12.4 Simular telemetría

```bash
curl.exe -X POST http://localhost:3000/api/simulate/telemetry/represa
```

```bash
curl.exe -X POST http://localhost:3000/api/simulate/telemetry/garra
```

## 12.5 Reset general

```bash
curl.exe -X POST http://localhost:3000/api/simulate/reset
```

---

# 13. Pruebas de arcade

## 13.1 Desde API

Garra:

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"1\"}"
```

Ciudad:

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"2\"}"
```

Grúa:

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"3\"}"
```

Camión:

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"4\"}"
```

Cintas:

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"5\"}"
```

Represa:

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"6\"}"
```

Acción contextual A:

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"a\"}"
```

Acción contextual B:

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"b\"}"
```

Joystick arriba:

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"ArrowUp\"}"
```

Atrás:

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"8\"}"
```

## 13.2 Desde teclado / arcade físico

Con la página web abierta y con foco en el navegador:

```text
1 → Garra
2 → Ciudad
3 → Grúa
4 → Camión
5 → Cintas
6 → Represa
A → Acción contextual 1
B → Acción contextual 2
7 → Menú / Click
8 → Atrás
Flechas → Joystick
```

Si el arcade físico emite otras teclas, editar:

```text
config/arcadeMap.json
```

---

# 14. Pruebas de OSC

## 14.1 Configuración OSC

Editar:

```text
config/osc.json
```

Ejemplo:

```json
{
  "enabled": true,
  "address": "/escena",
  "targets": {
    "magicq": {
      "enabled": true,
      "host": "192.168.10.20",
      "port": 8000
    },
    "resolume": {
      "enabled": true,
      "host": "192.168.10.21",
      "port": 7000
    }
  }
}
```

## 14.2 Escenas OSC

El sistema envía el mismo mensaje a MagicQ y Resolume:

```text
/escena escena_0
/escena escena_1
/escena escena_2
/escena escena_3
/escena escena_4
/escena escena_5
/escena escena_6
/escena escena_7
```

Equivalencias:

```text
escena_0 → inicio del evento
escena_1 → Garra
escena_2 → Ciudad
escena_3 → Grúa
escena_4 → Camión
escena_5 → Cintas / Chatarrera
escena_6 → Represa
escena_7 → final evento
```

## 14.3 Enviar escena por API

```bash
curl.exe -X POST http://localhost:3000/api/scene/escena_0
```

```bash
curl.exe -X POST http://localhost:3000/api/scene/escena_1
```

```bash
curl.exe -X POST http://localhost:3000/api/scene/escena_2
```

```bash
curl.exe -X POST http://localhost:3000/api/scene/escena_3
```

```bash
curl.exe -X POST http://localhost:3000/api/scene/escena_4
```

```bash
curl.exe -X POST http://localhost:3000/api/scene/escena_5
```

```bash
curl.exe -X POST http://localhost:3000/api/scene/escena_6
```

```bash
curl.exe -X POST http://localhost:3000/api/scene/escena_7
```

Resultado esperado:

- `/debug` muestra el evento OSC;
- si MagicQ/Resolume están configurados, reciben `/escena escena_X`.

## 14.4 Enviar escena desde arcade

Al presionar:

```text
1 → escena_1
2 → escena_2
3 → escena_3
4 → escena_4
5 → escena_5
6 → escena_6
```

El sistema debe:

- cambiar módulo activo;
- cambiar panel activo;
- enviar OSC a MagicQ;
- enviar OSC a Resolume;
- registrar en `/debug`.

---

# 15. Configuración básica de MagicQ / Resolume

## 15.1 MagicQ

En MagicQ, habilitar entrada OSC o red según la configuración del show.

Datos que deben coincidir con `config/osc.json`:

```text
IP de PC MagicQ
Puerto OSC de MagicQ
Dirección OSC esperada: /escena
Argumento esperado: escena_1, escena_2, etc.
```

Si MagicQ no responde, verificar:

- firewall;
- IP;
- puerto;
- que MagicQ tenga habilitada la recepción OSC/red;
- que ambas PCs estén en la misma red.

## 15.2 Resolume

En Resolume, habilitar OSC input.

Datos que deben coincidir:

```text
IP de PC Resolume
Puerto OSC de Resolume
Dirección OSC: /escena
Argumento: escena_1, escena_2, etc.
```

En Resolume puede ser necesario mapear la dirección `/escena` o usar un patch/intermediario si se requiere lógica específica.

---

# 16. go2rtc

## 16.1 Configuración conceptual

Archivo de go2rtc, por ejemplo:

```text
go2rtc.yaml
```

Ejemplo:

```yaml
streams:
  cam_garra: http://192.168.10.101/stream
  cam_ciudad: http://192.168.10.102/stream
  cam_grua: http://192.168.10.103/stream
  cam_camion: http://192.168.10.104/stream
  cam_cintas: http://192.168.10.105/stream
  cam_represa: http://192.168.10.106/stream
```

La idea es que go2rtc sea el único cliente directo de cada ESP32-CAM.

## 16.2 Probar go2rtc

Si go2rtc corre en la PC de control:

```text
http://localhost:1984
```

o desde otra PC:

```text
http://IP_PC_CONTROL:1984
```

## 16.3 Configurar el sistema para consumir go2rtc

Editar:

```text
config/video.json
```

Actualizar la URL base:

```json
{
  "mode": "go2rtc",
  "go2rtcBaseUrl": "http://192.168.10.10:1984"
}
```

---

# 17. Checklist de arranque en evento

1. Encender router / red local.
2. Encender PC de control.
3. Verificar IP de PC de control.
4. Verificar Mosquitto:
   ```powershell
   netstat -ano | findstr :1883
   ```
5. Verificar que escucha:
   ```text
   0.0.0.0:1883 LISTENING
   ```
6. Levantar go2rtc.
7. Probar una cámara desde go2rtc.
8. Levantar sistema:
   ```bash
   cd /c/ohmbu/control-center-v2
   npm run dev
   ```
9. Abrir:
   ```text
   http://localhost:3000/
   http://localhost:3000/debug
   http://localhost:3000/output
   ```
10. Probar MQTT con una estación.
11. Probar joystick arcade.
12. Probar escena OSC.
13. Encender ESP32.
14. Verificar que `/debug` reciba health.
15. Probar recorrido completo.

---

# 18. Problemas frecuentes

## 18.1 Node no conecta a MQTT

Revisar:

```text
config/mqtt.json
```

Verificar que Mosquitto esté iniciado:

```powershell
net start mosquitto
```

Verificar puerto:

```powershell
netstat -ano | findstr :1883
```

## 18.2 ESP32 no conecta a MQTT

Revisar que el firmware tenga la IP real de la PC, no `127.0.0.1`.

Correcto:

```cpp
const char* MQTT_SERVER = "192.168.10.10";
```

Incorrecto:

```cpp
const char* MQTT_SERVER = "127.0.0.1";
```

Verificar también:

- firewall;
- Mosquitto con `listener 1883 0.0.0.0`;
- ESP32 en la misma red.

## 18.3 Las cámaras se cortan

No abrir la ESP32-CAM directamente desde varias páginas.

Usar:

```text
ESP32-CAM → go2rtc → sistema web
```

Si la cámara se corta igual:

- bajar resolución;
- bajar FPS;
- mejorar alimentación;
- usar fuente 5V estable;
- verificar señal WiFi.

## 18.4 El arcade no responde

Verificar si Windows lo reconoce como teclado:

- abrir Bloc de notas;
- presionar botones;
- ver qué caracteres genera.

Si las teclas no coinciden, editar:

```text
config/arcadeMap.json
```

## 18.5 OSC no llega

Revisar:

- `config/osc.json`;
- IP correcta de MagicQ/Resolume;
- puerto correcto;
- firewall;
- OSC habilitado en el software destino.

---

# 19. Comandos rápidos

## Ejecutar sistema

```bash
cd /c/ohmbu/control-center-v2
npm run dev
```

## Estado

```bash
curl.exe http://localhost:3000/api/state
```

## Reset

```bash
curl.exe -X POST http://localhost:3000/api/simulate/reset
```

## Garra por arcade

```bash
curl.exe -X POST http://localhost:3000/api/arcade/input -H "Content-Type: application/json" -d "{\"input\":\"1\"}"
```

## Escena 1

```bash
curl.exe -X POST http://localhost:3000/api/scene/escena_1
```

## Estación Garra ON

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "estaciones/garra/state" -m "{\"node_id\":\"est_garra\",\"module_id\":\"garra\",\"enabled\":true,\"state\":\"ENABLED\",\"rssi\":-50,\"uptime_ms\":12345}"
```

## Telemetría Represa

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "maqueta/represa/telemetry" -m "{\"node_id\":\"maq_represa\",\"module_id\":\"represa\",\"sensors\":{\"voltaje_represa\":110.5,\"caudal\":60,\"rpm\":70},\"uptime_ms\":12345,\"rssi\":-48}"
```

---

# 20. Conclusión

El sistema debe funcionar como un centro de orquestación:

```text
ESP32 → MQTT → Node.js → Estado global → Front / Output
Node.js → OSC → MagicQ / Resolume
ESP32-CAM → go2rtc → Front / Output
Arcade USB → navegador/API → Node.js → estado/escenas/comandos
```

Para una prueba offline, lo fundamental es verificar primero:

1. Mosquitto activo.
2. Node conectado.
3. MQTT simulado funcionando.
4. Arcade funcionando.
5. OSC registrado en debug.
6. go2rtc levantando cámaras.
7. ESP32 conectando con IP correcta del broker.
