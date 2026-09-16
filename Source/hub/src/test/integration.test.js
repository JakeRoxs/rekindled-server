const assert = require("node:assert");
const test = require("node:test");

process.env.MASTER_SERVER_WRITE_SECRET = "test-secret";
process.env.SHARDING_ALLOWLIST =
  "https://allowed.example,https://allowed2.example";

const app = require("../index");

let server;
let baseUrl;

test.before(async () => {
  await new Promise((resolve) => {
    server = app.listen(0, resolve);
  });
  const { port } = server.address();
  baseUrl = `http://127.0.0.1:${port}`;
});

test.after(async () => {
  await new Promise((resolve) => {
    server.close(resolve);
    server.closeAllConnections?.();
    server.closeIdleConnections?.();
  });
});

async function req(path, opts = {}) {
  const { method = "GET", body, headers = {}, auth } = opts;
  const finalHeaders = { ...headers };
  if (auth !== undefined) {
    finalHeaders["x-master-server-write-secret"] = auth;
  }
  if (body !== undefined) {
    finalHeaders["content-type"] = "application/json";
  }
  finalHeaders["connection"] = "close";

  const res = await fetch(`${baseUrl}${path}`, {
    method,
    headers: finalHeaders,
    body: body !== undefined ? JSON.stringify(body) : undefined,
  });

  const text = await res.text();
  let json = null;
  try {
    json = JSON.parse(text);
  } catch {
    json = null;
  }
  return { status: res.status, json, text, headers: res.headers };
}

function serverBody(overrides = {}) {
  return {
    Hostname: "example.com",
    PrivateHostname: "private.example.com",
    Description: "Test server",
    Name: "Test Server",
    PublicKey: "QUJDREVGRw==",
    PlayerCount: 5,
    Password: "pass",
    ModsWhiteList: "1",
    ModsBlackList: "1",
    ModsRequiredList: "1",
    WebAddress: "https://example.com",
    Port: 50050,
    GameType: "DarkSouls3",
    ServerVersion: 2,
    ...overrides,
  };
}

test("GET / renders the dashboard", async () => {
  const r = await req("/");
  assert.strictEqual(r.status, 200);
  assert.match(r.text, /<html/i);
});

test("GET /api/v1/servers returns success with a list", async () => {
  const r = await req("/api/v1/servers");
  assert.strictEqual(r.status, 200);
  assert.strictEqual(r.json.status, "success");
  assert.ok(Array.isArray(r.json.servers));
});

test("GET /api/v1/servers/status returns status data", async () => {
  const r = await req("/api/v1/servers/status");
  assert.strictEqual(r.status, 200);
  assert.strictEqual(r.json.status, "success");
  assert.ok(typeof r.json.statusData.activeServerCount === "number");
  assert.ok(Array.isArray(r.json.statusData.shardingAllowList));
});

test("POST /api/v1/servers rejects an incorrect secret", async () => {
  const r = await req("/api/v1/servers", {
    method: "POST",
    body: serverBody(),
    auth: "wrong-secret",
  });
  assert.strictEqual(r.status, 401);
});

test("POST /api/v1/servers rejects missing required fields", async () => {
  const body = serverBody();
  delete body.Hostname;
  const r = await req("/api/v1/servers", {
    method: "POST",
    body,
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 400);
});

test("POST /api/v1/servers rejects invalid public key", async () => {
  const r = await req("/api/v1/servers", {
    method: "POST",
    body: serverBody({ PublicKey: "not-a-valid-key" }),
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 400);
});

test("POST /api/v1/servers rejects too-long passwords", async () => {
  const r = await req("/api/v1/servers", {
    method: "POST",
    body: serverBody({ Password: "x".repeat(2000) }),
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 400);
});

test("POST /api/v1/servers rejects invalid player count", async () => {
  const r = await req("/api/v1/servers", {
    method: "POST",
    body: serverBody({ PlayerCount: "not-a-number" }),
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 400);
});

test("POST /api/v1/servers adds a server that then appears in the list", async () => {
  const r = await req("/api/v1/servers", {
    method: "POST",
    body: serverBody({ ServerId: "testserver1" }),
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 200);
  assert.strictEqual(r.json.status, "success");

  const list = await req("/api/v1/servers");
  const found = list.json.servers.find((s) => s.Id === "testserver1");
  assert.ok(found);
  assert.strictEqual(found.Name, "Test Server");
  assert.strictEqual(found.PasswordRequired, true);
});

test("POST /api/v1/servers allows a whitelisted sharding server", async () => {
  const r = await req("/api/v1/servers", {
    method: "POST",
    body: serverBody({
      ServerId: "shardallowed",
      Hostname: "https://allowed.example",
      AllowSharding: "1",
    }),
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 200);

  const list = await req("/api/v1/servers");
  assert.ok(list.json.servers.find((s) => s.Id === "shardallowed"));
});

test("POST /api/v1/servers drops a non-whitelisted sharding server", async () => {
  const r = await req("/api/v1/servers", {
    method: "POST",
    body: serverBody({
      ServerId: "sharddropped",
      Hostname: "https://not-allowed.example",
      AllowSharding: "true",
    }),
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 200);

  const list = await req("/api/v1/servers");
  assert.ok(!list.json.servers.find((s) => s.Id === "sharddropped"));
});

test("POST /:id/public_key returns the public key with correct password", async () => {
  const r = await req("/api/v1/servers/testserver1/public_key", {
    method: "POST",
    body: { password: "pass" },
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 200);
  assert.strictEqual(r.json.status, "success");
  assert.strictEqual(r.json.PublicKey, "QUJDREVGRw==");
});

test("POST /:id/public_key rejects an incorrect password", async () => {
  const r = await req("/api/v1/servers/testserver1/public_key", {
    method: "POST",
    body: { password: "wrong" },
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 401);
});

test("POST /:id/public_key rejects a missing password", async () => {
  const r = await req("/api/v1/servers/testserver1/public_key", {
    method: "POST",
    body: {},
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 400);
});

test("POST /:id/public_key rejects a missing server", async () => {
  const r = await req("/api/v1/servers/doesnotexist/public_key", {
    method: "POST",
    body: { password: "pass" },
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 404);
});

test("DELETE /api/v1/servers removes a server", async () => {
  const r = await req("/api/v1/servers", {
    method: "DELETE",
    body: { ServerId: "testserver1" },
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 200);
  assert.strictEqual(r.json.status, "success");

  const list = await req("/api/v1/servers");
  assert.ok(!list.json.servers.find((s) => s.Id === "testserver1"));
});

test("DELETE /api/v1/servers rejects invalid ServerId", async () => {
  const r = await req("/api/v1/servers", {
    method: "DELETE",
    body: { ServerId: "bad id with spaces" },
    auth: "test-secret",
  });
  assert.strictEqual(r.status, 400);
});

test("DELETE /api/v1/servers rejects an incorrect secret", async () => {
  const r = await req("/api/v1/servers", {
    method: "DELETE",
    body: { ServerId: "testserver1" },
    auth: "wrong-secret",
  });
  assert.strictEqual(r.status, 401);
});
