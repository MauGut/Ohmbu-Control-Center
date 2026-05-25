const mqtt = require("mqtt");

function parsePayload(buffer) {
  const raw = buffer.toString().trim();
  if (!raw) return { raw, value: "", malformed: false };
  try {
    return { raw, value: JSON.parse(raw), malformed: false };
  } catch {
    return { raw, value: raw, malformed: false };
  }
}

function asNumber(value) {
  const number = Number(value);
  return Number.isFinite(number) ? number : null;
}

function asOnOff(value) {
  const text = String(value).trim().toUpperCase();
  if (text === "ON" || text === "OFF") return text;
  return null;
}

function asMode(value) {
  const text = String(value).trim().toUpperCase();
  if (text === "REMOTO" || text === "LOCAL") return text;
  return null;
}

const topicHandlers = {
  "ambientales/temp": { type: "sensor", id: "temperature", parser: asNumber, prototypeId: "ambientales" },
  "ambientales/humedad": { type: "sensor", id: "humidity", parser: asNumber, prototypeId: "ambientales" },
  "ambientales/co2": { type: "sensor", id: "co2", parser: asNumber, prototypeId: "ambientales" },
  "ambientales/presion": { type: "sensor", id: "pressure", parser: asNumber, prototypeId: "ambientales" },
  "ambientales/estado": { type: "health", prototypeId: "ambientales" },
  "aerogenerador/voltaje": { type: "energy", id: "wind_voltage", parser: asNumber, prototypeId: "aerogenerador" },
  "aerogenerador/estado": { type: "health", prototypeId: "aerogenerador" },
  "anemometro/velocidad": { type: "sensor", id: "wind", parser: asNumber, prototypeId: "anemometro" },
  "anemometro/estado": { type: "health", prototypeId: "anemometro" },
  "bici/rpm": { type: "sensor", id: "crank_rpm", parser: asNumber, prototypeId: "bici" },
  "bici/estado": { type: "health", prototypeId: "bici" },
  "campo/humedad1": { type: "sensor", id: "soil_moisture_1", parser: asNumber, prototypeId: "campo" },
  "campo/humedad2": { type: "sensor", id: "soil_moisture_2", parser: asNumber, prototypeId: "campo" },
  "campo/estado": { type: "health", prototypeId: "campo" },
  "represa/voltaje": { type: "energy", id: "dam_voltage", parser: asNumber, prototypeId: "represa" },
  "represa/estado": { type: "health", prototypeId: "represa" },
  "vumetro/nivel": { type: "sensor", id: "noise", parser: asNumber, prototypeId: "vumetro" },
  "vumetro/estado": { type: "health", prototypeId: "vumetro" },
  "garra/estado/disp": { type: "health", prototypeId: "garra" },
  "garra/estado/modo": { type: "mode", prototypeId: "garra" },
  "garra/estado/servo1": { type: "actuator", prototypeId: "garra", key: "servo1", parser: asNumber },
  "garra/estado/servo2": { type: "actuator", prototypeId: "garra", key: "servo2", parser: asNumber },
  "garra/estado/servo3": { type: "actuator", prototypeId: "garra", key: "servo3", parser: asNumber },
  "grua/estado/disp": { type: "health", prototypeId: "grua" },
  "grua/estado/modo": { type: "mode", prototypeId: "grua" },
  "grua/estado/iman": { type: "actuator", prototypeId: "grua", key: "iman", parser: asOnOff },
  "chatarra/estado/disp": { type: "health", prototypeId: "chatarra" },
  "chatarra/estado/modo": { type: "mode", prototypeId: "chatarra" },
  "chatarra/estado/motor1": { type: "actuator", prototypeId: "chatarra", key: "motor1", parser: asOnOff },
  "chatarra/estado/motor2": { type: "actuator", prototypeId: "chatarra", key: "motor2", parser: asOnOff },
  "fundicion/estado/disp": { type: "health", prototypeId: "fundicion" },
  "fundicion/estado/modo": { type: "mode", prototypeId: "fundicion" },
  "fundicion/estado/motor1": { type: "actuator", prototypeId: "fundicion", key: "motor1", parser: asOnOff },
  "fundicion/estado/motor2": { type: "actuator", prototypeId: "fundicion", key: "motor2", parser: asOnOff },
  "fundicion/estado/motor3": { type: "actuator", prototypeId: "fundicion", key: "motor3", parser: asOnOff },
  "fundicion/estado/rele": { type: "actuator", prototypeId: "fundicion", key: "rele", parser: asOnOff },
  "ciudad/estado/disp": { type: "health", prototypeId: "ciudad" },
  "ciudad/estado/rele1": { type: "actuator", prototypeId: "ciudad", key: "rele1", parser: asOnOff },
  "ciudad/estado/rele2": { type: "actuator", prototypeId: "ciudad", key: "rele2", parser: asOnOff }
};

function applyCanonicalMessage(store, topic, parsed, source = "mqtt") {
  const balizaEntry = Object.entries(store.snapshot().balizas || {}).find(([, baliza]) => baliza.topic === topic);
  if (balizaEntry) {
    const [balizaId] = balizaEntry;
    const value = asOnOff(parsed.value);
    if (!value) {
      store.addMqttLog({ topic, payload: parsed.value, raw: parsed.raw, source, handled: false, reason: "invalid-baliza-payload" });
      return { handled: false, changed: false, reason: "invalid-baliza-payload" };
    }
    store.addMqttLog({ topic, payload: value, raw: parsed.raw, source, handled: true, type: "baliza", balizaId });
    const changed = store.setBalizaState(balizaId, value, { topic, source });
    return { handled: true, changed, type: "baliza" };
  }

  const handler = topicHandlers[topic];
  if (!handler) {
    store.addMqttLog({ topic, payload: parsed.value, raw: parsed.raw, source, handled: false, reason: "unknown-topic" });
    return { handled: false, changed: false, reason: "unknown-topic" };
  }

  let value = parsed.value;
  if (handler.parser) value = handler.parser(value);
  if (value === null) {
    store.addMqttLog({ topic, payload: parsed.value, raw: parsed.raw, source, handled: false, reason: "invalid-payload" });
    return { handled: false, changed: false, reason: "invalid-payload" };
  }

  const logEntry = { topic, payload: value, raw: parsed.raw, source, handled: true, type: handler.type, prototypeId: handler.prototypeId };
  store.addMqttLog(logEntry);

  if (handler.type === "sensor") {
    const changed = store.updateReading("sensor", handler.id, value, { topic, prototypeId: handler.prototypeId, source });
    return { handled: true, changed, type: handler.type };
  }
  if (handler.type === "energy") {
    const changed = store.updateReading("energy", handler.id, value, { topic, prototypeId: handler.prototypeId, source });
    return { handled: true, changed, type: handler.type };
  }
  if (handler.type === "health") {
    const status = String(value).toLowerCase();
    const changedHealth = store.updateDeviceHealth(handler.prototypeId, status, { topic, source });
    const changedPrototype = store.updatePrototype(handler.prototypeId, { online: status === "online", availability: status }, { topic });
    return { handled: true, changed: changedHealth || changedPrototype, type: handler.type };
  }
  if (handler.type === "mode") {
    const mode = asMode(value);
    if (!mode) {
      store.addMqttLog({ topic, payload: value, raw: parsed.raw, source, handled: false, reason: "invalid-mode" });
      return { handled: false, changed: false, reason: "invalid-mode" };
    }
    const changed = store.updatePrototype(handler.prototypeId, { mode }, { topic });
    return { handled: true, changed, type: handler.type };
  }
  if (handler.type === "actuator") {
    const changedActuator = store.updateActuator(handler.prototypeId, handler.key, value, { topic, source });
    const changedPrototype = store.updatePrototype(handler.prototypeId, { state: { [handler.key]: value } }, { topic });
    return { handled: true, changed: changedActuator || changedPrototype, type: handler.type };
  }

  return { handled: false, changed: false, reason: "unhandled-type" };
}

function createMqttAdapter(config, store) {
  let client = null;

  function publish(topic, payload, options = {}) {
    const body = typeof payload === "string" ? payload : String(payload);
    const entry = { topic, payload: body, retain: Boolean(options.retain), qos: options.qos || 0 };

    if (!config.mqtt.enabled || !client || !client.connected) {
      store.addMqttPublication({ ...entry, status: "not-connected" });
      return false;
    }

    client.publish(topic, body, { retain: Boolean(options.retain), qos: options.qos || 0 }, (error) => {
      store.addMqttPublication({ ...entry, status: error ? "error" : "sent", error: error ? error.message : null });
    });
    return true;
  }

  function publishCommand(prototypeId, command, value, options = {}) {
    const topic = config.mqtt.commands?.[prototypeId]?.[command];
    if (!topic) return false;
    return publish(topic, value, options);
  }

  if (!config.mqtt.enabled) {
    store.setMqttStatus("disabled");
    return { client: null, publish, publishCommand, applyCanonicalMessage: (topic, payload) => applyCanonicalMessage(store, topic, payload, "simulation") };
  }

  client = mqtt.connect(config.mqtt.url, {
    clientId: config.mqtt.clientId,
    connectTimeout: config.mqtt.connectTimeoutMs,
    reconnectPeriod: config.mqtt.reconnectPeriodMs
  });

  client.on("connect", () => {
    store.setMqttStatus("connected", { lastError: null });
    client.subscribe(config.mqtt.subscriptions, (error) => {
      if (error) store.setMqttStatus("error", { lastError: error.message });
    });
  });

  client.on("reconnect", () => store.setMqttStatus("reconnecting"));
  client.on("offline", () => store.setMqttStatus("offline"));
  client.on("error", (error) => {
    store.setMqttStatus("error", { lastError: error.message });
  });

  client.on("message", (topic, buffer) => {
    try {
      applyCanonicalMessage(store, topic, parsePayload(buffer), "mqtt");
    } catch (error) {
      store.addMqttLog({ topic, payload: buffer.toString(), handled: false, reason: "handler-error", error: error.message });
    }
  });

  return {
    client,
    publish,
    publishCommand,
    applyCanonicalMessage: (topic, payload, source = "simulation") => {
      const buffer = Buffer.isBuffer(payload) ? payload : Buffer.from(String(payload));
      return applyCanonicalMessage(store, topic, parsePayload(buffer), source);
    }
  };
}

module.exports = { createMqttAdapter };
