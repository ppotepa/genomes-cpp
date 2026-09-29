const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),assert=require('node:assert/strict');
const {chromium}=require('../tools/destruction/node_modules/playwright');
const root=path.resolve(__dirname,'..');
const server=http.createServer((req,res)=>{const file=path.resolve(root,'.'+decodeURIComponent(new URL(req.url,'http://localhost').pathname));if(!file.startsWith(root+path.sep)){res.writeHead(403).end();return;}fs.readFile(file,(e,d)=>{if(e){res.writeHead(404).end();return;}const type={'.html':'text/html','.js':'text/javascript','.css':'text/css','.wasm':'application/wasm'}[path.extname(file)];if(type)res.setHeader('Content-Type',type);res.end(d);});});
(async()=>{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  const browser=await chromium.launch({channel:'chrome',headless:true,args:['--use-angle=swiftshader','--disable-gpu','--disable-dev-shm-usage']});
  const page=await browser.newPage(); const errors=[]; page.on('pageerror',e=>errors.push(e.message));
  await page.goto('http://127.0.0.1:'+server.address().port+'/index.html',{waitUntil:'domcontentloaded',timeout:120000});
  await page.waitForFunction(()=>window.RTS,{timeout:120000});
  try{await page.waitForFunction(()=>window.RTS?.game,{timeout:120000});}catch(e){console.error('BOOT',await page.evaluate(()=>({keys:Object.keys(window.RTS||{}),status:document.querySelector('#bootStatus')?.textContent,error:document.querySelector('#error')?.textContent})),errors);throw e;}
  const result=await page.evaluate(async()=>{const game=window.RTS.game;await game.startNew(20260927,{size:400,vegetation:0,buildings:0,fencedParcels:0,skipEnvironment:true});const w=game.battlefield;w.units.forEach((u,i)=>{const a=i<25?0:1,n=i%25;u.setWorldPosition(a?-4:4,(n%5-2)*3,w.terrain);u.heading=a?0:Math.PI;u.marchTarget=a?4:-4;});for(let i=0;i<360;i++)w.step(1/60);return {units:w.units.length,held:w.units.filter(u=>u.aiHold).length,active:w.units.filter(u=>u.weapon?.activeWeapon).length,shots:w.ballisticsWorld.metrics.shots,hits:w.ballisticsWorld.metrics.hits,alive:w.units.filter(u=>u.alive!==false).length,ragdolls:w.worldDestruction.physics.ragdolls.size,diagnostics:w.ai.diagnostics.length};});
  assert.equal(result.units,50); assert(result.held>0); assert(result.shots>0); assert(result.hits>0); assert.equal(result.diagnostics,0); assert.deepEqual(errors,[]); console.log(JSON.stringify(result));
  await browser.close(); server.close();
})().catch(e=>{console.error(e.stack||e);process.exitCode=1;});
