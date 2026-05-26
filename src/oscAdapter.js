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

  function sendMessage(address, args = [], targetName = "resolume", source = "api") {
    const targetConfig = config.osc[targetName];
    if (!targetConfig) {
      store.addOscLog({ target: targetName, address, source, status: "unknown-target" });
      return false;
    }

    const message = { address, args };
    const entry = {
      target: targetName,
      host: targetConfig.host,
      port: targetConfig.port,
      address,
      argument: args.map((arg) => arg.value).join(","),
      source
    };

    try {
      if (config.osc.enabled && udpPort) {
        udpPort.send(message, targetConfig.host, targetConfig.port);
        store.addOscLog({ ...entry, status: "sent" });
      } else {
        store.addOscLog({ ...entry, status: "disabled-log-only" });
      }
      return true;
    } catch (error) {
      store.addOscLog({ ...entry, status: "error", error: error.message });
      return false;
    }
  }

  return { sendScene, sendMessage };
}

module.exports = { createOscAdapter };
