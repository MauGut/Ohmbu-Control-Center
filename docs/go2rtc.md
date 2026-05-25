# go2rtc

The web UI must not consume ESP32-CAM streams directly. go2rtc should ingest each camera once and expose relay URLs to the control center.

Example conceptual go2rtc config:

```yaml
streams:
  cam_garra: http://CAMERA_IP_1/stream
  cam_ciudad: http://CAMERA_IP_2/stream
  cam_grua: http://CAMERA_IP_3/stream
  cam_camion: http://CAMERA_IP_4/stream
  cam_cintas: http://CAMERA_IP_5/stream
  cam_represa: http://CAMERA_IP_6/stream
```

Current UI URL template in `config/video.json`:

```text
http://localhost:1984/api/stream.mjpeg?src={stream}
```

The final output mode can be adjusted later to MJPEG, WebRTC, MSE or snapshots without changing UI code.
