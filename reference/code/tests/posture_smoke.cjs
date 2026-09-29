// Focused v0.11 checks. Node + numeric_math.cjs, NOT Three.js/WebGL.
const fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require(root+'/tests/numeric_math.cjs');
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const[i,a]of(this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
const modules=vm.runInNewContext(fs.readFileSync(root+'/js/loader.js','utf8').match(/const local=(\[[\s\S]*?\]);/)[1]);
for(const f of modules)vm.runInThisContext(fs.readFileSync(root+'/'+f,'utf8'),{filename:f});
const R=RTS,side=new R.Side('SIDE_A',R.Config.SIDES.SIDE_A),V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z),dump=[];

const assert=(condition,message)=>{if(!condition)throw new Error(message);};
const report={version:R.Config.VERSION,environment:'Node + numeric_math.cjs; not Three.js/WebGL',checks:[]};
for(const seed of [1003,2003]) {
 const u=new R.InfantryUnit({id:'pose',side,seed,detail:'world',equipmentOptions:{loadout:'ENGINEER'}});
 const samples=[];
 for(const state of ['IDLE','SITTING','CROUCH','CROUCH_WALK','PRONE','PRONE_MOVE']) {
  for(const phase of R.GaitProfile.isMoving(state)?[0,.25,.5,.75]:[0]) {
   u.animator.setState(state,true);u.seek(phase);
   for(const b of u.rig.bones)assert(b.matrixWorld.elements.every(Number.isFinite),'Nonfinite '+b.name);
   assert(u.rig.maxLengthError()<1e-5,'Bone length changed');
   const feet=[];
   for(const [s,sign]of [['L',1],['R',-1]]) {
    const q=new THREE.Quaternion();u.rig.byName['foot.'+s].getWorldQuaternion(q);
    const up=V(0,1,0).applyQuaternion(q),fwd=V(0,0,1).applyQuaternion(q);
    const calf=u.rig.modelPoint('shin.'+s,V()).sub(u.rig.modelPoint('foot.'+s,V())).normalize();
    const align=up.dot(calf);
    if(state.startsWith('PRONE')){assert(align>.98,'Ankle does not follow calf');assert(fwd.x*sign>.4,'Foot not splayed outwards');}
    feet.push({calfAlignment:align,forward:fwd.toArray()});
   }
   assert(Math.min(...u.animator.metrics.minBoot)>-.006,'Boot penetrates flat ground');
   const hip=u.rig.modelPoint('hips',V()),torso=u.rig.modelPoint('neck',V()).sub(hip);
   samples.push({state,phase,torsoPitchDeg:Math.atan2(torso.z,torso.y)*180/Math.PI,feet});
  }
 }
 const sit=samples.find(x=>x.state==='SITTING'),crouch=samples.find(x=>x.state==='CROUCH');
 assert(crouch.torsoPitchDeg>sit.torsoPitchDeg+15,'CROUCH still resembles old SITTING');
 report.checks.push({name:'Static poses seed '+seed,ok:true,samples});u.dispose();
}
for(const state of ['PRONE_MOVE','CROUCH_WALK']) {
 const u=new R.InfantryUnit({id:'motion',side,seed:2003,state,detail:'world',equipmentOptions:{loadout:'ENGINEER'}});
 u.setDemoPatrol(0,0,.6,0,state,{startT:0,minPause:.35,maxPause:.4});u.setWorldPosition(0,0,R.FlatSurface);
 const seen=new Set(),locks=[0,0];let maxError=0,minBoot=1;
 for(let step=0;step<260;step++){
  u.step(1/60,R.FlatSurface);u.render(1);seen.add(u.animator.state);
  for(const c of u.animator.footContacts)if(c.locked)locks[0]++;
  for(const c of u.animator.handContacts)if(c.locked)locks[1]++;
  maxError=Math.max(maxError,u.rig.maxLengthError());minBoot=Math.min(minBoot,...u.animator.metrics.minBoot);
 }
 assert(seen.has(R.GaitProfile.restState(state)),'Demo did not enter matching rest state');
 assert(!seen.has('IDLE'),'Low posture stood up at patrol endpoint');
 assert(locks[0]>0,'No foot locking');if(state==='PRONE_MOVE')assert(locks[1]>0,'No palm locking');
 assert(maxError<1e-5,'Movement stretched bones');
 report.checks.push({name:'Short patrol '+state,ok:true,states:[...seen],lockedFrames:locks,minBoot,maxBoneLengthError:maxError});u.dispose();
}
fs.mkdirSync(path.join(__dirname,'out'),{recursive:true});
fs.writeFileSync(path.join(__dirname,'out/v11_postures_numeric.json'),JSON.stringify(report,null,2));
console.log('PASS',report.checks.length,'focused groups; numeric layer only');
