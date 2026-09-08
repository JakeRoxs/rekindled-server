const assert = require("node:assert");
const test = require("node:test");

process.env.MASTER_SERVER_WRITE_SECRET = "test-secret";
process.env.MASTER_SERVER_CORS_ORIGINS = "https://ok.com";

const app = require("../index");

// Swallow the expected CORS error so Express does not dump a stack trace
// during the "disallowed origin" test (still results in a 500).
app.use((err, req, res, next) => {
  if (res.headersSent || err === undefined) return next(err);
  res.status(500).json({ status: "error" });
});

let server;
let baseUrl;

test.before(async () => {
  server = await new Promise((resolve) => {
    const s = app.listen(0, () => resolve(s));
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

test("CORS echoes a configured allowed origin", async () => {
  const res = await fetch(`${baseUrl}/`, {
    headers: { origin: "https://ok.com", connection: "close" },
  });
  assert.strictEqual(res.status, 200);
  assert.strictEqual(
    res.headers.get("access-control-allow-origin"),
    "https://ok.com",
  );
});

test("CORS rejects a disallowed origin", async () => {
  const res = await fetch(`${baseUrl}/`, {
    headers: { origin: "https://evil.com", connection: "close" },
  });
  assert.strictEqual(res.status, 500);
});

test("CORS allows requests without an origin header", async () => {
  const res = await fetch(`${baseUrl}/`, {
    headers: { connection: "close" },
  });
  assert.strictEqual(res.status, 200);
});

test("CORS wildcard policy allows any origin", async () => {
  process.env.MASTER_SERVER_CORS_ORIGINS = "*";
  delete require.cache[require.resolve("../config")];
  delete require.cache[require.resolve("../routes/api/v1/servers")];
  delete require.cache[require.resolve("../index")];
  const wildcardApp = require("../index");

  const wildcardServer = await new Promise((resolve) => {
    const s = wildcardApp.listen(0, () => resolve(s));
  });
  const { port } = wildcardServer.address();

  const res = await fetch(`http://127.0.0.1:${port}/`, {
    headers: { origin: "https://anything.com", connection: "close" },
  });
  assert.strictEqual(res.status, 200);
  assert.strictEqual(res.headers.get("access-control-allow-origin"), "*");

  await new Promise((resolve) => {
    wildcardServer.close(resolve);
    wildcardServer.closeAllConnections?.();
    wildcardServer.closeIdleConnections?.();
  });
});
