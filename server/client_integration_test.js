"use strict";
const assert = require("node:assert/strict");
const { createClient } = require("../arena-client.js");
(async () => {
  const client = createClient({ baseUrl: process.argv[2] });
  await client.health();
  assert.equal((await client.games()).length, 2);
  assert.equal((await client.setProfile("연결 테스트")).playerName, "연결 테스트");
  const record = { eventId: "frontend-live-1", playerName: "연결 테스트", gameId: "night-pulse", score: 4200 };
  assert.equal((await client.submit(record)).record.score, 4200);
  assert.equal((await client.submit(record)).duplicate, true);
  assert.equal((await client.scores("night-pulse", "all"))[0].score, 4200);
  assert.equal((await client.scores("dino", "all")).length, 0);
  await assert.rejects(client.submit({ ...record, eventId: "bad", score: -1 }));
  console.log("PASS actual page client -> HTTP -> SQLite -> ranking readback");
})().catch(error => { console.error(error); process.exitCode = 1; });
