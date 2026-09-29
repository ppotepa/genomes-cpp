const fs=require('fs'),path=require('path'),http=require('http'),{spawn,execFileSync}=require('child_process');
const root=path.resolve(__dirname,'../../..'),destination=path.join(root,'docs/research/images');
const revision=execFileSync('git',['-c','safe.directory='+root.replaceAll('\\','/'),'rev-parse','--short','HEAD'],{cwd:root,encoding:'utf8'}).trim();
const dirty=!!execFileSync('git',['status','--porcelain'],{cwd:root,encoding:'utf8'}).trim();
const pause=ms=>new Promise(r=>setTimeout(r,ms));
fs.mkdirSync(destination,{recursive:true});
const server=http.createServer((req,res)=>{
 const file=path.resolve(root,'.'+decodeURIComponent(new URL(req.url,'http://localhost').pathname));
 if(!file.startsWith(root+path.sep)){res.writeHead(403).end();return;}
 fs.readFile(file,(err,data)=>{if(err){res.writeHead(404).end();return;}
  const ext=path.extname(file);res.setHeader('Content-Type',({'.js':'application/javascript','.html':'text/html','.css':'text/css','.png':'image/png','.json':'application/json'})[ext]||'application/octet-stream');res.end(data);});
});
let chrome,ws;
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 chrome=spawn('C:/Program Files/Google/Chrome/Application/chrome.exe',['--headless=new','--no-first-run','--no-default-browser-check','--disable-background-networking','--use-angle=swiftshader','--enable-unsafe-swiftshader','--remote-debugging-port=9338','--user-data-dir='+path.join(root,'tests/out/chrome-infantry-profile'),'about:blank'],{windowsHide:true,stdio:'ignore'});
 let page;for(let i=0;i<80;i++){try{page=(await(await fetch('http://127.0.0.1:9338/json/list')).json()).find(p=>p.type==='page');if(page)break;}catch{}await pause(250);}
 if(!page)throw Error('No Chrome debugging page');
 ws=new WebSocket(page.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.onopen=r;ws.onerror=j;});
 let id=0;const pending=new Map(),errors=[];
 ws.onmessage=e=>{const msg=JSON.parse(e.data);if(msg.id){const p=pending.get(msg.id);if(p){pending.delete(msg.id);msg.error?p.reject(Error(JSON.stringify(msg.error))):p.resolve(msg.result);}}if(msg.method==='Runtime.exceptionThrown')errors.push(msg.params.exceptionDetails);};
 const call=(method,params={})=>new Promise((resolve,reject)=>{const n=++id;pending.set(n,{resolve,reject});ws.send(JSON.stringify({id:n,method,params}));});
 await call('Runtime.enable');await call('Page.enable');await call('Emulation.setDeviceMetricsOverride',{width:1440,height:990,deviceScaleFactor:1,mobile:false});
 const sheets=process.argv[2]?process.argv[2].split(','):['faces','expressions','intensity','necks','bodies','experiment','lod'];
 for(const sheet of sheets){
  if(sheet==='battlefield'){
   await call('Page.navigate',{url:'http://127.0.0.1:'+server.address().port+'/index.html'});
   let ready=false;
   for(let i=0;i<90;i++){const r=await call('Runtime.evaluate',{expression:'!!window.RTS?.game?.debug',returnByValue:true});if(r.result.value){ready=true;break;}await pause(500);}
   if(!ready)throw Error('Application did not initialize');
   const checked=await call('Runtime.evaluate',{expression:fs.readFileSync(path.join(root,'tests/battlefield_checks.js'),'utf8'),returnByValue:true,awaitPromise:true});
   if(checked.exceptionDetails)throw Error(JSON.stringify(checked.exceptionDetails));
   const image=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(destination,'battlefield.png'),Buffer.from(image.data,'base64'));fs.writeFileSync(path.join(destination,'battlefield.json'),JSON.stringify(checked.result.value,null,2)+'\n');console.log('Battlefield:',JSON.stringify(checked.result.value));continue;
  }
  if(sheet==='environment'||sheet==='rocks'){
   await call('Page.navigate',{url:'http://127.0.0.1:'+server.address().port+'/environment.html'+(sheet==='rocks'?'?category=rocks':'')});
   let ready=false;
   for(let i=0;i<90;i++){const r=await call('Runtime.evaluate',{expression:'!!window.environmentLab?.model',returnByValue:true});if(r.result.value){ready=true;break;}await pause(500);}
   if(!ready)throw Error('Environment lab did not initialize');
   const checked=await call('Runtime.evaluate',{expression:fs.readFileSync(path.join(root,sheet==='rocks'?'tests/rocks_checks.js':'tests/environment_checks.js'),'utf8'),returnByValue:true,awaitPromise:true});
   if(checked.exceptionDetails)throw Error(JSON.stringify(checked.exceptionDetails));
   await pause(250);
   const image=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(destination,sheet+'.png'),Buffer.from(image.data,'base64'));
   fs.writeFileSync(path.join(destination,sheet+'.json'),JSON.stringify({...checked.result.value,baseRevision:revision,workingTree:dirty},null,2)+'\n');console.log(sheet+':',checked.result.value.results.length,'cases passed');
   if(sheet==='rocks')continue;
   const gallery=await call('Runtime.evaluate',{expression:fs.readFileSync(path.join(root,'tests/environment_gallery.js'),'utf8'),returnByValue:true});
   if(gallery.exceptionDetails)throw Error(JSON.stringify(gallery.exceptionDetails));
   const sheetImage=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(destination,'plants.png'),Buffer.from(sheetImage.data,'base64'));fs.writeFileSync(path.join(destination,'plants.json'),JSON.stringify(gallery.result.value,null,2)+'\n');continue;
  }
  if(sheet==='weaponui'||sheet==='cache'){
   await call('Page.navigate',{url:'http://127.0.0.1:'+server.address().port+'/index.html'});
   let ready=false;
   for(let i=0;i<90;i++){const r=await call('Runtime.evaluate',{expression:'!!window.RTS?.game?.debug',returnByValue:true});if(r.result.value){ready=true;break;}await pause(500);}
   if(!ready)throw Error('Application did not initialize');
   const checked=await call('Runtime.evaluate',{expression:fs.readFileSync(path.join(root,sheet==='cache'?'tests/cache_checks.js':'tests/weapon_ui_checks.js'),'utf8'),returnByValue:true});
   if(checked.exceptionDetails)throw Error(JSON.stringify(checked.exceptionDetails));
   await pause(250);
   const image=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(destination,sheet+'.png'),Buffer.from(image.data,'base64'));
   fs.writeFileSync(path.join(destination,sheet+'.json'),JSON.stringify({...checked.result.value,baseRevision:revision,workingTree:dirty},null,2)+'\n');console.log(sheet+':',JSON.stringify(checked.result.value));continue;
  }
  const suffix=process.argv[3]?'&seeds='+encodeURIComponent(process.argv[3]):'';
  const label=process.argv[4]||('commit '+revision+(dirty?' + local changes':''));
  await call('Page.navigate',{url:'http://127.0.0.1:'+server.address().port+'/docs/research/tools/infantry_visual_probe.html?sheet='+sheet+suffix+'&revision='+encodeURIComponent(revision)+'&label='+encodeURIComponent(label)});
  let result;
  for(let i=0;i<90;i++){const r=await call('Runtime.evaluate',{expression:'window.reviewResult || window.reviewError || null',returnByValue:true});result=r.result.value;if(result)break;await pause(500);}
  if(!result||typeof result==='string')throw Error('Render '+sheet+': '+result);
  const outputName=sheet+(process.argv[3]?'-selected':'')+(process.argv[4]?'-'+process.argv[4].replace(/[^a-z0-9-]/gi,'-'):'');
  const image=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(path.join(destination,outputName+'.png'),Buffer.from(image.data,'base64'));
  fs.writeFileSync(path.join(destination,outputName+'.json'),JSON.stringify(result,null,2)+'\n');console.log('Rendered',outputName,':',result.results.length,'samples');
 }
 await call('Browser.close');if(errors.length)throw Error(JSON.stringify(errors));
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(()=>{if(ws)ws.close();if(chrome)chrome.kill();server.close();});
