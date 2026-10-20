(function (root, factory) {
  if (typeof module === "object" && module.exports) module.exports = factory();
  else root.LottolApi = factory();
})(typeof globalThis !== "undefined" ? globalThis : this, function () {
  "use strict";
  function baseUrl(value) {
    const url = new URL(String(value).trim());
    if (!["http:", "https:"].includes(url.protocol) || url.username || url.password || url.search || url.hash) {
      throw new Error("http:// 또는 https:// 형식의 서버 주소를 입력해 주세요.");
    }
    url.pathname = url.pathname.replace(/\/+$/, "") + "/";
    return url.href;
  }
  function createClient(options) {
    const base = baseUrl(options.baseUrl);
    const fetcher = options.fetchImpl || globalThis.fetch.bind(globalThis);
    async function request(path, body) {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), options.timeoutMs || 5000);
      try {
        const response = await fetcher(new URL(path, base).href, {
          method: body === undefined ? "GET" : "POST", cache: "no-store", signal: controller.signal,
          ...(body === undefined ? {} : { headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) })
        });
        let data;
        try { data = await response.json(); }
        catch (_) { throw new Error("랭킹 API 대신 웹페이지가 응답했습니다. 서버 주소를 확인해 주세요."); }
        if (!response.ok) throw new Error(data.error || `서버 오류 (${response.status})`);
        return data;
      } catch (error) {
        if (error.name === "AbortError") throw new Error("서버 응답 시간이 초과됐습니다. 서버 실행 상태를 확인해 주세요.");
        if (error instanceof TypeError) throw new Error("서버에 연결할 수 없습니다. 실행 BAT와 서버 주소를 확인해 주세요.");
        throw error;
      } finally { clearTimeout(timer); }
    }
    return {
      baseUrl: base,
      async health() {
        const data = await request("api/health");
        if (data.app !== "lottol-ranking" || data.status !== "ok") throw new Error("LOTTOL 랭킹 서버 주소가 아닙니다.");
        return data;
      },
      async games() { const data = await request("api/games"); if (!Array.isArray(data.games)) throw new Error("게임 목록 응답이 잘못되었습니다."); return data.games; },
      profile: () => request("api/profile"),
      setProfile: playerName => request("api/profile", { playerName }),
      async scores(gameId, period) {
        const query = new URLSearchParams({ gameId, period });
        const data = await request(`api/scores?${query}`);
        if (!Array.isArray(data.scores)) throw new Error("랭킹 응답이 잘못되었습니다.");
        return data.scores;
      },
      submit: record => request("api/scores", record)
    };
  }
  return { baseUrl, createClient };
});
