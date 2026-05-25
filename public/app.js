const socket = io();
const panels = [
  ["sensores", "Sensores"],
  ["camaras", "Camaras"],
  ["garra", "Garra"],
  ["ciudad", "Ciudad"],
  ["grua", "Grua"],
  ["camion", "Camion"],
  ["cintas", "Cinta de minerales"],
  ["represa", "Cinta de fundicion"]
];

let state = null;
let focusIndex = 0;
let menuSelectorVisible = true;
let lastOperationalPanel = null;
let garraRemoteActive = false;
const garraControl = {
  servo1: 55,
  servo2: 90,
  servo3: 30,
  step: 4,
  lastButtonRepeat: 0
};
const gamepadState = {
  buttons: new Map(),
  lastAxisMove: 0
};

function currentPanel() {
  return window.location.pathname.replace("/", "") || "sensores";
}

function formatValue(item) {
  const unit = item.unit ? ` ${item.unit}` : "";
  return `${item.value}${unit}`;
}

const sensorMission = {
  temperature: "7",
  humidity: "7",
  pressure: "7",
  noise: "7",
  co2: "7",
  soil_moisture_1: "4",
  soil_moisture_2: "4",
  wind: "6",
  crank_rpm: "2"
};

function displayValue(item) {
  const balizaId = sensorMission[item.id];
  if (balizaId && !state.balizas[balizaId]?.enabled) return "ERROR!";
  return formatValue(item);
}

function sensorInError(item) {
  const balizaId = sensorMission[item.id];
  return Boolean(balizaId && !state.balizas[balizaId]?.enabled);
}

function moistureColor(value) {
  const percent = Math.max(0, Math.min(100, Number(value) || 0));
  const hue = percent * 1.1;
  return `hsl(${hue}, 90%, 50%)`;
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, Number(value) || 0));
}

function arcPoint(cx, cy, radius, angle) {
  const radians = (angle - 90) * Math.PI / 180;
  return {
    x: cx + radius * Math.cos(radians),
    y: cy + radius * Math.sin(radians)
  };
}

function arcPath(percent, startAngle = 250, endAngle = 70) {
  const sweep = endAngle + 360 - startAngle;
  const angle = startAngle + sweep * percent;
  const start = arcPoint(100, 116, 76, startAngle);
  const end = arcPoint(100, 116, 76, angle);
  const largeArc = sweep * percent > 180 ? 1 : 0;
  return `M ${start.x} ${start.y} A 72 72 0 ${largeArc} 1 ${end.x} ${end.y}`;
}

function renderSensorCard(item) {
  const error = sensorInError(item);
  if (item.id === "soil_moisture_1" || item.id === "soil_moisture_2") {
    const percent = Math.max(0, Math.min(100, Number(item.value) || 0));
    return `
      <article class="card moisture-card" tabindex="-1">
        <small>${item.label}</small>
        ${error ? `<strong class="error-value">ERROR!</strong>` : `
          <div class="moisture-track" aria-label="${item.label} ${percent}%">
            <span class="moisture-fill" style="width:${percent}%; background:${moistureColor(percent)}"></span>
          </div>
          <div class="moisture-labels"><span>Bajo</span><span>Medio</span><span>Alto</span></div>
        `}
      </article>
    `;
  }
  if (item.id === "wind") {
    const value = clamp(item.value, 0, 50);
    const percent = value / 50;
    const dot = arcPoint(100, 116, 76, 250 + 180 * percent);
    return `
      <article class="card wind-card" tabindex="-1">
        <small>${item.label}</small>
        ${error ? `<strong class="error-value">ERROR!</strong>` : `
          <svg class="wind-gauge" viewBox="0 0 200 128" aria-label="${item.label} ${value} km/h">
            <path class="gauge-bg" d="${arcPath(1)}"></path>
            <path class="gauge-fill" d="${arcPath(percent)}"></path>
            <circle class="gauge-dot" cx="${dot.x}" cy="${dot.y}" r="6"></circle>
          </svg>
          <strong>${Math.round(value)}km/h</strong>
        `}
      </article>
    `;
  }
  if (item.id === "noise") {
    const percent = clamp(item.value, 0, 100);
    return `
      <article class="card noise-card" tabindex="-1">
        <small>${item.label}</small>
        ${error ? `<strong class="error-value">ERROR!</strong>` : `
          <div class="noise-readout"><strong>${Math.round(percent)}%</strong></div>
          <div class="noise-slider" aria-label="${item.label} ${percent}%">
            <span class="noise-fill" style="height:${percent}%"></span>
            <span class="noise-knob" style="bottom:calc(${percent}% - 8px)"></span>
          </div>
        `}
      </article>
    `;
  }
  return `
    <article class="card" tabindex="-1">
      <small>${item.label}</small>
      <strong class="${error ? "error-value" : ""}">${error ? "ERROR!" : formatValue(item)}</strong>
    </article>
  `;
}

function focusables() {
  if (menuSelectorVisible) return Array.from(document.querySelectorAll("[data-nav-target]"));
  return Array.from(document.querySelectorAll("[data-nav-target], .card, .station, .camera"));
}

function syncFocusToPanel(panel) {
  const items = focusables();
  if (!menuSelectorVisible) {
    items.forEach((item) => item.classList.remove("arcade-focus"));
    return;
  }
  const activeIndex = items.findIndex((item) => item.dataset.navTarget === panel);
  focusIndex = activeIndex >= 0 ? activeIndex : Math.min(focusIndex, Math.max(items.length - 1, 0));
  updateFocus();
}

function refreshFocus() {
  const items = focusables();
  focusIndex = Math.min(focusIndex, Math.max(items.length - 1, 0));
  updateFocus();
}

function updateFocus() {
  const items = focusables();
  if (!menuSelectorVisible) {
    items.forEach((item) => item.classList.remove("arcade-focus"));
    return;
  }
  items.forEach((item, index) => item.classList.toggle("arcade-focus", index === focusIndex));
  if (items[focusIndex]) items[focusIndex].scrollIntoView({ block: "nearest", inline: "nearest" });
}

function moveFocus(delta) {
  if (!menuSelectorVisible) return;
  const items = focusables();
  if (!items.length) return;
  focusIndex = (focusIndex + delta + items.length) % items.length;
  updateFocus();
}

function acceptFocus() {
  if (!menuSelectorVisible) return;
  const item = focusables()[focusIndex];
  const target = item?.dataset.navTarget;
  if (target) {
    menuSelectorVisible = false;
    window.history.pushState({}, "", `/${target}`);
    render();
  }
}

function goBack() {
  if (!menuSelectorVisible) {
    menuSelectorVisible = true;
    syncOperationalMode(currentPanel());
    syncFocusToPanel(currentPanel());
    return;
  }
  window.history.pushState({}, "", "/sensores");
  render();
}

async function publishCommand(prototypeId, command, value) {
  try {
    await fetch(`/api/mqtt/command/${prototypeId}/${command}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ value: String(value) })
    });
  } catch {
    // Hardware/MQTT may be offline during UI testing.
  }
}

function currentServoValue(servo, fallback) {
  const value = Number(state?.actuators?.garra?.[servo]);
  return Number.isFinite(value) ? value : fallback;
}

function syncGarraValuesFromState() {
  garraControl.servo1 = currentServoValue("servo1", garraControl.servo1);
  garraControl.servo2 = currentServoValue("servo2", garraControl.servo2);
  garraControl.servo3 = currentServoValue("servo3", garraControl.servo3);
}

function commandGarraServo(servo, delta) {
  const limits = {
    servo1: [0, 110],
    servo2: [0, 180],
    servo3: [0, 60]
  };
  const [min, max] = limits[servo];
  const next = clamp((garraControl[servo] ?? currentServoValue(servo, min)) + delta, min, max);
  garraControl[servo] = next;
  publishCommand("garra", servo, Math.round(next));
  render();
}

function handleGarraControl(action) {
  if (!action || currentPanel() !== "garra" || menuSelectorVisible) return false;
  if (action.type === "joystick_left") commandGarraServo("servo2", -garraControl.step);
  if (action.type === "joystick_right") commandGarraServo("servo2", garraControl.step);
  if (action.type === "joystick_up") commandGarraServo("servo1", garraControl.step);
  if (action.type === "joystick_down") commandGarraServo("servo1", -garraControl.step);
  if (action.type === "contextual_action_1") commandGarraServo("servo3", garraControl.step);
  if (action.type === "contextual_action_2") commandGarraServo("servo3", -garraControl.step);
  return ["joystick_left", "joystick_right", "joystick_up", "joystick_down", "contextual_action_1", "contextual_action_2"].includes(action.type);
}

function handleCiudadControl(action) {
  if (!action || currentPanel() !== "ciudad" || menuSelectorVisible) return false;
  if (action.type === "contextual_action_1") {
    publishCommand("ciudad", "rele1", "ON");
    return true;
  }
  if (action.type === "contextual_action_2") {
    publishCommand("ciudad", "rele1", "OFF");
    return true;
  }
  return false;
}

function syncOperationalMode(panel) {
  const operationalPanel = panel === "garra" && !menuSelectorVisible ? "garra" : null;
  if (operationalPanel === lastOperationalPanel) return;
  if (lastOperationalPanel === "garra" && garraRemoteActive) {
    publishCommand("garra", "modo", "LOCAL");
    garraRemoteActive = false;
  }
  if (operationalPanel === "garra") {
    syncGarraValuesFromState();
    publishCommand("garra", "modo", "REMOTO");
    garraRemoteActive = true;
  }
  lastOperationalPanel = operationalPanel;
}

function renderNav(panel) {
  const nav = document.getElementById("panelNav");
  nav.innerHTML = panels.map(([id, label]) => (
    `<a data-nav-target="${id}" class="${panel === id ? "active" : ""}" href="/${id}"><span>${label}</span></a>`
  )).join("");
}

function renderStations() {
  const el = document.getElementById("stations");
  el.innerHTML = Object.values(state.balizas)
    .sort((a, b) => Number(a.id) - Number(b.id))
    .map((baliza) => `
    <div class="station ${baliza.enabled ? "enabled" : ""}" tabindex="-1">
      <span class="dot"></span>
      <strong>${baliza.label || `Mision ${baliza.id}`}</strong>
    </div>
  `).join("");
}

function renderCards(title, items, className = "") {
  return `
    <section>
      <div class="section-title"><span>${title}</span></div>
      <div class="cards ${className}">
        ${Object.values(items).map((item) => className.includes("sensor-grid") ? renderSensorCard(item) : `
          <article class="card" tabindex="-1">
            <small>${item.label}</small>
            <strong>${formatValue(item)}</strong>
          </article>
        `).join("")}
      </div>
    </section>
  `;
}

function renderDebug() {
  return `
    <section class="debug-panel">
      <h1>Debug</h1>
      <div class="module-stats">
        <article class="card" tabindex="-1"><small>Ultimo MQTT recibido</small><strong>${state.mqtt.logs[0]?.topic || "sin mensajes"}</strong></article>
        <article class="card" tabindex="-1"><small>Ultima publicacion MQTT</small><strong>${state.mqtt.publications[0]?.topic || "sin publicaciones"}</strong></article>
        <article class="card" tabindex="-1"><small>Ultimo ACK</small><strong>${state.acks[0]?.topic || "sin ACKs"}</strong></article>
      </div>
      <pre>${JSON.stringify(state, null, 2)}</pre>
    </section>`;
}

function renderModule(panel) {
  if (panel === "garra") return renderGarraPanel();
  if (panel === "ciudad") return renderCiudadPanel();
  const module = state.modules.find((item) => item.id === panel);
  const station = state.stations[panel] || {};
  const prototypes = (module?.prototypes || []).map((id) => state.prototypes[id]).filter(Boolean);
  const prototypeCards = prototypes.map((prototype) => `
    <article class="card" tabindex="-1">
      <small>${prototype.label || prototype.id}</small>
      <strong>${prototype.availability || "unknown"}</strong>
      <small>modo ${prototype.mode || "sin dato"}</small>
    </article>
  `).join("");
  const actuatorCards = prototypes.flatMap((prototype) => Object.entries(state.actuators[prototype.id] || {})
    .filter(([key]) => !["updatedAt", "lastTopic"].includes(key))
    .map(([key, value]) => `
      <article class="card" tabindex="-1">
        <small>${prototype.id} - ${key}</small>
        <strong>${value}</strong>
        <small>${state.actuators[prototype.id]?.lastTopic || ""}</small>
      </article>
    `)).join("");

  return `
    <section class="module-panel">
      <h1>${module?.display || module?.label || panel}</h1>
      <div class="module-stats">
        <article class="card" tabindex="-1"><small>Modulo activo</small><strong>${state.activeModule}</strong></article>
        <article class="card" tabindex="-1"><small>Escena activa</small><strong>${state.activeScene}</strong></article>
        <article class="card" tabindex="-1"><small>Estacion</small><strong>${station.enabled ? "ON" : "OFF"}</strong><small>${(station.balizas || []).map((id) => `Baliza ${id}`).join(", ")}</small></article>
        ${prototypeCards}
        ${actuatorCards}
      </div>
    </section>`;
}

function renderGarraPanel() {
  const camera = state.cameras.streams[2] || state.cameras.streams[0];
  return `
    <section class="module-panel garra-panel">
      <div class="garra-stage">
        <article class="garra-camera">
          ${camera ? `<img src="${camera.url}" alt="${camera.label}" onerror="this.removeAttribute('src')">` : ""}
          <footer>${camera?.label || "Camara 3"}</footer>
        </article>
        <aside class="garra-controls">
          <h1>Garra</h1>
          <div class="control-line"><strong>Palanca arriba/abajo</strong><span>Servo 1</span></div>
          <div class="control-line"><strong>Palanca izquierda/derecha</strong><span>Servo 2</span></div>
          <div class="control-line"><strong>Boton A / Boton B</strong><span>Servo 3</span></div>
          <div class="servo-readouts">
            <article><small>Servo 1</small><strong>${Math.round(garraControl.servo1)}°</strong></article>
            <article><small>Servo 2</small><strong>${Math.round(garraControl.servo2)}°</strong></article>
            <article><small>Servo 3</small><strong>${Math.round(garraControl.servo3)}°</strong></article>
          </div>
        </aside>
      </div>
    </section>
  `;
}

function renderCiudadPanel() {
  const camera = state.cameras.streams[0];
  const rele = state.actuators?.ciudad?.rele1 || "sin dato";
  return `
    <section class="module-panel operation-panel">
      <div class="garra-stage">
        <article class="garra-camera">
          ${camera ? `<img src="${camera.url}" alt="${camera.label}" onerror="this.removeAttribute('src')">` : ""}
          <footer>${camera?.label || "Camara Ciudad"}</footer>
        </article>
        <aside class="garra-controls">
          <h1>Ciudad</h1>
          <div class="control-line"><strong>Boton A</strong><span>Activar energia</span></div>
          <div class="control-line"><strong>Boton B</strong><span>Cortar energia</span></div>
          <div class="servo-readouts">
            <article><small>Rele 1</small><strong>${rele}</strong></article>
          </div>
        </aside>
      </div>
    </section>
  `;
}

function renderMain(panel) {
  const content = document.getElementById("content");
  if (panel === "debug") {
    content.innerHTML = renderDebug();
    return;
  }
  if (panel === "output") {
    document.getElementById("app").classList.add("output-mode");
    content.innerHTML = `
      <section class="output-panel">
        <h1>OHMBU</h1>
        <div class="section-title"><span>${state.activeScene}</span><strong>${state.activeModule}</strong></div>
        <div class="output-metrics">
          ${Object.values(state.energy).map((item) => `<article class="card" tabindex="-1"><small>${item.label}</small><strong>${formatValue(item)}</strong></article>`).join("")}
        </div>
      </section>`;
    return;
  }
  document.getElementById("app").classList.remove("output-mode");
  if (panel === "sensores" || panel === "camaras") {
    content.innerHTML = `<div class="content-grid">${renderCards("Sensores", state.sensors, "sensor-grid")}${renderCards("Eficiencia energetica", state.energy, "energy-grid")}</div>`;
    return;
  }
  content.innerHTML = renderModule(panel);
}

function renderCameras() {
  document.getElementById("cameraGroup").textContent = state.cameras.activeGroup;
  const el = document.getElementById("cameras");
  el.innerHTML = state.cameras.streams.map((camera) => `
    <article class="camera" tabindex="-1">
      <img src="${camera.url}" alt="${camera.label}" onerror="this.removeAttribute('src')">
      <footer>${camera.label} - ${camera.stream}</footer>
    </article>
  `).join("");
}

function render() {
  if (!state) return;
  const panel = currentPanel();
  syncOperationalMode(panel);
  document.getElementById("mqttStatus").textContent = state.mqtt.status;
  document.getElementById("activeScene").textContent = `${state.activeScene} - ${state.activeModule}`;
  renderNav(panel);
  renderStations();
  renderMain(panel);
  renderCameras();
  refreshFocus();
}

async function sendArcadeInput(key) {
  const response = await fetch("/api/arcade/input", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ key })
  });
  if (!response.ok) return null;
  return response.json();
}

function applyArcadeAction(action) {
  if (!action) return;
  if (handleGarraControl(action)) return;
  if (handleCiudadControl(action)) return;
  if (action.panel) {
    menuSelectorVisible = false;
    window.history.pushState({}, "", `/${action.panel}`);
    render();
    return;
  }
  if (action.type === "joystick_up") moveFocus(-1);
  if (action.type === "joystick_down") moveFocus(1);
  if (action.type === "accept") acceptFocus();
  if (action.type === "back") goBack();
}

async function handleArcadeKey(key) {
  const result = await sendArcadeInput(key);
  applyArcadeAction(result?.action);
}

function pollGamepads() {
  const pads = navigator.getGamepads ? navigator.getGamepads() : [];
  const pad = Array.from(pads).find(Boolean);
  if (pad) {
    pad.buttons.forEach((button, index) => {
      const wasPressed = gamepadState.buttons.get(index) || false;
      const pressed = button.pressed || button.value > 0.5;
      if (pressed && !wasPressed) {
        if (index >= 0 && index <= 9) handleArcadeKey(String(index).padStart(2, "0"));
        if (index === 12) handleArcadeKey("ArrowDown");
        if (index === 13) handleArcadeKey("ArrowUp");
        if (index === 14) handleArcadeKey("ArrowRight");
        if (index === 15) handleArcadeKey("ArrowLeft");
      }
      gamepadState.buttons.set(index, pressed);
    });

    const now = performance.now();
    if (currentPanel() === "garra" && !menuSelectorVisible && now - garraControl.lastButtonRepeat > 150) {
      if (pad.buttons[6]?.pressed || pad.buttons[6]?.value > 0.5) {
        garraControl.lastButtonRepeat = now;
        handleArcadeKey("06");
      } else if (pad.buttons[7]?.pressed || pad.buttons[7]?.value > 0.5) {
        garraControl.lastButtonRepeat = now;
        handleArcadeKey("07");
      } else if (pad.buttons[12]?.pressed || pad.buttons[12]?.value > 0.5) {
        garraControl.lastButtonRepeat = now;
        handleArcadeKey("ArrowDown");
      } else if (pad.buttons[13]?.pressed || pad.buttons[13]?.value > 0.5) {
        garraControl.lastButtonRepeat = now;
        handleArcadeKey("ArrowUp");
      } else if (pad.buttons[14]?.pressed || pad.buttons[14]?.value > 0.5) {
        garraControl.lastButtonRepeat = now;
        handleArcadeKey("ArrowRight");
      } else if (pad.buttons[15]?.pressed || pad.buttons[15]?.value > 0.5) {
        garraControl.lastButtonRepeat = now;
        handleArcadeKey("ArrowLeft");
      }
    }

    const x = pad.axes[0] || 0;
    const y = pad.axes[1] || 0;
    if (now - gamepadState.lastAxisMove > 150) {
      if (y < -0.55) {
        gamepadState.lastAxisMove = now;
        handleArcadeKey("ArrowDown");
      } else if (y > 0.55) {
        gamepadState.lastAxisMove = now;
        handleArcadeKey("ArrowUp");
      } else if (x < -0.55) {
        gamepadState.lastAxisMove = now;
        handleArcadeKey("ArrowRight");
      } else if (x > 0.55) {
        gamepadState.lastAxisMove = now;
        handleArcadeKey("ArrowLeft");
      }
    }
  }
  window.requestAnimationFrame(pollGamepads);
}

socket.on("state:snapshot", (snapshot) => {
  state = snapshot;
  render();
});

window.addEventListener("popstate", render);

window.addEventListener("keydown", async (event) => {
  const keys = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "Enter", "Escape", "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"];
  if (!keys.includes(event.key)) return;
  event.preventDefault();
  handleArcadeKey(event.key);
});

document.addEventListener("click", (event) => {
  const link = event.target.closest("[data-nav-target]");
  if (!link) return;
  event.preventDefault();
  menuSelectorVisible = false;
  window.history.pushState({}, "", `/${link.dataset.navTarget}`);
  render();
});

window.addEventListener("gamepadconnected", () => {
  gamepadState.buttons.clear();
});

window.requestAnimationFrame(pollGamepads);

fetch("/api/state").then((res) => res.json()).then((snapshot) => {
  state = snapshot;
  render();
});
