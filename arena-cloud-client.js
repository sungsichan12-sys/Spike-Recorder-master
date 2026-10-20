(function(root,factory) {
  if(typeof module==="object" && module.exports) module.exports=factory();
  else root.LottolCloudApi=factory();
})(typeof globalThis!=="undefined"?globalThis:this,function() {
  "use strict";
  function endpoint(value) {
    const url=new URL(String(value).trim());
    if(url.protocol!=="https:" || url.hostname!=="script.google.com" || url.username || url.password || url.search || url.hash || !/^\/macros\/s\/[A-Za-z0-9_-]+\/exec$/.test(url.pathname))
      throw new Error("Google Apps Script의 배포된 /exec 주소를 입력해 주세요.");
    return url.href;
  }
  function jsonp(url,options={}) {
    const document=options.document||globalThis.document,root=options.root||globalThis;
    return new Promise((resolve,reject)=>{
      const random=new Uint8Array(12);(options.crypto||globalThis.crypto).getRandomValues(random);
      const name="lottol_"+Array.from(random,b=>b.toString(16).padStart(2,"0")).join("");
      const script=document.createElement("script");
      let timer;
      function finish(error,data) {
        clearTimeout(timer);script.remove();delete root[name];
        if(error) reject(error);else if(!data || data.ok!==true) reject(new Error(data?.error||"클라우드 응답이 잘못되었습니다."));else resolve(data);
      }
      root[name]=data=>finish(null,data);
      const target=new URL(url);target.searchParams.set("callback",name);target.searchParams.set("_",Date.now());
      script.src=target.href;script.referrerPolicy="no-referrer";
      script.onerror=()=>finish(new Error("클라우드에 연결할 수 없습니다. 웹 앱의 배포 및 공개 접속 설정을 확인해 주세요."));
      timer=setTimeout(()=>finish(new Error("클라우드 응답 시간이 초과됐습니다.")),options.timeoutMs||15000);
      document.head.appendChild(script);
    });
  }
  function createClient(options) {
    const base=endpoint(options.baseUrl);
    const read=async(action,values={})=>{
      const url=new URL(base);url.search=new URLSearchParams({action,...values});
      return (options.readImpl||jsonp)(url.href,options);
    };
    let name="";
    return {
      baseUrl:base,readOnly:true,
      async health() {const data=await read("health");if(data.app!=="lottol-ranking" || data.status!=="ok") throw new Error("LOTTOL 클라우드 주소가 아닙니다.");return data;},
      async games() {const data=await read("games");if(!Array.isArray(data.games)) throw new Error("게임 목록이 잘못되었습니다.");return data.games;},
      async scores(gameId,period) {const data=await read("scores",{gameId,period});if(!Array.isArray(data.scores)) throw new Error("랭킹 응답이 잘못되었습니다.");return data.scores;},
      async profile() {return {playerName:name};},
      async setProfile(value) {name=String(value).trim();return {playerName:name};},
      async submit() {throw new Error("클라우드 점수는 프로그램에서 자동 전송합니다.");}
    };
  }
  return {endpoint,jsonp,createClient};
});
