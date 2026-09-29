'use strict';
const fs=require('fs'),vm=require('vm'),path=require('path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');global.window=global;global.THREE=require('./numeric_math.cjs');
THREE.Quaternion.prototype.dot=function(q){return this.x*q.x+this.y*q.y+this.z*q.z+this.w*q.w;};
THREE.SkinnedMesh.prototype.updateMorphTargets=function(){this.morphTargetInfluences=[];this.morphTargetDictionary={};for(const[i,a]of(this.geometry.morphAttributes.position||[]).entries()){this.morphTargetInfluences.push(0);this.morphTargetDictionary[a.name]=i;}};
global.document={createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})};
const modules=vm.runInNewContext(fs.readFileSync(root+'/js/loader.js','utf8').match(/const local=(\[[\s\S]*?\]);/)[1]);
for(const f of modules)vm.runInThisContext(fs.readFileSync(root+'/'+f,'utf8'),{filename:f});
const R=RTS,side=new R.Side('SIDE_A',R.Config.SIDES.SIDE_A);
const unit=new R.InfantryUnit({id:'face-cadence',side,seed:381,state:'IDLE',detail:'world'});
unit.setExpression('EYES_CLOSED');
unit.setVisualCadence(.05,.03);unit._animationUpdateAccumulator=0;unit._animationDistanceAccumulator=0;unit._faceUpdateAccumulator=0;
let bodyUpdates=0,faceUpdates=0,faceCaptures=0;const bodyUpdate=unit.animator.update.bind(unit.animator),faceUpdate=unit.faceAnimator.update.bind(unit.faceAnimator),captureFacePose=unit.captureFacePose.bind(unit);
unit.animator.update=(...args)=>{bodyUpdates++;return bodyUpdate(...args);};
unit.faceAnimator.update=(...args)=>{faceUpdates++;return faceUpdate(...args);};
unit.captureFacePose=(...args)=>{faceCaptures++;return captureFacePose(...args);};
unit.setVisualCadence(.05,.03);unit._animationUpdateAccumulator=0;unit._animationDistanceAccumulator=0;unit._faceUpdateAccumulator=0;
unit.setLookTarget(new THREE.Vector3(0,1,5));unit.setExpression('EYES_CLOSED');
let faceApplyCalls=0,headLayerCalls=0;const faceApply=unit.faceAnimator.apply.bind(unit.faceAnimator),headLayer=unit.faceAnimator.applyInterpolatedHeadLayer.bind(unit.faceAnimator);
unit.faceAnimator.apply=(...args)=>{faceApplyCalls++;return faceApply(...args);};
unit.faceAnimator.applyInterpolatedHeadLayer=(...args)=>{headLayerCalls++;return headLayer(...args);};
let bodyOnlyBoundaries=0;
for(let i=0;i<12;i++){
  const oldBodyPrevious=unit.previous,oldBodyCurrent=unit.current,oldFacePrevious=unit.previousFace,oldFaceCurrent=unit.currentFace,bodyBefore=bodyUpdates,faceBefore=faceUpdates;
  const bodyOnly=i===4||i===9,eyeBefore=bodyOnly?unit.rig.byName['eye.L'].quaternion.clone():null,browBefore=bodyOnly?unit.rig.byName['browInner.L'].position.clone():null,lipBefore=bodyOnly?unit.rig.byName.mouthUpper.position.clone():null;
  unit.step(.01,R.FlatSurface,true);
  if(bodyUpdates>bodyBefore)assert(unit.previous===oldBodyCurrent&&unit.current===oldBodyPrevious,'body update copied channel snapshots instead of rotating the reusable buffers');
  if(faceUpdates>faceBefore)assert(unit.previousFace===oldFaceCurrent&&unit.currentFace===oldFacePrevious,'face update copied channel snapshots instead of rotating the reusable buffers');
  if(bodyOnly){bodyOnlyBoundaries++;assert(Math.abs(eyeBefore.dot(unit.rig.byName['eye.L'].quaternion))>1-1e-10,'body-only cadence changed the cached eye expression');assert(browBefore.distanceTo(unit.rig.byName['browInner.L'].position)<1e-10,'body-only cadence changed the cached brow expression');assert(lipBefore.distanceTo(unit.rig.byName.mouthUpper.position)<1e-10,'body-only cadence changed the cached mouth expression');}
  if(i===2){assert.equal(bodyUpdates,0,'face cadence fires before body cadence');assert.equal(faceUpdates,1,'face advances at first independent boundary');}
  assert.equal(unit.faceAnimator._hasHeadLayer,false,'simulation snapshots store base neck/head without a composed look layer');
}
assert.equal(bodyUpdates,2,'body animator runs at its 50 ms cadence');
assert.equal(faceUpdates,4,'face animator independently runs at its 30 ms cadence');
assert.equal(faceCaptures,faceUpdates,'face-only and combined samples each capture the face stream exactly once');
assert.equal(bodyOnlyBoundaries,2,'different cadence intervals produced two body-only boundaries');
assert.equal(headLayerCalls,2,'both body-only boundaries compose the cached head layer');
assert(faceApplyCalls<=faceUpdates,'facial control application does not exceed face cadence');
assert(Number.isFinite(unit.animator.phase),'body gait clock remains finite');
assert.equal(unit.currentFace.positions.length,unit.poseChannels.facePositionIndices.length,'face position channels are snapshotted independently');
assert.equal(unit.currentFace.quaternions.length,unit.poseChannels.faceQuaternionIndices.length,'face quaternion channels are snapshotted independently');
assert.equal(unit.current.morphs.length,unit.poseChannels.bodyMorphIndices.length,'body morph channels exclude face-only lids');
assert.equal(unit.currentFace.morphs.length,unit.poseChannels.faceMorphIndices.length,'face morph channels have their own snapshot');
assert.equal(unit.currentFace.morphIndices.length+unit.current.morphIndices.length,Object.keys(unit.model.mesh.morphTargetDictionary).length,'every morph channel belongs to exactly one snapshot');
const closedLids=unit.model.mesh.morphTargetDictionary.eyelidsClose,faceMorphSlot=unit.currentFace.morphIndices.indexOf(closedLids);
assert(faceMorphSlot>=0&&unit.currentFace.morphs[faceMorphSlot]>0,'face-only update snapshots the closed-lid morph independently');
assert(!unit.current.morphIndices.includes(closedLids),'body snapshot does not alias the independent face morph channel');
unit.setExpression('NEUTRAL');unit.step(.01,R.FlatSurface,true);unit.step(.01,R.FlatSurface,true);unit.step(.01,R.FlatSurface,true);
assert(unit._faceMorphInterpolationDirty,'expression transition creates a face-only morph delta');
unit.step(.01,R.FlatSurface,true);const faceAlpha=Math.min(1,unit._faceUpdateAccumulator/unit.faceUpdateInterval),expectedFaceMorph=unit.previousFace.morphs[faceMorphSlot]*(1-faceAlpha)+unit.currentFace.morphs[faceMorphSlot]*faceAlpha;
unit.currentFace.morphIndices.indexOf=()=>{throw new Error('face morph render used a linear slot search');};
unit.render(.99);assert(Math.abs(unit.model.mesh.morphTargetInfluences[closedLids]-expectedFaceMorph)<1e-9,'face morph interpolates between expression snapshots using face cadence');

const headIndex=unit.rig.bones.findIndex(b=>b.name==='head'),neckIndex=unit.rig.bones.findIndex(b=>b.name==='neck');
const headSlot=unit.current.quaternionIndices.indexOf(headIndex),neckSlot=unit.current.quaternionIndices.indexOf(neckIndex);
assert(headSlot>=0&&neckSlot>=0,'base head and neck remain in body snapshots');
unit.render(.5);
assert(Number.isFinite(unit.rig.byName.head.quaternion.w)&&Number.isFinite(unit.rig.byName.neck.quaternion.w),'rendered head layer composes onto interpolated body state');
const plainFaceInfluences=unit.model.mesh.morphTargetInfluences;let neckFlexWrites=0;
unit.model.mesh.morphTargetInfluences=new Proxy(plainFaceInfluences,{set(target,key,value){neckFlexWrites++;target[key]=value;return true;}});
const attentionYaw=unit.faceAnimator.attentionYaw.value,attentionPitch=unit.faceAnimator.attentionPitch.value;
unit.faceAnimator.applyInterpolatedHeadLayer(attentionYaw,attentionPitch,true);neckFlexWrites=0;
unit.faceAnimator.applyInterpolatedHeadLayer(attentionYaw,attentionPitch,true);
assert.equal(neckFlexWrites,0,'stable interpolated head composition skips an unchanged neck-flex morph write');
unit.model.mesh.morphTargetInfluences=plainFaceInfluences;
{
 const samples=unit.animator.supportIndices.body,skin=unit.model.skinSupportVertex,closedLids=unit.model.mesh.morphTargetDictionary.eyelidsClose;
 let skinReads=0;unit.model.skinSupportVertex=function(...args){skinReads++;return skin.call(this,...args);};
 unit.animator.minimumClearance(samples,R.FlatSurface,false,true,false);const firstReads=skinReads;
 assert(firstReads>0,'clearance fixture did not scan support vertices');
 unit.animator.minimumClearance(samples,R.FlatSurface,false,true,false);
 assert.equal(skinReads,firstReads,'same pose and stable surface rescanned cached body clearance indices');
 const oldWeight=plainFaceInfluences[closedLids];plainFaceInfluences[closedLids]=oldWeight+.1;
 unit.animator.minimumClearance(samples,R.FlatSurface,false,true,false);
 assert(skinReads>firstReads,'morph change did not invalidate clearance result cache');
 plainFaceInfluences[closedLids]=oldWeight;unit.animator.minimumClearance(samples,R.FlatSurface,false,true,false);
 let dynamicQueries=0;const dynamicSurface={getHeightAt(){dynamicQueries++;return 0;}};
 unit.animator.minimumClearance(samples,dynamicSurface,false,true,false);const firstDynamic=dynamicQueries;
 unit.animator.minimumClearance(samples,dynamicSurface,false,true,false);
 assert(dynamicQueries>firstDynamic,'dynamic custom surface incorrectly reused a clearance minimum');
 unit.model.skinSupportVertex=skin;
}
const originalMinimumClearance=unit.animator.minimumClearance,headSupport=unit.animator.supportIndices.head;let headBaseClearance=-.02,headClearanceScans=0;
unit.animator.minimumClearance=function(indices,...args){if(indices===headSupport){headClearanceScans++;return headBaseClearance+this.metrics.headLift;}return originalMinimumClearance.call(this,indices,...args);};
unit.setVisualCadence(.05,.01);unit._animationUpdateAccumulator=0;unit._animationDistanceAccumulator=0;unit._faceUpdateAccumulator=0;
unit.step(.05,R.FlatSurface,true);const liftedHipY=unit.rig.byName.hips.position.y,stableHeadLift=unit.animator.metrics.headLift;
assert(stableHeadLift>.02,'clearance fixture applies a measurable head correction');
let faceOnlyIkRuns=0;const solveFeet=unit.animator.solveFeet.bind(unit.animator);unit.animator.solveFeet=(...args)=>{faceOnlyIkRuns++;return solveFeet(...args);};
for(let i=0;i<3;i++){unit.step(.01,R.FlatSurface,true);assert(Math.abs(unit.rig.byName.hips.position.y-liftedHipY)<1e-9,'face-only tick accumulated the previous head clearance lift');}
assert.equal(unit.animator.metrics.headLift,stableHeadLift,'stable head pose keeps the same absolute clearance correction');
assert.equal(faceOnlyIkRuns,0,'stable face-only clearance re-ran foot IK despite an unchanged correction');
unit.animator.finalizeAfterLook(unit._animContext,true);const headScansAfterRefresh=headClearanceScans;
unit.animator.finalizeAfterLook(unit._animContext,true);
assert.equal(headClearanceScans,headScansAfterRefresh,'repeated clearance finalization for an exact pose/surface revision rescanned head supports');
headBaseClearance=.02;unit.step(.01,R.FlatSurface,true);
assert.equal(unit.animator.metrics.headLift,0,'face-only clearance should remove a correction that is no longer needed');
assert(Math.abs(unit.rig.byName.hips.position.y-(liftedHipY-stableHeadLift))<1e-9,'face-only clearance did not apply a negative correction delta');
assert.equal(faceOnlyIkRuns,1,'changed head clearance should re-solve contacts once');
{
 const faceUpdate=unit.faceAnimator.update,finalize=unit.animator.finalizeAfterLook.bind(unit.animator),captureBody=unit.captureBodyPose.bind(unit);
 let faceOnlyFinalizations=0,bodyCaptures=0;
 unit.faceAnimator.update=()=>false;
 unit.animator.finalizeAfterLook=(context,correctionAlreadyApplied)=>{if(correctionAlreadyApplied)faceOnlyFinalizations++;return finalize(context,correctionAlreadyApplied);};
 unit.captureBodyPose=()=>{bodyCaptures++;return captureBody();};
 unit.setVisualCadence(.5,.01);unit._animationUpdateAccumulator=0;unit._animationDistanceAccumulator=0;unit._faceUpdateAccumulator=0;
 unit.step(.01,R.FlatSurface,true);
 unit.faceAnimator.update=faceUpdate;unit.animator.finalizeAfterLook=finalize;unit.captureBodyPose=captureBody;
 assert.equal(faceOnlyFinalizations,0,'unchanged face pose ran post-look clearance/contact finalization');
 assert.equal(bodyCaptures,0,'unchanged face pose recopied the body snapshot');
}
const rigRoot=unit.rig.root,updateMatrixWorld=rigRoot.updateMatrixWorld;let fullRigUpdates=0;
rigRoot.updateMatrixWorld=function(...args){fullRigUpdates++;return updateMatrixWorld.apply(this,args);};
unit.root.rotation.y=.37;unit.root.updateMatrix();
unit.animator.placeFeet(unit._animContext);
assert.equal(fullRigUpdates,0,'foot placement should update only queried ancestor paths, not traverse the whole rig');
assert(Math.abs(Math.abs(unit.animator._rootQ.dot(new THREE.Quaternion().setFromEuler(unit.root.rotation)))-1)<1e-9,'ancestor-path refresh returned a stale world orientation');
unit.animator.state='REST';unit.animator.update(0,unit._animContext);
assert.equal(fullRigUpdates,0,'REST animation should defer world matrices until a consumer or renderer needs them');
rigRoot.updateMatrixWorld=updateMatrixWorld;
const fixedFace=unit.rig.byName['mouthCorner.L'],expressionFace=unit.rig.byName['eye.L'],bodyBone=unit.rig.byName['upperArm.L'];
const fixedBefore=fixedFace.quaternion.clone(),expressionBefore=expressionFace.quaternion.clone(),bodyBefore=bodyBone.quaternion.clone();
const brow=unit.rig.byName['browInner.L'],lip=unit.rig.byName.mouthUpper,browPositionBefore=brow.position.clone(),lipPositionBefore=lip.position.clone();
brow.position.y+=.012;brow.position.z+=.004;lip.position.y-=.009;
const browExpressionY=brow.position.y,lipExpressionY=lip.position.y;
fixedFace.quaternion.setFromEuler(new THREE.Euler(.17,-.09,.11,'XYZ'));
expressionFace.quaternion.setFromEuler(new THREE.Euler(-.12,.08,.04,'XYZ'));
unit.animator.current.q[unit.rig.index['upperArm.L']].setFromEuler(new THREE.Euler(.04,.03,-.02,'XYZ'));
const bodyExpected=unit.animator.current.q[unit.rig.index['upperArm.L']].clone();
unit.animator.applyPose(unit.animator.current);
assert(Math.abs(fixedFace.quaternion.dot(new THREE.Quaternion().setFromEuler(new THREE.Euler(.17,-.09,.11,'XYZ'))))>1-1e-10,'body pose overwrote a fixed facial rotation channel');
assert(Math.abs(expressionFace.quaternion.dot(new THREE.Quaternion().setFromEuler(new THREE.Euler(-.12,.08,.04,'XYZ'))))>1-1e-10,'body pose overwrote the expression-owned eye rotation');
assert.equal(brow.position.y,browExpressionY,'body pose reset an expression-owned brow translation');
assert.equal(lip.position.y,lipExpressionY,'body pose reset an expression-owned lip translation');
assert(Math.abs(bodyBone.quaternion.dot(bodyExpected))>1-1e-10,'body pose failed to apply an animated arm rotation');
fixedFace.quaternion.copy(fixedBefore);expressionFace.quaternion.copy(expressionBefore);bodyBone.quaternion.copy(bodyBefore);brow.position.copy(browPositionBefore);lip.position.copy(lipPositionBefore);
unit.captureFacePose();let facialSnapshotCopies=0,facialMorphWrites=0;const restoreFaceCopies=[];
for(const value of [...unit.currentFace.positions,...unit.currentFace.quaternions]){const copy=value.copy;value.copy=function(...args){facialSnapshotCopies++;return copy.apply(this,args);};restoreFaceCopies.push(()=>{value.copy=copy;});}
const faceMorphValues=unit.currentFace.morphs;unit.currentFace.morphs=new Proxy(faceMorphValues,{set(target,key,value){facialMorphWrites++;target[key]=value;return true;}});
unit.captureFacePose();unit.currentFace.morphs=faceMorphValues;restoreFaceCopies.forEach(restore=>restore());
assert.equal(facialSnapshotCopies,0,'stable face snapshot does not recopy unchanged positions or quaternions');
assert.equal(facialMorphWrites,0,'stable face snapshot does not rewrite unchanged morph values');
const originalSetFromEuler=THREE.Quaternion.prototype.setFromEuler;let headLayerEulerWrites=0;
try{
  unit.faceAnimator.restoreHeadLayer();
  assert.equal(unit.faceAnimator.applyHeadLayer(.2,-.1),unit.rig.byName.neck,'head-layer composition returns the existing neck bone without allocating a wrapper object');
  THREE.Quaternion.prototype.setFromEuler=function(...args){headLayerEulerWrites++;return originalSetFromEuler.apply(this,args);};
  unit.faceAnimator.applyHeadLayer(.2,-.1,true);
  assert.equal(headLayerEulerWrites,0,'stable head layer should reuse the already composed body/look quaternions');
  unit.faceAnimator.applyHeadLayer(.25,-.1,true);
  assert.equal(headLayerEulerWrites,2,'changed interpolated attention must recompose neck and head');
}finally{THREE.Quaternion.prototype.setFromEuler=originalSetFromEuler;}
unit.faceAnimator.restoreHeadLayer();
// Face-only contact clearance starts from the latest body simulation pose,
// but most rig channels already match it after rendering. Restore only the
// channels that interpolation/weapon grip actually changed.
const restorePose=unit.current,restorePositionBone=restorePose.positionIndices[0],restoreQuaternionBone=restorePose.quaternionIndices[0],restoreMorph=restorePose.morphIndices[0];
for(let i=0;i<restorePose.positionIndices.length;i++)unit.rig.bones[restorePose.positionIndices[i]].position.copy(restorePose.positions[i]);
for(let i=0;i<restorePose.quaternionIndices.length;i++)unit.rig.bones[restorePose.quaternionIndices[i]].quaternion.copy(restorePose.quaternions[i]);
for(let i=0;i<restorePose.morphIndices.length;i++)unit.model.mesh.morphTargetInfluences[restorePose.morphIndices[i]]=restorePose.morphs[i];
unit.rig.bones[restorePositionBone].position.x+=.01;unit.rig.bones[restoreQuaternionBone].quaternion.x+=.01;
unit.model.mesh.morphTargetInfluences[restoreMorph]+=.1;
unit._dirtyPositionSlots.length=0;unit._dirtyPositionSlots.push(restorePose.positionIndices.indexOf(restorePositionBone));
unit._dirtyQuaternionSlots.length=0;unit._dirtyQuaternionSlots.push(restorePose.quaternionIndices.indexOf(restoreQuaternionBone));
unit._dirtyMorphIndices.length=0;unit._dirtyMorphIndices.push(restoreMorph);
let restoredPositions=0,restoredQuaternions=0,restoredMorphs=0;const restoreMethods=[];
for(const index of restorePose.positionIndices){const value=unit.rig.bones[index].position,copy=value.copy;value.copy=function(...args){restoredPositions++;return copy.apply(this,args);};restoreMethods.push(()=>{value.copy=copy;});}
for(const index of restorePose.quaternionIndices){const value=unit.rig.bones[index].quaternion,copy=value.copy;value.copy=function(...args){restoredQuaternions++;return copy.apply(this,args);};restoreMethods.push(()=>{value.copy=copy;});}
const plainInfluences=unit.model.mesh.morphTargetInfluences;unit.model.mesh.morphTargetInfluences=new Proxy(plainInfluences,{set(target,key,value){restoredMorphs++;target[key]=value;return true;}});
const snapshotArrays=['positionIndices','positions','quaternionIndices','quaternions','morphIndices','morphs'],snapshotValues={},snapshotReads={};
for(const key of snapshotArrays){const target=restorePose[key];snapshotValues[key]=target;snapshotReads[key]=0;restorePose[key]=new Proxy(target,{get(array,index,receiver){if(typeof index==='string'&&index!=='length'&&Number.isInteger(Number(index)))snapshotReads[key]++;return Reflect.get(array,index,receiver);}});}
unit.restoreBodySnapshot();
for(const key of snapshotArrays)restorePose[key]=snapshotValues[key];
unit.model.mesh.morphTargetInfluences=plainInfluences;restoreMethods.forEach(restore=>restore());
assert.equal(restoredPositions,1,'body restore only writes the single changed position channel');
assert.equal(restoredQuaternions,1,'body restore only writes the single changed rotation channel');
assert.equal(restoredMorphs,1,'body restore only writes the single changed morph');
assert.equal(snapshotReads.positionIndices,1,'body restore visits only dirty position slots');
assert.equal(snapshotReads.positions,1,'body restore reads only dirty position values');
const expectedRestoreQuaternionSlots=new Set(unit._dirtyQuaternionSlots);for(const slot of unit._weaponQuaternionSlots)if(!unit._quaternionInterpolationDirty[slot])expectedRestoreQuaternionSlots.add(slot);
assert.equal(snapshotReads.quaternionIndices,expectedRestoreQuaternionSlots.size,'body restore visits only dirty and weapon-arm quaternion slots');
assert.equal(snapshotReads.quaternions,expectedRestoreQuaternionSlots.size,'body restore reads only dirty and weapon-arm quaternion values');
assert.equal(snapshotReads.morphIndices,0,'body restore uses the precomputed body morph slot map instead of scanning snapshot indices');
assert.equal(snapshotReads.morphs,1,'body restore reads only dirty body morph values');
assert.equal(unit.rig.bones[restorePositionBone].position.x,restorePose.positions[0].x,'changed body position returns to snapshot');
assert.equal(unit.rig.bones[restoreQuaternionBone].quaternion.x,restorePose.quaternions[0].x,'changed body rotation returns to snapshot');
assert.equal(plainInfluences[restoreMorph],restorePose.morphs[0],'changed body morph returns to snapshot');
unit.dispose();console.log('PASS infantry body/face cadence and separated snapshot integration');




