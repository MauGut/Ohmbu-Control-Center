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

function focusables() {
  return Array.from(document.querySelectorAll("[data-nav-target], .card, .station, .camera"));
}

function syncFocusToPanel(panel) {
  const items = focusables();
  const activeIndex = items.findIndex((item) => item.dataset.navTarget === panel);
  focusIndex = activeIndex >= 0 ? activeIndex : Math.min(focusIndex, Math.max(items.length - 1, 0));
  updateFocus();
}

function updateFocus() {
  const items = focusables();
  items.forEach((item, index) => item.classList.toggle("arcade-focus", index === focusIndex));
  if (items[focusIndex]) items[focusIndex].scrollIntoView({ block: "nearest", inline: "nearest" });
}

function moveFocus(delta) {
  const items = focusables();
  if (!items.length) return;
  focusIndex = (focusIndex + delta + items.length) % items.length;
  updateFocus();
}

function acceptFocus() {
  const item = focusables()[focusIndex];
  const target = item?.dataset.navTarget;
  if (target) window.location.href = `/${target}`;
}

function goBack() {
  if (window.history.length > 1) {
    window.history.back();
  } else {
    window.location.href = "/sensores";
  }
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
        ${Object.values(items).map((item) => `
          <article class="card" tabindex="-1">
            <small>${item.label}</small>
            <strong class="${displayValue(item) === "ERROR!" ? "error-value" : ""}">${displayValue(item)}</strong>
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
  document.getElementById("mqttStatus").textContent = state.mqtt.status;
  document.getElementById("activeScene").textContent = `${state.activeScene} - ${state.activeModule}`;
  renderNav(panel);
  renderStations();
  renderMain(panel);
  renderCameras();
  syncFocusToPanel(panel);
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
  if (action.panel) {
    window.history.pushState({}, "", `/${action.panel}`);
    render();
    return;
  }
  if (action.type === "joystick_up" || action.type === "joystick_left") moveFocus(-1);
  if (action.type === "joystick_down" || action.type === "joystick_right") moveFocus(1);
  if (action.type === "accept") acceptFocus();
  if (action.type === "back") goBack();
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
  const result = await sendArcadeInput(event.key);
  applyArcadeAction(result?.action);
});

fetch("/api/state").then((res) => res.json()).then((snapshot) => {
  state = snapshot;
  render();
});
