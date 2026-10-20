"use strict";
// Execute the actual page controller against the real HTTP API with a minimal
// DOM fixture. No browser, window, screen capture or computer-use API is used.
const vm = require("node:vm"), fs = require("node:fs"), path = require("node:path");
const assert = require("node:assert/strict");
const Api = require("../arena-client.js");
const source = fs.readFileSync(path.join(__dirname, "../arena-page.js"), "utf8");
class Element {
  constructor() {
    this.value = ""; this.textContent = ""; this.innerHTML = ""; this.events = {};
    const set = new Set();
    this.classList = { add: key => set.add(key), remove: key => set.delete(key), contains: key => set.has(key),
      toggle(key, on) { if (on) set.add(key); else set.delete(key); } };
  }
  addEventListener(name, handler) { this.events[name] = handler; }
  querySelector() { return this.button || (this.button = new Element()); }
  showModal() { this.open = true; }
  close() { this.open = false; }
}
async function until(predicate) {
  for (let i = 0; i < 150; i++) { if (predicate()) return; await new Promise(resolve => setTimeout(resolve, 20)); }
  throw new Error("Page did not reach expected state");
}
(async () => {
  const elements = new Map(), storage = new Map(), intervals = [];
  const html = fs.readFileSync(path.join(__dirname, "../index.html"), "utf8");
  const ids = new Set(Array.from(html.matchAll(/\bid="([^"]+)"/g), match => match[1]));
  const el = id => {
    if (!id.startsWith("#") && !id.startsWith("[")) assert.equal(ids.has(id), true, `Missing HTML element: ${id}`);
    if (!elements.has(id)) elements.set(id, new Element()); return elements.get(id);
  };
  const periods = [new Element(), new Element()]; periods[0].dataset = { period: "all" }; periods[1].dataset = { period: "week" };
  let offline = false;
  const url = new URL(process.argv[2]);
  const context = {
    console, URL, URLSearchParams, Intl, Date, Promise, crypto: require("node:crypto").webcrypto,
    location: { protocol: url.protocol, hostname: url.hostname, origin: url.origin },
    document: { hidden: false, getElementById: el, querySelector: selector => el(selector), querySelectorAll: () => periods },
    localStorage: { getItem: key => storage.get(key) || null, setItem: (key, value) => storage.set(key, value), removeItem: key => storage.delete(key) },
    window: { addEventListener() {} }, setInterval: callback => intervals.push(callback), setTimeout: () => 1, clearTimeout() {},
    FormData: class { constructor(form) { this.fields = form.fields; } get(key) { return this.fields[key]; } },
    LottolApi: { createClient: options => Api.createClient({ ...options, fetchImpl: (...args) => {
      if (offline) return Promise.reject(new TypeError("Offline")); return fetch(...args);
    } }) }
  };
  vm.runInNewContext(source, context);
  await until(() => el("#data-status span").textContent === "서버 연결됨");
  assert.match(el("game-tabs").innerHTML, /Night Pulse 3D/);
  el("player-name").value = "학생 테스트";
  await el("profile-form").events.submit({ preventDefault() {} });
  const form = el("score-form");
  form.fields = { playerName: "<img src=x onerror=1>", gameId: "dino", score: "1234" };
  await form.events.submit({ preventDefault() {}, currentTarget: form });
  assert.match(el("rank-rows").innerHTML, /1,234/);
  assert.match(el("rank-rows").innerHTML, /&lt;img/);
  assert.doesNotMatch(el("rank-rows").innerHTML, /<img/);
  assert.match(el("rank-rows").innerHTML, /수동/);
  offline = true; intervals[0]();
  await until(() => el("#data-status span").textContent === "연결 끊김");
  assert.equal(el("preview-notice").classList.contains("visible"), true);
  assert.equal(el("submit-score").disabled, true);
  offline = false; intervals[0]();
  await until(() => el("#data-status span").textContent === "서버 연결됨");
  assert.match(el("rank-rows").innerHTML, /1,234/);
  el("server-url").value="";
  vm.runInNewContext(source, {...context,LOTTOL_CONFIG:{mode:"cloud",apiUrl:""},location:{protocol:"file:",hostname:"",origin:"null",replace(){throw new Error("A file page must not redirect to localhost");}}});
  assert.equal(el("server-url").value, "");
  assert.match(el("connection-message").textContent,/클라우드 API 배포 전/);
  const endpoint="https://script.google.com/macros/s/CLOUD_TEST/exec";
  const Cloud=require("../arena-cloud-client.js");
  vm.runInNewContext(source,{...context,LOTTOL_CONFIG:{mode:"cloud",apiUrl:endpoint},
    location:{protocol:"https:",hostname:"school.example",origin:"https://school.example"},
    LottolApi:{createClient(){throw new Error("Cloud page must not fall back to a local server");}},
    LottolCloudApi:{createClient:options=>Cloud.createClient({...options,readImpl:async value=>{
      const action=new URL(value).searchParams.get("action");
      if(action==="health") return {ok:true,app:"lottol-ranking",status:"ok"};
      if(action==="games") return {ok:true,games:[{id:"dino",name:"Signal Dino 3D"}]};
      return {ok:true,scores:[{playerName:"<학생>",gameId:"dino",score:320,createdAt:new Date().toISOString(),source:"program"}]};
    }})}});
  await until(()=>el("rank-rows").innerHTML.includes("320"));
  assert.equal(el("server-url").value,endpoint);
  assert.match(el("rank-rows").innerHTML,/&lt;학생&gt;/);
  assert.equal(el("submit-score").disabled,true);assert.equal(el("submit-score").hidden,true);
  assert.equal(el("profile-form").hidden,true);
  console.log("PASS actual page controller: connect, submit, escape names, offline, reconnect, no localhost redirect");
  console.log("PASS cloud page: configured endpoint, read-only rankings, escaped names, no local server");
})().catch(error => { console.error(error); process.exitCode = 1; });
