const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..");
const configDir = path.join(root, "config");

function readJson(name) {
  const filePath = path.join(configDir, name);
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

function loadConfig() {
  return {
    root,
    system: readJson("system.json"),
    modules: readJson("modules.json"),
    arcadeMap: readJson("arcadeMap.json"),
    scenes: readJson("scenes.json"),
    video: readJson("video.json"),
    mqtt: readJson("mqtt.json"),
    osc: readJson("osc.json"),
    sensors: readJson("sensors.json")
  };
}

module.exports = { loadConfig };
