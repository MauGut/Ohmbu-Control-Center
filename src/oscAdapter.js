const osc = require("osc");

function createOscAdapter(config, store) {
  let udpPort = null;
  if (config.osc.enabled) {
    try {
      udpPort = new osc.UDPPort({ localAddress: "0.0.0.0", localPort: 0, metadata: true });
      udpPort.open();
      udpPort.on("error", (error) => {
        store.addOscLog({ status: "error", error: error.message });
      });
    } catch (error) {
      store.addOscLog({ status: "stub", error: error.message });
    }
  }

  function sendScene(sceneId, source = "api") {
    const targets = [
      { name: "magicq", ...config.osc.magicq },
      { name: "resolume", ...config.osc.resolume }
    ];
    const message = {
      address: config.osc.address,
      args: [{ type: "s", value: sceneId }]
    };

    for (const target of targets) {
      const entry = { target: target.name, host: target.host, port: target.port, address: message.address, argument: sceneId, source };
      try {
        if (config.osc.enabled && udpPort) {
          udpPort.send(message, target.host, target.port);
          store.addOscLog({ ...entry, status: "sent" });
        } else {
          store.addOscLog({ ...entry, status: "disabled-log-only" });
        }
      } catch (error) {
        store.addOscLog({ ...entry, status: "error", error: error.message });
      }
    }
  }

  return { sendScene };
}

module.exports = { createOscAdapter };
