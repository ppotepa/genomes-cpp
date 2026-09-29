(function () {
  'use strict';

  const R = window.RTS;

  // Only hips and face controls translate after bind pose. All other bone
  // positions remain at rest; quaternion snapshots still include every bone
  // except the expression-only controls whose rotations never change.
  const FACE_POSITION_BONE = /^(lidUpper|lidLower|browInner|browOuter|cheek|mouthCorner)(\.|$)|^(mouthUpper|mouthLower)$/;
  const FACE_QUATERNION_BONE = /^(eye|jaw|browInner|browOuter)\./;
  const FIXED_QUATERNION_BONE = /^(cheek|mouthCorner)\.|^(mouthUpper|mouthLower)$/;

  function poseChannels(rig,mesh) {
    const positionIndices=[],quaternionIndices=[],facePositionIndices=[],faceQuaternionIndices=[];
    rig.bones.forEach((bone,index)=>{
      if(FACE_POSITION_BONE.test(bone.name))facePositionIndices.push(index);else positionIndices.push(index);
      if(FACE_QUATERNION_BONE.test(bone.name))faceQuaternionIndices.push(index);
      else if(!FIXED_QUATERNION_BONE.test(bone.name))quaternionIndices.push(index);
    });
    const morphTargetDictionary=mesh.morphTargetDictionary||{},bodyMorphIndices=[],faceMorphIndices=[];
    for(const name of Object.keys(morphTargetDictionary)){
      const index=morphTargetDictionary[name];
      if(name==='eyelidsClose'||name==='eyelidsArc'||name==='neckFlex')faceMorphIndices.push(index);
      else bodyMorphIndices.push(index);
    }
    return {positionIndices,quaternionIndices,facePositionIndices,faceQuaternionIndices,bodyMorphIndices,faceMorphIndices};
  }

  function cadencePhase(seed,salt){
    let h=(seed^salt)>>>0;h^=h>>>16;h=Math.imul(h,0x7feb352d);h^=h>>>15;h=Math.imul(h,0x846ca68b);h^=h>>>16;
    return (h>>>0)/4294967296;
  }

  function snapshot(rig,mesh,channels) {
    return {
      positionIndices:channels.positionIndices,
      quaternionIndices:channels.quaternionIndices,
      positions: channels.positionIndices.map(index=>rig.bones[index].position.clone()),
      quaternions: channels.quaternionIndices.map(index=>rig.bones[index].quaternion.clone()),
      position: new THREE.Vector3(),
      rotation: new THREE.Quaternion(),
      morphs:channels.bodyMorphIndices.map(index=>(mesh.morphTargetInfluences||[])[index]||0),morphIndices:channels.bodyMorphIndices
    };
  }

  function faceSnapshot(rig,mesh,channels,faceAnimator){
    const influences=mesh.morphTargetInfluences||[];
    return {
      positionIndices:channels.facePositionIndices,quaternionIndices:channels.faceQuaternionIndices,morphIndices:channels.faceMorphIndices,
      positions:channels.facePositionIndices.map(index=>rig.bones[index].position.clone()),
      quaternions:channels.faceQuaternionIndices.map(index=>rig.bones[index].quaternion.clone()),
      morphs:channels.faceMorphIndices.map(index=>influences[index]||0),
      attentionYaw:faceAnimator.attentionYaw?faceAnimator.attentionYaw.value:0,
      attentionPitch:faceAnimator.attentionPitch?faceAnimator.attentionPitch.value:0
    };
  }

  function copySnapshot(target, source) {
    target.position.copy(source.position);
    target.rotation.copy(source.rotation);
    for(let i=0;i<target.positions.length;i++)target.positions[i].copy(source.positions[i]);
    for(let i=0;i<target.quaternions.length;i++)target.quaternions[i].copy(source.quaternions[i]);
    for(let i=0;i<source.morphs.length;i++)target.morphs[i]=source.morphs[i];
  }

  function copyRootSnapshot(target, source) {
    const p=target.position,q=target.rotation,sp=source.position,sq=source.rotation;
    if(p.x!==sp.x||p.y!==sp.y||p.z!==sp.z)p.copy(sp);
    if(q.x!==sq.x||q.y!==sq.y||q.z!==sq.z||q.w!==sq.w)q.copy(sq);
  }
  function sameRootSnapshot(a,b){
    return a.position.x===b.position.x&&a.position.y===b.position.y&&a.position.z===b.position.z&&
      a.rotation.x===b.rotation.x&&a.rotation.y===b.rotation.y&&a.rotation.z===b.rotation.z&&a.rotation.w===b.rotation.w;
  }
  function rotateSnapshots(unit,face=false){
    if(face){const previous=unit.previousFace;unit.previousFace=unit.currentFace;unit.currentFace=previous;}
    else {const previous=unit.previous;unit.previous=unit.current;unit.current=previous;}
  }
  function copyFaceSnapshot(target,source){
    for(let i=0;i<target.positions.length;i++)target.positions[i].copy(source.positions[i]);
    for(let i=0;i<target.quaternions.length;i++)target.quaternions[i].copy(source.quaternions[i]);
    for(let i=0;i<source.morphs.length;i++)target.morphs[i]=source.morphs[i];
    target.attentionYaw=source.attentionYaw;target.attentionPitch=source.attentionPitch;
  }

  R.InfantryUnit = class InfantryUnit extends R.Unit {
    static async createAsync(options,yieldFrame,check=null,budgetMs=4){
      const {side,seed,detail='world',genome:providedGenome=null,equipment:providedEquipment=null,equipmentOptions={}}=options;
      const genome=providedGenome||R.InfantryGenome.create(seed),phenotype=R.InfantryGenome.express(genome);
      const equipment=providedEquipment||new R.Equipment({unitSeed:seed,slotSchema:R.EquipmentSlots,...equipmentOptions});
      const model=await R.InfantryFactory.createModelAsync(side,phenotype,detail,equipment,yieldFrame,check,budgetMs);
      try{return new R.InfantryUnit({...options,genome,equipment,_preparedModel:model,_preparedPhenotype:phenotype});}
      catch(error){model.dispose();throw error;}
    }

    constructor({ id, side, seed, detail = 'world', state = 'IDLE', genome = null, expression = 'NEUTRAL', expressionIntensity = 1, equipment=null, equipmentOptions={}, _preparedModel=null, _preparedPhenotype=null }) {
      super({id,side,seed,category:'NON_VEHICULAR',type:'INFANTRY',slotSchema:R.EquipmentSlots,equipment,equipmentOptions:{loadout:'RIFLEMAN',...equipmentOptions}});
      this.id = id;
      this.side = side;
      this.seed = seed;
      this.category = 'NON_VEHICULAR';
      this.type = 'INFANTRY';

      this.genome = genome || R.InfantryGenome.create(seed);
      this.phenotype = _preparedPhenotype||R.InfantryGenome.express(this.genome);

      this.model = _preparedModel||R.InfantryFactory.createModel(side, this.phenotype, detail, this.equipment);
      this.root = this.model.root;
      this.rig = this.model.rig;
      this.animator = new R.InfantryAnimator(this.model, this.genome, this.phenotype);
      this.animator.setState(state, true);
      this.locomotion = this.animator.locomotion;
      this.faceAnimator = new R.FaceAnimator(this.model, this.genome, this.seed);
      this.faceAnimator.setExpression(expression, expressionIntensity);
      this._physicsSchema = null;

      this.position = new THREE.Vector3();
      this.heading = 0;
      this.facingHeading = 0;
      this.facingVelocity = 0;
      this._facingExpDt=-1;this._facingExpOmega=NaN;this._facingExp=0;

      this.poseChannels=poseChannels(this.rig,this.model.mesh);
      const rigIndex=this.rig.index||{};
      this._headQuaternionSlot=this.poseChannels.quaternionIndices.indexOf(rigIndex.head);
      this._neckQuaternionSlot=this.poseChannels.quaternionIndices.indexOf(rigIndex.neck);
      this.previous = snapshot(this.rig,this.model.mesh,this.poseChannels);
      this.current = snapshot(this.rig,this.model.mesh,this.poseChannels);
      this.previousFace=faceSnapshot(this.rig,this.model.mesh,this.poseChannels,this.faceAnimator);
      this.currentFace=faceSnapshot(this.rig,this.model.mesh,this.poseChannels,this.faceAnimator);
      const morphCount=(this.model.mesh.morphTargetInfluences||[]).length;
      this._bodyMorphSlots=new Int32Array(morphCount);this._faceMorphSlots=new Int32Array(morphCount);
      this._bodyMorphSlots.fill(-1);this._faceMorphSlots.fill(-1);
      for(let i=0;i<this.poseChannels.bodyMorphIndices.length;i++)this._bodyMorphSlots[this.poseChannels.bodyMorphIndices[i]]=i;
      for(let i=0;i<this.poseChannels.faceMorphIndices.length;i++)this._faceMorphSlots[this.poseChannels.faceMorphIndices[i]]=i;
      this._rootInterpolationDirty=false;this._morphInterpolationDirty=false;
      this._dirtyMorphIndices=[];
      this._faceMorphInterpolationDirty=false;this._dirtyFaceMorphIndices=[];
      this._positionInterpolationDirty=new Uint8Array(this.poseChannels.positionIndices.length);
      this._quaternionInterpolationDirty=new Uint8Array(this.poseChannels.quaternionIndices.length);
      this._weaponQuaternionSide=new Uint8Array(this.poseChannels.quaternionIndices.length);
      this._facePositionInterpolationDirty=new Uint8Array(this.poseChannels.facePositionIndices.length);
      this._faceQuaternionInterpolationDirty=new Uint8Array(this.poseChannels.faceQuaternionIndices.length);
      this._dirtyPositionSlots=[];this._dirtyQuaternionSlots=[];
      this._dirtyFacePositionSlots=[];this._dirtyFaceQuaternionSlots=[];
      this._weaponQuaternionSlots=[];
      for(let i=0;i<this.poseChannels.quaternionIndices.length;i++){
        const name=this.rig.bones[this.poseChannels.quaternionIndices[i]].name;
        if(/^(upperArm|foreArm|hand)\.L$/.test(name)){this._weaponQuaternionSide[i]=1;this._weaponQuaternionSlots.push(i);}
        else if(/^(upperArm|foreArm|hand)\.R$/.test(name)){this._weaponQuaternionSide[i]=2;this._weaponQuaternionSlots.push(i);}
      }

      this.demo = null;
      this.speed = 0;
      this.totalDistance = 0;
      this.faceUpdateInterval=0;this._faceUpdateAccumulator=0;
      this.animationUpdateInterval=0;this._animationUpdateAccumulator=0;this._animationDistanceAccumulator=0;
      this._lastBodySample={valid:false,state:null,surface:null,treadmill:null,moveAngle:0,turnRate:0,turning:false,speed:0,distance:0};
      this._animationCadencePhase=cadencePhase(this.seed,0x91e10da5);this._faceCadencePhase=cadencePhase(this.seed,0x6d2b79f5);
      this._before = new THREE.Vector3();
      this._terrainHeightCache={terrain:null,heights:null,ix:-1,iz:-1,a:0,b:0,c:0,d:0};
      this._terrainHeightSample={surface:null,heights:null,x:NaN,z:NaN,y:NaN};
      this._weaponLook = new THREE.Vector3();
      this._weaponLookQ = new THREE.Quaternion();
      this._weaponLookEuler = new THREE.Euler();

      this._animContext = {
        surface: R.FlatSurface,
        distance: 0,
        treadmill: true,
        speed: 0,
        moveAngle: 0,
        turnRate: 0
      };

      this.weapons = new R.WeaponController(this);
      this.animator.weaponLayer = this.weapons;
      this.animator.update(0, this._animContext);
      this.faceAnimator.update(0);
      this.animator.finalizeAfterLook(this._animContext);
      if(this.faceAnimator.restoreHeadLayer)this.faceAnimator.restoreHeadLayer();
      this.weapons.apply(this._animContext, 0);
      this.capture();
      copySnapshot(this.previous, this.current);
      copyFaceSnapshot(this.previousFace,this.currentFace);
      this._rootInterpolationDirty=false;this._morphInterpolationDirty=false;
      this._positionInterpolationDirty.fill(0);this._quaternionInterpolationDirty.fill(0);
      this._facePositionInterpolationDirty.fill(0);this._faceQuaternionInterpolationDirty.fill(0);this._faceMorphInterpolationDirty=false;
      this._dirtyPositionSlots.length=this._dirtyQuaternionSlots.length=0;
      this._dirtyFacePositionSlots.length=this._dirtyFaceQuaternionSlots.length=0;
      this.weapons.syncSnapshots();
    }

    get physicsSchema() {
      if(!this._physicsSchema)this._physicsSchema=R.RagdollSchema.create(this.rig);
      return this._physicsSchema;
    }

    movementSpeed() {
      const p = this.phenotype;
      if (R.PostureProfile.isBiped(this.animator.state)) return this.locomotion.targetSpeedMps;

      if (this.animator.state === 'WALK') return p.walkSpeed;
      if (this.animator.state === 'RUN') return p.runSpeed;
      if (this.animator.state === 'PRONE_MOVE') return p.proneSpeed;
      if (this.animator.state === 'CROUCH_WALK') return p.crouchSpeed;
      return 0;
    }

    sampleTerrainHeight(surface,x,z){
      const sample=this._terrainHeightSample,heights=surface.heights||null,
        stable=surface===R.FlatSurface||!!(heights&&surface.getHeightAtCached);
      if(stable&&sample.surface===surface&&sample.heights===heights&&sample.x===x&&sample.z===z&&sample.y===this.position.y)return sample.y;
      const y=surface.getHeightAtCached?surface.getHeightAtCached(x,z,this._terrainHeightCache):surface.getHeightAt(x,z);
      if(stable){sample.surface=surface;sample.heights=heights;sample.x=x;sample.z=z;sample.y=y;}
      else sample.surface=null;
      return y;
    }

    setWorldPosition(x, z, surface) {
      this._terrainHeightCache.terrain=null;this._terrainHeightCache.heights=null;
      this._terrainHeightSample.surface=null;
      this._animationUpdateAccumulator=0;this._animationDistanceAccumulator=0;this._faceUpdateAccumulator=0;
      this.position.set(x, this.sampleTerrainHeight(surface,x,z), z);
      this.facingHeading = this.heading;
      this.facingVelocity = 0;

      this.root.position.copy(this.position);
      this.root.rotation.set(0, this.facingHeading, 0);
      this.rig.root.updateMatrixWorld(true);

      this.animator.releaseContacts();
      this._animContext.surface = surface;
      this._animContext.distance = 0;
      this._animContext.treadmill = !this.demo;
      this._animContext.speed = 0;
      this._animContext.moveAngle = 0;
      this._animContext.turnRate = 0;
      this._animContext.turning = false;
      this.animator.update(0, this._animContext);
      this.faceAnimator.update(0);
      this.animator.finalizeAfterLook(this._animContext);
      this.weapons.apply(this._animContext, 0);

      this.syncSnapshots();
    }

    setEquipment(equipment) {
      if(!(equipment instanceof R.Equipment))throw new TypeError('Oczekiwano Equipment.');
      const appearanceChanged=R.InfantryFactory.setEquipment(this.model,equipment);
      this.equipment=equipment;
      // An exact appearance-cache key means slots, colors, wear, fit and detail
      // are unchanged. Keep weapon instances and contact supports intact too.
      if(!appearanceChanged)return this.equipment;
      this.weapons.reconcile(equipment);
      this.animator.supportIndices=this.animator.buildSupports();
      this.animator.releaseContacts();
      return this.equipment;
    }
    equip(slot,id){return this.setEquipment(this.equipment.withSlot(slot,id));}
    setLoadout(loadout){return this.setEquipment(this.equipment.clone({loadout,overrides:{}}));}
    setEquipmentSeed(seed){return this.setEquipment(this.equipment.clone({seed,overrides:{}}));}

    setSeatAnchor(point) { this.animator.setSeatAnchor(point); }

    setState(state) {
      if(this.weapons&&!this.weapons.guardState(state))return;
      const changed=this.animator.setState(state);
      if(changed!==false&&this._lastBodySample)this._lastBodySample.valid=false;
      // State changes from UI/script should appear on the next fixed step,
      // even when the unit is currently using a distant animation cadence.
      if(changed!==false)this._animationUpdateAccumulator=Math.max(this._animationUpdateAccumulator,this.animationUpdateInterval);
    }

    setVisualCadence(animationInterval,faceInterval){
      // The game re-evaluates camera LOD periodically; unchanged profiles need
      // no normalization or cadence-phase remapping for this unit.
      if(Number.isFinite(animationInterval)&&animationInterval>=0&&animationInterval===this.animationUpdateInterval&&
        Number.isFinite(faceInterval)&&faceInterval>=0&&faceInterval===this.faceUpdateInterval)return;
      const animation=Math.max(0,Number(animationInterval)||0),face=Math.max(0,Number(faceInterval)||0);
      if(animation!==this.animationUpdateInterval){
        const previous=this.animationUpdateInterval,progress=previous>0?Math.min(1,this._animationUpdateAccumulator/previous):this._animationCadencePhase;
        this.animationUpdateInterval=animation;this._animationUpdateAccumulator=animation>0?progress*animation:0;
        // The simulation context is accumulated between visual pose samples.
        // Changing the cadence changes that sample's integration window.
        if(this._lastBodySample)this._lastBodySample.valid=false;
      }
      if(face!==this.faceUpdateInterval){
        const previous=this.faceUpdateInterval,progress=previous>0?Math.min(1,this._faceUpdateAccumulator/previous):this._faceCadencePhase;
        this.faceUpdateInterval=face;this._faceUpdateAccumulator=face>0?progress*face:0;
      }
    }

    applyWeaponLookTarget(){
      const old=this.faceAnimator.lookTargetWorld;
      if(this.weapons.occupiesHands&&this.weapons.readiness>.5){
        this._weaponLookEuler.set(-this.weapons.aimPitch,this.weapons.aimYaw,0,'YXZ');
        this._weaponLookQ.setFromEuler(this._weaponLookEuler);
        this._weaponLook.set(0,0,5).applyQuaternion(this._weaponLookQ);
        this.rig.modelPoint('head',this._before);this._weaponLook.add(this._before);
        this.root.localToWorld(this._weaponLook);
        this.faceAnimator.lookTargetWorld=this._weaponLook;
      }
      return old;
    }

    restoreBodySnapshot(snapshot=this.current){
      const dirtyPositionSlots=this._dirtyPositionSlots;
      for(let i=0;i<dirtyPositionSlots.length;i++){
        const slot=dirtyPositionSlots[i],position=this.rig.bones[snapshot.positionIndices[slot]].position,source=snapshot.positions[slot];
        if(position.x!==source.x||position.y!==source.y||position.z!==source.z)position.copy(source);
      }
      const dirtyQuaternionSlots=this._dirtyQuaternionSlots;
      for(let i=0;i<dirtyQuaternionSlots.length;i++){
        const slot=dirtyQuaternionSlots[i],quaternion=this.rig.bones[snapshot.quaternionIndices[slot]].quaternion,source=snapshot.quaternions[slot];
        if(quaternion.x!==source.x||quaternion.y!==source.y||quaternion.z!==source.z||quaternion.w!==source.w)quaternion.copy(source);
      }
      const weaponQuaternionSlots=this._weaponQuaternionSlots;
      for(let i=0;i<weaponQuaternionSlots.length;i++){
        const slot=weaponQuaternionSlots[i];if(this._quaternionInterpolationDirty[slot])continue;
        const quaternion=this.rig.bones[snapshot.quaternionIndices[slot]].quaternion,source=snapshot.quaternions[slot];
        if(quaternion.x!==source.x||quaternion.y!==source.y||quaternion.z!==source.z||quaternion.w!==source.w)quaternion.copy(source);
      }
      const values=this.model.mesh.morphTargetInfluences||[];
      const dirtyMorphs=this._dirtyMorphIndices;
      for(let i=0;i<dirtyMorphs.length;i++){
        const index=dirtyMorphs[i],slot=this._bodyMorphSlots[index],value=snapshot.morphs[slot];
        if(values[index]!==value)values[index]=value;
      }
    }

    setLocomotion(values) {
      if (!R.PostureProfile.isBiped(this.animator.state)) throw new Error('Wybierz najpierw postawę na stopach.');
      this.locomotion.set(values);
      return this.locomotion;
    }

    setExpression(expression, intensity = 1) {
      this.faceAnimator.setExpression(expression, intensity);
    }

    setExpressionWeight(expression, value) {
      this.faceAnimator.setExpressionWeight(expression, value);
    }

    setEyesClosed(value) {
      this.faceAnimator.setEyesClosed(value);
    }

    setLookTarget(target) {
      this.faceAnimator.setLookTargetWorld(target);
    }

    clearLookTarget() {
      this.faceAnimator.clearLookTarget();
    }

    setDemoPatrol(fromX, fromZ, toX, toZ, moveState = 'WALK', options = {}) {
      if(!R.GaitProfile.isMoving(moveState))throw new Error('Patrol wymaga stanu ruchu.');
      const length=Math.hypot(toX-fromX,toZ-fromZ);
      if(length<.1)throw new Error('Demo patrol wymaga odcinka dluzszego niz 0.1 m.');
      const direction=options.direction===-1?-1:1;
      const startT=R.Math.clamp(Number.isFinite(options.startT)?options.startT:.5,0,1);
      const minPause=Math.max(.35,Number.isFinite(options.minPause)?options.minPause:1.15);
      const maxPause=Math.max(minPause,Number.isFinite(options.maxPause)?options.maxPause:2.8);
      this.demo={
        type:'patrol',fromX,fromZ,toX,toZ,length,t:startT,direction,
        moveState,restState:R.GaitProfile.restState(moveState),minPause,maxPause,
        pause:Math.max(0,Number.isFinite(options.initialPause)?options.initialPause:0),
        random:new R.SeededRandom((this.seed^0x51f15e2d)>>>0)
      };
      const dx=(toX-fromX)*direction,dz=(toZ-fromZ)*direction;
      this.heading=Math.atan2(dx,dz);
      this.facingHeading=this.heading;
      this.facingVelocity=0;
      this.root.rotation.set(0,this.facingHeading,0);
      this.demo.resumeSpeed=R.PostureProfile.isBiped(moveState)?R.PostureProfile.preset(moveState,this.phenotype).speedMps:this.movementSpeed();
      if(this.demo.pause>0)this.setState(this.demo.restState);
      else this.setState(moveState);
    }

    demoPauseDuration(demo) {
      return demo.minPause+(demo.maxPause-demo.minPause)*demo.random.next();
    }

    springFacing(targetAngle, dt) {
      if (!(dt > 0)) {
        this.facingHeading = targetAngle;
        this.facingVelocity = 0;
        return;
      }
      if(targetAngle===this.facingHeading&&this.facingVelocity===0)return;

      const family=R.GaitProfile.restState(this.animator.state);
      const omega = family==='PRONE'?3.0:family==='CROUCH'?4.7:this.animator.state==='RUN'?9.0:7.2;
      const delta = R.Math.angle(targetAngle - this.facingHeading);
      const target = this.facingHeading + delta;
      const y = this.facingHeading - target;
      if(this._facingExpDt!==dt||this._facingExpOmega!==omega){this._facingExp=Math.exp(-omega*dt);this._facingExpDt=dt;this._facingExpOmega=omega;}
      const exp=this._facingExp;
      const temp = (this.facingVelocity + omega * y) * dt;

      this.facingHeading = target + (y + temp) * exp;
      this.facingVelocity = (this.facingVelocity - omega * temp) * exp;
      this.facingHeading = R.Math.angle(this.facingHeading);
    }

    step(dt, surface, treadmill = false) {
      this._animationUpdateAccumulator+=dt;
      const animate=this.animationUpdateInterval<=0||this._animationUpdateAccumulator+1e-9>=this.animationUpdateInterval||
        (this.weapons.trigger||this.weapons.pendingShot);
      this.weapons.beginStep(dt,animate);
      // Root motion stays on the fixed simulation clock. Bone snapshots only
      // advance when the visual pose is evaluated, so a distant unit can keep
      // moving smoothly without running its IK/contact solvers every step.
      copyRootSnapshot(this.previous, this.current);

      this._before.copy(this.position);
      let desiredHeading=this.heading;
      let patrolMoving=false;

      if(this.demo && this.demo.type==='patrol') {
        const d=this.demo;
        if(d.pause>0) {
          d.pause=Math.max(0,d.pause-dt);
          if(this.animator.state!==d.restState) {
            if(R.PostureProfile.isBiped(d.moveState)){this.animator.state=d.restState;this.locomotion.set({speedMps:0});}
            else this.setState(d.restState);
          }
          if(d.pause===0) {
            if(R.PostureProfile.isBiped(d.moveState)){this.animator.state=d.moveState;this.locomotion.set({speedMps:d.resumeSpeed});}
            else this.setState(d.moveState);
          }
        }
        if(this._lastBodySample&&this.animator.state!==this._lastBodySample.state)this._lastBodySample.valid=false;
        patrolMoving=d.pause===0;
      }

      if(R.PostureProfile.isBiped(this.animator.state)) this.locomotion.update(dt,this.speed);
      const family=R.GaitProfile.restState(this.demo?this.demo.moveState:this.animator.state);
      const accel=family==='PRONE'?.85:family==='CROUCH'?1.25:this.animator.state==='RUN'?4.5:2.8;
      const decel=family==='PRONE'?1.15:family==='CROUCH'?1.65:5.0;
      let targetSpeed=this.movementSpeed();
      if(this.demo && this.demo.type==='patrol') {
        const d=this.demo;
        const remaining=(d.direction>0?1-d.t:d.t)*d.length;
        targetSpeed=patrolMoving?Math.min(targetSpeed,Math.sqrt(2*decel*Math.max(0,remaining))):0;
      }
      if(this.animator.transition && !this.animator.transition.keepContacts)targetSpeed=0;
      const delta=targetSpeed-this.speed;
      this.speed+=Math.sign(delta)*Math.min(Math.abs(delta),(delta>0?accel:decel)*dt);

      if(this.demo && this.demo.type==='patrol' && patrolMoving) {
        const d=this.demo;
        d.t=R.Math.clamp(d.t+d.direction*this.speed*dt/d.length,0,1);
        this.position.x=R.Math.mix(d.fromX,d.toX,d.t);
        this.position.z=R.Math.mix(d.fromZ,d.toZ,d.t);
        const remaining=(d.direction>0?1-d.t:d.t)*d.length;
        if(remaining<.004) {
          d.t=d.direction>0?1:0;
          this.position.x=R.Math.mix(d.fromX,d.toX,d.t);
          this.position.z=R.Math.mix(d.fromZ,d.toZ,d.t);
          d.direction*=-1;
          this.heading=Math.atan2((d.toX-d.fromX)*d.direction,(d.toZ-d.fromZ)*d.direction);
          desiredHeading=this.heading;
          d.pause=this.demoPauseDuration(d);
          if(R.PostureProfile.isBiped(d.moveState)) {
            d.resumeSpeed=this.locomotion.requestedSpeedMps;this.animator.state=d.restState;this.locomotion.set({speedMps:0});
          } else this.setState(d.restState);
          this.speed=0;
          if(this._lastBodySample)this._lastBodySample.valid=false;
          this.animator.releaseContacts();
        }
      } else if(this.demo && this.demo.type!=='patrol' && this.speed>0) {
        const d=this.demo;
        d.angle+=this.speed*dt/d.radius;
        this.position.x=d.x+Math.cos(d.angle)*d.radius;
        this.position.z=d.z+Math.sin(d.angle)*d.radius;
        desiredHeading=Math.atan2(-Math.sin(d.angle),Math.cos(d.angle));
        this.heading=desiredHeading;
      } else if(!treadmill && !this.demo && this.speed>0) {
        this.position.x+=Math.sin(this.heading)*this.speed*dt;
        this.position.z+=Math.cos(this.heading)*this.speed*dt;
      }

      this.springFacing(desiredHeading, dt);

      this.position.y = this.sampleTerrainHeight(surface,this.position.x,this.position.z);
      const rootPosition=this.root.position;
      if(rootPosition.x!==this.position.x||rootPosition.y!==this.position.y||rootPosition.z!==this.position.z)rootPosition.copy(this.position);
      const rootRotation=this.root.rotation;
      if(rootRotation.x!==0||rootRotation.y!==this.facingHeading||rootRotation.z!==0)rootRotation.set(0,this.facingHeading,0);

      const dx = this.position.x - this._before.x;
      const dz = this.position.z - this._before.z;
      const distance = treadmill ? this.speed * dt : (dx===0&&dz===0?0:Math.hypot(dx, dz));
      this.totalDistance += distance;
      // Face-only clearance correction may invoke foot/hand IK. Keep its
      // surface and instantaneous movement context current even when the body
      // pose is on a slower visual cadence; only wrapped heading work waits.
      this._animContext.surface=surface;
      this._animContext.distance=distance;
      this._animContext.treadmill=treadmill;
      this._animContext.speed=dt>0?distance/dt:this.speed;
      this._animContext.turnRate=this.facingVelocity;

      this._faceUpdateAccumulator+=dt;
      this._animationDistanceAccumulator+=distance;
      const faceDue=this.faceUpdateInterval<=0||this._faceUpdateAccumulator+1e-9>=this.faceUpdateInterval;
      if(animate){
        // Rendering may leave its interpolated head layer on the rig. Restore
        // it only when this simulation sample will read the base body pose.
        this.faceAnimator.restoreHeadLayer();
        const visualDt=this._animationUpdateAccumulator;
        const turnError=R.Math.angle(desiredHeading-this.facingHeading);
        const moveAngle=this.speed>0?turnError:0;
        const turning=Math.abs(turnError)>.16;
        this._animContext.distance=this._animationDistanceAccumulator;
        this._animContext.treadmill=treadmill;
        this._animContext.speed=visualDt>0?this._animationDistanceAccumulator/visualDt:this.speed;
        this._animContext.turning=turning;
        this._animContext.moveAngle=moveAngle;
        this._animContext.turnRate=this.facingVelocity;
        // Keep the last completed pose as the interpolation baseline and
        // write the new sample into the other preallocated snapshot buffer.
        rotateSnapshots(this);
        this.animator.update(visualDt,this._animContext);
        if(faceDue){
          rotateSnapshots(this,true);
          const oldLook=this.applyWeaponLookTarget();
          this.faceAnimator.update(this._faceUpdateAccumulator);this._faceUpdateAccumulator=0;
          this.faceAnimator.lookTargetWorld=oldLook;
        }else this.faceAnimator.applyInterpolatedHeadLayer(this.faceAnimator.attentionYaw.value,this.faceAnimator.attentionPitch.value);
        this.animator.finalizeAfterLook(this._animContext);
        this.faceAnimator.restoreHeadLayer();
        this.weapons.apply(this._animContext,visualDt);
        this.weapons.capture();
        this._animationUpdateAccumulator=0;this._animationDistanceAccumulator=0;
      }else if(faceDue){
        rotateSnapshots(this,true);
        // Face-only cadence starts from the last body simulation pose, never
        // from whichever interpolated pose the renderer left on the skeleton.
        this.faceAnimator.restoreHeadLayer();this.restoreBodySnapshot();
        const oldLook=this.applyWeaponLookTarget();
        const facePoseChanged=this.faceAnimator.update(this._faceUpdateAccumulator);this._faceUpdateAccumulator=0;
        this.faceAnimator.lookTargetWorld=oldLook;
        if(facePoseChanged){
          this.animator.finalizeAfterLook(this._animContext,true);
          this.faceAnimator.restoreHeadLayer();
          this.captureBodyPose();
        }
      }else{
        // No pose phase reads the rig on this tick. Leave the render
        // composition in place until the next pose sample or render pass.
      }
      // Each snapshot stream has its own visual cadence. Face-only samples
      // capture their body baseline above; this common finalizer captures the
      // face stream once. Body-only samples leave it intact.
      // When neither stream advanced, capturing hundreds of unchanged bone
      // and morph channels only repeats work already done at the last sample.
      if(animate)this.captureBodyPose();
      if(faceDue&&this.currentFace)this.captureFacePose();
      this.captureRoot();
    }

    syncSnapshots() {
      if(this.faceAnimator.restoreHeadLayer)this.faceAnimator.restoreHeadLayer();
      this.capture();
      copySnapshot(this.previous, this.current);
      copyFaceSnapshot(this.previousFace,this.currentFace);
      this._rootInterpolationDirty=false;this._morphInterpolationDirty=false;
      this._positionInterpolationDirty.fill(0);this._quaternionInterpolationDirty.fill(0);
      this._facePositionInterpolationDirty.fill(0);this._faceQuaternionInterpolationDirty.fill(0);this._faceMorphInterpolationDirty=false;
      this._dirtyPositionSlots.length=this._dirtyQuaternionSlots.length=0;
      this._dirtyFacePositionSlots.length=this._dirtyFaceQuaternionSlots.length=0;
      this.weapons.syncSnapshots();
    }

    captureRoot(){
      const out = this.current;
      const rootPosition=this.root.position,rootRotation=this.root.quaternion;
      if(out.position.x!==rootPosition.x||out.position.y!==rootPosition.y||out.position.z!==rootPosition.z)out.position.copy(rootPosition);
      if(out.rotation.x!==rootRotation.x||out.rotation.y!==rootRotation.y||out.rotation.z!==rootRotation.z||out.rotation.w!==rootRotation.w)out.rotation.copy(rootRotation);
      this._rootInterpolationDirty=!sameRootSnapshot(this.previous,out);
    }

    captureBodyPose(){
      const out=this.current;
      const dirtyPositionSlots=this._dirtyPositionSlots;dirtyPositionSlots.length=0;
      const dirtyQuaternionSlots=this._dirtyQuaternionSlots;dirtyQuaternionSlots.length=0;
      this._morphInterpolationDirty=false;
      const dirtyMorphs=this._dirtyMorphIndices;dirtyMorphs.length=0;
      const influences=this.model.mesh.morphTargetInfluences||[];
      const bodyMorphIndices=out.morphIndices;
      for(let i=0;i<bodyMorphIndices.length;i++){
        const index=bodyMorphIndices[i],value=influences[index]||0;if(out.morphs[i]!==value)out.morphs[i]=value;
        if(this.previous.morphs[i]!==value){this._morphInterpolationDirty=true;dirtyMorphs.push(index);}
      }
      for(let i=0;i<out.positionIndices.length;i++){
        const value=this.rig.bones[out.positionIndices[i]].position,previous=this.previous.positions[i];
        const dirty=previous.x!==value.x||previous.y!==value.y||previous.z!==value.z;
        this._positionInterpolationDirty[i]=dirty?1:0;if(dirty)dirtyPositionSlots.push(i);
        const current=out.positions[i];if(current.x!==value.x||current.y!==value.y||current.z!==value.z)current.copy(value);
      }
      for(let i=0;i<out.quaternionIndices.length;i++){
        const value=this.rig.bones[out.quaternionIndices[i]].quaternion,previous=this.previous.quaternions[i];
        const dirty=previous.x!==value.x||previous.y!==value.y||previous.z!==value.z||previous.w!==value.w;
        this._quaternionInterpolationDirty[i]=dirty?1:0;if(dirty)dirtyQuaternionSlots.push(i);
        const current=out.quaternions[i];if(current.x!==value.x||current.y!==value.y||current.z!==value.z||current.w!==value.w)current.copy(value);
      }
    }

    captureFacePose(){
      const out=this.currentFace,previous=this.previousFace,influences=this.model.mesh.morphTargetInfluences||[];
      const dirtyPositionSlots=this._dirtyFacePositionSlots;dirtyPositionSlots.length=0;
      const dirtyQuaternionSlots=this._dirtyFaceQuaternionSlots;dirtyQuaternionSlots.length=0;
      this._faceMorphInterpolationDirty=false;this._dirtyFaceMorphIndices.length=0;
      for(let i=0;i<out.positionIndices.length;i++){
        const value=this.rig.bones[out.positionIndices[i]].position,prev=previous.positions[i];
        const dirty=prev.x!==value.x||prev.y!==value.y||prev.z!==value.z;
        this._facePositionInterpolationDirty[i]=dirty?1:0;if(dirty)dirtyPositionSlots.push(i);
        const current=out.positions[i];if(current.x!==value.x||current.y!==value.y||current.z!==value.z)current.copy(value);
      }
      for(let i=0;i<out.quaternionIndices.length;i++){
        const value=this.rig.bones[out.quaternionIndices[i]].quaternion,prev=previous.quaternions[i];
        const dirty=prev.x!==value.x||prev.y!==value.y||prev.z!==value.z||prev.w!==value.w;
        this._faceQuaternionInterpolationDirty[i]=dirty?1:0;if(dirty)dirtyQuaternionSlots.push(i);
        const current=out.quaternions[i];if(current.x!==value.x||current.y!==value.y||current.z!==value.z||current.w!==value.w)current.copy(value);
      }
      for(let i=0;i<out.morphIndices.length;i++){
        const index=out.morphIndices[i],value=influences[index]||0;if(out.morphs[i]!==value)out.morphs[i]=value;
        if(previous.morphs[i]!==value){this._faceMorphInterpolationDirty=true;this._dirtyFaceMorphIndices.push(index);}
      }
      out.attentionYaw=this.faceAnimator.attentionYaw?this.faceAnimator.attentionYaw.value:0;
      out.attentionPitch=this.faceAnimator.attentionPitch?this.faceAnimator.attentionPitch.value:0;
    }

    capture(capturePose=true){
      if(capturePose&&this.weapons)this.weapons.capture();
      this.captureRoot();
      if(capturePose){this.captureBodyPose();if(this.currentFace)this.captureFacePose();}
    }

    render(alpha) {
      if(this.faceAnimator)this.faceAnimator.restoreHeadLayer();
      if(this._rootInterpolationDirty){
        this.root.position.copy(this.previous.position).lerp(this.current.position, alpha);
        this.root.quaternion.copy(this.previous.rotation).slerp(this.current.rotation, alpha);
      }

      const dirtyPositionSlots=this._dirtyPositionSlots;
      for(let i=0;i<dirtyPositionSlots.length;i++){
        const slot=dirtyPositionSlots[i];
        const index=this.previous.positionIndices[slot];
        this.rig.bones[index].position.copy(this.previous.positions[slot]).lerp(this.current.positions[slot],alpha);
      }
      const solveGrip=this.animationUpdateInterval<1/15;
      const weaponArmMask=this.weapons.renderArmMask?this.weapons.renderArmMask(solveGrip):0;
      const dirtyQuaternionSlots=this._dirtyQuaternionSlots;
      for(let i=0;i<dirtyQuaternionSlots.length;i++){
        const slot=dirtyQuaternionSlots[i];
        const index=this.previous.quaternionIndices[slot];
        this.rig.bones[index].quaternion.copy(this.previous.quaternions[slot]).slerp(this.current.quaternions[slot],alpha);
      }
      const weaponQuaternionSlots=this._weaponQuaternionSlots;
      for(let i=0;i<weaponQuaternionSlots.length;i++){
        const slot=weaponQuaternionSlots[i];
        if(!(this._weaponQuaternionSide[slot]&weaponArmMask)||this._quaternionInterpolationDirty[slot])continue;
        const index=this.previous.quaternionIndices[slot];
        this.rig.bones[index].quaternion.copy(this.previous.quaternions[slot]).slerp(this.current.quaternions[slot],alpha);
      }
      if(this._morphInterpolationDirty){
        const influences=this.model.mesh.morphTargetInfluences||[];
        const dirtyMorphs=this._dirtyMorphIndices;
        for(let i=0;i<dirtyMorphs.length;i++){
          const index=dirtyMorphs[i],slot=this._bodyMorphSlots[index];
          if(slot>=0)influences[index]=R.Math.mix(this.previous.morphs[slot]||0,this.current.morphs[slot]||0,alpha);
        }
      }
      const faceAlpha=this.faceUpdateInterval>0?Math.min(1,this._faceUpdateAccumulator/this.faceUpdateInterval):alpha;
      if(this.previousFace&&this.currentFace&&this.faceAnimator){
      const dirtyFacePositionSlots=this._dirtyFacePositionSlots;
      for(let i=0;i<dirtyFacePositionSlots.length;i++){
        const slot=dirtyFacePositionSlots[i];
        const index=this.previousFace.positionIndices[slot];
        this.rig.bones[index].position.copy(this.previousFace.positions[slot]).lerp(this.currentFace.positions[slot],faceAlpha);
      }
      const dirtyFaceQuaternionSlots=this._dirtyFaceQuaternionSlots;
      for(let i=0;i<dirtyFaceQuaternionSlots.length;i++){
        const slot=dirtyFaceQuaternionSlots[i];
        const index=this.previousFace.quaternionIndices[slot];
        this.rig.bones[index].quaternion.copy(this.previousFace.quaternions[slot]).slerp(this.currentFace.quaternions[slot],faceAlpha);
      }
      if(this._faceMorphInterpolationDirty){
        const influences=this.model.mesh.morphTargetInfluences||[];
        for(const index of this._dirtyFaceMorphIndices){
          const slot=this._faceMorphSlots[index];
          if(slot>=0)influences[index]=R.Math.mix(this.previousFace.morphs[slot]||0,this.currentFace.morphs[slot]||0,faceAlpha);
        }
      }
      const yaw=R.Math.mix(this.previousFace.attentionYaw,this.currentFace.attentionYaw,faceAlpha);
      const pitch=R.Math.mix(this.previousFace.attentionPitch,this.currentFace.attentionPitch,faceAlpha);
      if(this.faceAnimator.applyInterpolatedHeadLayer){
        const stableHeadPose=this._headQuaternionSlot>=0&&this._neckQuaternionSlot>=0&&
          !this._quaternionInterpolationDirty[this._headQuaternionSlot]&&!this._quaternionInterpolationDirty[this._neckQuaternionSlot];
        this.faceAnimator.applyInterpolatedHeadLayer(yaw,pitch,stableHeadPose);
      }
      }
      // The renderer updates visible scene descendants after interpolation.
      // Weapon grip queries refresh their own ancestor/arm paths, so traversing
      // the whole rig here only to prepare that render pass is redundant.
      this.weapons.render(alpha,solveGrip);
    }

    seek(phase, surface = R.FlatSurface) {
      this._animationUpdateAccumulator=0;this._animationDistanceAccumulator=0;this._faceUpdateAccumulator=0;
      this.animator.transition = null;
      this.animator.phase = R.Math.clamp(phase, 0, 0.999999);
      this.animator.releaseContacts();

      this.root.position.copy(this.position);
      this.root.rotation.set(0, this.facingHeading, 0);

      this._animContext.surface = surface;
      this._animContext.distance = 0;
      this._animContext.treadmill = true;
      if(R.PostureProfile.isBiped(this.animator.state)){this.locomotion.snap();this.speed=this.locomotion.actualSpeedMps;}
      this._animContext.speed = this.movementSpeed();
      this._animContext.moveAngle = 0;
      this._animContext.turnRate = 0;
      this._animContext.turning = false;

      this.animator.update(0, this._animContext);
      this.faceAnimator.update(0);
      this.animator.finalizeAfterLook(this._animContext);
      if(this.faceAnimator.restoreHeadLayer)this.faceAnimator.restoreHeadLayer();
      this.weapons.apply(this._animContext, 0);
      this.capture();
      copySnapshot(this.previous, this.current);
      copyFaceSnapshot(this.previousFace,this.currentFace);
      this._rootInterpolationDirty=false;this._morphInterpolationDirty=false;
      this._positionInterpolationDirty.fill(0);this._quaternionInterpolationDirty.fill(0);
      this._facePositionInterpolationDirty.fill(0);this._faceQuaternionInterpolationDirty.fill(0);this._faceMorphInterpolationDirty=false;
      this._dirtyPositionSlots.length=this._dirtyQuaternionSlots.length=0;
      this._dirtyFacePositionSlots.length=this._dirtyFaceQuaternionSlots.length=0;
      this.weapons.syncSnapshots();
    }

    setDetail(detail) {
      if(R.InfantryFactory.setDetail(this.model,detail)){this.animator.supportIndices=this.animator.buildSupports();this.animator.releaseContacts();}
    }

    async setDetailAsync(detail,yieldFrame,check=null,budgetMs=4){
      const changed=await R.InfantryFactory.setDetailAsync(this.model,detail,yieldFrame,check,budgetMs);
      if(changed){this.animator.supportIndices=this.animator.buildSupports();this.animator.releaseContacts();}
      return changed;
    }

    dispose() {
      this.weapons.dispose();
      this.side.removeUnit(this);
      this.model.dispose();
    }
  };
})();
