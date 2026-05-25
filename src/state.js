function now() {
  return new Date().toISOString();
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function same(a, b) {
  return JSON.stringify(a) === JSON.stringify(b);
}

function createInitialState(config) {
  const stations = {};
  for (const module of config.modules.modules) {
    stations[module.id] = {
      id: module.id,
      label: module.display || module.label,
      enabled: false,
      state: "OFF",
      balizas: module.balizas || [],
      prototypes: module.prototypes || [],
      lastSeen: null
    };
  }
  for (const station of config.modules.optionalStations) {
    stations[station.id] = {
      id: station.id,
      label: station.label,
      enabled: false,
      state: "OFF",
      balizas: [],
      prototypes: [],
      lastSeen: null
    };
  }

  const balizas = {};
  for (const [id, item] of Object.entries(config.mqtt.balizas || {})) {
    balizas[id] = {
      id,
      label: item.label || `Baliza ${id}`,
      topic: item.topic,
      state: "OFF",
      enabled: false,
      modules: item.modules || [],
      prototypes: item.prototypes || [],
      logic: item.logic || "standard",
      channel: item.channel || null,
      updatedAt: null,
      source: "default"
    };
  }

  const sensors = {};
  for (const item of config.sensors.sensors) {
    sensors[item.id] = { ...item, value: item.default, updatedAt: null, sourceTopic: item.topic, source: "default" };
  }

  const energy = {};
  for (const item of config.sensors.energy) {
    energy[item.id] = { ...item, value: item.default, updatedAt: null, sourceTopic: item.topic, source: "default" };
  }

  const prototypes = {};
  for (const item of [...config.modules.modules, ...config.modules.sensorPrototypes]) {
    for (const prototypeId of item.prototypes || [item.id]) {
      if (!prototypeId || prototypes[prototypeId]) continue;
      prototypes[prototypeId] = {
        id: prototypeId,
        label: item.display || item.label || prototypeId,
        online: false,
        availability: "unknown",
        mode: null,
        state: {},
        updatedAt: null
      };
    }
  }
  for (const prototypeId of ["chatarra", "fundicion", "ambientales", "aerogenerador", "anemometro", "bici", "campo", "represa", "vumetro"]) {
    if (!prototypes[prototypeId]) {
      prototypes[prototypeId] = { id: prototypeId, label: prototypeId, online: false, availability: "unknown", mode: null, state: {}, updatedAt: null };
    }
  }

  return {
    activePanel: config.system.defaultPanel,
    activeModule: config.system.defaultModule,
    activeScene: config.system.defaultScene,
    stations,
    balizas,
    sensors,
    energy,
    prototypes,
    actuators: {},
    deviceHealth: {},
    acks: [],
    cameras: buildCameraState(config, config.system.defaultPanel),
    mqtt: { enabled: config.mqtt.enabled, connected: false, status: "offline", lastError: null, logs: [], publications: [] },
    osc: { enabled: config.osc.enabled, status: "ready", logs: [] },
    arcade: { lastInput: null, lastAction: null },
    modules: config.modules.modules,
    scenes: config.scenes.scenes,
    updatedAt: now()
  };
}

function buildCameraState(config, panel) {
  const byId = Object.fromEntries(config.video.cameras.map((camera) => [camera.id, camera]));
  const group = config.video.groups[panel] || config.video.groups.sensores || [];
  return {
    mode: config.video.mode,
    baseUrl: config.video.go2rtcBaseUrl,
    activeGroup: panel,
    streams: group.slice(0, 3).map((id) => {
      const camera = byId[id];
      if (!camera) return null;
      const path = config.video.streamPathTemplate.replace("{stream}", encodeURIComponent(camera.stream));
      return { ...camera, url: `${config.video.go2rtcBaseUrl}${path}` };
    }).filter(Boolean)
  };
}

function createStore(config) {
  let state = createInitialState(config);
  const listeners = new Set();
  const limit = config.system.logLimit || 80;

  function emit(event, payload) {
    for (const listener of listeners) listener(event, payload, snapshot());
  }

  function touch() {
    state.updatedAt = now();
  }

  function pushLog(list, entry) {
    list.unshift({ time: now(), ...entry });
    if (list.length > limit) list.length = limit;
  }

  function updateIfChanged(getter, setter, event, payload) {
    const before = clone(getter());
    setter();
    const after = getter();
    if (same(before, after)) return false;
    touch();
    emit(event, payload);
    return true;
  }

  function snapshot() {
    return clone(state);
  }

  function subscribe(listener) {
    listeners.add(listener);
    return () => listeners.delete(listener);
  }

  function setPanel(panel) {
    return updateIfChanged(
      () => ({ activePanel: state.activePanel, cameras: state.cameras }),
      () => {
        state.activePanel = panel;
        state.cameras = buildCameraState(config, panel);
      },
      "state:changed",
      { reason: "panel", panel }
    );
  }

  function setScene(sceneId, source = "api") {
    const scene = config.scenes.scenes.find((item) => item.id === sceneId);
    if (!scene) return false;
    const changed = updateIfChanged(
      () => ({ activeScene: state.activeScene, activeModule: state.activeModule, activePanel: state.activePanel }),
      () => {
        state.activeScene = sceneId;
        if (scene.module) state.activeModule = scene.module;
        if (scene.panel) {
          state.activePanel = scene.panel;
          state.cameras = buildCameraState(config, scene.panel);
        }
      },
      "scene:changed",
      { sceneId, source }
    );
    if (changed) emit("state:changed", { reason: "scene", sceneId });
    return true;
  }

  function setBalizaState(balizaId, value, meta = {}) {
    const baliza = state.balizas[balizaId];
    if (!baliza) return false;
    const normalized = String(value).toUpperCase() === "ON" ? "ON" : "OFF";
    return updateIfChanged(
      () => ({ baliza: state.balizas[balizaId], stations: state.stations, actuators: state.actuators }),
      () => {
        state.balizas[balizaId] = {
          ...baliza,
          state: normalized,
          enabled: normalized === "ON",
          updatedAt: now(),
          source: meta.source || "mqtt",
          lastTopic: meta.topic || baliza.topic
        };
        const affectedModules = new Set(baliza.modules || []);
        for (const [stationId, station] of Object.entries(state.stations)) {
          if ((station.balizas || []).includes(balizaId)) affectedModules.add(stationId);
        }
        for (const moduleId of affectedModules) {
          if (!state.stations[moduleId]) continue;
          const moduleBalizas = state.stations[moduleId].balizas || [];
          const enabled = moduleBalizas.length
            ? moduleBalizas.every((id) => state.balizas[id]?.state === "ON")
            : normalized === "ON";
          state.stations[moduleId] = {
            ...state.stations[moduleId],
            enabled,
            state: enabled ? "ON" : "OFF",
            lastSeen: now()
          };
        }
        if (baliza.channel) {
          state.actuators.ciudad = {
            ...(state.actuators.ciudad || {}),
            [`${baliza.channel}_habilitado`]: normalized === "ON",
            updatedAt: now()
          };
        }
        if (balizaId === "5" && state.energy.solar_voltage) {
          state.energy.solar_voltage = {
            ...state.energy.solar_voltage,
            value: normalized === "ON" ? 10.4 : 0,
            updatedAt: now(),
            sourceTopic: baliza.topic,
            source: "baliza_5"
          };
        }
      },
      "state:changed",
      { reason: "baliza", balizaId, value: normalized, topic: meta.topic || baliza.topic }
    );
  }

  function updateReading(kind, id, value, meta = {}) {
    const collection = kind === "energy" ? state.energy : state.sensors;
    if (!collection[id]) return false;
    return updateIfChanged(
      () => collection[id],
      () => {
        collection[id] = {
          ...collection[id],
          value,
          updatedAt: now(),
          sourceTopic: meta.topic || collection[id].sourceTopic,
          source: meta.prototypeId || meta.source || "mqtt"
        };
      },
      "state:changed",
      { reason: kind, id, topic: meta.topic }
    );
  }

  function updatePrototype(prototypeId, patch, meta = {}) {
    if (!state.prototypes[prototypeId]) {
      state.prototypes[prototypeId] = { id: prototypeId, label: prototypeId, online: false, availability: "unknown", mode: null, state: {}, updatedAt: null };
    }
    return updateIfChanged(
      () => state.prototypes[prototypeId],
      () => {
        state.prototypes[prototypeId] = {
          ...state.prototypes[prototypeId],
          ...patch,
          state: { ...state.prototypes[prototypeId].state, ...(patch.state || {}) },
          updatedAt: now(),
          lastTopic: meta.topic || state.prototypes[prototypeId].lastTopic
        };
      },
      "state:changed",
      { reason: "prototype", prototypeId, topic: meta.topic }
    );
  }

  function updateActuator(prototypeId, key, value, meta = {}) {
    return updateIfChanged(
      () => state.actuators[prototypeId] || {},
      () => {
        state.actuators[prototypeId] = {
          ...(state.actuators[prototypeId] || {}),
          [key]: value,
          updatedAt: now(),
          lastTopic: meta.topic
        };
      },
      "state:changed",
      { reason: "actuator", prototypeId, key, topic: meta.topic }
    );
  }

  function updateDeviceHealth(prototypeId, status, meta = {}) {
    return updateIfChanged(
      () => state.deviceHealth[prototypeId] || {},
      () => {
        state.deviceHealth[prototypeId] = {
          prototypeId,
          status,
          online: status === "online",
          updatedAt: now(),
          topic: meta.topic
        };
      },
      "state:changed",
      { reason: "health", prototypeId, topic: meta.topic }
    );
  }

  function addAck(entry) {
    pushLog(state.acks, entry);
    touch();
    emit("state:changed", { reason: "ack", topic: entry.topic });
  }

  function setMqttStatus(status, extra = {}) {
    return updateIfChanged(
      () => state.mqtt,
      () => {
        state.mqtt = { ...state.mqtt, status, connected: status === "connected", ...extra };
      },
      "state:changed",
      { reason: "mqtt" }
    );
  }

  function addMqttLog(entry) {
    pushLog(state.mqtt.logs, entry);
    touch();
    emit("mqtt:message", entry);
  }

  function addMqttPublication(entry) {
    pushLog(state.mqtt.publications, entry);
    touch();
    emit("mqtt:published", entry);
  }

  function addOscLog(entry) {
    pushLog(state.osc.logs, entry);
    emit("osc:sent", entry);
  }

  function setArcade(input, action) {
    state.arcade.lastInput = { ...input, time: now() };
    state.arcade.lastAction = action || null;
    touch();
    emit("arcade:input", { input: state.arcade.lastInput, action });
    emit("state:changed", { reason: "arcade" });
  }

  function reset() {
    state = createInitialState(config);
    emit("state:changed", { reason: "reset" });
  }

  return {
    snapshot,
    subscribe,
    setPanel,
    setScene,
    setBalizaState,
    updateReading,
    updatePrototype,
    updateActuator,
    updateDeviceHealth,
    addAck,
    setMqttStatus,
    addMqttLog,
    addMqttPublication,
    addOscLog,
    setArcade,
    reset
  };
}

module.exports = { createStore };
