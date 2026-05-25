const path = require("path");
const express = require("express");
const http = require("http");
const { Server } = require("socket.io");
const { loadConfig } = require("./config");
const { createStore } = require("./state");
const { createOscAdapter } = require("./oscAdapter");
const { createArcadeHandler } = require("./arcade");
const { createMqttAdapter } = require("./mqttAdapter");

const config = loadConfig();
const app = express();
const server = http.createServer(app);
const io = new Server(server);
const store = createStore(config);
const oscAdapter = createOscAdapter(config, store);
const arcade = createArcadeHandler(config, store, oscAdapter);
const mqttAdapter = createMqttAdapter(config, store);

app.use(express.json({ limit: "1mb" }));
app.use(express.static(path.join(config.root, "public")));

store.subscribe((event, payload, state) => {
  io.emit(event, payload);
  io.emit("state:snapshot", state);
});

io.on("connection", (socket) => {
  socket.emit("state:snapshot", store.snapshot());
});

const panels = ["/", "/sensores", "/camaras", "/garra", "/ciudad", "/grua", "/camion", "/cintas", "/represa", "/debug", "/output"];
for (const route of panels) {
  app.get(route, (req, res) => {
    const panel = req.path === "/" ? "sensores" : req.path.slice(1);
    if (config.modules.panels.includes(panel)) store.setPanel(panel);
    res.sendFile(path.join(config.root, "public", "index.html"));
  });
}

app.get("/api/state", (_req, res) => res.json(store.snapshot()));

app.post("/api/arcade/input", (req, res) => {
  const result = arcade.handleInput(req.body || {});
  res.status(result.ok ? 200 : 400).json(result);
});

app.post("/api/scene/:sceneId", (req, res) => {
  const ok = store.setScene(req.params.sceneId, "api");
  if (!ok) return res.status(404).json({ ok: false, error: "Unknown scene" });
  oscAdapter.sendScene(req.params.sceneId, "api");
  res.json({ ok: true, sceneId: req.params.sceneId });
});

app.post("/api/simulate/station/:moduleId/enabled", (req, res) => {
  const module = config.modules.modules.find((item) => item.id === req.params.moduleId);
  if (!module) return res.status(404).json({ ok: false, error: "Unknown module" });
  for (const balizaId of module.balizas || []) {
    const topic = config.mqtt.balizas[balizaId]?.topic;
    if (topic) mqttAdapter.applyCanonicalMessage(topic, "ON", "simulation");
  }
  res.json({ ok: true, moduleId: req.params.moduleId, enabled: true, balizas: module.balizas || [] });
});

app.post("/api/simulate/station/:moduleId/disabled", (req, res) => {
  const module = config.modules.modules.find((item) => item.id === req.params.moduleId);
  if (!module) return res.status(404).json({ ok: false, error: "Unknown module" });
  for (const balizaId of module.balizas || []) {
    const topic = config.mqtt.balizas[balizaId]?.topic;
    if (topic) mqttAdapter.applyCanonicalMessage(topic, "OFF", "simulation");
  }
  res.json({ ok: true, moduleId: req.params.moduleId, enabled: false, balizas: module.balizas || [] });
});

app.post("/api/simulate/telemetry/:moduleId", (req, res) => {
  const topicMap = {
    ambientales: {
      temp: "ambientales/temp",
      temperature: "ambientales/temp",
      humedad: "ambientales/humedad",
      humidity: "ambientales/humedad",
      co2: "ambientales/co2",
      presion: "ambientales/presion",
      pressure: "ambientales/presion"
    },
    aerogenerador: { voltaje: "aerogenerador/voltaje", voltage: "aerogenerador/voltaje", wind_voltage: "aerogenerador/voltaje" },
    anemometro: { velocidad: "anemometro/velocidad", wind: "anemometro/velocidad" },
    bici: { rpm: "bici/rpm", crank_rpm: "bici/rpm" },
    campo: { humedad1: "campo/humedad1", humedad2: "campo/humedad2", soil_moisture_1: "campo/humedad1", soil_moisture_2: "campo/humedad2" },
    represa: { voltaje: "represa/voltaje", dam_voltage: "represa/voltaje" },
    vumetro: { nivel: "vumetro/nivel", noise: "vumetro/nivel" }
  };
  const body = req.body || {};
  const values = body.sensors || body.energy || body;
  const applied = [];
  if (body.topic && Object.prototype.hasOwnProperty.call(body, "payload")) {
    mqttAdapter.applyCanonicalMessage(body.topic, body.payload, "simulation");
    applied.push(body.topic);
  } else {
    const moduleMap = topicMap[req.params.moduleId] || {};
    for (const [key, value] of Object.entries(values)) {
      const topic = moduleMap[key];
      if (topic) {
        mqttAdapter.applyCanonicalMessage(topic, value, "simulation");
        applied.push(topic);
      }
    }
  }
  res.json({ ok: true, moduleId: req.params.moduleId, applied });
});

app.post("/api/simulate/reset", (_req, res) => {
  store.reset();
  res.json({ ok: true });
});

app.post("/api/simulate/mqtt", (req, res) => {
  const { topic, payload } = req.body || {};
  if (!topic || !Object.prototype.hasOwnProperty.call(req.body || {}, "payload")) {
    return res.status(400).json({ ok: false, error: "Expected topic and payload" });
  }
  const result = mqttAdapter.applyCanonicalMessage(topic, payload, "simulation");
  res.status(result.handled ? 200 : 400).json({ ok: result.handled, ...result });
});

app.post("/api/mqtt/command/:prototypeId/:command", (req, res) => {
  const value = req.body?.value;
  if (typeof value === "undefined") return res.status(400).json({ ok: false, error: "Expected value" });
  const ok = mqttAdapter.publishCommand(req.params.prototypeId, req.params.command, value, { source: "api" });
  res.status(ok ? 200 : 400).json({ ok, prototypeId: req.params.prototypeId, command: req.params.command, value });
});

const port = Number(process.env.PORT || config.system.port || 3000);
server.listen(port, config.system.host, () => {
  console.log(`[OHMBU] Control Center listening on http://localhost:${port}`);
});
