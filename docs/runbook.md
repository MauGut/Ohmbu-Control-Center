# Runbook

## Instalar

```powershell
npm install
```

## Ejecutar

```powershell
npm run dev
```

Abrir:

- `http://localhost:3000/`
- `http://localhost:3000/debug`
- `http://localhost:3000/output`

## Simulaciones HTTP

Habilitar una estacion segun sus balizas canonicas:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/simulate/station/garra/enabled
```

Enviar telemetria canonica simulada:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/simulate/telemetry/ambientales -ContentType application/json -Body '{"temp":23.5,"humedad":65.2,"co2":22,"presion":1013.25}'
```

Simular cualquier topic MQTT canonico:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/simulate/mqtt -ContentType application/json -Body '{"topic":"garra/estado/servo1","payload":"45"}'
```

Simular una baliza entrante por HTTP:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/simulate/mqtt -ContentType application/json -Body '{"topic":"/baliza/8/estado","payload":"ON"}'
```

Enviar comando canonico:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/mqtt/command/garra/modo -ContentType application/json -Body '{"value":"REMOTO"}'
```

Probar boton fisico/arcade:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/arcade/input -ContentType application/json -Body '{"key":"03"}'
```

Reset:

```powershell
Invoke-RestMethod -Method Post http://localhost:3000/api/simulate/reset
```

Ver `docs/MQTT_TEST_COMMANDS.md` para comandos `mosquitto_pub` y `mosquitto_sub`.
