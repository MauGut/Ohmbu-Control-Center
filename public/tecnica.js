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

document.getElementById("balizasOn").addEventListener("click", () => sendBalizas("ON"));
document.getElementById("balizasOff").addEventListener("click", () => sendBalizas("OFF"));
