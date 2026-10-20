/* LOTTOL cloud ranking API. Deploy as owner, access: Anyone.
 * Website files live in the user's DriveX folder. Scores and upload credentials
 * live in a separate PRIVATE folder created by setup().
 */
const PUBLIC_FOLDER_ID = '1LwVyq5kHoEvFfaSmj--GKY7fMGwckk8u';
const GAMES = [{id:'dino',name:'Signal Dino 3D'}, {id:'night-pulse',name:'Night Pulse 3D'}];
const CALLBACK = /^lottol_[a-f0-9]{24}$/;
const CACHE_KEY = 'lottol-records-v1';

function setup() {
  const properties=PropertiesService.getScriptProperties();
  if(!properties.getProperty('DATA_FOLDER_ID'))
    properties.setProperty('DATA_FOLDER_ID',DriveApp.createFolder('LOTTOL Ranking Data (private)').getId());
  if(!properties.getProperty('UPLOAD_KEY'))
    properties.setProperty('UPLOAD_KEY',Utilities.getUuid().replace(/-/g,'')+Utilities.getUuid().replace(/-/g,''));
  console.log('Setup complete. Deploy as a web app, then run publishSite().');
}

function dataFolder_() {
  const id=PropertiesService.getScriptProperties().getProperty('DATA_FOLDER_ID');
  if(!id) throw new Error('Cloud setup has not been run.');
  return DriveApp.getFolderById(id);
}

// Run in the editor AFTER deployment. Updates only the public API URL in the
// uploaded index.html. The private PC connection file must never go in DriveX.
function publishSite() {
  const endpoint=ScriptApp.getService().getUrl();
  if(!endpoint || !/\/exec$/.test(endpoint)) throw new Error('Deploy the web app first.');
  const properties=PropertiesService.getScriptProperties();
  const key=properties.getProperty('UPLOAD_KEY');
  if(!key) throw new Error('Run setup() first.');
  const files=DriveApp.getFolderById(PUBLIC_FOLDER_ID).getFilesByName('index.html');
  if(!files.hasNext()) throw new Error('index.html is missing from LOTTOL SV.');
  const site=files.next();
  if(files.hasNext()) throw new Error('More than one index.html exists. Keep one canonical file.');
  const html=site.getBlob().getDataAsString('UTF-8');
  const marker=/\/\*LOTTOL_CLOUD_CONFIG\*\/[\s\S]*?\/\*LOTTOL_CLOUD_CONFIG_END\*\//g;
  if((html.match(marker)||[]).length!==1) throw new Error('Public config marker is missing or duplicated.');
  site.setContent(html.replace(marker,'/*LOTTOL_CLOUD_CONFIG*/globalThis.LOTTOL_CONFIG='+JSON.stringify({mode:'cloud',apiUrl:endpoint})+';/*LOTTOL_CLOUD_CONFIG_END*/'));
  const folder=dataFolder_(), connection=JSON.stringify({endpoint:endpoint,uploadKey:key},null,2);
  const previous=folder.getFilesByName('LOTTOL-PC-connection.json');
  const file=previous.hasNext()?previous.next():folder.createFile('LOTTOL-PC-connection.json','',MimeType.PLAIN_TEXT);
  file.setContent(connection);
  console.log('API: '+endpoint);
  console.log('PRIVATE PC connection file (download; do not publish): '+file.getUrl());
}

function output_(value,callback) {
  const json=JSON.stringify(value).replace(/</g,'\\u003c').replace(/\u2028/g,'\\u2028').replace(/\u2029/g,'\\u2029');
  if(callback && CALLBACK.test(callback))
    return ContentService.createTextOutput(callback+'('+json+');').setMimeType(ContentService.MimeType.JAVASCRIPT);
  return ContentService.createTextOutput(json).setMimeType(ContentService.MimeType.JSON);
}

function publicRecords_() {
  const cache=CacheService.getScriptCache(),cached=cache.get(CACHE_KEY);
  if(cached) return JSON.parse(cached);
  const rows=[],files=dataFolder_().getFiles();
  while(files.hasNext()) {
    const file=files.next();
    if(!/^[A-Za-z0-9_-]{1,128}\.json$/.test(file.getName()) || file.getName()==='LOTTOL-PC-connection.json') continue;
    const row=JSON.parse(file.getBlob().getDataAsString('UTF-8'));
    // Only whitelisted ranking fields are public. Never return folder IDs,
    // connection settings, upload keys or other file content through JSONP.
    if(!row.eventId || !GAMES.some(g=>g.id===row.gameId) || row.source!=='program') continue;
    rows.push({eventId:row.eventId,playerName:row.playerName,gameId:row.gameId,score:row.score,createdAt:row.createdAt,source:'program'});
  }
  const json=JSON.stringify(rows);
  if(Utilities.newBlob(json).getBytes().length<90000) cache.put(CACHE_KEY,json,20);
  return rows;
}

function leaderboard_(game,period) {
  if(game!=='all' && !GAMES.some(g=>g.id===game)) throw new Error('Unknown game.');
  if(period!=='all' && period!=='week') throw new Error('Unknown period.');
  const earliest=period==='week'?Date.now()-7*86400000:0;
  const best=Object.create(null);
  publicRecords_().filter(row=>(game==='all'||row.gameId===game) && Date.parse(row.createdAt)>=earliest)
    .sort((a,b)=>b.score-a.score||a.createdAt.localeCompare(b.createdAt)||a.eventId.localeCompare(b.eventId))
    .forEach(row=>{
      const key=JSON.stringify([row.gameId,row.playerName.normalize('NFC').toLowerCase()]);
      if(!best[key]) best[key]=row;
    });
  return Object.keys(best).map(key=>best[key]).sort((a,b)=>b.score-a.score||a.createdAt.localeCompare(b.createdAt)||a.eventId.localeCompare(b.eventId));
}

function doGet(event) {
  const p=event&&event.parameter||{},callback=p.callback;
  if(callback && !CALLBACK.test(callback)) return output_({ok:false,error:'Invalid callback.'});
  try {
    if(p.action==='health') return output_({ok:true,app:'lottol-ranking',status:'ok',transport:'apps-script',version:1},callback);
    if(p.action==='games') return output_({ok:true,games:GAMES},callback);
    if(p.action==='scores') return output_({ok:true,scores:leaderboard_(p.gameId||'all',p.period||'all')},callback);
    throw new Error('Unknown action.');
  } catch(error) {return output_({ok:false,error:String(error.message)},callback);}
}

function record_(input) {
  if(!input || typeof input!=='object' || Array.isArray(input)) throw new Error('Invalid result.');
  if(typeof input.eventId!=='string' || !/^[A-Za-z0-9_-]{1,128}$/.test(input.eventId)) throw new Error('Invalid event ID.');
  if(!GAMES.some(g=>g.id===input.gameId)) throw new Error('Unknown game.');
  if(!Number.isSafeInteger(input.score)||input.score<0||input.score>1000000000) throw new Error('Invalid score.');
  if(typeof input.playerName!=='string') throw new Error('Player name required.');
  const name=input.playerName.normalize('NFC').trim();
  if(!name || Array.from(name).length>32 || /[\x00-\x1f\x7f-\x9f]/.test(name)) throw new Error('Invalid player name.');
  if(!Number.isSafeInteger(input.recordedAtMs)||input.recordedAtMs<946684800000||input.recordedAtMs>Date.now()+300000) throw new Error('Invalid play time.');
  return {eventId:input.eventId,playerName:name,gameId:input.gameId,score:input.score,createdAt:new Date(input.recordedAtMs).toISOString(),source:'program'};
}

function doPost(event) {
  let lock;
  try {
    const body=event&&event.postData&&event.postData.contents;
    if(!body || body.length>16384) throw new Error('Invalid request size.');
    const payload=JSON.parse(body),expected=PropertiesService.getScriptProperties().getProperty('UPLOAD_KEY');
    if(!expected || typeof payload.uploadKey!=='string' || expected.length!==payload.uploadKey.length) throw new Error('Upload authorization failed.');
    let difference=0;for(let i=0;i<expected.length;i++) difference|=expected.charCodeAt(i)^payload.uploadKey.charCodeAt(i);
    if(difference) throw new Error('Upload authorization failed.');
    const row=record_(payload.record);
    if(payload.record.practice===true) return output_({ok:true,ignored:true,eventId:row.eventId});
    if(payload.record.practice!==false) throw new Error('Invalid practice flag.');
    lock=LockService.getScriptLock();
    if(!lock.tryLock(5000)) throw new Error('Cloud is busy. Retry later.');
    const folder=dataFolder_(),existing=folder.getFilesByName(row.eventId+'.json');
    if(existing.hasNext()) {
      const previous=JSON.parse(existing.next().getBlob().getDataAsString('UTF-8'));
      if(JSON.stringify(previous)!==JSON.stringify(row)) throw new Error('Event ID already belongs to another result.');
    } else folder.createFile(row.eventId+'.json',JSON.stringify(row),MimeType.PLAIN_TEXT);
    CacheService.getScriptCache().remove(CACHE_KEY);
    return output_({ok:true,eventId:row.eventId,record:row});
  } catch(error) {return output_({ok:false,error:String(error.message)});}
  finally {if(lock && lock.hasLock()) lock.releaseLock();}
}
