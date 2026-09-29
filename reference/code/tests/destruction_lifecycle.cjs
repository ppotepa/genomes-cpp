// DOM/lifecycle tests only; renderer is a spy, no browser/WebGL rendering.
const {JSDOM}=require('../tools/destruction/node_modules/jsdom'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const dom=new JSDOM('<div id="editor"><canvas></canvas><span id="next"></span></div>',{url:'file:///D:/Git/genomes/index.html'});
global.window=dom.window;global.document=dom.window.document;global.devicePixelRatio=1;global.THREE=require('../tools/destruction/node_modules/three');global.RTS={};
for(const f of ['catalogs','solid','impactProfiles','projectileMath','materialImpact','rubbleField','projectileState','impactSolver','surfaceDamage','materialModel','projectileTrace','ballistics','physics','adapters','effects','demo'])vm.runInThisContext(fs.readFileSync('js/destruction/'+f+'.js','utf8'),{filename:f});
require('../js/vendor/rapier-0.19.3.js');require('../js/vendor/convex-hull-r128.js');
const canvas=document.querySelector('canvas'),parent=canvas.parentNode,size=new THREE.Vector2(640,480);let ratio=1,renders=0;
const renderer={domElement:canvas,getSize:out=>out.copy(size),getPixelRatio:()=>ratio,setPixelRatio:r=>ratio=r,setSize:(x,y)=>size.set(x,y),render:()=>renders++};
const source=new THREE.Group(),geometry=new THREE.BoxGeometry(2,2,2),material=new THREE.MeshStandardMaterial();const mesh=new THREE.Mesh(geometry,material);mesh.position.y=1;source.add(mesh);
let disposedGeometry=0;geometry.addEventListener('dispose',()=>disposedGeometry++);
const controls={enabled:true},startButton=document.createElement('button'),adapter=()=>({renderer,controls,target:RTS.DestructionAdapters.capture(source,'rock',{seed:9})});document.body.appendChild(startButton);
(async()=>{
 await RTS.DestructionPhysics.init();
 for(let i=0;i<12;i++){
  startButton.focus();assert(await RTS.DestructionDemo.open(adapter()));assert(!controls.enabled);assert.equal(document.querySelectorAll('.destruction-demo').length,1);assert.match(document.querySelector('.dd-help').textContent,/bez limitu/);assert(!document.querySelector('.dd-help').textContent.includes('przeładuj'));assert.equal(document.querySelector('[data-id="close"]').textContent,'Powrót do edytora');
  assert(RTS.DestructionDemo.frame(renderer,16));document.querySelector('[data-id="distance"]').value='1000';document.querySelector('[data-id="distance"]').dispatchEvent(new window.Event('change'));
  await RTS.DestructionDemo.reset();window.dispatchEvent(new window.Event('blur'));assert(RTS.DestructionDemo.frame(renderer,32));RTS.DestructionDemo.close();
  assert.equal(canvas.parentNode,parent);assert.equal(canvas.nextSibling.id,'next');assert.equal(document.querySelectorAll('.destruction-demo').length,0);assert(controls.enabled);assert.equal(document.activeElement,startButton);assert.equal(ratio,1);assert.equal(size.x,640);assert.equal(size.y,480);assert.equal(disposedGeometry,0);
 }
 console.log('PASS 12 open/reset/blur/close cycles restore renderer, controls and shared geometry');
 const original=RTS.DestructionPhysics.init;let release;RTS.DestructionPhysics.init=()=>new Promise(r=>release=r);const pending=RTS.DestructionDemo.open(adapter());RTS.DestructionDemo.close();release();assert.equal(await pending,false);assert.equal(canvas.parentNode,parent);assert(!RTS.DestructionDemo.active);console.log('PASS preparation can be cancelled');
 RTS.DestructionPhysics.init=async()=>{throw Error('load failure');};await assert.rejects(RTS.DestructionDemo.open(adapter()),/load failure/);assert.equal(canvas.parentNode,parent);assert(controls.enabled);assert(!RTS.DestructionDemo.active);RTS.DestructionPhysics.init=original;console.log('PASS load failure leaves editor operational');
 assert(renders===24);
 let locked=null;Object.defineProperty(document,'pointerLockElement',{get:()=>locked,configurable:true});document.exitPointerLock=()=>{locked=null;document.dispatchEvent(new window.Event('pointerlockchange'));};
 await RTS.DestructionDemo.open(adapter());locked=canvas;document.dispatchEvent(new window.Event('pointerlockchange'));RTS.DestructionDemo.frame(renderer,0);canvas.dispatchEvent(new window.MouseEvent('mousedown',{button:0}));for(let i=1;i<=20;i++)RTS.DestructionDemo.frame(renderer,i*1000/60);
 const fired=RTS.DestructionDemo.diagnostics.shots;assert(fired>=4&&fired<=6,'automatic fire cadence');window.dispatchEvent(new window.Event('blur'));assert(RTS.DestructionDemo.diagnostics.paused);RTS.DestructionDemo.frame(renderer,1000);assert.equal(RTS.DestructionDemo.diagnostics.shots,fired);
 locked=canvas;document.dispatchEvent(new window.Event('pointerlockchange'));for(let i=0;i<10;i++)RTS.DestructionDemo.frame(renderer,1100+i*1000/60);assert.equal(RTS.DestructionDemo.diagnostics.shots,fired,'blur must clear trigger');
 window.dispatchEvent(new window.KeyboardEvent('keydown',{code:'Escape'}));assert(RTS.DestructionDemo.diagnostics.paused);RTS.DestructionDemo.close();assert.equal(disposedGeometry,0);console.log('PASS firing cadence, pause, Escape and blur clear active input');
 let rebuildingTarget;const rebuildingAdapter=adapter(),create=rebuildingAdapter.target.createAsync;rebuildingAdapter.target.createAsync=async function(cancelled){return rebuildingTarget=await create.call(this,cancelled);};
 await RTS.DestructionDemo.open(rebuildingAdapter);locked=canvas;document.dispatchEvent(new window.Event('pointerlockchange'));RTS.DestructionDemo.frame(renderer,0);
 const update=rebuildingTarget.update;rebuildingTarget.update=()=>{};rebuildingTarget.jobs.push({next:()=>({done:false})});RTS.DestructionDemo.frame(renderer,1000);assert.equal(RTS.DestructionDemo.diagnostics.tick,0);assert(RTS.DestructionDemo.diagnostics.visualTime>0,'VFX clock advances while geometry blocks simulation');
 rebuildingTarget.jobs.length=0;rebuildingTarget.update=update;RTS.DestructionDemo.frame(renderer,1017);assert.equal(RTS.DestructionDemo.diagnostics.tick,1,'rebuilding must not accrue one second of catch-up ticks');RTS.DestructionDemo.close();console.log('PASS geometry rebuild pauses do not create a catch-up burst');
 const resetAdapter=adapter();await RTS.DestructionDemo.open(resetAdapter);const resetCreate=resetAdapter.target.createAsync;let resetRelease;
 resetAdapter.target.createAsync=async function(cancelled){await new Promise(resolve=>resetRelease=resolve);return resetCreate.call(this,cancelled);};
 const resetPending=RTS.DestructionDemo.reset();assert(RTS.DestructionDemo.diagnostics.preparing);assert(document.querySelector('[data-id="resume"]').disabled);
 let lockRequests=0;canvas.requestPointerLock=()=>{lockRequests++;};document.querySelector('[data-id="resume"]').onclick();assert.equal(lockRequests,0);
 locked=canvas;document.dispatchEvent(new window.Event('pointerlockchange'));assert(RTS.DestructionDemo.diagnostics.paused);assert.equal(locked,null);canvas.dispatchEvent(new window.MouseEvent('mousedown',{button:0}));RTS.DestructionDemo.frame(renderer,2000);assert.equal(RTS.DestructionDemo.diagnostics.shots,0);
 resetRelease();await resetPending;assert(!RTS.DestructionDemo.diagnostics.preparing);assert(!document.querySelector('[data-id="resume"]').disabled);assert.equal(RTS.DestructionDemo.diagnostics.effects.drawCalls,0);RTS.DestructionDemo.close();console.log('PASS reset blocks resume and stale pointer lock while copying target');
 const worldFire=RTS.BallisticsWorld.prototype.fire,render=renderer.render,origins=[];
 RTS.BallisticsWorld.prototype.fire=function(shot){origins.push({position:shot.position,direction:shot.direction});return worldFire.call(this,shot);};
 try{for(const offset of [0,1]){await RTS.DestructionDemo.open(adapter());renderer.render=(scene,camera)=>{const gun=camera.children.find(child=>child.isGroup);gun.position.set(.25+offset,-.24+offset,-.45+offset);gun.rotation.x=offset;};locked=canvas;document.dispatchEvent(new window.Event('pointerlockchange'));RTS.DestructionDemo.frame(renderer,0);canvas.dispatchEvent(new window.MouseEvent('mousedown',{button:0}));RTS.DestructionDemo.frame(renderer,17);RTS.DestructionDemo.close();}}
 finally{RTS.BallisticsWorld.prototype.fire=worldFire;renderer.render=render;}
 assert.equal(origins.length,2);assert.deepEqual(origins[0],origins[1],'rendered gun transform must not alter physical muzzle or shot direction');console.log('PASS visual recoil cannot alter ballistic muzzle');
 console.log('No actual rendering performed.');dom.window.close();
})().catch(e=>{console.error(e);process.exitCode=1;});
