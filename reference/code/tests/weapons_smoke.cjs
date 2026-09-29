// Isolated generator smoke test, using numeric_math.cjs; NOT Three.js/WebGL.
const fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'..');
global.window=global;global.THREE=require(root+'/tests/numeric_math.cjs');
// Test-only support for the YXZ rotations used by the real Three.js API.
const xyz=THREE.Quaternion.prototype.setFromEuler;
THREE.Quaternion.prototype.setFromEuler=function(e){if(e.order!=='YXZ')return xyz.call(this,e);const c1=Math.cos(e.x/2),c2=Math.cos(e.y/2),c3=Math.cos(e.z/2),s1=Math.sin(e.x/2),s2=Math.sin(e.y/2),s3=Math.sin(e.z/2);return this.set(s1*c2*c3+c1*s2*s3,c1*s2*c3-s1*c2*s3,c1*c2*s3-s1*s2*c3,c1*c2*c3+s1*s2*s3);};
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const[i,a]of(this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
const loader=fs.readFileSync(root+'/js/loader.js','utf8');const modules=vm.runInNewContext(loader.match(/const local=(\[[\s\S]*?\]);/)[1]);
for(const file of modules)vm.runInThisContext(fs.readFileSync(root+'/'+file,'utf8'),{filename:file});
const R=RTS,side=new R.Side('SIDE_A',R.Config.SIDES.SIDE_A);
const assert=(cond,msg)=>{if(!cond)throw Error(msg);};

fs.mkdirSync(root+'/tests/out',{recursive:true});
const results=[];
for(const id of Object.keys(R.WeaponCatalog.entries)) {
 const slot=R.WeaponCatalog.get(id).kind==='long'?'primaryWeapon':{sidearm:'secondaryWeapon',knife:'meleeWeapon',grenade:'throwable'}[id];
 const u=new R.InfantryUnit({id:'weapon',side,seed:1001,detail:'world'});
 let settledExpCalls=0;const originalExp=Math.exp;Math.exp=function(value){settledExpCalls++;return originalExp(value);};
 try{u.weapons.beginStep(1/60,false);}finally{Math.exp=originalExp;}
 assert(settledExpCalls===0,'settled stowed weapon springs/aim performed exponential updates: '+id);
 u.equip(slot,id);const w=u.weapons;w.select(slot);w.setReadiness(1);w.draw();
 for(let i=0;i<210;i++){u.step(1/60,R.FlatSurface,true);u.render(.5);}
 const held={id,state:w.state,status:w.metrics.status,grip:w.metrics.gripError,ready:w.readiness,bones:u.rig.bones.length,phase:u.animator.phase};
 assert(w.state==='HELD','draw did not finish: '+id);
 assert(w.metrics.gripError<.01,'held grip error: '+id);
 if(id==='carbine'){
   w.syncSnapshots();w._renderGripCache.ready=false;const solveTasks=w.solveTasks;let gripSolves=0;
   w.solveTasks=function(tasks){gripSolves++;return solveTasks.call(this,tasks);};
   u.render(.37);assert(gripSolves===1,'first stable render must solve the interpolated weapon grip once');
   u.render(.37);assert(gripSolves===1,'identical weapon/arm inputs repeated the render IK solve');
   w.current.objects.primaryWeapon.p.x+=.02;u.render(.75);
   assert(gripSolves===2,'changed interpolated weapon pose did not wake render IK');
   assert(w.metrics.gripError<.01,'cached render IK changed the held grip');
   w.solveTasks=solveTasks;
 }
 let cachedHeightQueries=0,scalarHeightQueries=0;const cachedSurface=Object.assign(Object.create(R.FlatSurface),{getHeightAt(){scalarHeightQueries++;return 0;},getHeightAtCached(x,z,cache){cachedHeightQueries++;cache.terrain=this;cache.heights=this;return 0;}});
 u.step(1/60,cachedSurface,true);
 assert(cachedHeightQueries>0&&scalarHeightQueries===0,'held-weapon clearance bypassed cached terrain interpolation: '+id);
 let fullRootTraversals=0;const updateRoot=u.root.updateMatrixWorld;
 u.root.updateMatrixWorld=function(force){if(force===true)fullRootTraversals++;return updateRoot.call(this,force);};
 const armWorldUpdates=[];let fingerSubtreeRefreshes=0;
 for(const side of ['L','R']){const arm=u.rig.byName['upperArm.'+side],update=arm.updateWorldMatrix;armWorldUpdates.push([arm,update]);arm.updateWorldMatrix=function(parents,children){if(children===true)fingerSubtreeRefreshes++;return update.call(this,parents,children);};}
 u.render(.37);
 for(const [arm,update] of armWorldUpdates)arm.updateWorldMatrix=update;
 assert(fullRootTraversals===0,'render interpolation should leave full scene traversal to Three.js: '+id);
 assert(fingerSubtreeRefreshes===0,'weapon grip solver redundantly refreshed finger descendants: '+id);
 u.root.updateMatrixWorld=updateRoot;u.root.updateMatrixWorld(true);
 assert(w.metrics.gripError<.01,'lazy matrix paths changed held grip: '+id);
 // A size variant must move every vertex and attachment together, rather than
 // only changing the barrel/stock lengths around unscaled hand contacts.
 const source=w.active.item,models=[1,1.07].map(size=>R.WeaponGeometry.create({...source,variant:{...source.variant,size}}));
 for(const part of ['mesh','slide'])if(models[0][part]){
   const a=models[0][part].geometry.attributes.position.array,b=models[1][part].geometry.attributes.position.array;
   assert(a.length===b.length,'size changed topology: '+id);
   assert(Array.from(a).every((v,i)=>Number.isFinite(b[i])&&Math.abs(b[i]-v*1.07)<1e-6),'nonuniform weapon size: '+id);
 }
 for(const key of ['primary','secondary','muzzle','butt'])if(models[0][key]){
   const a=models[0][key].position||models[0][key],b=models[1][key].position||models[1][key];
   assert(a.clone().multiplyScalar(1.07).distanceTo(b)<1e-8,'unscaled attachment: '+id+'/'+key);
 }
 for(const model of models)if(model.slide){
   const position=model.slide.position,original=Object.getOwnPropertyDescriptor(position,'z');let z=position.z,writes=0;
   Object.defineProperty(position,'z',{configurable:true,get(){return z;},set(value){writes++;z=value;}});
   model.updateVisual(-1);assert(writes===0,'unchanged stowed slide was rewritten: '+id);
   model.updateVisual(.07);
   assert(writes===1,'changed recoil offset was not written exactly once: '+id);
   model.updateVisual(.07);assert(writes===1,'stable recoil offset was redundantly rewritten: '+id);
   Object.defineProperty(position,'z',{...original,value:z});
   const rear=model.slide.geometry.boundingBox.min.z+model.slide.position.z;
   assert(Math.min(...model.points.map(p=>p.z))<=rear+1e-8,'clearance misses recoiling slide');
   model.updateVisual(-1);assert(model.slide.position.z===0,'stowed slide did not reset');
 }
 models.forEach(model=>model.dispose());
 w.fire();for(let i=0;i<3;i++)u.step(1/60,R.FlatSurface,true);
 if(id==='sidearm')assert(w.active.slide.position.z<-.001,'pistol slide did not cycle');
 for(let i=0;i<9;i++)u.step(1/60,R.FlatSurface,true);
 if(id==='sidearm'){
   assert(w.active.slide.position.z===0,'pistol slide did not return');
   const shots=w.shotId,anchor=w.active.primary.position.clone();
   w.seekAction('SHOT',.14);u.seek(u.animator.phase);u.render(1);
   assert(w.active.slide.position.z<-.01,'scrubbing lost visual slide');
   assert(w.active.primary.position.distanceTo(anchor)<1e-10,'slide moved hand attachment');
   w.seekAction('SHOT',.8);u.seek(u.animator.phase);u.render(1);
   assert(w.active.slide.position.z===0&&w.shotId===shots,'scrubbing emitted a shot or failed to reset slide');
 }
 assert(w.shotId===(w.active.def.firearm?1:0),'firearm capability mismatch');
 held.shots=w.shotId;held.right=u.rig.modelPoint('hand.R',new THREE.Vector3()).toArray();
 held.weapon=w.active? w.active.root.position.toArray():null;
 if(id==='sidearm'){w.setGrip('2H');for(let i=0;i<20;i++)u.step(1/60,R.FlatSurface,true);held.twoHandError=w.metrics.gripError;}
 w.holster();for(let i=0;i<120;i++)u.step(1/60,R.FlatSurface,true);held.end=w.state;
 assert(w.state==='STOWED','holster did not finish');
 if(id==='sidearm'){
   const flash=w.instances[slot].flash,material=flash.material[0],visibleDescriptor=Object.getOwnPropertyDescriptor(flash,'visible'),opacityDescriptor=Object.getOwnPropertyDescriptor(material,'opacity'),scaleSet=flash.scale.set;
   let visible=flash.visible,opacity=material.opacity,scaleWrites=0,visibleWrites=0,opacityWrites=0;
   Object.defineProperty(flash,'visible',{configurable:true,get(){return visible;},set(value){visibleWrites++;visible=value;}});
   Object.defineProperty(material,'opacity',{configurable:true,get(){return opacity;},set(value){opacityWrites++;opacity=value;}});
   flash.scale.set=function(...values){scaleWrites++;return scaleSet.apply(this,values);};
   w.updateFlash(10);w.updateFlash(10);
   assert(visibleWrites===0&&opacityWrites===0&&scaleWrites===0,'stowed flash rewrote unchanged material/transform state');
   Object.defineProperty(flash,'visible',{...visibleDescriptor,value:visible});Object.defineProperty(material,'opacity',{...opacityDescriptor,value:opacity});flash.scale.set=scaleSet;
 }
 assert(u.rig.maxLengthError()<1e-6,'stretched bone');assert(u.rig.bones.every(b=>b.matrixWorld.elements.every(Number.isFinite)),'nonfinite');
 results.push(held);u.dispose();console.log(held);
}
fs.writeFileSync(root+'/tests/out/weapons_probe.json',JSON.stringify(results,null,2));

// Stable stowed weapon snapshots skip all transform interpolation/writes;
// one changed slot still interpolates and applies normally.
{
 const unit=new R.InfantryUnit({id:'stowed-render-cache',side,seed:2010,detail:'world'});
 unit.equip('primaryWeapon','carbine');unit.equip('secondaryWeapon','sidearm');
 const controller=unit.weapons,assign=controller.assign.bind(controller);let writes=0;
 controller.assign=(...args)=>{writes++;return assign(...args);};
 controller.render(.5,false);assert(writes===0,'unchanged stowed snapshots performed transform writes');
 controller.current.objects.primaryWeapon.p.x+=.1;controller.render(.5,false);
 assert(writes===1,'changed weapon slot did not perform exactly one interpolated write');
 assert(Math.abs(controller.instances.primaryWeapon.root.position.x-controller.current.objects.primaryWeapon.p.x+.05)<1e-8,
   'changed weapon transform was not interpolated');
 unit.dispose();
}

// One integration case: controls preserve phase, actions and inventory are independent.
const u=new R.InfantryUnit({id:'integration',side,seed:2003,detail:'world'});
u.equip('primaryWeapon','carbine');u.equip('secondaryWeapon','sidearm');u.equip('meleeWeapon','knife');
const w=u.weapons,step=n=>{for(let i=0;i<n;i++){u.step(1/60,R.FlatSurface,true);u.render(.37);}};
w.select('primaryWeapon');w.setReadiness(1);w.draw();step(230);
assert(w.state==='HELD','integration draw');
u.setLocomotion({crouch:.65,speedMps:.4});step(150);
assert(w.metrics.gripError<.015,'moving crouch grip');
const phase=u.animator.phase,identity=JSON.stringify(u.genome);
u.equip('head','field_cap');step(1);
assert(JSON.stringify(u.genome)===identity,'equipment changed genome');
assert(w.activeSlot==='primaryWeapon'&&w.state==='HELD','gear lost active weapon');
const shotCount=w.shotId;
for(const t of [.05,.55,.95,.35]){w.seekAction('SHOT',t);u.seek(phase);u.render(.5);}
assert(w.shotId===shotCount,'seeking emitted shot');
w.seekAction('DRAW',1);u.seek(phase);step(80);
w.select('secondaryWeapon');step(260);
assert(w.state==='HELD'&&w.activeSlot==='secondaryWeapon','weapon switch');
u.setState('PRONE_MOVE');step(310);
assert(w.state==='STOWED'&&u.animator.state==='PRONE_MOVE','crawl did not wait for holster');
w.draw();step(290);
assert(u.animator.state==='PRONE'&&w.state==='HELD','draw did not stop crawl');
assert(w.metrics.gripError<.025,'prone grip');
w.select('meleeWeapon');step(260);
const oldState=w.exportState();let rejected=false;
try{w.seekAction('SHOT',.4);}catch(e){rejected=true;}
assert(rejected&&JSON.stringify(w.exportState())===JSON.stringify(oldState),'invalid preview action mutated state');
u.dispose();console.log('Weapon integration: OK (isolated numeric test, no WebGL).');
