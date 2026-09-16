const assert = require("node:assert");
const test = require("node:test");

const {
  formatDuration,
  getClientIp,
  sendError,
  requireField,
} = require("../utils/helpers");

function makeRes() {
  const res = {
    statusCode: null,
    body: null,
  };
  res.status = function (code) {
    res.statusCode = code;
    return res;
  };
  res.json = function (body) {
    res.body = body;
    return res;
  };
  return res;
}

test("formatDuration handles zero", () => {
  assert.strictEqual(formatDuration(0), "0s");
});

test("formatDuration handles seconds only", () => {
  assert.strictEqual(formatDuration(5000), "5s");
});

test("formatDuration handles minutes and seconds", () => {
  assert.strictEqual(formatDuration(65 * 1000), "1m 5s");
});

test("formatDuration handles hours minutes and seconds", () => {
  assert.strictEqual(
    formatDuration((3600 + 120 + 5) * 1000),
    "1h 2m 5s",
  );
});

test("formatDuration handles days hours minutes and seconds", () => {
  assert.strictEqual(
    formatDuration((86400 + 3600 + 120 + 5) * 1000),
    "1d 1h 2m 5s",
  );
});

test("formatDuration omits zero units except seconds", () => {
  assert.strictEqual(formatDuration(3600000), "1h 0s");
});

test("getClientIp uses req.ip and strips IPv4-mapped prefix", () => {
  assert.strictEqual(getClientIp({ ip: "192.168.1.1" }), "192.168.1.1");
  assert.strictEqual(
    getClientIp({ ip: "::ffff:192.168.1.1" }),
    "192.168.1.1",
  );
});

test("getClientIp falls back to connection remote address", () => {
  assert.strictEqual(
    getClientIp({ connection: { remoteAddress: "10.0.0.1" } }),
    "10.0.0.1",
  );
});

test("getClientIp returns empty string when unavailable", () => {
  assert.strictEqual(getClientIp({ connection: {} }), "");
});

test("sendError sets status and error payload", () => {
  const res = makeRes();
  sendError(res, 400, "bad");
  assert.strictEqual(res.statusCode, 400);
  assert.deepStrictEqual(res.body, { status: "error", message: "bad" });
});

test("requireField rejects missing, empty, null and undefined values", () => {
  assert.strictEqual(
    requireField({ body: {} }, makeRes(), "name"),
    false,
  );
  assert.strictEqual(
    requireField({ body: { name: "" } }, makeRes(), "name"),
    false,
  );
  assert.strictEqual(
    requireField({ body: { name: null } }, makeRes(), "name"),
    false,
  );
  assert.strictEqual(
    requireField({ body: { name: undefined } }, makeRes(), "name"),
    false,
  );
});

test("requireField sends a 400 when a field is missing", () => {
  const res = makeRes();
  requireField({ body: {} }, res, "Name");
  assert.strictEqual(res.statusCode, 400);
  assert.match(res.body.message, /name/i);
});

test("requireField accepts a valid value", () => {
  assert.strictEqual(
    requireField({ body: { name: "ok" } }, makeRes(), "name"),
    true,
  );
});
