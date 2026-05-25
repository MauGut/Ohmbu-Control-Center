function normalizeKey(raw) {
  if (!raw) return "";
  if (raw.length === 1) return raw.toLowerCase();
  return raw;
}

function createArcadeHandler(config, store, oscAdapter) {
  function handleInput(input) {
    const key = normalizeKey(input.key || input.code || input);
    const action = config.arcadeMap[key];
    if (!action) {
      store.setArcade({ key, raw: input }, { type: "unknown" });
      return { ok: false, key, action: { type: "unknown" } };
    }

    if (action.type === "select_module") {
      store.setScene(action.scene, "arcade");
      oscAdapter.sendScene(action.scene, "arcade");
    }

    store.setArcade({ key, raw: input }, action);
    return { ok: true, key, action };
  }

  return { handleInput };
}

module.exports = { createArcadeHandler };
