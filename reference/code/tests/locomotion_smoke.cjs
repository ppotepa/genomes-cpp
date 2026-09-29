// Targeted v0.12 checks. This is a mathematical test, not Three.js/WebGL.
const fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'..');global.window=global;global.THREE=require('./numeric_math.cjs');
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const[i,a]of(this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
const modules=vm.runInNewContext(fs.readFileSync(root+'/js/loader.js','utf8').match(/const local=(\[[\s\S]*?\]);/)[1]);
for(const f of modules)vm.runInThisContext(fs.readFileSync(root+'/'+f,'utf8'),{filename:f});
const R=RTS,side=new R.Side('SIDE_A',R.Config.SIDES.SIDE_A),assert=(x,m)=>{if(!x)throw Error(m);};
const report={version:R.Config.VERSION,environment:'Node + numeric_math.cjs; NOT Three.js/WebGL',checks:[]};
{
 const unit=new R.InfantryUnit({id:'settled-locomotion',side,seed:7780,state:'IDLE',detail:'world'}),locomotion=unit.locomotion;
 locomotion.set({crouch:0,speedMps:0});
 for(let i=0;i<2400&&(locomotion.depth.value!==0||locomotion.depth.velocity!==0);i++)locomotion.update(1/60,0);
 assert(locomotion.depth.value===0&&locomotion.depth.velocity===0,'fixture did not reach exact settled locomotion state');
 let describes=0;const describe=locomotion.describeLimit.bind(locomotion);locomotion.describeLimit=()=>{describes++;return describe();};
 locomotion.update(1/60,0);describes=0;
 for(let i=0;i<120;i++)locomotion.update(1/60,0);
 assert(describes===0,'stable settled locomotion should skip redundant spring/limit work');
 locomotion.constraintDepth=.8;locomotion.update(1/60,0);
 assert(describes===1,'constraint change must wake locomotion update');
 locomotion.set({speedMps:.2});locomotion.update(1/60,0);
 assert(describes===2,'requested speed change must wake locomotion update');
 unit.dispose();report.checks.push({name:'settled locomotion skips unchanged spring/limit work and wakes on input changes',skippedStableUpdates:120,wakeups:2});
}
{
 const unit=new R.InfantryUnit({id:'settled-gait-profile',side,seed:7783,state:'IDLE',detail:'world'}),locomotion=unit.locomotion;
 locomotion.sampleGait(0,true);const cached=locomotion.gait,originalClamp=R.Math.clamp;let clamps=0;
 R.Math.clamp=(...args)=>{clamps++;return originalClamp(...args);};
 try{for(let i=0;i<120;i++)assert(locomotion.sampleGait(1/60)===cached,'stable gait profile replaced its cached output');}
 finally{R.Math.clamp=originalClamp;}
 assert(clamps===0,'settled gait profile should skip profile curve evaluation');
 let wakeClamps=0;R.Math.clamp=(...args)=>{wakeClamps++;return originalClamp(...args);};
 try{locomotion.actualSpeedMps=.2;locomotion.sampleGait(1/60);}finally{R.Math.clamp=originalClamp;}
 assert(wakeClamps>0,'speed change must wake gait profile evaluation');
 unit.dispose();report.checks.push({name:'settled gait profile reuses exact output and wakes on speed change',skippedSamples:120,wakeClampCalls:wakeClamps});
}
{
 const u=new R.InfantryUnit({id:'settled-facing',side,seed:7781,state:'IDLE',detail:'world'});
 let angleCalls=0;const angle=R.Math.angle;R.Math.angle=(...args)=>{angleCalls++;return angle(...args);};
 try{for(let i=0;i<120;i++)u.springFacing(u.facingHeading,1/60);}finally{R.Math.angle=angle;}
 assert(angleCalls===0,'settled facing solver skips angle wrapping at rest');
 u.facingVelocity=.4;R.Math.angle=(...args)=>{angleCalls++;return angle(...args);};
 try{u.springFacing(.2,1/60);}finally{R.Math.angle=angle;}
 assert(angleCalls>0,'nonzero facing velocity continues through the spring solver');
 report.checks.push({name:'settled facing avoids redundant angular wrapping',restSolverSteps:120,angleCallsAtRest:0});
}
{
 const u=new R.InfantryUnit({id:'cached-facing',side,seed:7782,state:'IDLE',detail:'world'}),referenceExp=Math.exp;
 let expectedHeading=u.facingHeading,expectedVelocity=.4,expCalls=0;u.facingVelocity=.4;
 function referenceStep(target,dt,omega){const delta=R.Math.angle(target-expectedHeading),resolved=expectedHeading+delta,y=expectedHeading-resolved,e=referenceExp(-omega*dt),temp=(expectedVelocity+omega*y)*dt;expectedHeading=R.Math.angle(resolved+(y+temp)*e);expectedVelocity=(expectedVelocity-omega*temp)*e;}
 const originalExp=Math.exp;Math.exp=function(value){expCalls++;return originalExp(value);};
 try{
  for(let i=0;i<100;i++){
   const state=i<40?'IDLE':i<80?'CROUCH':'RUN',dt=i<60?1/60:.01,omega=state==='CROUCH'?4.7:state==='RUN'?9:7.2,target=Math.sin(i*.037)*.8;
   u.animator.state=state;referenceStep(target,dt,omega);u.springFacing(target,dt);
   assert(u.facingHeading===expectedHeading&&u.facingVelocity===expectedVelocity,'cached facing coefficient changed the exact spring trajectory at step '+i);
  }
 }finally{Math.exp=originalExp;}
 assert(expCalls===4,'facing exponential should be recomputed only after dt or omega changes ('+expCalls+')');
 report.checks.push({name:'facing spring reuses exact coefficient for stable dt/state',steps:100,coefficientRecomputations:expCalls,trajectoryExact:true});u.dispose();
}
for(const seed of [1003,2003]){
 const u=new R.InfantryUnit({id:'probe',side,seed,detail:'world',equipmentOptions:{loadout:'RIFLEMAN'}});
 const genome=JSON.stringify(u.genome),equipment=JSON.stringify(u.equipment.toJSON()),samples=[];
 for(const c of [0,.25,.5,.75,1]){
  u.setLocomotion({crouch:c,speedMps:0});u.seek(.1);
  const head=u.rig.modelPoint('head',new THREE.Vector3());
  samples.push({requested:c,actual:u.locomotion.actualCrouch,headPivotRatio:head.y/u.phenotype.height,hipY:u.rig.byName.hips.position.y,groundLift:u.animator.bodyLift,minimumBoot:Math.min(...u.animator.metrics.minBoot)});
  assert(u.rig.maxLengthError()<1e-5,'bone scale changed');
  assert(Math.min(...u.animator.metrics.minBoot)>-.007,'boot penetration');
 }
 for(let i=1;i<samples.length;i++)assert(samples[i].headPivotRatio<samples[i-1].headPivotRatio,'nonmonotone lowering');
 assert(samples.at(-1).headPivotRatio<samples[2].headPivotRatio-.15,'maximum crouch not lower');
 assert(genome===JSON.stringify(u.genome)&&equipment===JSON.stringify(u.equipment.toJSON()),'sliders changed identity');
 u.setLocomotion({crouch:.37,speedMps:.6});u.seek(.47);assert(Math.abs(u.animator.phase-.47)<1e-9,'phase reset');
 u.setLocomotion({speedMps:0});assert(u.locomotion.requestedCrouch===.37,'partial API overwrote depth');
 for(const bad of [NaN,Infinity]){let failed=false;try{u.setLocomotion({crouch:bad});}catch{failed=true;}assert(failed,'NaN accepted');}
 report.checks.push({name:'static range, seed '+seed,ok:true,samples});u.dispose();
}
const u=new R.InfantryUnit({id:'moving',side,seed:1003,state:'RUN',detail:'world'});
{
 const unit=new R.InfantryUnit({id:'idle-terrain-sample',side,seed:1005,state:'IDLE',detail:'world'}),heights=new Float32Array(4);let cached=0,rootQueries=0,scalar=0,dynamicReads=0,insideRootSample=false;
 const surface=Object.assign(Object.create(R.FlatSurface),{heights,getHeightAt(){scalar++;if(insideRootSample)rootQueries++;return 0;},getHeightAtCached(x,z,cache){cached++;if(insideRootSample)rootQueries++;cache.terrain=this;cache.heights=this;return 0;}});
 const sampleHeight=unit.sampleTerrainHeight.bind(unit);unit.sampleTerrainHeight=(...args)=>{insideRootSample=true;try{return sampleHeight(...args);}finally{insideRootSample=false;}};
 try{
  for(let i=0;i<60;i++)unit.step(1/60,surface,false);
  assert(rootQueries===1,'stationary unit root should sample terrain only once (count='+rootQueries+')');
  unit.position.y=.5;unit.step(1/60,surface,false);assert(rootQueries===2,'external vertical correction invalidates the cached sample');
  unit.position.x=.25;unit.step(1/60,surface,false);assert(rootQueries===3,'horizontal movement invalidates the cached sample');
  surface.heights=new Float32Array(4);unit.step(1/60,surface,false);assert(rootQueries===4,'terrain backing-array replacement invalidates the cached sample');
  unit.setWorldPosition(.25,0,surface);assert(rootQueries===5,'explicit reposition refreshes cached terrain height');
  const dynamic=Object.assign(Object.create(R.FlatSurface),{getHeightAt(){if(insideRootSample)dynamicReads++;return 0;}});
  for(let i=0;i<3;i++)unit.step(1/60,dynamic,false);
  assert(dynamicReads===3,'surfaces without an immutable height backing keep sampling every tick (count='+dynamicReads+')');
  assert(scalar===0,'height cache fixture should not fall back to scalar interpolation');
 }finally{unit.dispose();}
 report.checks.push({name:'stationary units skip stable terrain height queries with exact invalidation',stationaryRootQueries:rootQueries,allSolverQueries:cached,dynamicQueries:dynamicReads});
}
{
 const unit=new R.InfantryUnit({id:'biped-height-cache',side,seed:1004,state:'RUN',detail:'world'});unit.setLocomotion({speedMps:1.5});
 let cached=0,scalar=0;const surface=Object.assign(Object.create(R.FlatSurface),{getHeightAt(){scalar++;return 0;},getHeightAtCached(x,z,cache){cached++;cache.terrain=this;cache.heights=this;return 0;}});
 try{for(let i=0;i<90;i++)unit.step(1/60,surface,false);assert(cached>0&&scalar===0,'biped foot contact bypassed cached terrain interpolation');}
 finally{unit.dispose();}
 report.checks.push({name:'biped feet reuse cached terrain height interpolation',queries:cached,scalarFallbacks:scalar});
}
u.setLocomotion({crouch:0,speedMps:u.phenotype.runSpeed});u.seek(.12);
let highSpeedDepth=0,maxDepthJump=0,prev=u.locomotion.actualCrouch,prevPhase=u.animator.phase;
u.setLocomotion({crouch:1});
for(let i=0;i<140;i++){
 u.step(1/60,R.FlatSurface,true);u.render(1);
 if(u.speed>2)highSpeedDepth=Math.max(highSpeedDepth,u.locomotion.actualCrouch);
 maxDepthJump=Math.max(maxDepthJump,Math.abs(prev-u.locomotion.actualCrouch));prev=u.locomotion.actualCrouch;
 for(const b of u.rig.bones)assert(b.matrixWorld.elements.every(Number.isFinite),'nonfinite dynamic bone');
}
assert(highSpeedDepth<.5,'descended while sprinting');assert(maxDepthJump<.045,'depth jumped');
assert(u.speed<.4&&u.locomotion.actualCrouch>.90,'posture/speed did not settle');
const phase=u.animator.phase;u.setState('WALK');assert(u.animator.phase===phase,'preset reset phase');assert(!u.animator.transition,'biped preset started frozen pose transition');
report.checks.push({name:'sprint->deep crouch, phase continuity',ok:true,highSpeedDepth,maxDepthJump,endSpeed:u.speed});u.dispose();
// A short WORLD-space patrol with continuous slider updates (not just treadmill).
const w=new R.InfantryUnit({id:'world',side,seed:2003,state:'WALK',detail:'world'});
w.setDemoPatrol(0,0,3,0,'WALK',{startT:0});w.setWorldPosition(0,0,R.FlatSurface);
let locks=0,restReplants=0;
for(let i=0;i<180;i++){if(i===50)w.setLocomotion({crouch:.83,speedMps:.35});w.step(1/60,R.FlatSurface);w.render(1);locks+=w.animator.footContacts.filter(c=>c.locked).length;assert(w.rig.maxLengthError()<1e-5,'world bone length changed');}
assert(locks>0,'no planted feet');
const fullRefresh=w.rig.root.updateMatrixWorld;let solveFootFullRefreshes=0;
w.rig.root.updateMatrixWorld=function(force){if(force)solveFootFullRefreshes++;return fullRefresh.call(this,force);};
w.animator.solveFoot(0,w._animContext);
w.rig.root.updateMatrixWorld=fullRefresh;
assert(solveFootFullRefreshes===0,'single-leg IK performed a redundant full-rig matrix traversal');
report.checks.push({name:'world contacts during depth change',ok:true,lockedFrames:locks,reanchors:w.animator.metrics.reanchors});w.dispose();
fs.mkdirSync(root+'/tests/out',{recursive:true});fs.writeFileSync(root+'/tests/out/v12_locomotion_numeric.json',JSON.stringify(report,null,2));
console.log('PASS',report.checks.length,'focused groups; numeric layer only');
