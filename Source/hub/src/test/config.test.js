const assert = require("node:assert");
const test = require("node:test");

const CONFIG_PATH = require.resolve("../config");

// Load the config module with a controlled set of environment variables,
// then restore the environment afterwards. Each test file runs in its own
// process, so this does not leak into other tests.
function loadConfig(envOverrides = {}) {
  const saved = {};
  const keys = Object.keys(envOverrides);

  for (const key of keys) {
    saved[key] = process.env[key];
    if (envOverrides[key] === undefined) {
      delete process.env[key];
    } else {
      process.env[key] = envOverrides[key];
    }
  }

  delete require.cache[CONFIG_PATH];
  const config = require(CONFIG_PATH);

  for (const key of keys) {
    if (saved[key] === undefined) {
      delete process.env[key];
    } else {
      process.env[key] = saved[key];
    }
  }

  return config;
}

test("config uses file defaults when no env is set", () => {
  const config = loadConfig();
  assert.strictEqual(config.port, 50020);
  assert.strictEqual(config.pollIntervalMs, 30000);
  assert.strictEqual(config.serverTimeoutMs, 240000);
  assert.deepStrictEqual(config.corsOrigins, ["*"]);
});

test("config overrides port from env", () => {
  const config = loadConfig({ HUB_PORT: "8080" });
  assert.strictEqual(config.port, 8080);
});

test("config falls back on invalid port values", () => {
  assert.strictEqual(
    loadConfig({ HUB_PORT: "not-a-number" }).port,
    50020,
  );
  assert.strictEqual(
    loadConfig({ HUB_PORT: "0" }).port,
    50020,
  );
  assert.strictEqual(
    loadConfig({ HUB_PORT: "-1" }).port,
    50020,
  );
});

test("config overrides poll interval and timeout from env", () => {
  const config = loadConfig({
    HUB_POLL_INTERVAL_MS: "1500",
    HUB_TIMEOUT_MS: "12345",
  });
  assert.strictEqual(config.pollIntervalMs, 1500);
  assert.strictEqual(config.serverTimeoutMs, 12345);
});

test("config parses comma-separated CORS origins from env", () => {
  const config = loadConfig({
    HUB_CORS_ORIGINS: " https://a.com , https://b.com ",
  });
  assert.deepStrictEqual(config.corsOrigins, ["https://a.com", "https://b.com"]);
});

test("config trims and drops empty CORS origins", () => {
  const config = loadConfig({
    HUB_CORS_ORIGINS: "https://a.com,, ,",
  });
  assert.deepStrictEqual(config.corsOrigins, ["https://a.com"]);
});

test("config returns empty CORS origins for blank env", () => {
  const config = loadConfig({ HUB_CORS_ORIGINS: "" });
  assert.deepStrictEqual(config.corsOrigins, []);
});


test("sharding allowlist publication requires explicit true", () => {
  for (const value of [undefined, "", "false", "1", "TRUE", "invalid"]) {
    assert.strictEqual(loadConfig({ HUB_SHOW_SHARDING_ALLOWLIST: value }).showShardingAllowList, false);
  }
  assert.strictEqual(loadConfig({ HUB_SHOW_SHARDING_ALLOWLIST: "true" }).showShardingAllowList, true);
});
