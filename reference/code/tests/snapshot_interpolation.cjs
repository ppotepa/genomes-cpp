'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');

class Vector3{
  constructor(x=0,y=0,z=0){this.set(x,y,z);}
  set(x,y,z){this.x=x;this.y=y;this.z=z;return this;}
  copy(v){return this.set(v.x,v.y,v.z);}
  lerp(v,t){this.x+=(v.x-this.x)*t;this.y+=(v.y-this.y)*t;this.z+=(v.z-this.z)*t;return this;}
  clone(){return new Vector3(this.x,this.y,this.z);}
}
class Quaternion{
  constructor(x=0,y=0,z=0,w=1){Object.assign(this,{x,y,z,w});}
  copy(q){Object.assign(this,q);return this;}
  slerp(q,t){this.x+=(q.x-this.x)*t;this.y+=(q.y-this.y)*t;this.z+=(q.z-this.z)*t;this.w+=(q.w-this.w)*t;return this;}
  clone(){return new Quaternion(this.x,this.y,this.z,this.w);}
}
class Euler{set(){return this;}}
let morphBlends=0;
const R={Unit:class{},Math:{mix(a,b,t){morphBlends++;return a+(b-a)*t;}}};
vm.runInNewContext(fs.readFileSync(require.resolve('../js/units/infantryUnit.js'),'utf8'),
  {window:{RTS:R},THREE:{Vector3,Quaternion,Euler}});

const unit=Object.create(R.InfantryUnit.prototype);
unit.position=new Vector3();
const root={position:new Vector3(),quaternion:new Quaternion()};
const morphTargetInfluences=[0.25,0.5];
const bone={name:'cheek.L',position:new Vector3(),quaternion:new Quaternion()},armBone={name:'upperArm.R',position:new Vector3(),quaternion:new Quaternion()};
unit.root=root;
unit.rig={bones:[bone,armBone]};
unit.model={mesh:{morphTargetInfluences}};
unit.poseChannels={positionIndices:[0],quaternionIndices:[0,1]};
unit.previous={position:new Vector3(),rotation:new Quaternion(),positions:[bone.position.clone()],quaternions:[bone.quaternion.clone(),armBone.quaternion.clone()],morphs:[0.25,0.5],morphIndices:[0,1],positionIndices:[0],quaternionIndices:[0,1]};
unit.current={position:new Vector3(),rotation:new Quaternion(),positions:[bone.position.clone()],quaternions:[bone.quaternion.clone(),armBone.quaternion.clone()],morphs:[0.25,0.5],morphIndices:[0,1],positionIndices:[0],quaternionIndices:[0,1]};
unit._rootInterpolationDirty=false;
unit._morphInterpolationDirty=false;
unit._dirtyMorphIndices=[];
unit._bodyMorphSlots=new Int32Array([0,1]);
unit._positionInterpolationDirty=new Uint8Array(1);
unit._quaternionInterpolationDirty=new Uint8Array(2);
unit._weaponQuaternionSide=new Uint8Array([0,2]);
unit._dirtyPositionSlots=[];unit._dirtyQuaternionSlots=[];
unit._dirtyFacePositionSlots=[];unit._dirtyFaceQuaternionSlots=[];
unit._weaponQuaternionSlots=[1];
let snapshotPositionWrites=0,snapshotRotationWrites=0,snapshotBonePositionWrites=0,snapshotBoneQuaternionWrites=0,snapshotMorphWrites=0;
const copySnapshotPosition=unit.current.position.copy.bind(unit.current.position),copySnapshotRotation=unit.current.rotation.copy.bind(unit.current.rotation),
  copySnapshotBonePosition=unit.current.positions[0].copy.bind(unit.current.positions[0]),copySnapshotBoneQuaternion=unit.current.quaternions[0].copy.bind(unit.current.quaternions[0]);
unit.current.position.copy=function(v){snapshotPositionWrites++;return copySnapshotPosition(v);};
unit.current.rotation.copy=function(v){snapshotRotationWrites++;return copySnapshotRotation(v);};
unit.current.positions[0].copy=function(v){snapshotBonePositionWrites++;return copySnapshotBonePosition(v);};
unit.current.quaternions[0].copy=function(v){snapshotBoneQuaternionWrites++;return copySnapshotBoneQuaternion(v);};
unit.current.morphs=new Proxy(unit.current.morphs,{set(target,key,value){snapshotMorphWrites++;target[key]=value;return true;}});
let weaponRenders=0,weaponArmMask=0;const armBasesAtWeaponRender=[];
unit.weapons={renderArmMask(solveGrip){return solveGrip?weaponArmMask:0;},render(){weaponRenders++;armBasesAtWeaponRender.push(armBone.quaternion.x);if(weaponArmMask)armBone.quaternion.x=.5;},capture(){}};

unit.capture();
assert.deepEqual([snapshotPositionWrites,snapshotRotationWrites,snapshotBonePositionWrites,snapshotBoneQuaternionWrites,snapshotMorphWrites],[0,0,0,0,0],
  'stable capture does not rewrite the current root, pose or morph snapshots');
assert.equal(unit._rootInterpolationDirty,false,'stationary root snapshots remain clean');
assert.equal(unit._morphInterpolationDirty,false,'unchanged morph snapshots remain clean');
assert.equal(unit._positionInterpolationDirty[0],0,'unchanged bone position snapshot remains clean');
assert.equal(unit._quaternionInterpolationDirty[0],0,'unchanged bone quaternion snapshot remains clean');
assert.equal(unit._weaponQuaternionSide[1],2,'right arm channels are associated with the right-hand grip');
let bonePositionWrites=0,boneQuaternionWrites=0,armQuaternionWrites=0;
let renderedPositionSlotReads=0,renderedQuaternionSlotReads=0;
unit.previous.positionIndices=new Proxy(unit.previous.positionIndices,{get(target,key){if(key!=='length'&&/^\d+$/.test(String(key)))renderedPositionSlotReads++;return target[key];}});
unit.previous.quaternionIndices=new Proxy(unit.previous.quaternionIndices,{get(target,key){if(key!=='length'&&/^\d+$/.test(String(key)))renderedQuaternionSlotReads++;return target[key];}});
const copyPosition=bone.position.copy.bind(bone.position),copyQuaternion=bone.quaternion.copy.bind(bone.quaternion);
const copyArmQuaternion=armBone.quaternion.copy.bind(armBone.quaternion);
bone.position.copy=function(v){bonePositionWrites++;return copyPosition(v);};
bone.quaternion.copy=function(v){boneQuaternionWrites++;return copyQuaternion(v);};
armBone.quaternion.copy=function(v){armQuaternionWrites++;return copyArmQuaternion(v);};
unit.render(0.5);
assert.equal(bonePositionWrites,0,'stable bone position skips interpolation write');
assert.equal(boneQuaternionWrites,0,'stable non-IK bone quaternion skips interpolation write');
assert.deepEqual([renderedPositionSlotReads,renderedQuaternionSlotReads],[0,0],'stable pose render does not scan unchanged pose channel indices');
assert.equal(armQuaternionWrites,0,'stowed arms skip baseline interpolation when no weapon IK task is attached');
assert.equal(morphBlends,0,'stable morph snapshots skip blend work');
assert.equal(weaponRenders,1,'weapon render IK still runs for a stable pose');

root.position.x=5;
morphTargetInfluences[0]=0.75;
bone.position.x=4;bone.quaternion.x=.8;armBone.quaternion.x=0;weaponArmMask=2;
unit.capture();
assert.deepEqual([snapshotPositionWrites,snapshotRotationWrites,snapshotBonePositionWrites,snapshotBoneQuaternionWrites,snapshotMorphWrites],[1,0,1,1,1],
  'changed capture only copies current snapshot channels whose values changed');
assert.equal(unit._rootInterpolationDirty,true,'root change enables root interpolation');
assert.equal(unit._morphInterpolationDirty,true,'morph change enables morph interpolation');
assert.equal(unit._positionInterpolationDirty[0],1,'changed bone position enables interpolation');
assert.equal(unit._quaternionInterpolationDirty[0],1,'changed bone quaternion enables interpolation');
unit.current.morphIndices.indexOf=()=>{throw new Error('render used a linear morph-slot search');};
unit.render(0.5);
assert.equal(root.position.x,2.5,'root is interpolated between snapshots');
assert.equal(morphTargetInfluences[0],0.5,'morph is interpolated between snapshots');
assert.equal(morphTargetInfluences[1],0.5,'unchanged morph stays untouched');
assert.equal(bone.position.x,2,'changed bone position is interpolated');
assert.equal(bone.quaternion.x,.4,'changed bone quaternion is interpolated');
assert.equal(bonePositionWrites,1,'only dirty bone position was written');
assert.equal(boneQuaternionWrites,1,'only dirty non-IK bone quaternion was written');
assert.equal(armQuaternionWrites,0,'dirty arm channels interpolate from changed snapshots without redundant IK baseline writes');
assert.deepEqual([renderedPositionSlotReads,renderedQuaternionSlotReads],[1,1],'render reads only the position and quaternion slots marked dirty by capture');
assert.equal(morphBlends,1,'only changed morphs enter the blend loop');
assert.deepEqual(unit._dirtyMorphIndices,[0],'dirty morph list contains only the changed influence');
assert.equal(weaponRenders,2,'weapon render IK still runs after conditional interpolation');
assert.deepEqual(armBasesAtWeaponRender,[0,0],'weapon IK starts from the captured arm pose when the right-hand task is attached');

// The game can evaluate root motion more often than either visual snapshot
// stream. In that gap, no body/face channel capture is needed.
unit.current.position.copy(root.position);
unit.previous.position.copy(root.position);
unit.current.rotation.copy(root.quaternion);
unit.previous.rotation.copy(root.quaternion);
unit._rootInterpolationDirty=false;
unit._animationUpdateAccumulator=0;unit.animationUpdateInterval=.1;
unit._faceUpdateAccumulator=0;unit.faceUpdateInterval=.1;
unit._animationDistanceAccumulator=0;
unit.demo=null;unit.speed=0;unit.heading=0;unit.facingHeading=0;unit.facingVelocity=0;
unit.position.set(0,0,0);unit.root.position.set(0,0,0);
unit.current.position.copy(unit.root.position);unit.previous.position.copy(unit.root.position);
unit.current.rotation.copy(unit.root.quaternion);unit.previous.rotation.copy(unit.root.quaternion);
let sleepingRootPositionWrites=0,sleepingRootRotationWrites=0;
const rootPositionCopy=unit.root.position.copy.bind(unit.root.position);
unit.root.position.copy=function(v){sleepingRootPositionWrites++;return rootPositionCopy(v);};
unit.root.rotation={x:0,y:0,z:0,set(){sleepingRootRotationWrites++;this.x=0;this.y=arguments[1];this.z=0;}};
unit._animContext={};unit._before=new Vector3();unit.totalDistance=0;
unit._terrainHeightCache={terrain:null,heights:null,ix:-1,iz:-1,a:0,b:0,c:0,d:0};unit._terrainHeightSample={surface:null,heights:null,x:NaN,z:NaN,y:NaN};
R.PostureProfile={isBiped:()=>false};
R.GaitProfile={isMoving:()=>false,restState:()=> 'IDLE'};
R.Math.angle=value=>value;R.Math.clamp=(value,min,max)=>Math.max(min,Math.min(max,value));
unit.animator={state:'IDLE',transition:null,update(){throw new Error('body cadence should sleep');},finalizeAfterLook(){}};
unit.locomotion={update(){}};
let sleepingHeadRestores=0;
unit.faceAnimator={restoreHeadLayer(){sleepingHeadRestores++;},update(){throw new Error('face cadence should sleep');}};
unit.weapons={trigger:false,pendingShot:false,beginStep(){},capture(){throw new Error('weapon capture should sleep');}};
unit.movementSpeed=()=>0;
let rootSnapshotPositionWrites=0,rootSnapshotRotationWrites=0;
const previousPositionCopy=unit.previous.position.copy.bind(unit.previous.position),previousRotationCopy=unit.previous.rotation.copy.bind(unit.previous.rotation);
unit.previous.position.copy=function(v){rootSnapshotPositionWrites++;return previousPositionCopy(v);};
unit.previous.rotation.copy=function(v){rootSnapshotRotationWrites++;return previousRotationCopy(v);};
let sleepingBodyCaptures=0,sleepingFaceCaptures=0;
unit.captureBodyPose=()=>{sleepingBodyCaptures++;};unit.captureFacePose=()=>{sleepingFaceCaptures++;};
unit.step(.01,{getHeightAt(){return 0;}},true);
assert.deepEqual([rootSnapshotPositionWrites,rootSnapshotRotationWrites],[0,0],'stable root snapshots skip redundant position/quaternion copies');
assert.equal(sleepingBodyCaptures,0,'sub-cadence simulation ticks skip body snapshot capture');
assert.equal(sleepingFaceCaptures,0,'sub-cadence simulation ticks skip face snapshot capture');
assert.equal(sleepingHeadRestores,0,'sub-cadence ticks skip restoring a rig pose no simulation phase reads');
assert.deepEqual([sleepingRootPositionWrites,sleepingRootRotationWrites],[0,0],'stationary fixed ticks skip redundant root transform writes');
unit.position.x=.25;unit.step(.01,{getHeightAt(){return 0;}},true);unit.step(.01,{getHeightAt(){return 0;}},true);const changedRootSnapshotWrites=rootSnapshotPositionWrites;unit.step(.01,{getHeightAt(){return 0;}},true);
assert(changedRootSnapshotWrites>0,'changed root position is copied into the previous snapshot');
assert.equal(rootSnapshotPositionWrites,changedRootSnapshotWrites,'stable root position does not repeat snapshot copies');
assert.equal(rootSnapshotRotationWrites,0,'unchanged root orientation remains uncopied');
console.log('PASS infantry snapshot interpolation skips stable root/morph/bone writes and preserves changing pose plus weapon render');
