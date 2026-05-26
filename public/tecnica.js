const statusEl = document.getElementById("status");

async function sendBalizas(value) {
  statusEl.textContent = `Enviando ${value}...`;
  try {
    const response = await fetch("/api/tecnica/balizas", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ value })
    });
    const result = await response.json();
    const sent = result.published?.length || 0;
    statusEl.textContent = response.ok ? `${value} enviado a ${sent} balizas` : `Error: ${result.error || "no enviado"}`;
  } catch {
    statusEl.textContent = "Error de conexion";
  }
}

async function sendColumn(column) {
  statusEl.textContent = `Enviando columna ${column}...`;
  try {
    const requests = [fetch(`/api/tecnica/osc/${column}`, { method: "POST" })];
    if (String(column) === "4") {
      requests.push(fetch("/api/tecnica/balizas", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ value: "OFF" })
      }));
    }
    const [response, balizasResponse] = await Promise.all(requests);
    const result = await response.json();
    const balizasResult = balizasResponse ? await balizasResponse.json() : null;
    statusEl.textContent = response.ok
      ? `OSC ${result.address} = 1${balizasResult ? " + balizas OFF" : ""}`
      : `Error: ${result.error || "OSC no enviado"}`;
  } catch {
    statusEl.textContent = "Error de conexion";
  }
}

document.querySelectorAll("[data-osc-column]").forEach((button) => {
  button.addEventListener("click", () => sendColumn(button.dataset.oscColumn));
});

document.getElementById("balizasOn").addEventListener("click", () => sendBalizas("ON"));
document.getElementById("balizasOff").addEventListener("click", () => sendBalizas("OFF"));
