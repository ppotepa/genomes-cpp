(function () {
  'use strict';
  const R=window.RTS, V=()=>new THREE.Vector3();
  const assert=(ok,message)=>{if(!ok)throw new Error(message);};
  const angle=(a,b)=>2*Math.acos(Math.min(1,Math.abs(a.dot(b))));
  const point=(unit,name)=>unit.rig.modelPoint(name,V());
  function valid(unit) {
    assert(unit.rig.maxLengthError()<1e-5,'Changed bone lengths');
    for(const b of unit.rig.bones)assert(b.matrixWorld.elements.every(Number.isFinite),'Nonfinite '+b.name);
    assert(Math.min(...unit.animator.metrics.minBoot)>-.007,'Boot below ground');
  }
  R.SprintChecks={run() {
    const side=new R.Side('SIDE_A',R.Config.SIDES.SIDE_A),checks=[];
    const test=(name,fn)=>{try{checks.push({name,ok:true,details:fn()});}catch(error){checks.push({name,ok:false,error:error.message});}};
    const make=(seed=1003,loadout='SCOUT',genome=null)=>new R.InfantryUnit({id:'sprint-check',side,seed,genome,state:'RUN',detail:'world',equipmentOptions:{loadout}});
    test('Hand contact reuses anatomy-only bind frames',()=>{
      const u=make();
      try {
        u.animator.setState('PRONE',true);u.animator.current.handIK=1;u.animator.weaponLayer=null;
        const original=THREE.Matrix4.prototype.makeBasis;let calls=0;
        THREE.Matrix4.prototype.makeBasis=function(...args){calls++;return original.apply(this,args);};
        try{u.animator.placeHands({surface:R.FlatSurface,moveAngle:0,turning:false,turnRate:0,treadmill:false});}
        finally{THREE.Matrix4.prototype.makeBasis=original;}
        assert(calls===10,'Unexpected dynamic frame count or static bind frames were rebuilt ('+calls+')');
        return {terrainFrames:calls,cachedBindFrames:u.animator._handBindFrames.length};
      } finally {u.dispose();}
    });
    test('Two-bone IK reuses normalized rest directions after chain setup',()=>{
      const points={upper:new THREE.Vector3(0,0,0),lower:new THREE.Vector3(0,-1,0),tip:new THREE.Vector3(0,-2,0)},poses={};
      const rig={anatomy:{points},modelPoint(name,out){return out.copy(points[name]);},setModelQuaternion(name,q){poses[name]=q.clone();}};
      const ik=new R.TwoBoneIK(rig),target=new THREE.Vector3(.36,-1.55,.24),pole=new THREE.Vector3(0,0,1);
      ik.solve('upper','lower','tip',target,pole,false);
      assert(ik.restOrientations.upper.direction===ik.lengths.upper.upperDirection,'Upper rest direction was copied instead of shared');
      assert(ik.restOrientations.lower.direction===ik.lengths.upper.lowerDirection,'Lower rest direction was copied instead of shared');
      let normalizations=0;const normalize=THREE.Vector3.prototype.normalize;
      THREE.Vector3.prototype.normalize=function(){normalizations++;return normalize.call(this);};
      try{ik.solve('upper','lower','tip',target,pole,false);}finally{THREE.Vector3.prototype.normalize=normalize;}
      assert(normalizations===3,'Repeated solve should normalize only bend and two solved segment directions; got '+normalizations);
      assert(Object.values(poses).every(q=>q.toArray().every(Number.isFinite)),'IK produced a nonfinite pose');
      return {normalizations,restFrames:2};
    });
    test('Two-bone IK reads root world orientation once per solve',()=>{
      const u=make(1003,'SCOUT'),root=u.rig.root,original=root.getWorldQuaternion;
      try{
        const start=u.rig.modelPoint('thigh.L',V()),target=start.clone().add(new THREE.Vector3(.025,-.25,.035)),pole=start.clone().add(new THREE.Vector3(.2,0,0));let reads=0;
        root.getWorldQuaternion=function(...args){reads++;return original.apply(this,args);};
        u.animator.ik.solve('thigh.L','shin.L','foot.L',target,pole,false);
        assert(reads===1,'Both IK bone writes should share one root world orientation read; got '+reads);
        return {rootWorldOrientationReads:reads,boneWrites:2};
      }finally{root.getWorldQuaternion=original;u.dispose();}
    });
    test('Continuous sprint intensity and contact trajectory',()=>{
      const u=make();
      try {
        const A=u.model.anatomy,p=u.phenotype,values=[];
        for(const ratio of [.5,.7,.701,.8,.9,1]) {
          const g=R.PostureProfile.gait(0,p.runSpeed*ratio,A,p,{});
          if(values.length)assert(g.sprint>=values.at(-1),'Sprint intensity reversed');
          values.push(g.sprint);
        }
        assert(values[0]===0&&values[1]===0&&values[2]<.001&&values.at(-1)>.999,'Wrong sprint range');
        assert(R.PostureProfile.gait(.6,p.runSpeed,A,p,{}).sprint===0,'Sprint in deep crouch');
        const g=R.PostureProfile.gait(0,p.runSpeed,A,p,{}),eps=1e-6;
        const sample=t=>R.GaitProfile.sampleLowContact(t,g.duty,g.cycleM,g.liftM,{},1);
        for(const boundary of [0,g.duty]) {
          const a=sample(boundary-eps),b=sample(boundary+eps);
          assert(Math.abs(a.z-b.z)<g.cycleM*eps*3,'Foot teleports across contact boundary');
          assert(Math.abs((b.z-a.z)/(2*eps)+g.cycleM)<.002,'Foot velocity discontinuity');
          assert(Math.max(a.lift,b.lift)<1e-8,'Foot lift discontinuity');
          assert(Math.max(Math.abs(a.recoveryZ),Math.abs(b.recoveryZ))<1e-8,'Recovery offset breaks landing');
        }
        let maximum={height:0,phase:0};
        for(let i=0;i<=100;i++) {
          const f=sample(g.duty+(1-g.duty)*i/100);
          if(f.lift>maximum.height)maximum={height:f.lift,phase:i/100};
        }
        assert(maximum.phase<.5,'Heel recovery must precede forward extension');
        return {intensities:values,peakSwing:maximum.phase};
      } finally {u.dispose();}
    });
    test('Post-look head and equipment clearance share one skin palette',()=>{
      const u=make(1003,'HEAVY_SUPPORT');let prepares=0,exactConfirmations=0;
      try {
        const prepare=u.model.prepareSkinPalette.bind(u.model);
        u.model.prepareSkinPalette=()=>{prepares++;return prepare();};
        const confirm=u.model.confirmUnchangedTransforms.bind(u.model);
        u.model.confirmUnchangedTransforms=()=>{exactConfirmations++;return confirm();};
        u.faceAnimator.applyHeadLayer(.31,-.12);
        u.animator.finalizeAfterLook(u._animContext);
        assert(prepares===1,'Head and equipment scans prepared separate palettes');
        const clearancePreparations=prepares;
        assert(exactConfirmations===0,'Finalizer should not rescan before its downstream head restore and weapon phase');
        const neck=u.rig.byName.neck,updateNeck=neck.updateWorldMatrix,updateRoot=u.model.root.updateMatrixWorld;let branchRefreshes=0,fullTraversals=0;
        neck.updateWorldMatrix=function(parents,children){if(parents===true&&children===true)branchRefreshes++;return updateNeck.call(this,parents,children);};
        u.model.root.updateMatrixWorld=function(force){if(force===true)fullTraversals++;return updateRoot.call(this,force);};
        u.faceAnimator.restoreHeadLayer();u.model.prepareSkinPalette();neck.updateWorldMatrix=updateNeck;u.model.root.updateMatrixWorld=updateRoot;
        assert(branchRefreshes===1&&fullTraversals===0,'Restoring the post-clearance head layer must refresh only the neck/head branch');
        return {clearancePalettePreparations:clearancePreparations,unnecessaryConfirmations:exactConfirmations,headBranchRefreshes:branchRefreshes,fullTraversals};
      } finally {u.dispose();}
    });
    test('Certified simulation pose skips the next palette transform scan once',()=>{
      const u=make(1003,'HEAVY_SUPPORT'),model=u.model,animator=u.animator;
      try{
        model.prepareSkinPalette();let rootUpdates=0,headBranchUpdates=0;
        const updateRoot=model.root.updateMatrixWorld,head=u.rig.byName.head,updateHead=head.updateWorldMatrix;
        model.root.updateMatrixWorld=function(force){if(force===true)rootUpdates++;return updateRoot.call(this,force);};
        head.updateWorldMatrix=function(parents,children){if(parents===true&&children===true)headBranchUpdates++;return updateHead.call(this,parents,children);};
        model.confirmUnchangedTransforms();
        model.prepareSkinPalette();
        assert(rootUpdates===0,'Certified unchanged pose rebuilt the palette');
        head.quaternion.x+=.013;
        model.prepareSkinPalette();
        assert(rootUpdates===0&&headBranchUpdates===1,'Direct bone mutation must update the changed head branch after one-shot certification');
        head.quaternion.x-=.013;
        model.prepareSkinPalette();
        assert(rootUpdates===0&&headBranchUpdates===2,'Restoring direct bone mutation did not refresh only the changed branch');
        model.root.updateMatrixWorld=updateRoot;head.updateWorldMatrix=updateHead;
        return {certificationIsOneShot:true,directMutationDetected:true,headBranchUpdates,fullTraversals:rootUpdates};
      }finally{u.dispose();}
    });
    test('CPU palette transform signature reuses its typed buffer across moving poses',()=>{
      const u=make(1003,'HEAVY_SUPPORT'),model=u.model,head=u.rig.byName.head,originalArray=globalThis.Float64Array,oldX=head.quaternion.x;
      try{
        model.prepareSkinPalette();let allocations=0;
        globalThis.Float64Array=function(length){allocations++;return new originalArray(length);};
        globalThis.Float64Array.prototype=originalArray.prototype;
        head.quaternion.x+=.013;model.prepareSkinPalette();
        assert(allocations===0,'A normal pose change allocated a replacement transform signature');
        head.quaternion.x-=.013;model.prepareSkinPalette();
        assert(allocations===0,'Restoring the pose allocated a replacement transform signature');
        return {typedArrayAllocationsForTwoPoseChanges:allocations,reused:true};
      }finally{globalThis.Float64Array=originalArray;head.quaternion.x=oldX;u.dispose();}
    });
    test('Sparse skin-matrix refresh matches a complete hierarchy update',()=>{
      const u=make(2003,'HEAVY_SUPPORT'),model=u.model,cases=[
        ()=>{u.rig.byName.head.quaternion.x+=.021;},
        ()=>{u.rig.byName.hips.position.y+=.013;},
        ()=>{u.rig.byName['hand.L'].quaternion.z-=.017;u.rig.byName['foot.R'].position.x+=.009;},
        ()=>{model.root.position.x+=.025;model.root.rotation.y+=.031;}
      ];
      try{
        model.prepareSkinPalette();let checks=0;
        for(const mutate of cases){
          mutate();model.prepareSkinPalette();
          const expected=u.rig.bones.map(bone=>Array.from(bone.matrixWorld.elements));
          model.root.updateMatrixWorld(true);
          for(let b=0;b<u.rig.bones.length;b++){const bone=u.rig.bones[b],full=bone.matrixWorld.elements;for(let i=0;i<16;i++)assert(expected[b][i]===full[i],'Sparse matrix refresh differed from full hierarchy update at '+bone.name+' element '+i);checks++;}
        }
        return {matrixComparisons:checks,poseAndAncestorCases:cases.length};
      }finally{u.dispose();}
    });
    test('Packed CPU skin palette preserves full-precision body and gear transforms',()=>{
      const u=make(1003,'HEAVY_SUPPORT'),model=u.model;
      try {
        const palette=model.prepareSkinPalette();
        assert(palette.length===u.rig.bones.length*12,'Palette must store only affine 3×4 rows');
        const direct=(mesh,index,includeMorphs)=>{
          const g=mesh.geometry,base=new THREE.Vector3().fromArray(g.attributes.position.array,index*3);
          if(includeMorphs){const targets=g.morphAttributes.position||[],values=mesh.morphTargetInfluences||[];for(let m=0;m<targets.length;m++)if(values[m])base.addScaledVector(new THREE.Vector3().fromArray(targets[m].array,index*3),values[m]);}
          base.applyMatrix4(mesh.bindMatrix);const out=V(),ids=g.attributes.skinIndex.array,weights=g.attributes.skinWeight.array;
          for(let k=0;k<4;k++){
            const weight=weights[index*4+k];if(weight<=0)continue;
            const bone=ids[index*4+k],matrix=new THREE.Matrix4().multiplyMatrices(u.rig.bones[bone].matrixWorld,u.rig.skeleton.boneInverses[bone]);
            out.addScaledVector(base.clone().applyMatrix4(matrix),weight);
          }
          return out.applyMatrix4(mesh.bindMatrixInverse);
        };
        const curl=model.mesh.morphTargetDictionary.handsRelax;if(curl!==undefined)model.mesh.morphTargetInfluences[curl]=.37;
        const samples=[];
        for(const [mesh,method,morphs] of [[model.mesh,model.skinVertex.bind(model),true],[model.gearMesh,model.skinGearVertex.bind(model),false]])if(mesh){
          const count=mesh.geometry.attributes.position.count;
          for(const index of [...new Set([0,Math.floor(count/3),count-1])]){
            const actual=method(index,V()),expected=direct(mesh,index,morphs),error=actual.distanceTo(expected);
            assert(error<1e-12,'Packed palette changed '+mesh.name+' vertex '+index+' by '+error);
            samples.push({mesh:mesh.name,index,error});
          }
        }
        return {bones:u.rig.bones.length,paletteElements:palette.length,samples};
      } finally {u.dispose();}
    });
    test('Contact skin samples reuse exact pose and invalidate on morph or bone changes',()=>{
      const u=make(1003,'HEAVY_SUPPORT'),model=u.model,animator=u.animator;
      try {
        model.prepareSkinPalette();
        let skinCalls=0,paletteTraversals=0,changedBranches=0;
        const skin=model.skinVertex,updateRoot=model.root.updateMatrixWorld,headUpdate=model.rig.byName.head.updateWorldMatrix;
        model.skinVertex=function(...args){skinCalls++;return skin.apply(this,args);};
        model.root.updateMatrixWorld=function(force){if(force===true)paletteTraversals++;return updateRoot.call(this,force);};
        model.rig.byName.head.updateWorldMatrix=function(parents,children){if(parents===true&&children===true)changedBranches++;return headUpdate.call(this,parents,children);};
        const bodySamples=animator.supportIndices.body.slice(0,18);
        const first=animator.minimumClearance(bodySamples,R.FlatSurface,false,false),initialCalls=skinCalls;
        const repeated=animator.minimumClearance(bodySamples,R.FlatSurface,false,false);
        assert(skinCalls===initialCalls&&repeated===first,'unchanged support scan missed its vertex cache');
        const influence=model.mesh.morphTargetInfluences,previousInfluence=influence[0];
        influence[0]=previousInfluence+.17;
        const morphed=animator.minimumClearance(bodySamples,R.FlatSurface,false,false);
        assert(skinCalls===initialCalls+bodySamples.length,'morph mutation did not invalidate cached support vertices');
        assert(paletteTraversals===0,'morph-only mutation rebuilt the unchanged bone palette');
        let reference=Infinity;
        for(const index of bodySamples){skin.call(model,index,animator._w);animator._w.applyMatrix4(model.mesh.matrixWorld);reference=Math.min(reference,animator._w.y);}
        assert(Math.abs(morphed-reference)<1e-12,'morph-invalidated cached clearance differs from direct skinning');
        influence[0]=previousInfluence;model.mesh.morphTargetInfluences=influence;
        const head=model.rig.byName.head,oldHead=head.quaternion.clone();head.quaternion.x+=.013;
        const moved=animator.minimumClearance(bodySamples,R.FlatSurface,false,false);
        assert(skinCalls===initialCalls+bodySamples.length*2,'bone mutation did not invalidate cached support vertices');
        assert(paletteTraversals===0&&changedBranches===1,'bone mutation must refresh only the changed head branch');
        head.quaternion.copy(oldHead);model.root.updateMatrixWorld=updateRoot;model.rig.byName.head.updateWorldMatrix=headUpdate;model.skinVertex=skin;
        return {samples:bodySamples.length,unchangedPoseReuses:true,morphInvalidatesWithoutPaletteRebuild:true,boneInvalidatesAndRefreshesPalette:true,changedBranches,fullTraversals:paletteTraversals,morphedError:Math.abs(morphed-reference),moved:Number.isFinite(moved)};
      } finally {u.dispose();}
    });
    test('Exact debug pose bounds reuse a stable skin revision',()=>{
      const u=make(1003,'HEAVY_SUPPORT'),model=u.model,animator=u.animator;
      try{
        let skinCalls=0;const skin=model.skinVertex;
        model.skinVertex=function(...args){skinCalls++;return skin.apply(this,args);};
        const first=animator.poseBounds(new THREE.Box3()),initialCalls=skinCalls;
        assert(initialCalls===model.surface.vertices,'first exact bounds query did not visit every surface vertex');
        const expected=[first.min.x,first.min.y,first.min.z,first.max.x,first.max.y,first.max.z];
        const repeated=animator.poseBounds(new THREE.Box3());
        assert(skinCalls===initialCalls,'stable debug bounds repeated full CPU skinning');
        assert(JSON.stringify([repeated.min.x,repeated.min.y,repeated.min.z,repeated.max.x,repeated.max.y,repeated.max.z])===JSON.stringify(expected),'cached bounds changed the exact AABB');
        const influences=model.mesh.morphTargetInfluences,oldInfluence=influences[0];influences[0]=oldInfluence+.13;
        animator.poseBounds(new THREE.Box3());
        assert(skinCalls===initialCalls+model.surface.vertices,'morph change did not invalidate exact bounds');
        influences[0]=oldInfluence;const head=model.rig.byName.head,oldHead=head.quaternion.clone();head.quaternion.x+=.011;
        animator.poseBounds(new THREE.Box3());
        assert(skinCalls===initialCalls+model.surface.vertices*2,'bone change did not invalidate exact bounds');
        head.quaternion.copy(oldHead);model.skinVertex=skin;
        return {surfaceVertices:model.surface.vertices,stablePoseReuses:true,morphAndBoneInvalidate:true};
      }finally{u.dispose();}
    });
    test('Animation pose writes only a changed hips translation',()=>{
      const u=make(1003,'SCOUT'),calls=new Array(u.rig.bones.length).fill(0),restore=[],hipsIndex=u.rig.index.hips,hips=u.rig.byName.hips.position,originalSet=hips.set;
      try {
        for(let i=0;i<u.rig.bones.length;i++){
          const position=u.rig.bones[i].position,copy=position.copy;
          position.copy=function(value){calls[i]++;return copy.call(this,value);};
          restore.push(()=>{position.copy=copy;});
        }
        let hipSets=0;hips.set=function(...args){hipSets++;return originalSet.apply(this,args);};restore.push(()=>{hips.set=originalSet;});
        u.animator.current.hips.x+=.01;
        u.animator.applyPose(u.animator.current);
        for(let i=0;i<calls.length;i++)assert(calls[i]===0,'Animation pose copied position for '+u.rig.bones[i].name+': '+calls[i]);
        assert(hipSets===1,'Changed hips translation should be written once; got '+hipSets);
        assert(hipsIndex!==undefined,'Rig has no hips channel');
        return {bones:u.rig.bones.length,positionCopies:calls.reduce((sum,count)=>sum+count,0),changedHipsSets:hipSets};
      } finally {restore.forEach(undo=>undo());u.dispose();}
    });
    test('Hot pose application uses allocation-free indexed traversal',()=>{
      const u=make(1003,'SCOUT'),restore=[];
      const forbid=(owner,key,label)=>{const original=owner[key];owner[key]=()=>{throw new Error('forEach used for '+label);};restore.push(()=>{owner[key]=original;});};
      try {
        const pose=u.animator.current;
        forbid(pose.q,'forEach','bone rotations');
        for(const key of ['feet','hands','knees','elbows'])forbid(pose[key],'forEach',key);
        forbid(u.rig.bones,'forEach','rig rotations');
        u.animator.dampPose(pose,u.animator.targetPose,1/60);
        u.animator.applyPose(pose);
        return {bones:u.rig.bones.length,vectorChannels:4,indexed:true};
      } finally {restore.forEach(undo=>undo());u.dispose();}
    });
    test('Contact release and free-hand path use indexed traversal',()=>{
      const u=make(1003,'SCOUT'),restore=[];
      const forbid=(owner,key,label)=>{const original=owner[key];owner[key]=()=>{throw new Error('forEach used for '+label);};restore.push(()=>{owner[key]=original;});};
      try {
        forbid(u.animator.footContacts,'forEach','foot contacts');forbid(u.animator.handContacts,'forEach','hand contacts');
        u.animator.current.handIK=0;u.animator.releaseContacts();u.animator.placeHands({});
        const feet=u.animator.bipedFeet,adjust=feet.adjustStance;feet.adjustStance=function(){};
        for(const track of feet.tracks){track.active=true;track.lastSwing=true;}
        forbid(feet.tracks,'forEach','biped foot tracks');
        feet.update(u.animator.current,u.animator,{speed:0,treadmill:false,turning:false,turnRate:0,surface:R.FlatSurface},1/60);
        assert(feet.tracks.every(track=>!track.active&&!track.lastSwing),'stationary stance did not release its swing tracks');
        feet.adjustStance=adjust;
        return {feet:u.animator.footContacts.length,hands:u.animator.handContacts.length,indexed:true};
      } finally {restore.forEach(undo=>undo());u.dispose();}
    });
    test('Pose clear, copy and blend use indexed traversal with exact channels',()=>{
      const u=make(1003,'SCOUT'),restore=[],fields=['feet','hands','knees','elbows'];
      const failForEach=(owner,label)=>{const original=owner.forEach;owner.forEach=function(){throw new Error('forEach used for '+label);};restore.push(()=>{owner.forEach=original;});};
      const near=(actual,expected,label)=>assert(Math.abs(actual-expected)<1e-12,label+': '+actual+' != '+expected);
      try {
        const animator=u.animator,dst=animator.current,a=animator.from,b=animator.targetPose;
        for(let i=0;i<a.q.length;i++){a.q[i].set(.01*(i+1),-.02*(i+1),.03*(i+1),1).normalize();b.q[i].set(-.02*(i+1),.01*(i+1),.015*(i+1),1).normalize();}
        for(const key of fields)for(let i=0;i<2;i++){a[key][i].set(i+.1,i+.2,i+.3);b[key][i].set(i+.7,i+.8,i+.9);}
        a.hips.set(.1,.2,.3);b.hips.set(.7,.8,.9);
        a.prone=.1;b.prone=.9;a.handIK=.2;b.handIK=.8;a.handCurl=.3;b.handCurl=.7;a.gait=.4;b.gait=.6;
        for(const key of ['footLift','footPlant','footPitch','toePitch','support','footYaw','footRelative','anklePitch','ankleYaw','handPlant','handLift'])for(let i=0;i<2;i++){a[key][i]=.1+i*.1;b[key][i]=.9-i*.1;}
        const t=.37,expectedQ=a.q.map((q,i)=>q.clone().slerp(b.q[i],t)),expectedVectors={};
        for(const key of fields)expectedVectors[key]=a[key].map((v,i)=>v.clone().lerp(b[key][i],t));
        const scalarKeys=['prone','handIK','handCurl','gait'],expectedScalars={};for(const key of scalarKeys)expectedScalars[key]=a[key]+(b[key]-a[key])*t;
        const pairKeys=['footLift','footPlant','footPitch','toePitch','support','footYaw','footRelative','anklePitch','ankleYaw','handPlant','handLift'],expectedPairs={};for(const key of pairKeys)expectedPairs[key]=a[key].map((v,i)=>v+(b[key][i]-v)*t);
        failForEach(dst.q,'destination rotations');for(const key of fields)failForEach(dst[key],'destination '+key);
        dst.copy(a);
        for(let i=0;i<a.q.length;i++)assert(Math.abs(dst.q[i].x-a.q[i].x)+Math.abs(dst.q[i].y-a.q[i].y)+Math.abs(dst.q[i].z-a.q[i].z)+Math.abs(dst.q[i].w-a.q[i].w)<1e-12,'copy rotation mismatch '+i);
        for(const key of fields)for(let i=0;i<2;i++)assert(dst[key][i].distanceTo(a[key][i])<1e-12,'copy '+key+' mismatch');
        dst.blend(a,b,t);
        for(let i=0;i<expectedQ.length;i++)assert(Math.abs(dst.q[i].x-expectedQ[i].x)+Math.abs(dst.q[i].y-expectedQ[i].y)+Math.abs(dst.q[i].z-expectedQ[i].z)+Math.abs(dst.q[i].w-expectedQ[i].w)<1e-12,'blend rotation mismatch '+i);
        for(const key of fields)for(let i=0;i<2;i++)assert(dst[key][i].distanceTo(expectedVectors[key][i])<1e-12,'blend '+key+' mismatch');
        for(const key of scalarKeys)near(dst[key],expectedScalars[key],'blend '+key);
        for(const key of pairKeys)for(let i=0;i<2;i++)near(dst[key][i],expectedPairs[key][i],'blend '+key+'['+i+']');
        dst.clear();
        assert(dst.q.every(q=>Math.abs(q.x)+Math.abs(q.y)+Math.abs(q.z)<1e-12&&Math.abs(q.w-1)<1e-12),'clear did not identity all rotations');
        assert(dst.footPlant[0]===1&&dst.footPlant[1]===1,'clear lost planted defaults');
        for(const key of pairKeys)if(key!=='footPlant')assert(dst[key][0]===0&&dst[key][1]===0,'clear did not reset '+key);
        return {bones:dst.q.length,vectorPairs:fields.length*2,scalarChannels:scalarKeys.length,pairChannels:pairKeys.length*2,indexed:true};
      } finally {restore.forEach(undo=>undo());u.dispose();}
    });
    test('Pose blend skips channels shared by both transition endpoints',()=>{
      const u=make(1003,'SCOUT'),animator=u.animator,a=animator.from,b=animator.targetPose,out=animator.transitionPose;
      const originalQuaternionCopy=THREE.Quaternion.prototype.copy,originalVectorCopy=THREE.Vector3.prototype.copy,
        originalVectorLerp=THREE.Vector3.prototype.lerp,originalMix=R.Math.mix;
      try{
        a.copy(animator.current);b.copy(animator.current);out.blend(a,b,.37);
        let quaternionCopies=0,vectorCopies=0,vectorLerps=0,mixes=0;
        THREE.Quaternion.prototype.copy=function(...args){quaternionCopies++;return originalQuaternionCopy.apply(this,args);};
        THREE.Vector3.prototype.copy=function(...args){vectorCopies++;return originalVectorCopy.apply(this,args);};
        THREE.Vector3.prototype.lerp=function(...args){vectorLerps++;return originalVectorLerp.apply(this,args);};
        R.Math.mix=function(...args){mixes++;return originalMix.apply(this,args);};
        out.blend(a,b,.37);
        assert(quaternionCopies===0,'shared rotation endpoints copied '+quaternionCopies+' quaternions');
        assert(vectorCopies===0&&vectorLerps===0,'shared vector endpoints performed '+vectorCopies+' copies and '+vectorLerps+' lerps');
        assert(mixes===0,'shared scalar/pair endpoints evaluated '+mixes+' blends');
        for(let i=0;i<a.q.length;i++)assert(out.q[i].x===a.q[i].x&&out.q[i].y===a.q[i].y&&out.q[i].z===a.q[i].z&&out.q[i].w===a.q[i].w,'shared rotation changed at '+i);
        for(const key of ['feet','hands','knees','elbows'])for(let i=0;i<2;i++)assert(out[key][i].x===a[key][i].x&&out[key][i].y===a[key][i].y&&out[key][i].z===a[key][i].z,'shared '+key+' vector changed');
        return {quaternionCopies,vectorCopies,vectorLerps,mixes,channels:a.q.length};
      }finally{
        THREE.Quaternion.prototype.copy=originalQuaternionCopy;THREE.Vector3.prototype.copy=originalVectorCopy;
        THREE.Vector3.prototype.lerp=originalVectorLerp;R.Math.mix=originalMix;u.dispose();
      }
    });
    test('REST pose resets each rotation once and skips neutral-arm writes',()=>{
      const u=make(1003,'SCOUT'),originalIdentity=THREE.Quaternion.prototype.identity;
      let identities=0;
      try {
        for(let i=0;i<u.animator.targetPose.q.length;i++)u.animator.targetPose.q[i].set(.01,.02,.03,.99).normalize();
        THREE.Quaternion.prototype.identity=function(){identities++;return originalIdentity.call(this);};
        u.animator.samplePose('REST',u.animator.targetPose,u.animator.motion);
        assert(identities===u.rig.bones.length,'REST cleared quaternions more than once: '+identities);
        for(let i=0;i<u.animator.targetPose.q.length;i++){const q=u.animator.targetPose.q[i];assert(q.x===0&&q.y===0&&q.z===0&&q.w===1,'REST rotation differs from the identity pose at bone '+i);}
        const firstReset=identities;identities=0;let vectorWrites=0;const originalSet=THREE.Vector3.prototype.set,originalCopy=THREE.Vector3.prototype.copy;
        THREE.Vector3.prototype.set=function(...args){vectorWrites++;return originalSet.apply(this,args);};
        THREE.Vector3.prototype.copy=function(...args){vectorWrites++;return originalCopy.apply(this,args);};
        try{u.animator.samplePose('REST',u.animator.targetPose,u.animator.motion);}finally{THREE.Vector3.prototype.set=originalSet;THREE.Vector3.prototype.copy=originalCopy;}
        assert(identities===0,'Repeated REST sample rewrote '+identities+' already-identity quaternions');
        assert(vectorWrites===0,'Repeated REST sample rewrote '+vectorWrites+' unchanged vector channels');
        return {bones:u.rig.bones.length,quaternionResets:firstReset,repeatedResetWrites:identities,repeatedVectorWrites:vectorWrites,neutralArmWrites:0};
      } finally {THREE.Quaternion.prototype.identity=originalIdentity;u.dispose();}
    });
    test('Stable pose application skips unchanged hips, rotations and hand morph writes',()=>{
      const u=make(1003,'SCOUT'),animator=u.animator,model=u.model,originalWeaponLayer=animator.weaponLayer;
      const hips=u.rig.byName.hips.position,originalHipSet=hips.set,originalQuaternionCopy=THREE.Quaternion.prototype.copy,originalInfluences=model.mesh.morphTargetInfluences;
      animator.weaponLayer=null;
      try{
        animator.applyPose(animator.current);
        let hipWrites=0,quaternionWrites=0,morphWrites=0;
        hips.set=function(...args){hipWrites++;return originalHipSet.apply(this,args);};
        THREE.Quaternion.prototype.copy=function(...args){quaternionWrites++;return originalQuaternionCopy.apply(this,args);};
        const influences=originalInfluences,index=model.mesh.morphTargetDictionary.handsRelax;
        model.mesh.morphTargetInfluences=new Proxy(influences,{set(target,key,value){morphWrites++;target[key]=value;return true;}});
        animator.applyPose(animator.current);
        assert(hipWrites===0,'identical hips wrote position '+hipWrites+' times');
        assert(quaternionWrites===0,'identical animation channels wrote '+quaternionWrites+' quaternions');
        assert(morphWrites===0,'identical hand-curl morph wrote '+morphWrites+' times');
        assert(index!==undefined,'SCOUT fixture has no hand relaxation morph');
        return {hipWrites,quaternionWrites,morphWrites,animationChannels:u.rig.animationQuaternionIndices.length};
      }finally{
        model.mesh.morphTargetInfluences=originalInfluences;
        hips.set=originalHipSet;THREE.Quaternion.prototype.copy=originalQuaternionCopy;animator.weaponLayer=originalWeaponLayer;u.dispose();
      }
    });
    test('Weapon render reuses the reconciled slot list',()=>{
      const u=make(1003,'RIFLEMAN'),originalEntries=Object.entries;
      try {
        assert(u.weapons.instanceSlots.length>0,'Rifleman has no weapon slots');
        Object.entries=function(){throw new Error('weapon render allocated Object.entries');};
        u.weapons.render(.5,false);
        return {weaponSlots:u.weapons.instanceSlots.length,entryAllocations:0};
      } finally {Object.entries=originalEntries;u.dispose();}
    });
    test('Weapon body aiming reuses its Euler scratch object',()=>{
      const u=make(1003,'RIFLEMAN'),controller=u.weapons,slot=Object.keys(controller.instances)[0],OriginalEuler=THREE.Euler,originalSetFromEuler=THREE.Quaternion.prototype.setFromEuler;
      assert(slot,'Rifleman has no weapon instance');let constructed=0;
      try {
        controller.activeSlot=slot;controller.state='HELD';
        THREE.Euler=class CountingEuler extends OriginalEuler{constructor(...args){super(...args);constructed++;}};
        // This numeric harness only implements XYZ quaternion conversion; the
        // production aim intentionally uses Three.js' supported YXZ order.
        THREE.Quaternion.prototype.setFromEuler=function(){return this;};
        controller.prepareBody();controller.prepareBody();
        assert(constructed===0,'Body aim allocated Euler objects after controller construction');
        return {aimBones:2,calls:2,eulerAllocations:constructed};
      } finally {THREE.Euler=OriginalEuler;THREE.Quaternion.prototype.setFromEuler=originalSetFromEuler;controller.activeSlot=null;controller.state='STOWED';u.dispose();}
    });
    test('Facial springs reuse one exact exponential per timestep and frequency',()=>{
      const u=make(1003,'SCOUT'),face=u.faceAnimator,dt=.017,originalExp=Math.exp;
      try {
        for(const name of face.expressionNames){face.weights[name].value=0;face.weights[name].velocity=0;face.weightTargets[name]=0;}
        face.weights.ALERT.value=.21;face.weights.ALERT.velocity=.03;face.weightTargets.ALERT=.72;
        face.channels.eyeOpen.value=.11;face.channels.eyeOpen.velocity=-.02;face._expressionsSettled=false;
        const spring=(state,target,omega)=>{const y=state.value-target,e=originalExp(-omega*dt),temp=(state.velocity+omega*y)*dt;return {value:target+(y+temp)*e,velocity:(state.velocity-omega*temp)*e};};
        const expectedWeight=spring(face.weights.ALERT,.72,8.5),scale=face.face.expressionScale*face.face.eyeExpressionScale;
        const expectedEye=spring(face.channels.eyeOpen,Math.max(-.78,Math.min(.82,.18*expectedWeight.value*scale)),12);
        let exponentials=0;Math.exp=function(value){exponentials++;return originalExp(value);};
        face.updateExpressions(dt);
        assert(exponentials===3,'Expected one decay each for expression, eye, and non-eye frequencies; got '+exponentials);
        assert(face.weights.ALERT.value===expectedWeight.value&&face.weights.ALERT.velocity===expectedWeight.velocity,'Cached expression decay changed the spring result');
        assert(face.channels.eyeOpen.value===expectedEye.value&&face.channels.eyeOpen.velocity===expectedEye.velocity,'Cached eye decay changed the spring result');
        return {springCalls:face.expressionNames.length+12,uniqueExponentials:exponentials,exact:true};
      } finally {Math.exp=originalExp;u.dispose();}
    });
    test('Settled expression springs sleep and wake on a new target',()=>{
      const u=make(1003,'SCOUT'),face=u.faceAnimator;
      try {
        face.setExpression('ALERT',.8);
        for(let i=0;i<600&&!face._expressionsSettled;i++)face.updateExpressions(1/60);
        assert(face._expressionsSettled,'Expression springs failed to reach the settled state');
        const before=JSON.stringify({weights:face.weights,channels:face.channels});
        face.updateExpressions(1/60);
        assert(JSON.stringify({weights:face.weights,channels:face.channels})===before,'Settled expression springs still performed work');
        face.setExpression('ANGER',.7);
        assert(!face._expressionsSettled,'New expression did not wake the spring channels');
        return {settled:true,wokeOnTargetChange:true};
      } finally {u.dispose();}
    });
    test('Flattened facial presets preserve exact expression spring trajectories',()=>{
      const u=make(2003,'SCOUT'),face=u.faceAnimator,presets={
        ALERT:{eyeOpen:.18,browInnerUp:.22,browOuterUp:.13,lipPress:.025},
        FEAR:{eyeOpen:.58,browInnerUp:.58,browOuterUp:.31,jawOpen:.18,mouthOpen:.36,mouthStretch:.15},
        ANGER:{eyeSquint:.34,browDown:.58,jawOpen:.008,lipPress:.62,mouthCornerDown:.18},
        PAIN:{eyeSquint:.64,browInnerUp:.43,browDown:.14,jawOpen:.12,mouthOpen:.17,mouthStretch:.13,mouthCornerDown:.38,cheekRaise:.24},
        FATIGUE:{eyeOpen:-.72,eyeSquint:.16,browOuterUp:.06,jawOpen:.05,mouthOpen:.07,mouthCornerDown:.13}
      },limits={eyeOpen:[-.78,.82],eyeSquint:[0,.72],browInnerUp:[-.28,.62],browOuterUp:[-.24,.42],browDown:[0,.72],jawOpen:[0,.42],mouthOpen:[0,.66],mouthStretch:[0,.58],mouthCornerUp:[0,.48],mouthCornerDown:[0,.58],lipPress:[0,.72],cheekRaise:[0,.42]};
      const weights=Object.fromEntries(face.expressionNames.map(name=>[name,{value:0,velocity:0}])),channels=Object.fromEntries(Object.keys(face.channels).map(name=>[name,{value:0,velocity:0}]));
      const spring=(state,target,dt,omega)=>{if(!(dt>0)){state.value=target;state.velocity=0;return;}const y=state.value-target,e=Math.exp(-omega*dt),temp=(state.velocity+omega*y)*dt;state.value=target+(y+temp)*e;state.velocity=(state.velocity-omega*temp)*e;};
      try{
        for(const [expression,intensity] of [['ALERT',.73],['FEAR',.62],['ANGER',.81],['PAIN',.44],['FATIGUE',.57],['NEUTRAL',1]]){
          face.setExpression(expression,intensity);
          for(const dt of [1/60,.025,.05,1/30,.016]){
            if(!(face._expressionsSettled&&dt>0)){
              for(const name of face.expressionNames)spring(weights[name],face.weightTargets[name],dt,8.5);
              const target=Object.fromEntries(Object.keys(channels).map(name=>[name,0]));
              for(const name of face.expressionNames){const weight=weights[name].value;if(weight===0)continue;for(const [channel,amount] of Object.entries(presets[name]||{}))target[channel]+=amount*weight;}
              const f=face.face;
              for(const name of ['eyeOpen','eyeSquint'])target[name]*=f.expressionScale*f.eyeExpressionScale;
              for(const name of ['browInnerUp','browOuterUp','browDown'])target[name]*=f.expressionScale*f.browExpressionScale;
              for(const name of ['jawOpen','mouthOpen','mouthStretch','mouthCornerUp','mouthCornerDown','lipPress'])target[name]*=f.expressionScale*f.mouthExpressionScale;
              target.cheekRaise*=f.expressionScale;
              for(const name of Object.keys(channels)){target[name]=Math.max(limits[name][0],Math.min(limits[name][1],target[name]));spring(channels[name],target[name],dt,name.startsWith('eye')?12:10);}
            }
            face.updateExpressions(dt);
            for(const name of face.expressionNames)assert(face.weights[name].value===weights[name].value&&face.weights[name].velocity===weights[name].velocity,'Flattened preset changed '+expression+' weight trajectory for '+name);
            for(const name of Object.keys(channels))assert(face.channels[name].value===channels[name].value&&face.channels[name].velocity===channels[name].velocity,'Flattened preset changed '+expression+' channel trajectory for '+name);
          }
        }
        return {expressions:Object.keys(presets).length+1,channelChecks:Object.keys(channels).length,exact:true};
      }finally{u.dispose();}
    });
    test('Repeated expression targets do not wake settled facial springs',()=>{
      const u=make(1003,'SCOUT'),face=u.faceAnimator;
      try {
        face.setExpression('NEUTRAL');
        assert(face._expressionsSettled,'repeating the neutral target woke settled expression channels');
        face.setExpression('ALERT',.8);for(let i=0;i<12;i++)face.updateExpressions(.05);
        const before=JSON.stringify({state:face.state,intensity:face.intensity,weights:face.weights,targets:face.weightTargets,settled:face._expressionsSettled});
        face.setExpression('ALERT',.8);
        assert(JSON.stringify({state:face.state,intensity:face.intensity,weights:face.weights,targets:face.weightTargets,settled:face._expressionsSettled})===before,
          'repeated expression setter restarted existing spring progress');
        face.setExpressionWeight('ALERT',.45);for(let i=0;i<4;i++)face.updateExpressions(.05);
        const blend=JSON.stringify({weights:face.weights,targets:face.weightTargets,settled:face._expressionsSettled});
        face.setExpressionWeight('ALERT',.45);
        assert(JSON.stringify({weights:face.weights,targets:face.weightTargets,settled:face._expressionsSettled})===blend,'repeated blend target reset spring state');
        face.setEyesClosed(false);const settled=face._expressionsSettled;face.setEyesClosed(false);
        assert(face._expressionsSettled===settled,'repeated eyelid state changed expression scheduler state');
        return {sameExpressionIdempotent:true,sameBlendIdempotent:true,sameEyesStateIdempotent:true};
      } finally {u.dispose();}
    });
    test('Settled gaze springs sleep until their next attention or saccade event',()=>{
      const u=make(1003,'SCOUT'),face=u.faceAnimator,originalExp=Math.exp;
      try {
        assert(face._gazeSettled,'neutral gaze should start settled');
        let calls=0;Math.exp=function(value){calls++;return originalExp(value);};
        face.update(.01);
        assert(calls===0,'settled gaze performed spring exponentials before its next event');
        face.nextSaccade=face.saccadeTime+.005;
        face.update(.01);
        assert(calls>0&&!face._gazeSettled,'saccade event did not wake gaze springs');
        return {sleepingExpCalls:0,wokeOnSaccade:true};
      } finally {Math.exp=originalExp;u.dispose();}
    });
    test('Stable facial pose skips control application until blink, expression or gaze changes',()=>{
      const u=make(1003,'SCOUT'),face=u.faceAnimator;
      try{
        face._expressionsSettled=true;face._gazeSettled=true;face.nextBlink=1000;face.nextAttention=1000;face.nextSaccade=1000;
        let applies=0;const apply=face.apply.bind(face);face.apply=()=>{applies++;return apply();};
        assert(face.update(.01)===false,'stable face should report no pose application');
        assert(applies===0,'stable expression, blink and gaze should not rewrite facial bones');
        face.setEyesClosed(true);assert(face.update(.01)===true,'forced blink state should apply immediately');
        assert(applies===1,'forced eyelid change should apply once');
        assert(face.update(.01)===false,'stable forced-close state should sleep');
        assert(applies===1,'stable forced-close state rewrote facial bones');
        face.setEyesClosed(false);assert(face.update(.01)===true,'reopening forced eyelids should apply immediately');
        assert(applies===2,'forced eyelid release should be applied');
        face.setExpression('ALERT',.8);assert(face.update(.01)===true,'expression target should wake facial pose application');
        assert(applies===3,'new expression should apply');
        const byName=u.rig.byName;let lookups=0;
        u.rig.byName=new Proxy(byName,{get(target,key,receiver){lookups++;return Reflect.get(target,key,receiver);}});
        face.apply();assert(lookups===0,'facial hot application should use constructor-cached bone/rest bindings');
        u.rig.byName=byName;
        return {stableSkips:2,wakeEvents:3,boneNameLookups:lookups};
      }finally{u.dispose();}
    });
    test('Sleeping gaze preserves exact seeded attention, saccade and blink schedule',()=>{
      const a=make(1003,'SCOUT'),b=make(1003,'SCOUT'),sleeping=a.faceAnimator,reference=b.faceAnimator;
      try {
        for(let i=0;i<1200;i++){
          reference._gazeSettled=false;
          sleeping.update(.05);reference.update(.05);
          for(const key of ['attentionTime','nextAttention','saccadeTime','nextSaccade','nextBlink','blinkTime'])
            assert(sleeping[key]===reference[key],'sleep changed facial event timing: '+key+' at step '+i);
          assert(sleeping.random.state===reference.random.state,'sleep changed the seeded random stream at step '+i);
          for(const key of ['attentionTargetYaw','attentionTargetPitch','saccadeTargetYaw','saccadeTargetPitch'])
            assert(sleeping[key]===reference[key],'sleep changed a seeded gaze target: '+key+' at step '+i);
          for(const key of ['attentionYaw','attentionPitch','saccadeYaw','saccadePitch','gazeYaw','gazePitch'])
            assert(Math.abs(sleeping[key].value-reference[key].value)<2e-8&&Math.abs(sleeping[key].velocity-reference[key].velocity)<2e-8,
              'sleep changed settled gaze beyond numerical tolerance: '+key+' at step '+i);
        }
        return {steps:1200,eventTimesExact:true,randomStreamExact:true};
      } finally {a.dispose();b.dispose();}
    });
    test('Far visual cadence uses stable per-seed phases',()=>{
      const a=make(1003),b=make(2003);
      try {
        a.setVisualCadence(.1,.1);b.setVisualCadence(.1,.1);
        const phaseA=a._animationUpdateAccumulator/.1,phaseB=b._animationUpdateAccumulator/.1,facePhaseA=a._faceUpdateAccumulator/.1;
        assert(phaseA>0&&phaseA<1&&phaseB>0&&phaseB<1&&Math.abs(phaseA-phaseB)>.05,'Units did not receive distinct deterministic phases');
        const before=a._animationUpdateAccumulator;a.setVisualCadence(.1,.1);
        assert(a._animationUpdateAccumulator===before,'Reapplying the same cadence reset its phase');
        a.setVisualCadence(.05,.05);
        assert(Math.abs(a._animationUpdateAccumulator/.05-phaseA)<1e-12,'Cadence change lost normalized progress');
        return {animationPhases:[phaseA,phaseB],facePhaseA};
      } finally {a.dispose();b.dispose();}
    });
    test('Repeated animation state does not force an early distant pose update',()=>{
      const u=make(1003,'RIFLEMAN');
      try {
        u.setVisualCadence(.1,.1);u._animationUpdateAccumulator=.03;
        u.setState('RUN');
        assert(u._animationUpdateAccumulator===.03,'same state forced a premature pose update');
        u.setState('WALK');
        assert(u._animationUpdateAccumulator===.1,'actual state change did not force prompt pose evaluation');
        return {sameStatePreservesCadence:true,changedStateUpdatesImmediately:true};
      } finally {u.dispose();}
    });
    test('Sprint silhouette, hand relaxation and all loadouts',()=>{
      let maxIK=0,minBoot=Infinity,maxHipExcursion=0;
      const loadouts=Object.keys(R.InfantryLoadouts);
      for(const [n,loadout] of loadouts.entries()) {
        const u=make(n%2?2003:1003,loadout);
        try {
          const phase=.5,idx=u.rig.index['foreArm.L'];
          u.setLocomotion({crouch:0,speedMps:u.phenotype.runSpeed*.68});u.seek(phase);
          const jog=u.animator.current.q[idx].clone(),jogHand=point(u,'hand.L').y-point(u,'hips').y;
          u.setLocomotion({speedMps:u.phenotype.runSpeed});u.seek(phase);
          assert(angle(jog,u.animator.current.q[idx])>.2,'Sprint arm identical to jog');
          assert(point(u,'hand.L').y-point(u,'hips').y>jogHand+.02*u.phenotype.height,'Forward hand does not rise');
          const curl=u.model.mesh.morphTargetDictionary.handsRelax;
          assert(curl!==undefined&&u.model.mesh.morphTargetInfluences[curl]>.8,'Missing relaxed fingers');
          let lo=Infinity,hi=-Infinity;
          for(let i=0;i<40;i++) {
            u.seek(i/40);valid(u);
            const hip=point(u,'hips');lo=Math.min(lo,hip.y);hi=Math.max(hi,hip.y);
            maxIK=Math.max(maxIK,u.animator.metrics.maxIKError);
            minBoot=Math.min(minBoot,...u.animator.metrics.minBoot);
            for(const s of ['L','R'])assert(Math.abs(point(u,'hand.'+s).x)>.06*u.phenotype.height,'Hand crosses torso centre');
          }
          maxHipExcursion=Math.max(maxHipExcursion,(hi-lo)/u.phenotype.height);
          assert((hi-lo)/u.phenotype.height<.075,'Excessive vertical bounding');
          const identity=JSON.stringify(u.genome),savedPhase=u.animator.phase;
          u.setDetail('high');u.equip('hands','gloves_full');
          assert(u.animator.phase===savedPhase&&JSON.stringify(u.genome)===identity,'Gear/LOD reset animation or identity');
          assert(u.model.mesh.morphTargetInfluences[u.model.mesh.morphTargetDictionary.handsRelax]>.8,'Gear/LOD lost hand pose');
          u.animator.setState('PRONE',true);u.seek(.3);
          assert(u.model.mesh.morphTargetInfluences[u.model.mesh.morphTargetDictionary.handsRelax]===0,'Support hand remains curled');
          valid(u);
        }finally{u.dispose();}
      }
      assert(maxIK<.02,'Static sprint exceeds leg reach');
      return {loadouts:loadouts.length,maxIK,minBoot,maxHipExcursion};
    });
    test('Extreme body proportions and isolated hand morph',()=>{
      const samples=[];
      for(const extreme of [0,1]) {
        const base=R.InfantryGenome.create(63),overrides={heightGene:extreme,speedGene:extreme};
        for(const key of Object.keys(base.body))overrides['body.'+key]=extreme;
        const u=make(63,'HEAVY_SUPPORT',R.InfantryGenome.withOverrides(base,overrides));
        try {
          u.setLocomotion({crouch:0,speedMps:u.phenotype.runSpeed});
          for(let i=0;i<32;i++){u.seek(i/32);valid(u);assert(u.animator.metrics.maxIKError<.02,'Extreme body exceeds leg reach');}
          const mesh=u.model.mesh,delta=mesh.geometry.morphAttributes.position[mesh.morphTargetDictionary.handsRelax].array;
          const hands=new Set([...u.model.surface.tags['hand.L'],...u.model.surface.tags['hand.R']]);
          let changed=0;
          for(let i=0;i<delta.length/3;i++) {
            const d=Math.hypot(delta[3*i],delta[3*i+1],delta[3*i+2]);
            assert(Number.isFinite(d),'Invalid hand morph');
            if(d>1e-8){assert(hands.has(i),'Hand relaxation deforms another body part');changed++;}
          }
          assert(changed>100,'No finger deformation');samples.push({extreme,height:u.phenotype.height,changedVertices:changed});
        }finally{u.dispose();}
      }
      return samples;
    });
    test('Arm timing preserved at 30/60/120 Hz',()=>{
      const samples=[];
      for(const seed of [1003,2003])for(const hz of [30,60,120]) {
        const u=make(seed);
        try {
          u.setLocomotion({crouch:0,speedMps:u.phenotype.runSpeed});u.seek(.17);
          let error=0;
          for(let i=0;i<hz*3;i++) {
            u.step(1/hz,R.FlatSurface,true);
            if(i>hz)for(const name of ['upperArm.L','upperArm.R','foreArm.L','foreArm.R']) {
              const idx=u.rig.index[name];
              error=Math.max(error,angle(u.animator.current.q[idx],u.animator.targetPose.q[idx]));
            }
          }
          assert(error<.015,'Cyclic arm motion is attenuated or delayed');valid(u);
          samples.push({seed,hz,maxAngularError:error});
        }finally{u.dispose();}
      }
      return samples;
    });
    test('Acceleration, braking, posture and support transitions',()=>{
      const u=make(),a=u.animator;let maxArmJump=0;
      try {
        u.setLocomotion({crouch:0,speedMps:u.phenotype.walkSpeed});u.seek(.27);
        const saved=a.phase;
        u.setLocomotion({speedMps:u.phenotype.runSpeed});assert(a.phase===saved,'Speed change reset phase');
        const idx=u.rig.index['upperArm.L'];let previous=a.current.q[idx].clone();
        const step=()=>{u.step(1/60,R.FlatSurface,true);valid(u);maxArmJump=Math.max(maxArmJump,angle(previous,a.current.q[idx]));previous.copy(a.current.q[idx]);};
        for(let i=0;i<240;i++)step();
        assert(u.locomotion.sprintWeight>.99,'Never reaches sprint');
        u.setLocomotion({crouch:1});
        for(let i=0;i<240;i++)step();
        assert(u.locomotion.sprintWeight<.001&&u.locomotion.actualCrouch>.88,'Sprint persists in deep crouch');
        u.setLocomotion({crouch:0,speedMps:u.phenotype.runSpeed});for(let i=0;i<240;i++)step();
        u.setLocomotion({speedMps:0});for(let i=0;i<240;i++)step();
        assert(u.speed<.01&&a.current.handCurl<.001,'Hands do not relax at rest');
        const settled=a.phase;for(let i=0;i<60;i++)step();assert(Math.abs(a.phase-settled)<1e-7,'Gait never settles');
        for(const state of ['PRONE_MOVE','RUN','SITTING','RUN']){u.setState(state);for(let i=0;i<120;i++)step();}
        assert(maxArmJump<.4,'Arm pose snaps during transition');
        return {maxArmJump};
      }finally{u.dispose();}
    });
    test('World sprint contacts and recovery',()=>{
      const u=make(2003,'HEAVY_SUPPORT');
      try {
        u.setLocomotion({crouch:0,speedMps:u.phenotype.runSpeed});u.seek(.12);
        const a=u.animator,previous=[null,null];let lockedFrames=0,maxSlip=0,maxIK=0;
        for(let n=0;n<240;n++) {
          u.step(1/60,R.FlatSurface,false);valid(u);
          maxIK=Math.max(maxIK,a.metrics.maxIKError);
          for(let i=0;i<2;i++) {
            const c=a.footContacts[i];
            if(c.locked) {
              lockedFrames++;
              const sole=u.rig.byName['foot.'+(i===0?'L':'R')].localToWorld(c.local.clone());
              maxSlip=Math.max(maxSlip,Math.hypot(sole.x-c.point.x,sole.z-c.point.z));
              if(previous[i])assert(c.point.distanceTo(previous[i])<1e-6,'Planted foot reanchored');
              previous[i]=c.point.clone();
            }else previous[i]=null;
          }
        }
        assert(lockedFrames>60,'No sustained world contacts');
        assert(maxSlip<.02,'Planted foot slides');
        assert(maxIK<.002,'World sprint exceeds leg reach: '+maxIK.toFixed(4)+' m');
        return {lockedFrames,maxSlip,maxIK,reanchors:a.metrics.reanchors};
      }finally{u.dispose();}
    });
    return checks;
  }};
})();
