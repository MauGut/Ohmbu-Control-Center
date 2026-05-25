# OSC

OSC is configured in `config/osc.json`.

Default message:

- Address: `/escena`
- Argument: scene id string, for example `escena_1`

The same message is sent to:

- MagicQ: `magicq.host:magicq.port`
- Resolume: `resolume.host:resolume.port`

Scenes:

- `escena_0`: Inicio del evento
- `escena_1`: Garra
- `escena_2`: Ciudad
- `escena_3`: Grúa
- `escena_4`: Camión
- `escena_5`: Cintas / Chatarrera
- `escena_6`: Represa
- `escena_7`: Final evento

If targets are unavailable, the adapter still logs intended sends and the app continues running.
