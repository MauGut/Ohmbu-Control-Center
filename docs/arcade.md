# Arcade Input

The first milestone treats the arcade USB encoder as keyboard input in the browser. There is no HID or gamepad backend dependency yet.

Mapping lives in `config/arcadeMap.json`:

- `00`: Camión, panel `camion`, scene `escena_4`
- `01`: Cinta de minerales, panel `cintas`, scene `escena_5`
- `02`: Cinta de fundicion, panel `represa`, scene `escena_6`
- `03`: Garra, panel `garra`, scene `escena_1`
- `04`: Ciudad, panel `ciudad`, scene `escena_2`
- `05`: Grúa, panel `grua`, scene `escena_3`
- `06`: contextual action 1
- `07`: contextual action 2
- `08`: accept / enter
- `09`: back
- Arrow keys: joystick directions

The browser posts input to `POST /api/arcade/input`.
