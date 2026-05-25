# Auditoría de rutas MQTT

Esta auditoría histórica fue reemplazada por la especificación canónica en:

- `referencias/mqtt_topics.md`

Y por la implementación actual documentada en:

- `docs/MQTT_IMPLEMENTATION.md`
- `docs/MQTT_TEST_COMMANDS.md`
- `docs/mqtt.md`

## Estado actual

El sistema ya no usa como contrato MQTT el mapa anterior `estaciones/...` / `maqueta/...`.

El Control Center V2 ahora se adapta al mapa canónico de prototipos y balizas:

- Sensores y energia: `ambientales/*`, `aerogenerador/*`, `anemometro/*`, `bici/*`, `campo/*`, `represa/*`, `vumetro/*`
- Estados de actuadores: `garra/estado/*`, `grua/estado/*`, `chatarra/estado/*`, `fundicion/estado/*`, `ciudad/estado/*`
- Balizas leidas por Node.js: `/baliza/{id}/estado`
- Comandos publicados por Node.js: `garra/cmd/*`, `grua/cmd/*`, `chatarra/cmd/*`, `fundicion/cmd/*`, `ciudad/cmd/*`

Para auditoría vigente, usar `docs/MQTT_IMPLEMENTATION.md`.
