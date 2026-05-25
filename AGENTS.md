# Project: OHMBU Interactive Control Center V2

## General context

This project is a local offline interactive control system for an educational installation / physical scale model.

The system will run on a Windows 11 control PC connected to a closed local network. It coordinates:

- Physical arcade USB joystick/button interface.
- ESP32-C3 beacon/table/state nodes.
- ESP32 model sector nodes with sensors and/or actuators.
- ESP32-CAM cameras, accessed through go2rtc instead of directly.
- Web control interface.
- Web output/display interface.
- Debug/diagnostic interface.
- OSC scene messages to MagicQ and Resolume.

The system must be robust for live event usage and work fully offline.

## Core architectural decision

Use a hybrid architecture:

- MQTT for ESP32 devices, sensors, actuators, states, telemetry, commands, ACKs and health.
- WebSocket / Socket.IO for live browser updates.
- OSC for MagicQ and Resolume scene selection.
- go2rtc as video relay/proxy for ESP32-CAM streams.
- Node.js as central orchestrator.

Do not replace MQTT with OSC for ESP32 device communication.

## Role of Node.js

Node.js is the central orchestrator.

It must:

- serve the web UI;
- connect to Mosquitto MQTT;
- receive ESP32 messages;
- parse and validate messages;
- keep an in-memory global state;
- expose API endpoints;
- broadcast state updates to browsers via Socket.IO;
- send MQTT commands to ESP32 actuators;
- send OSC messages to MagicQ and Resolume;
- expose a debug page;
- read configuration files from `/config`.

## Main UI reference

The default front is the Sensors control panel.

Visual structure based on the provided reference image:

1. Left sidebar: "Panel de control"
   Buttons:
   - Sensores
   - Cámaras
   - Garra
   - Ciudad
   - Grúa
   - Camión
   - Cintas
   - Represa

2. Top center: "Estaciones"
   A horizontal row of icons representing enabled/disabled stations.
   Each icon changes from red to green depending on whether the corresponding station is enabled.
   These enabled/disabled states come from ESP32-C3 beacon/state nodes.

3. Main center: "Sensores"
   Sensor cards show values received from model sectors:
   - Humedad de tierra 1
   - Humedad de tierra 2
   - Fuego
   - Temperatura
   - Humedad
   - Ruido
   - Presión atmosférica
   - Sensor de viento
   - CO2
   - RPM de manivela

4. Lower center: "Eficiencia energética"
   Derived/calculated values from received sensor data:
   - Voltaje de represa
   - Voltaje de paneles
   - Voltaje de aerogenerador

5. Right column: "Cámaras"
   Shows up to 3 camera streams from go2rtc, selected according to the active control section or active sensor sector.

Do not attempt to exactly recreate the graphic design pixel-perfect in the first milestone.
Create a clean, dark, industrial-style responsive layout following the reference structure.

## Physical arcade USB control panel

The physical arcade interface has:

- Button 1: Garra
- Button 2: Ciudad
- Button 3: Grúa
- Button 4: Camión
- Button 5: Cintas / Chatarrera
- Button 6: Represa
- Button A: Acción contextual 1
- Button B: Acción contextual 2
- Joystick: directional control
- Button 7: Click / Menu
- Button 8: Atrás

Initial implementation assumption:

- The arcade USB board is treated as keyboard input in the browser.
- Do not add HID/gamepad backend dependencies yet.
- Keep the mapping configurable.

## UI state model

Separate:

- activePanel: which panel is visually selected in the UI.
- activeModule: which module is controlled by the arcade.
- activeScene: OSC scene selection for MagicQ/Resolume.
- station states: enabled/disabled for the six main modules plus optional inicio/final.

Default selected panel:

- sensores

Available panels:

- sensores
- camaras
- garra
- ciudad
- grua
- camion
- cintas
- represa
- debug
- output

Available modules:

- garra
- ciudad
- grua
- camion
- cintas
- represa
- none

## Arcade mapping

Create configuration for:

- key "1" -> select module garra, panel garra, scene escena_1
- key "2" -> select module ciudad, panel ciudad, scene escena_2
- key "3" -> select module grua, panel grua, scene escena_3
- key "4" -> select module camion, panel camion, scene escena_4
- key "5" -> select module cintas, panel cintas, scene escena_5
- key "6" -> select module represa, panel represa, scene escena_6
- key "a" -> contextual_action_1
- key "b" -> contextual_action_2
- key "7" -> menu_click
- key "8" -> back
- ArrowUp -> joystick_up
- ArrowDown -> joystick_down
- ArrowLeft -> joystick_left
- ArrowRight -> joystick_right

## OSC scenes

The control center must send the same OSC scene selection message to MagicQ and Resolume.

Scenes:

- escena_0: inicio del evento
- escena_1: Garra
- escena_2: Ciudad
- escena_3: Grúa
- escena_4: Camión
- escena_5: Cintas / Chatarrera
- escena_6: Represa
- escena_7: final evento

OSC sending must be configurable:

- osc.enabled
- magicq.host
- magicq.port
- resolume.host
- resolume.port
- address pattern

Initial OSC message recommendation:

Address:
- /escena

Argument:
- escena_0, escena_1, escena_2, etc.

Do not require real MagicQ or Resolume to run the first milestone. If OSC targets are unavailable, the system must still run and log the intended OSC message.

## Video architecture

Do not consume ESP32-CAM streams directly from the web UI.

Use go2rtc as relay/proxy.

go2rtc will ingest each ESP32-CAM once and the control system will read streams from go2rtc.

Create configuration for camera streams such as:

- cam_garra
- cam_ciudad
- cam_grua
- cam_camion
- cam_cintas
- cam_represa

The web UI should use camera URLs resolved from configuration.

Create config/video.json with:

- mode: "go2rtc"
- go2rtcBaseUrl
- camera stream names
- camera display groups for each panel

Do not install or manage go2rtc from Node.js in the first milestone.
Just generate config and use its URLs.

## go2rtc conceptual config

Create docs/go2rtc.md with an example:

streams:
  cam_garra: http://CAMERA_IP_1/stream
  cam_ciudad: http://CAMERA_IP_2/stream
  cam_grua: http://CAMERA_IP_3/stream

The final go2rtc stream URLs can later be adjusted depending on the best output mode:
- MJPEG
- WebRTC
- MSE
- snapshot/preview if needed

## MQTT conventions

Use Mosquitto as MQTT broker.

Expected topic families:

### Beacon / station states from ESP32-C3

These nodes control red/green station enablement.

- estaciones/{moduleId}/state
- estaciones/{moduleId}/health
- estaciones/{moduleId}/event

Example payload:

{
  "node_id": "est_garra",
  "module_id": "garra",
  "enabled": true,
  "state": "ENABLED",
  "rssi": -50,
  "uptime_ms": 12345
}

### Model sector telemetry

- maqueta/{moduleId}/telemetry
- maqueta/{moduleId}/state
- maqueta/{moduleId}/health
- maqueta/{moduleId}/cmd
- maqueta/{moduleId}/ack

Example telemetry:

{
  "node_id": "maq_garra",
  "module_id": "garra",
  "sensors": {
    "servo1": 80,
    "servo2": 120,
    "servo3": 40
  },
  "uptime_ms": 12345,
  "rssi": -50
}

### Commands to actuators

For Ciudad relay:

Topic:
- maqueta/ciudad/cmd

Payload examples:

{
  "action": "set_relay",
  "value": true
}

{
  "action": "set_relay",
  "value": false
}

For Garra:

Topic:
- maqueta/garra/cmd

Payload examples:

{
  "action": "set_mode",
  "mode": "REMOTE"
}

{
  "action": "set_servo",
  "servo": 1,
  "value": 80
}

Do not hardcode topics inside UI code.
Use configuration.

## Firmware reference files

The repository may include firmware reference files in:

- firmware-referencia/

Codex should read them to understand existing MQTT topic patterns, payloads, pins, and device behavior.

Important:

- Do not blindly rewrite firmware in the first milestone.
- Create docs/firmware-analysis.md summarizing what each firmware sends/receives.
- If firmware topics differ from the new system convention, document the mapping needed.
- Later tasks will adapt firmware one module at a time.

## Required first milestone

Build a runnable local first version of the new control center.

Use:

- Node.js
- Express
- Socket.IO
- MQTT client
- OSC sender module/stub
- JSON configuration
- static frontend or simple frontend without heavy frameworks

Do not add a database.
Keep state in memory.

Required routes:

- / or /sensores: default sensors panel
- /camaras
- /garra
- /ciudad
- /grua
- /camion
- /cintas
- /represa
- /debug
- /output

Required API:

- GET /api/state
- POST /api/arcade/input
- POST /api/scene/:sceneId
- POST /api/simulate/station/:moduleId/enabled
- POST /api/simulate/station/:moduleId/disabled
- POST /api/simulate/telemetry/:moduleId
- POST /api/simulate/reset

Required Socket.IO events:

- state:snapshot
- state:changed
- arcade:input
- scene:changed
- osc:sent
- mqtt:message

Required config files:

- config/system.json
- config/modules.json
- config/arcadeMap.json
- config/scenes.json
- config/video.json
- config/mqtt.json
- config/osc.json
- config/sensors.json

Required docs:

- docs/architecture.md
- docs/runbook.md
- docs/mqtt.md
- docs/osc.md
- docs/go2rtc.md
- docs/arcade.md
- docs/firmware-analysis.md

Required frontend behavior:

- Default page is the Sensores panel.
- The UI must visually resemble the reference layout:
  - left control sidebar
  - station icon/status row
  - sensor cards
  - energy efficiency cards
  - right camera column
- Keyboard/arcade input changes active module/panel and triggers OSC scene selection.
- Debug page shows full state, MQTT logs, OSC logs, last arcade input, active module, active panel, active scene.
- Output page is a clean display for external projection, without debug controls.

## Quality rules

- Keep code simple.
- Keep modules small.
- Configuration must be separate from runtime code.
- The app must run without hardware attached.
- MQTT connection failure must not crash the app.
- OSC target failure must not crash the app.
- go2rtc missing/unavailable must not crash the app.
- Add meaningful logs.
- Avoid adding unnecessary dependencies.
- Run basic checks after implementation.