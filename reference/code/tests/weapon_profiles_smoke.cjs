// Targeted animation tests. Run from any directory: node tests/weapon_profiles_smoke.cjs
// Uses the repository's numerical shim, not a browser or a WebGL renderer.
const fs=require('fs'),vm=require('vm'),path=require('path'),assert=require('assert/strict');
const root=path.resolve(__dirname,'..');global.window=global;
global.THREE=require('./numeric_math.cjs');
const xyz=THREE.Quaternion.prototype.setFromEuler;
THREE.Quaternion.prototype.setFromEuler=function(e){
  if(e.order!=='YXZ')return xyz.call(this,e);
  const c1=Math.cos(e.x/2),c2=Math.cos(e.y/2),c3=Math.cos(e.z/2),s1=Math.sin(e.x/2),s2=Math.sin(e.y/2),s3=Math.sin(e.z/2);
  return this.set(s1*c2*c3+c1*s2*s3,c1*s2*c3-s1*c2*s3,c1*c2*s3-s1*s2*c3,c1*c2*c3+s1*s2*s3);
};
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const [i,a]of(this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
const modules=vm.runInNewContext(fs.readFileSync(root+'/js/loader.js','utf8').match(/const local=(\[[\s\S]*?\]);/)[1]);
for(const file of modules)vm.runInThisContext(fs.readFileSync(root+'/'+file,'utf8'),{filename:file});
const R=RTS,side=new R.Side('SIDE_A',R.Config.SIDES.SIDE_A),report=[];
const make=(id='sidearm',seed=1001,preview=false)=>{
  const unit=new R.InfantryUnit({id:'grip-test',side,seed,detail:'world'});
  const slot=id==='sidearm'?'secondaryWeapon':id==='knife'?'meleeWeapon':id==='grenade'?'throwable':'primaryWeapon';
  unit.equip(slot,id);unit.weapons.select(slot);unit.weapons.setReadiness(0);
  // Stress poses in the same explicit action-preview mode as the editor.
  // A remote holster may legitimately be DRAW_BLOCKED for short-arm genomes.
  if(preview){unit.weapons.seekAction('DRAW',1);unit.seek(.2);step(unit,1);}
  else {unit.weapons.draw();step(unit,150);}
  assert.equal(unit.weapons.state,'HELD',id+'/'+seed+' '+unit.weapons.metrics.status);return unit;
};
function step(u,n){for(let i=0;i<n;i++){u.step(1/60,R.FlatSurface,true);u.render(.5);}}
function valid(u){assert(u.rig.maxLengthError()<1e-6);for(const b of u.rig.bones)assert(b.matrixWorld.elements.every(Number.isFinite));}
function test(name,fn){try{const details=fn();report.push({name,ok:true,details});console.log('PASS',name,details||'');}catch(e){report.push({name,ok:false,error:e.message});console.error('FAIL',name,e.stack);}}
test('Distinct 1H poses and cylinder profile resolution',()=>{
  const positions=[];
  for(const id of ['sidearm','knife','grenade']) {
    const u=make(id);try{
      valid(u);assert.equal(u.weapons.metrics.owners[0],'FREE');assert.equal(u.weapons.tasks[0].curl,0);
      positions.push(u.weapons.active.root.position.clone());assert(u.weapons.metrics.gripError<.01);
    }finally{u.dispose();}
  }
  for(let i=0;i<positions.length;i++)for(let j=0;j<i;j++)assert(positions[i].distanceTo(positions[j])>.04,'Indistinguishable carry targets');
  const P=R.WeaponHandlingProfiles;
  assert.equal(P.resolve({kind:'grenade',gripShape:'CYLINDER'},'1H').id,'GRENADE_CYLINDER_1H');
  assert.notDeepEqual(P.get('GRENADE_CYLINDER_1H').low,P.get('GRENADE_OVAL_1H').low);
  return positions.map(p=>p.toArray());
});
test('1H/2H transitions, interruption and free-hand release',()=>{
  const u=make();try{
    const w=u.weapons,identity=JSON.stringify(u.genome);w.setReadiness(1);step(u,90);
    const one=w.active.root.position.clone();let maxJump=0,previous=one.clone();
    for(const [grip,frames]of [['2H',9],['1H',7],['2H',80],['1H',90]]) {
      const phase=u.animator.phase;w.setGrip(grip);assert.equal(u.animator.phase,phase);
      for(let i=0;i<frames;i++){
        u.step(1/60,R.FlatSurface,true);maxJump=Math.max(maxJump,previous.distanceTo(w.active.root.position));previous.copy(w.active.root.position);
        valid(u);assert(w.metrics.gripError<.012);u.render(.37);
      }
      if(frames>=80)assert(grip==='1H'?w.metrics.owners[0]==='FREE':w.tasks[0].attached);
    }
    assert(maxJump<.035,'Weapon teleports on grip change');assert.equal(JSON.stringify(u.genome),identity);
    w.setGrip('2H');w.snapGrip();u.seek(.2);
    assert(w.active.root.position.distanceTo(one)>.06,'1H/2H share the same pose');
    const state=w.exportState();w.restoreState(state);assert.equal(w.supportSpring.x,state.supportWeight);
    return {maxJumpM:maxJump};
  }finally{u.dispose();}
});
test('Idle, crouch, locomotion, sprint and holster for each 1H family',()=>{
  let maxError=0;
  for(const id of ['sidearm','knife','grenade'])for(const seed of [1003,2003]) {
    const u=make(id,seed,true);try{
      for(const [depth,speed,ready]of [[0,0,0],[.65,.4,.55],[0,u.phenotype.runSpeed,0],[.9,0,1]]){
        u.weapons.setReadiness(ready);u.setLocomotion({crouch:depth,speedMps:speed});step(u,90);valid(u);
        maxError=Math.max(maxError,u.weapons.metrics.gripError);
        assert.equal(u.weapons.metrics.owners[0],'FREE');assert(u.weapons.metrics.gripError<.02);
      }
      u.weapons.holster();step(u,100);assert.equal(u.weapons.state,'STOWED');
    }finally{u.dispose();}
  }
  return {maxGripErrorM:maxError};
});
test('Draw/holster scrubbing is repeatable and emits no shots',()=>{
  const u=make();try{
    const w=u.weapons,count=w.shotId;
    const sample=t=>{w.seekAction('DRAW',t);u.seek(.31);return w.active.root.position.clone();};
    const a=sample(.23);sample(.8);sample(.05);const b=sample(.23);assert(a.distanceTo(b)<1e-8);
    for(const t of [.1,.7,.3]){w.seekAction('HOLSTER',t);u.seek(.31);valid(u);}
    assert.equal(w.shotId,count);
  }finally{u.dispose();}
});
test('Pistol 1H/2H recoil differs; non-firearms have no shot action',()=>{
  const u=make();try{
    const w=u.weapons,peaks=[];
    for(const grip of ['1H','2H']){w.setGrip(grip);w.seekAction('SHOT',.08);u.seek(.2);peaks.push(w.kick.x);}
    assert(peaks[0]>peaks[1]*1.20);assert.equal(w.shotId,0);
    for(const id of ['knife','grenade']){
      const slot=id==='knife'?'meleeWeapon':'throwable';u.equip(slot,id);w.holster();step(u,90);w.select(slot);
      assert.throws(()=>w.setGrip('2H'));assert.throws(()=>w.seekAction('SHOT',.2));
    }
    return {previewKick1H:peaks[0],previewKick2H:peaks[1]};
  }finally{u.dispose();}
});
fs.mkdirSync(path.join(root,'tests/out'),{recursive:true});
fs.writeFileSync(path.join(root,'tests/out/weapon_profiles.json'),JSON.stringify({environment:'Node + numeric_math.cjs (NOT WebGL)',report},null,2));
if(report.some(t=>!t.ok))process.exitCode=1;
