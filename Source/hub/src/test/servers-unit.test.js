const assert = require("node:assert");
const test = require("node:test");

process.env.MASTER_SERVER_WRITE_SECRET = "test-secret";

delete require.cache[require.resolve("../routes/api/v1/servers")];
const servers = require("../routes/api/v1/servers");
const config = require("../config");

const {
  addServer,
  removeServer,
  buildServerObject,
  isServerFilter,
  isServerCensored,
  isServerAllowedToShard,
  sanitizeField,
  validateWebAddress,
  normalizePort,
  normalizeGameType,
  removeTimedOutServers,
  getServerIdFromRequest,
  getPortFromRequest,
  activeServers,
  filters,
  censors,
  shardingAllowList,
  validatePublicKey,
  normalizeServerId,
} = servers;

function makeServerData(overrides = {}) {
  return {
    id: "unit-server",
    ipAddress: "10.0.0.1",
    hostname: "example.com",
    privateHostname: "private.example.com",
    description: "Description",
    name: "Name",
    publicKey: "QUJDREVGRw==",
    playerCount: 3,
    password: "pw",
    modsWhiteList: "",
    modsBlackList: "",
    modsRequiredList: "",
    version: 2,
    allowSharding: false,
    webAddress: "",
    port: 50050,
    isShard: false,
    gameType: "DarkSouls3",
    ...overrides,
  };
}

test("sanitizeField strips HTML tag constructs and truncates", () => {
  assert.strictEqual(
    sanitizeField("hello<script>world</script>"),
    "helloworld",
  );
  assert.strictEqual(sanitizeField("<script>evil()</script>"), "evil()");
  assert.strictEqual(sanitizeField("a".repeat(300), 10), "a".repeat(10));
  assert.strictEqual(sanitizeField("abc<script"), "abc");
});

test("sanitizeField removes control characters", () => {
  assert.strictEqual(sanitizeField("ab\x00cd\x7Fef"), "abcdef");
});

test("validateWebAddress canonicalizes and rejects invalid URLs", () => {
  assert.strictEqual(
    validateWebAddress("https://example.com"),
    "https://example.com/",
  );
  assert.strictEqual(
    validateWebAddress("http://user:pass@example.com/x"),
    "http://example.com/x",
  );
  assert.strictEqual(validateWebAddress("ftp://example.com"), "");
  assert.strictEqual(validateWebAddress("not a url"), "");
  assert.strictEqual(validateWebAddress(""), "");
});

test("normalizePort validates port ranges", () => {
  assert.strictEqual(normalizePort(8080), 8080);
  assert.strictEqual(normalizePort("8080"), 8080);
  assert.strictEqual(normalizePort(1), 1);
  assert.strictEqual(normalizePort(65535), 65535);
  assert.strictEqual(normalizePort(0), null);
  assert.strictEqual(normalizePort(65536), null);
  assert.strictEqual(normalizePort(1.5), null);
  assert.strictEqual(normalizePort("abc"), null);
});

test("normalizeGameType falls back to DarkSouls3", () => {
  assert.strictEqual(normalizeGameType("DarkSouls2"), "DarkSouls2");
  assert.strictEqual(normalizeGameType("DarkSouls3"), "DarkSouls3");
  assert.strictEqual(normalizeGameType("unknown"), "DarkSouls3");
  assert.strictEqual(normalizeGameType("  DarkSouls2  "), "DarkSouls2");
});

test("validatePublicKey handles edge cases", () => {
  assert.strictEqual(validatePublicKey(""), null);
  assert.strictEqual(validatePublicKey(null), null);
  assert.strictEqual(validatePublicKey("abc<>"), null);
  assert.strictEqual(
    validatePublicKey("a".repeat(5000)),
    null,
  );
  // valid base64, length multiple of 4
  assert.strictEqual(validatePublicKey("QUJDREVGRw=="), "QUJDREVGRw==");
  // not multiple of 4
  assert.strictEqual(validatePublicKey("QUJD"), "QUJD");
  assert.strictEqual(validatePublicKey("QUJ"), null);
});

test("normalizeServerId rejects too-long and unsafe ids", () => {
  assert.strictEqual(normalizeServerId("a".repeat(200)), null);
  assert.strictEqual(normalizeServerId("a:b.c-d_e"), "a:b.c-d_e");
  assert.strictEqual(normalizeServerId("a..b"), null);
});

test("isServerAllowedToShard matches the allowlist", () => {
  const allowed = shardingAllowList[0];
  const allowedObj = buildServerObject(makeServerData({ hostname: allowed }));
  assert.ok(isServerAllowedToShard(allowedObj));

  const deniedObj = buildServerObject(
    makeServerData({ hostname: "https://not-allowed.example" }),
  );
  assert.ok(!isServerAllowedToShard(deniedObj));
});

test("isServerFilter filters servers below the oldest supported version", () => {
  const old = buildServerObject(makeServerData({ version: 1 }));
  assert.ok(isServerFilter(old));

  const newEnough = buildServerObject(makeServerData({ version: 2 }));
  assert.ok(!isServerFilter(newEnough));
});

test("isServerFilter detects filter matches", () => {
  filters.push("spammy");
  try {
    const obj = buildServerObject(
      makeServerData({ name: "this is SPAMMY content" }),
    );
    assert.ok(isServerFilter(obj));
  } finally {
    filters.pop();
  }
});

test("isServerCensored detects censor matches", () => {
  censors.push("badword");
  try {
    const obj = buildServerObject(
      makeServerData({ description: "contains BADWORD here" }),
    );
    assert.ok(isServerCensored(obj));
  } finally {
    censors.pop();
  }
});

test("addServer persists a normal server", () => {
  const id = "normal-server";
  addServer(makeServerData({ id }));
  assert.ok(activeServers.has(id));
});

test("addServer drops disallowed sharding servers", () => {
  const id = "drop-shard";
  addServer(
    makeServerData({
      id,
      allowSharding: true,
      hostname: "https://not-allowed.example",
    }),
  );
  assert.ok(!activeServers.has(id));
});

test("addServer keeps allowed sharding servers", () => {
  const id = "keep-shard";
  addServer(
    makeServerData({
      id,
      allowSharding: true,
      hostname: shardingAllowList[0],
    }),
  );
  assert.ok(activeServers.has(id));
});

test("addServer marks censored servers", () => {
  censors.push("badword");
  try {
    const id = "censored-server";
    addServer(makeServerData({ id, name: "BADWORD server" }));
    assert.ok(activeServers.has(id));
    assert.strictEqual(activeServers.get(id).Censored, true);
  } finally {
    censors.pop();
  }
});

test("addServer drops filtered servers", () => {
  filters.push("spammy");
  try {
    const id = "filtered-server";
    addServer(makeServerData({ id, name: "spammy server" }));
    assert.ok(!activeServers.has(id));
  } finally {
    filters.pop();
  }
});

test("removeServer removes an existing server", () => {
  const id = "to-remove";
  addServer(makeServerData({ id }));
  assert.ok(activeServers.has(id));
  removeServer(id);
  assert.ok(!activeServers.has(id));
});

test("removeTimedOutServers removes stale servers only", () => {
  const staleId = "stale-server";
  const freshId = "fresh-server";

  addServer(makeServerData({ id: staleId }));
  addServer(makeServerData({ id: freshId }));

  activeServers
    .get(staleId)
    .UpdatedTime = Date.now() - (config.serverTimeoutMs + 1000);

  removeTimedOutServers();

  assert.ok(!activeServers.has(staleId));
  assert.ok(activeServers.has(freshId));
});

test("getServerIdFromRequest prefers body ServerId", () => {
  const req = { ip: "192.168.0.1", body: { ServerId: "explicit-id" } };
  assert.strictEqual(getServerIdFromRequest(req), "explicit-id");
});

test("getServerIdFromRequest falls back to client ip", () => {
  const req = { ip: "192.168.0.1", body: {} };
  assert.strictEqual(getServerIdFromRequest(req), "192.168.0.1");
});

test("getServerIdFromRequest returns null for invalid id", () => {
  const req = { ip: "bad ip", body: {} };
  assert.strictEqual(getServerIdFromRequest(req), null);
});

test("getPortFromRequest uses body Port or default", () => {
  assert.strictEqual(
    getPortFromRequest({ body: { Port: 60000 } }),
    60000,
  );
  assert.strictEqual(
    getPortFromRequest({ body: { Port: 999999 } }),
    50050,
  );
  assert.strictEqual(
    getPortFromRequest({ body: {} }),
    50050,
  );
});
