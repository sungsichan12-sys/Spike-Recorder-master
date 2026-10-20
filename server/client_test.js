"use strict";
const { test } = require("node:test");
const assert = require("node:assert/strict");
const { baseUrl, createClient } = require("../arena-client.js");
const json = (data, status = 200) => new Response(JSON.stringify(data), { status });
test("server URLs are normalized and credentials or non-HTTP schemes rejected", () => {
  assert.equal(baseUrl("http://127.0.0.1:8765"), "http://127.0.0.1:8765/");
  for (const url of ["file:///index.html", "http://name:password@localhost", "http://localhost/?token=x"]) assert.throws(() => baseUrl(url));
});
test("health handshake checks the server identity", async () => {
  const client = createClient({ baseUrl: "http://localhost", fetchImpl: async () => json({ app: "other", status: "ok" }) });
  await assert.rejects(client.health(), /LOTTOL/);
});
test("filter parameters and an API base path are preserved", async () => {
  let observed;
  const client = createClient({ baseUrl: "https://example.test/arena/", fetchImpl: async url => { observed = url; return json({ scores: [] }); } });
  await client.scores("night-pulse", "week");
  assert.equal(observed, "https://example.test/arena/api/scores?gameId=night-pulse&period=week");
});
test("submissions carry JSON, stable event ID and no client credentials", async () => {
  const record = { eventId: "stable-id", playerName: "학생", gameId: "dino", score: 20 };
  const client = createClient({ baseUrl: "http://localhost", fetchImpl: async (url, request) => {
    assert.equal(request.method, "POST"); assert.equal(request.headers["Content-Type"], "application/json");
    assert.deepEqual(JSON.parse(request.body), record); return json({ record }, 201);
  } });
  assert.deepEqual((await client.submit(record)).record, record);
});
test("server errors remain visible", async () => {
  const client = createClient({ baseUrl: "http://localhost", fetchImpl: async () => json({ error: "저장 실패" }, 503) });
  await assert.rejects(client.profile(), /저장 실패/);
});
test("an HTML response is not mistaken for a working API", async () => {
  const client = createClient({ baseUrl: "http://localhost", fetchImpl: async () => new Response("<!doctype html>") });
  await assert.rejects(client.health(), /웹페이지/);
});
test("network failure is reported without invented preview scores", async () => {
  const client = createClient({ baseUrl: "http://localhost", fetchImpl: async () => { throw new TypeError("fetch failed"); } });
  await assert.rejects(client.scores("all", "all"), /서버에 연결/);
});
test("a stalled request has a bounded timeout", async () => {
  const client = createClient({ baseUrl: "http://localhost", timeoutMs: 20, fetchImpl: (_, request) => new Promise((resolve, reject) => {
    request.signal.addEventListener("abort", () => reject(new DOMException("Aborted", "AbortError")));
  }) });
  await assert.rejects(client.health(), /초과/);
});
