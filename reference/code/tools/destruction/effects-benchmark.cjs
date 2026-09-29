// Synthetic VFX pool CPU budget. No renderer, GPU, ballistics, or Rapier.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),os=require('node:os'),assert=require('node:assert/strict');
global.RTS={};global.THREE=require('./node_modules/three');const root=path.resolve(__dirname,'../..');
vm.runInThisContext(fs.readFileSync(path.join(root,'js/destruction/effects.js'),'utf8'));
const report={date:new Date().toISOString(),node:process.version,cpu:os.cpus()[0].model,rendering:false,scope:'Spawn and update CPU for pooled visual effects only; excludes rendering, GPU, audio, ballistics, building adapters and physics. Timing does not establish FPS.',scenarios:[]};
for(const low of [false,true]){
 const scene=new THREE.Scene(),effects=new RTS.DestructionEffects(scene),model={parts:new Map([['surface',{id:'surface',detached:false}]])},samples=[],peak={flash:0,dust:0,chips:0,marks:0,drawCalls:0};let explosions=0,impacts=0;
 for(let tick=0;tick<1800;tick++){
  const start=performance.now();
  if(tick%6===0){effects.explosion({point:[0,2,0],normal:[0,0,1],explosiveMass:2.1,material:'brick',seed:tick},low);explosions++;}
  if(tick%3===0){effects.impact({point:[1,2,0],normal:[0,0,1],material:'steel',lost:100,diameter:.0127,result:'stopped',part:'surface',seed:tick+3000},low);impacts++;}
  effects.update(1/60,low,null,model);samples.push(performance.now()-start);
  const d=effects.diagnostics;for(const key of Object.keys(peak))peak[key]=Math.max(peak[key],d[key]);assert(d.drawCalls<=4);
 }
 samples.sort((a,b)=>a-b);const result={quality:low?'low':'normal',simulatedSeconds:30,explosions,impacts,meanStepMs:samples.reduce((a,b)=>a+b,0)/samples.length,p95StepMs:samples[Math.ceil(samples.length*.95)-1],maxStepMs:samples.at(-1),peak};
 // All fixed lifetimes expire, without retaining historical upload work.
 for(let i=0;i<240;i++)effects.update(.1,low,null,model);assert.equal(effects.diagnostics.drawCalls,0);effects.dispose();assert.equal(scene.children.length,0);report.scenarios.push(result);
}
fs.writeFileSync(path.join(root,'docs/destruction/effects-benchmark.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
