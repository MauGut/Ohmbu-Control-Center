# MQTT Test Commands

Comandos para Git Bash en Windows. Si preferis, reemplaza la ruta por `"C:/Program Files/mosquitto/mosquitto_pub.exe"`.

## Monitor

Todo el trafico:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "#"
```

Solo balizas:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "/baliza/+/estado"
```

Solo telemetria/sensores:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "ambientales/#" -t "aerogenerador/#" -t "anemometro/#" -t "bici/#" -t "campo/#" -t "represa/#" -t "vumetro/#"
```

Solo comandos:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "garra/cmd/#" -t "grua/cmd/#" -t "chatarra/cmd/#" -t "fundicion/cmd/#" -t "ciudad/cmd/#"
```

Solo ACKs:

```bash
"/c/Program Files/mosquitto/mosquitto_sub.exe" -h 127.0.0.1 -t "+/ack/#"
```

Nota: `referencias/mqtt_topics.md` no define topics ACK canonicos; este monitor queda reservado para una futura convencion.

## Sensores y energia

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/temp" -m "23.5"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/humedad" -m "65.2"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/co2" -m "22"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/presion" -m "1013.25"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "aerogenerador/voltaje" -m "7.4"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "anemometro/velocidad" -m "12.3"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "bici/rpm" -m "45.2"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "campo/humedad1" -m "42"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "campo/humedad2" -m "51"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "represa/voltaje" -m "8.2"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "vumetro/nivel" -m "73"
```

## Health / Last Will

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ambientales/estado" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "aerogenerador/estado" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "anemometro/estado" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "bici/estado" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "campo/estado" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "represa/estado" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "vumetro/estado" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/estado/disp" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "grua/estado/disp" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "chatarra/estado/disp" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "fundicion/estado/disp" -m "online" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ciudad/estado/disp" -m "online" -r
```

## Estados de actuadores

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/estado/modo" -m "REMOTO"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/estado/servo1" -m "45"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/estado/servo2" -m "90"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/estado/servo3" -m "30"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "grua/estado/modo" -m "REMOTO"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "grua/estado/iman" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "chatarra/estado/modo" -m "REMOTO"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "chatarra/estado/motor1" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "chatarra/estado/motor2" -m "OFF"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "fundicion/estado/modo" -m "REMOTO"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "fundicion/estado/motor1" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "fundicion/estado/motor2" -m "OFF"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "fundicion/estado/motor3" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "fundicion/estado/rele" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ciudad/estado/rele1" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ciudad/estado/rele2" -m "OFF"
```

## Balizas entrantes

Estos comandos simulan mensajes publicados por las balizas. El Control Center los lee; no los publica.

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "/baliza/1/estado" -m "ON" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "/baliza/2/estado" -m "ON" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "/baliza/3/estado" -m "ON" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "/baliza/4/estado" -m "ON" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "/baliza/5/estado" -m "ON" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "/baliza/6/estado" -m "ON" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "/baliza/7/estado" -m "ON" -r
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "/baliza/8/estado" -m "ON" -r
```

## Comandos

```bash
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/cmd/modo" -m "REMOTO"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/cmd/servo1" -m "45"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/cmd/servo2" -m "90"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "garra/cmd/servo3" -m "30"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "grua/cmd/modo" -m "REMOTO"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "grua/cmd/x" -m "IZQUIERDA"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "grua/cmd/y" -m "ADELANTE"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "grua/cmd/iman" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "chatarra/cmd/modo" -m "REMOTO"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "chatarra/cmd/motor1" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "chatarra/cmd/motor2" -m "OFF"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "fundicion/cmd/modo" -m "REMOTO"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "fundicion/cmd/motor1" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "fundicion/cmd/motor2" -m "OFF"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "fundicion/cmd/motor3" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ciudad/cmd/rele1" -m "ON"
"/c/Program Files/mosquitto/mosquitto_pub.exe" -h 127.0.0.1 -t "ciudad/cmd/rele2" -m "OFF"
```
