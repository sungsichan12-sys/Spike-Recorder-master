"use strict";
const {test}=require("node:test"),assert=require("node:assert/strict"),Api=require("../arena-cloud-client.js");
const endpoint="https://script.google.com/macros/s/DEPLOYMENT_TEST/exec";
test("only a deployed HTTPS Google endpoint can execute JSONP",()=>{
  assert.equal(Api.endpoint(endpoint),endpoint);
  for(const value of ["https://evil.test/api","http://script.google.com/macros/s/x/exec",endpoint+"?key=secret",endpoint.replace("/exec","/dev"),"https://script.google.com.evil.test/macros/s/x/exec"])
    assert.throws(()=>Api.endpoint(value));
});
test("cloud transport uses action queries, preserves filters and blocks browser writes",async()=>{
  let observed;
  const api=Api.createClient({baseUrl:endpoint,readImpl:async value=>{observed=new URL(value);return {ok:true,scores:[]};}});
  assert.equal(api.readOnly,true);await api.scores("night-pulse","week");
  assert.equal(observed.searchParams.get("action"),"scores");assert.equal(observed.searchParams.get("gameId"),"night-pulse");
  assert.equal(observed.searchParams.get("period"),"week");await assert.rejects(api.submit({score:1}),/프로그램/);
});
test("JSONP cleans up script and callback on success and errors",async()=>{
  const root={},children=[];
  const document={createElement:()=>({remove(){children.splice(children.indexOf(this),1);}}),head:{appendChild(script){children.push(script);const callback=new URL(script.src).searchParams.get("callback");queueMicrotask(()=>root[callback]({ok:true,scores:[]}));}}};
  const result=await Api.jsonp(endpoint,{root,document,crypto:require("node:crypto").webcrypto,timeoutMs:100});
  assert.deepEqual(result.scores,[]);assert.equal(children.length,0);assert.equal(Object.keys(root).length,0);
  document.head.appendChild=script=>{children.push(script);queueMicrotask(()=>script.onerror());};
  await assert.rejects(Api.jsonp(endpoint,{root,document,crypto:require("node:crypto").webcrypto,timeoutMs:100}),/클라우드/);
  assert.equal(children.length,0);assert.equal(Object.keys(root).length,0);
});
