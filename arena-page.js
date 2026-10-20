/* DriveX hosts the page; Google Apps Script stores rankings in the cloud. */
(function () {
  "use strict";
  const config=globalThis.LOTTOL_CONFIG||{};
  const $ = id => document.getElementById(id);
  const stored = { get(key) { try { return localStorage.getItem(key); } catch (_) { return null; } },
    set(key, value) { try { localStorage.setItem(key, value); } catch (_) {} },
    remove(key) { try { localStorage.removeItem(key); } catch (_) {} } };
  let api, connected = false, allScores = [], games = [], selectedGame = "all", selectedPeriod = "all";
  let generation = 0, loadId = 0, toastTimer;
  let scoresBase = "";
  const localPage=["127.0.0.1", "localhost", "[::1]"].includes(location.hostname) && config.mode!=="cloud";
  const saved=stored.get("lottolCloudApiUrl");
  $("server-url").value = config.apiUrl || saved || (localPage?location.origin:"");
  function escapeHtml(value) { return String(value).replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]); }
  function formatScore(value) { return new Intl.NumberFormat("ko-KR", { maximumFractionDigits: 0 }).format(value); }
  function formatDate(value) { const date = new Date(value); return Number.isNaN(date.getTime()) ? "—" : new Intl.DateTimeFormat("ko-KR", { month: "2-digit", day: "2-digit" }).format(date); }
  function gameName(id) { return ({dino:"공룡 달리기","night-pulse":"나이트 펄스"})[id] || games.find(game => game.id === id)?.name || id; }
  function initial(name) { return [...name.replace(/[^\p{L}\p{N}]/gu, "")][0]?.toUpperCase() || "L"; }
  function toast(message) { $("toast").textContent = message; $("toast").classList.add("visible"); clearTimeout(toastTimer); toastTimer = setTimeout(() => $("toast").classList.remove("visible"), 5000); }
  function status(ok, message) {
    connected = ok;
    $("data-status").classList.toggle("live", ok);
    document.querySelector("#data-status span").textContent = ok ? "서버 연결됨" : "연결 끊김";
    $("preview-notice").classList.toggle("visible", !ok);
    $("connection-message").textContent = ok ? `연결됨 · ${api.baseUrl} · 5초마다 자동 갱신` : message;
    $("connection-error").textContent = message || "";
    $("submit-score").disabled = !ok || !!api?.readOnly;
    $("submit-score").textContent = api?.readOnly ? "점수는 게임에서 자동 등록" : "기록 제출";
  }
  function render() {
    const entries = [{ id: "all", name: "모든 최고 기록" }, ...games];
    const tabs = entries.map((g,i) => `<button type="button" class="tab${selectedGame === g.id ? " active" : ""}" data-game="${escapeHtml(g.id)}" aria-pressed="${selectedGame === g.id}"><span class="tab-index" aria-hidden="true">${String(i).padStart(2,"0")}</span><span class="tab-copy"><span class="tab-label">${g.id==="all"?"전체 게임":escapeHtml(gameName(g.id))}</span><small>${escapeHtml(g.name)}</small></span></button>`).join("");
    if($("game-tabs").innerHTML!==tabs) $("game-tabs").innerHTML=tabs;
    const search = $("search-input").value.trim().toLocaleLowerCase();
    const ranked = allScores.filter(s => selectedGame === "all" || s.gameId === selectedGame).map((record,index)=>({record,rank:index+1}));
    const visible = ranked.filter(({record}) => record.playerName.toLocaleLowerCase().includes(search));
    const scores = visible.map(({record})=>record);
    $("stat-records").textContent = formatScore(scores.length);
    $("stat-games").textContent = formatScore(games.length);
    $("stat-best").textContent = ranked.length ? formatScore(ranked[0].record.score) : "—";
    $("result-count").textContent = `${formatScore(scores.length)}개 기록`;
    $("ranking-caption").textContent = selectedGame==="all" ? "플레이어별·게임별 최고 점수를 보여드려요." : `${gameName(selectedGame)} · ${selectedPeriod==="week"?"최근 7일":"전체 기간"} 최고 기록`;
    $("table-wrap").classList.toggle("empty-state", !scores.length);
    $("empty-state").classList.toggle("visible", !scores.length);
    $("empty-title").textContent = !connected ? "기록을 불러오고 있어요" : search ? "찾는 이름이 없어요" : "아직 등록된 기록이 없어요";
    $("empty-description").textContent = !connected ? "잠시만 기다려 주세요." : search ? "이름을 다시 입력하거나 검색어를 지워보세요." : "게임을 플레이해서 첫 기록을 남겨보세요.";
    $("rank-rows").innerHTML = visible.map(({record:s,rank}) => `<tr class="rank-row${rank===1?" is-first":""}"><td><span class="rank${rank<=3?" top":""}">${String(rank).padStart(2,"0")}</span></td><td><div class="player-cell"><i class="avatar" aria-hidden="true">${escapeHtml(initial(s.playerName))}</i><span class="player-copy"><span class="player-name" title="${escapeHtml(s.playerName)}">${escapeHtml(s.playerName)}${s.source==="manual"?' <small class="manual-tag">수동</small>':""}</span><span class="player-game">${escapeHtml(gameName(s.gameId))}</span></span></div></td><td class="game-cell">${escapeHtml(gameName(s.gameId))}</td><td class="score-cell">${formatScore(s.score)}<small>점</small></td><td class="date-cell">${escapeHtml(formatDate(s.createdAt))}</td></tr>`).join("");
    const leader=ranked[0]?.record;
    $("podium").innerHTML = `<article class="leader-card"><div class="leader-label"><span>현재 최고 기록</span><b>${leader?"1위":"기록 대기"}</b></div><span class="podium-name">${leader?escapeHtml(leader.playerName):"첫 도전을 기다려요"}</span><span class="podium-score">${leader?formatScore(leader.score):"—"}<small>점</small></span><span class="podium-game">${leader?escapeHtml(gameName(leader.gameId)):"다음 주인공은 바로 여러분"}</span></article>`;
  }
  async function loadScores() {
    if (!api) return;
    const current = api, id = ++loadId;
    try {
      const rows = await current.scores(selectedGame, selectedPeriod);
      if (current !== api || id !== loadId) return;
      allScores = rows.filter(s => typeof s.playerName === "string" && Number.isSafeInteger(s.score) && s.score >= 0);
      scoresBase = current.baseUrl;
      status(true, ""); render();
      $("last-updated").textContent = `최근 갱신 ${new Date().toLocaleTimeString("ko-KR")}`;
    } catch (error) {
      if (current !== api || id !== loadId) return;
      status(false, `${error.message} 기존에 조회한 기록은 마지막 갱신 상태입니다.`);
    }
  }
  async function connect(event) {
    if (event) event.preventDefault();
    const turn = ++generation;
    ++loadId; connected = false;
    $("connection-message").textContent = "서버 연결 확인 중…";
    try {
      const address=$("server-url").value.trim();
      if(!address) throw new Error("클라우드 API 배포 전입니다. Google Apps Script 배포를 마친 뒤 publishSite를 실행해 주세요.");
      const current = /^https:\/\/script\.google\.com\//.test(address)
        ? LottolCloudApi.createClient({baseUrl:address}) : LottolApi.createClient({baseUrl:address});
      if(!localPage && !current.readOnly) throw new Error("DriveX 페이지에는 Google Apps Script의 /exec 주소를 연결해 주세요.");
      api = current;
      $("profile-form").hidden=!!current.readOnly;
      $("submit-score").hidden=!!current.readOnly;
      await current.health();
      const [available, profile] = await Promise.all([current.games(), current.profile()]);
      if (turn !== generation) return;
      games = available.filter(g => typeof g.id === "string" && typeof g.name === "string");
      if (!games.some(g => g.id === selectedGame)) selectedGame = "all";
      $("player-name").value = profile.playerName;
      $("submit-game").innerHTML = games.map(g => `<option value="${escapeHtml(g.id)}">${escapeHtml(g.name)}</option>`).join("");
      $("server-url").value = current.baseUrl;
      if(current.readOnly) stored.set("lottolCloudApiUrl", current.baseUrl);
      if (scoresBase !== current.baseUrl) allScores = [];
      render(); await loadScores();
    } catch (error) { if (turn === generation) status(false, error.message); }
  }
  $("connection-form").addEventListener("submit", connect);
  $("profile-form").addEventListener("submit", async event => {
    event.preventDefault(); if (!connected) { toast("먼저 서버에 연결해 주세요."); return; }
    try { const profile = await api.setProfile($("player-name").value.trim()); $("player-name").value = profile.playerName; toast("홈페이지 수동 입력에 사용할 이름을 저장했습니다."); }
    catch (error) { toast(error.message); }
  });
  $("refresh-scores").addEventListener("click", () => connected ? loadScores() : connect());
  $("game-tabs").addEventListener("click", event => { const b = event.target.closest("[data-game]"); if (!b) return; selectedGame = b.dataset.game; render(); loadScores(); });
  document.querySelectorAll("[data-period]").forEach(button => button.addEventListener("click", () => {
    selectedPeriod = button.dataset.period;
    document.querySelectorAll("[data-period]").forEach(b => {b.classList.toggle("active", b === button);b.setAttribute?.("aria-pressed",String(b===button));}); loadScores();
  }));
  $("search-input").addEventListener("input", render);
  const dialog = $("score-dialog");
  $("submit-score").addEventListener("click", () => { if (!connected) { toast("먼저 서버에 연결해 주세요."); return; } document.querySelector('[name="playerName"]').value = $("player-name").value; dialog.showModal(); });
  $("dialog-close").addEventListener("click", () => dialog.close());
  $("dialog-cancel").addEventListener("click", () => dialog.close());
  dialog.addEventListener("click", event => { if (event.target === dialog) dialog.close(); });
  $("score-form").addEventListener("submit", async event => {
    event.preventDefault(); if (!connected) { toast("먼저 서버에 연결해 주세요."); return; }
    const form = event.currentTarget, fields = new FormData(form), button = form.querySelector('[type="submit"]');
    const payload = { playerName: String(fields.get("playerName")).trim(), gameId: fields.get("gameId"), score: Number(fields.get("score")) };
    if (!Number.isSafeInteger(payload.score) || payload.score < 0 || payload.score > 1000000000) { toast("점수는 0~1,000,000,000 사이의 정수로 입력해 주세요."); return; }
    const signature = JSON.stringify(payload);
    let pending; try { pending = JSON.parse(stored.get("lottolPendingScore")); } catch (_) {}
    if (!pending || pending.signature !== signature) pending = { signature, eventId: typeof crypto.randomUUID === "function" ? crypto.randomUUID() : Array.from(crypto.getRandomValues(new Uint8Array(16)), b => b.toString(16).padStart(2, "0")).join("") };
    stored.set("lottolPendingScore", JSON.stringify(pending));
    button.disabled = true;
    try {
      await api.submit({ ...payload, eventId: pending.eventId });
      stored.remove("lottolPendingScore"); dialog.close(); toast("기록을 저장했습니다. 플레이어별 최고 점수를 표시합니다."); await loadScores();
    } catch (error) { toast(error.message); }
    finally { button.disabled = false; }
  });
  render(); connect();
  setInterval(() => { if (!document.hidden && $("server-url").value.trim()) connected ? loadScores() : connect(); }, 5000);
  window.addEventListener("online", () => connect());
})();
