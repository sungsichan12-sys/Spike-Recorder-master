"use strict";
const {test}=require("node:test"),assert=require("node:assert/strict"),fs=require("node:fs"),path=require("node:path"),vm=require("node:vm");
function fixture() {
  let files=[],locked=false,busy=false;
  const props=new Map([["DATA_FOLDER_ID","private"],["UPLOAD_KEY","a".repeat(64)]]),cache=new Map();
  const iterate=array=>{let i=0;return {hasNext:()=>i<array.length,next:()=>array[i++]};};
  const makeFile=(name,content)=>({getName:()=>name,getBlob:()=>({getDataAsString:()=>content}),setContent:value=>{content=value;},getUrl:()=>"https://drive.google.com/file/d/private-test/view"});
  const folder={getFiles:()=>iterate(files),getFilesByName:name=>iterate(files.filter(f=>f.getName()===name)),
    createFile:(name,content)=>{const file=makeFile(name,content);files.push(file);return file;}};
  const html=fs.readFileSync(path.join(__dirname,"../drivex-site/index.html"),"utf8");
  const site=makeFile("index.html",html);
  const publicFolder={getFilesByName:()=>iterate([site])};
  const endpoint="https://script.google.com/macros/s/DEPLOYMENT_TEST/exec";
  const context={console:{log(){}},Date,JSON,PropertiesService:{getScriptProperties:()=>({getProperty:k=>props.get(k),setProperty:(k,v)=>props.set(k,v)})},
    DriveApp:{getFolderById:id=>id==="private"?folder:publicFolder},
    ScriptApp:{getService:()=>({getUrl:()=>endpoint})},
    Utilities:{newBlob:value=>({getBytes:()=>Buffer.from(value)}),getUuid:()=>"random"},MimeType:{PLAIN_TEXT:"text/plain"},
    CacheService:{getScriptCache:()=>({get:k=>cache.get(k),put:(k,v)=>cache.set(k,v),remove:k=>cache.delete(k)})},
    LockService:{getScriptLock:()=>({tryLock:()=>{locked=!busy;return locked;},hasLock:()=>locked,releaseLock:()=>{locked=false;}})},
    ContentService:{MimeType:{JSON:"json",JAVASCRIPT:"js"},createTextOutput:text=>({text,setMimeType(type){this.type=type;return this;}})}};
  vm.createContext(context);vm.runInContext(fs.readFileSync(path.join(__dirname,"Code.gs"),"utf8"),context);
  const post=record=>JSON.parse(context.doPost({postData:{contents:JSON.stringify({uploadKey:"a".repeat(64),record})}}).text);
  const get=(action,query={})=>JSON.parse(context.doGet({parameter:{action,...query}}).text);
  return {context,post,get,files,site,props,folder,endpoint,isLocked:()=>locked,setBusy:value=>{busy=value;}};
}
const record=(id,score=100,name="한별")=>({eventId:id,gameId:"dino",score,playerName:name,recordedAtMs:Date.now()-1000,practice:false});
test("authorized native result round-trips and retry is idempotent",()=>{
  const f=fixture(),r=record("round-1",300,'한별 "1"\\A');
  assert.equal(f.post(r).ok,true);assert.equal(f.post(r).ok,true);assert.equal(f.files.length,1);
  assert.equal(f.get("scores").scores[0].playerName,r.playerName);assert.equal(f.isLocked(),false);
  assert.equal(f.post({...r,playerName:"다른 사람"}).ok,false);
});
test("unauthorized and unknown actions cannot read or write private connection",()=>{
  const f=fixture();f.folder.createFile("LOTTOL-PC-connection.json",JSON.stringify({uploadKey:"SECRET"}));
  const response=JSON.parse(f.context.doPost({postData:{contents:JSON.stringify({uploadKey:"wrong",record:record("bad")})}}).text);
  assert.equal(response.ok,false);assert.equal(f.files.length,1);
  assert.deepEqual(f.get("scores").scores,[]);assert.equal(f.get("connection").ok,false);
  assert.doesNotMatch(JSON.stringify(f.get("scores")),/SECRET|UPLOAD_KEY|DATA_FOLDER/);
});
test("score, game, name and play time validation reject malformed events",()=>{
  const f=fixture();
  for(const patch of [{score:-1},{score:1.5},{score:1e9+1},{gameId:"unknown"},{playerName:" "},{playerName:"a\nb"},{playerName:"가".repeat(33)},{recordedAtMs:Date.now()+600000},{eventId:"../bad"},{practice:"false"}])
    assert.equal(f.post({...record("bad"),...patch}).ok,false,JSON.stringify(patch));
  assert.equal(f.files.length,0);assert.equal(f.post(record("korean-limit",10,"가".repeat(32))).ok,true);
});
test("practice results are excluded and busy cloud is retryable",()=>{
  const f=fixture();assert.equal(f.post({...record("practice"),practice:true}).ignored,true);assert.equal(f.files.length,0);
  f.setBusy(true);assert.equal(f.post(record("retry")).ok,false);f.setBusy(false);
  assert.equal(f.post(record("retry")).ok,true);assert.equal(f.files.length,1);
});
test("week filter selects the best eligible round per player and game",()=>{
  const f=fixture();
  f.post({...record("old",900,"ALICE"),recordedAtMs:Date.now()-8*86400000});
  f.post(record("new",200,"alice"));f.post(record("other",400,"민수"));
  assert.deepEqual(f.get("scores").scores.map(r=>r.score),[900,400]);
  assert.deepEqual(f.get("scores",{period:"week"}).scores.map(r=>r.score),[400,200]);
  assert.equal(f.get("scores",{gameId:"night-pulse"}).scores.length,0);
});
test("JSONP is read-only with a constrained callback and safe script text",()=>{
  const f=fixture();f.post(record("html",20,"</script><img>"));
  const response=f.context.doGet({parameter:{action:"scores",callback:"lottol_"+"a".repeat(24)}});
  assert.equal(response.type,"js");assert.match(response.text,/^lottol_[a-f0-9]{24}\(/);assert.doesNotMatch(response.text,/<\/script>/);
  const bad=f.context.doGet({parameter:{action:"scores",callback:"alert(1)//"}});
  assert.equal(bad.type,"json");assert.equal(JSON.parse(bad.text).ok,false);
});
test("publish updates public URL while saving upload key only in private folder",()=>{
  const f=fixture();f.context.publishSite();
  const html=f.site.getBlob().getDataAsString();
  assert.match(html,/DEPLOYMENT_TEST/);assert.doesNotMatch(html,/a{64}/);
  const privateFile=f.files.find(file=>file.getName()==="LOTTOL-PC-connection.json");
  assert.equal(JSON.parse(privateFile.getBlob().getDataAsString()).uploadKey,"a".repeat(64));
  assert.deepEqual(f.get("scores").scores,[]);
});
