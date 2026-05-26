# go2rtc

El frontend no consume las ESP32-CAM directo. go2rtc toma cada camara una sola vez y el Control Center lee los streams desde `http://localhost:1984`.

## Binario local

Se descargo el binario oficial de Windows 64-bit:

```text
tools/go2rtc/go2rtc.exe
```

La configuracion del relay esta en:

```text
config/go2rtc.yaml
```

Arranque desde la raiz del proyecto:

```powershell
npm run go2rtc
```

WebUI de go2rtc:

```text
http://localhost:1984/
```

## Camaras ESP32-CAM

Las placas usan el ejemplo `CameraWebServer` de ESP. En ese ejemplo, la pagina principal queda en el puerto `80`, pero el stream MJPEG sale normalmente por:

```text
http://IP_DE_LA_CAMARA:81/stream
```

Configuracion actual:

```yaml
streams:
  cam_garra: http://192.168.88.150:81/stream
  cam_ciudad: http://192.168.88.151:81/stream
  cam_grua: http://192.168.88.152:81/stream
  cam_camion: http://192.168.88.153:81/stream
  cam_cintas: http://192.168.88.154:81/stream
  cam_represa: http://192.168.88.155:81/stream
  cam_extra_7: http://192.168.88.156:81/stream
  cam_extra_8: http://192.168.88.157:81/stream
```

Las primeras tres camaras existen hoy. Las otras cinco quedan reservadas para cuando se sumen mas ESP32-CAM.

## URLs de prueba

Stream desde go2rtc:

```text
http://localhost:1984/api/stream.mjpeg?src=cam_garra
http://localhost:1984/api/stream.mjpeg?src=cam_ciudad
http://localhost:1984/api/stream.mjpeg?src=cam_grua
```

Stream directo desde ESP32-CAM, util para diagnostico:

```text
http://192.168.88.150:81/stream
http://192.168.88.151:81/stream
http://192.168.88.152:81/stream
```

## Notas

- Si una camara no esta conectada, go2rtc puede iniciar igual; el error aparece al intentar abrir ese stream.
- Si el ejemplo de ESP se modifico para usar otro puerto, actualizar `config/go2rtc.yaml`.
- La UI del Control Center toma las URLs desde `config/video.json`, no desde codigo hardcodeado.
