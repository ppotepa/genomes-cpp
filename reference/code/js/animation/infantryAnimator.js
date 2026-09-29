(function () {
  'use strict';

  const R = window.RTS;
  const M = R.Math;
  const Gait = R.GaitProfile;
  const Profile = R.PostureProfile;
  let animationStates=null;
  const TAU = Math.PI * 2;
  const ARM_BONE = /^(clavicle|upperArm|foreArm|hand)\./;
  const PAIR_FIELDS = ['footLift','footPlant','footPitch','toePitch','support','footYaw','footRelative','anklePitch','ankleYaw','handPlant','handLift'];
  const POSE_VECTOR_FIELDS=['feet','hands','knees','elbows'];
  const POSE_SCALAR_FIELDS=['prone','handIK','handCurl','gait'];
  const DAMPED_VECTOR_FIELDS=['feet','hands','knees','elbows'];
  const V = (x = 0, y = 0, z = 0) => new THREE.Vector3(x, y, z);
  const SUPPORT_CACHE = new WeakMap();
  const CLEARANCE_PARTITION_CACHE = new WeakMap();
  const BROADPHASE_MARGIN = .002;

  function refreshBoneSubtree(bone,root){
    if(bone.updateWorldMatrix)bone.updateWorldMatrix(true,true);else root.updateMatrixWorld(true);
  }

  function clearancePartition(mesh, indices) {
    if(!mesh||!mesh.geometry||!indices||!indices.length)return null;
    const geometry=mesh.geometry;
    let byIndices=CLEARANCE_PARTITION_CACHE.get(geometry);
    if(!byIndices){byIndices=new WeakMap();CLEARANCE_PARTITION_CACHE.set(geometry,byIndices);}
    const cached=byIndices.get(indices);if(cached)return cached;
    const position=geometry.attributes.position,skinIndex=geometry.attributes.skinIndex,skinWeight=geometry.attributes.skinWeight;
    if(!position||!skinIndex||!skinWeight||!mesh.skeleton){byIndices.set(indices,null);return null;}
    const morphs=geometry.morphAttributes.position||[],groupsByBone=new Map(),fallback=[],point=new THREE.Vector3();
    for(const vertex of indices){
      const offset=vertex*4;let bone=-1,rigid=true;
      for(let k=0;k<4;k++){
        const weight=skinWeight.array[offset+k];if(weight===0)continue;
        if(weight===1&&bone<0)bone=skinIndex.array[offset+k];else{rigid=false;break;}
      }
      if(bone<0||!rigid){fallback.push(vertex);continue;}
      let morphed=false;
      for(const morph of morphs){
        const o=vertex*3,a=morph.array;
        if(a[o]!==0||a[o+1]!==0||a[o+2]!==0){morphed=true;break;}
      }
      if(morphed&&!geometry.morphTargetsRelative){fallback.push(vertex);continue;}
      let group=groupsByBone.get(bone);
      if(!group){group={bone,box:new THREE.Box3().makeEmpty(),poseBox:new THREE.Box3(),transform:new THREE.Matrix4(),transformReady:false,
        morphSnapshot:morphs.length?new Float64Array(morphs.length):null,indices:[],morphedIndices:[],lowerBound:0};
        if(group.morphSnapshot)group.morphSnapshot.fill(NaN);groupsByBone.set(bone,group);}
      group.box.expandByPoint(point.fromArray(position.array,vertex*3));
      group.indices.push(vertex);
      if(morphed)group.morphedIndices.push(vertex);
    }
      const result={groups:Array.from(groupsByBone.values()),fallback,poseRevision:-1,poseModel:null};
    byIndices.set(indices,result);return result;
  }

  function terrainCellHeight(a,d,b,c,u,v){
    return u+v<=1?a+(d-a)*u+(b-a)*v:c+(b-c)*(1-u)+(d-c)*(1-v);
  }

  function terrainMaximum(surface,minX,minZ,maxX,maxZ) {
    if(surface===R.FlatSurface)return 0;
    if(!R.TerrainSystem||!(surface instanceof R.TerrainSystem))return null;
    const heights=surface&&surface.heights,N=surface&&surface.segments,mapSize=surface&&surface.mapSize;
    if(!heights||!(N>0)||!(mapSize>0))return null;
    const step=mapSize/N,half=mapSize*.5;
    const fx0=M.clamp((minX+half)/step,0,N),fx1=M.clamp((maxX+half)/step,0,N);
    const fz0=M.clamp((minZ+half)/step,0,N),fz1=M.clamp((maxZ+half)/step,0,N);
    const cellX0=Math.min(N-1,Math.floor(fx0)),cellX1=Math.min(N-1,Math.floor(fx1));
    const cellZ0=Math.min(N-1,Math.floor(fz0)),cellZ1=Math.min(N-1,Math.floor(fz1));
    // Inside one grid cell, height is piecewise affine over the two indexed
    // triangles. Its exact rectangular maximum lies at a rectangle corner or
    // where the rectangle edge crosses the shared diagonal.
    if(cellX0===cellX1&&cellZ0===cellZ1){
      const x=cellX0,z=cellZ0,k=z*(N+1)+x,a=heights[k],d=heights[k+1],b=heights[k+N+1],c=heights[k+N+2];
      const u0=fx0-x,u1=fx1-x,v0=fz0-z,v1=fz1-z;
      let maximum=Math.max(terrainCellHeight(a,d,b,c,u0,v0),terrainCellHeight(a,d,b,c,u1,v0),
        terrainCellHeight(a,d,b,c,u0,v1),terrainCellHeight(a,d,b,c,u1,v1));
      let v=1-u0;if(v>=v0&&v<=v1)maximum=Math.max(maximum,d+(b-d)*v);
      v=1-u1;if(v>=v0&&v<=v1)maximum=Math.max(maximum,d+(b-d)*v);
      let u=1-v0;if(u>=u0&&u<=u1)maximum=Math.max(maximum,d+(b-d)*v0);
      u=1-v1;if(u>=u0&&u<=u1)maximum=Math.max(maximum,d+(b-d)*v1);
      return maximum;
    }
    const x0=Math.max(0,Math.min(N-1,Math.floor(fx0)-1));
    const x1=Math.max(0,Math.min(N-1,Math.floor(fx1)+1));
    const z0=Math.max(0,Math.min(N-1,Math.floor(fz0)-1));
    const z1=Math.max(0,Math.min(N-1,Math.floor(fz1)+1));
    let maximum=-Infinity;
    for(let z=z0;z<=z1+1;z++)for(let x=x0;x<=x1+1;x++)maximum=Math.max(maximum,heights[z*(N+1)+x]);
    return maximum;
  }

  function supportCacheFor(model, anatomy) {
    const surfaceGeometry = model.surface.geometry;
    let entry = SUPPORT_CACHE.get(surfaceGeometry);
    if (!entry) {
      entry = { withGear: new WeakMap(), withoutGear: new Map() };
      SUPPORT_CACHE.set(surfaceGeometry, entry);
    }
    const gearGeometry = model.gear && model.gear.geometry;
    let byAnatomy;
    if (gearGeometry) {
      byAnatomy = entry.withGear.get(gearGeometry);
      if (!byAnatomy) {
        byAnatomy = new Map();
        entry.withGear.set(gearGeometry, byAnatomy);
      }
    } else {
      byAnatomy = entry.withoutGear;
    }
    const p = anatomy.points;
    const anatomyKey = [anatomy.height, anatomy.ankleHeight,
      p['foot.L'].x, p['foot.L'].y, p['foot.L'].z,
      p['foot.R'].x, p['foot.R'].y, p['foot.R'].z,
      p['hand.L'].x, p['hand.L'].y, p['hand.L'].z,
      p['hand.R'].x, p['hand.R'].y, p['hand.R'].z].join(',');
    return { byAnatomy, anatomyKey };
  }

  function freezeSupports(supports) {
    for (const key of Object.keys(supports)) {
      const value = supports[key];
      for (const item of value) if (item && typeof item === 'object') Object.freeze(item);
      Object.freeze(value);
    }
    return Object.freeze(supports);
  }

  class Pose {
    constructor(rig) {
      this.rig = rig;
      this.q = rig.bones.map(() => new THREE.Quaternion());
      this.hips = V();
      this.feet = [V(), V()];
      this.hands = [V(), V()];
      this.knees = [V(), V()];
      this.elbows = [V(), V()];

      this.prone = 0;
      this.handIK = 0;
      this.handCurl = 0;
      this.gait = 0;
      this.footLift = [0, 0];
      this.footPlant = [0, 0];
      this.footPitch = [0, 0];
      this.toePitch = [0, 0];
      this.support = [0, 0];
      for (const key of PAIR_FIELDS) if (!this[key]) this[key] = [0, 0];

      this._e = new THREE.Euler();
    }

    clear() {
      for(let i=0;i<this.q.length;i++){
        const q=this.q[i];if(q.x!==0||q.y!==0||q.z!==0||q.w!==1)q.identity();
      }
      const A=this.rig.anatomy,H=A.height;
      if(this.hips.x!==0||this.hips.y!==A.hipY||this.hips.z!==0)this.hips.set(0,A.hipY,0);
      if(this.prone!==0)this.prone=0;
      if(this.handIK!==0)this.handIK=0;
      if(this.handCurl!==0)this.handCurl=0;
      if(this.gait!==0)this.gait=0;
      const invH=1/H;

      for (let i = 0; i < 2; i++) {
        const sign = i === 0 ? 1 : -1;
        const side=i===0?'L':'R',foot=A.points['foot.'+side],hand=A.points['hand.'+side],elbow=A.points['foreArm.'+side];
        let value=this.feet[i],x=foot.x*invH,y=foot.y*invH,z=foot.z*invH;
        if(value.x!==x||value.y!==y||value.z!==z)value.set(x,y,z);
        value=this.knees[i];x=sign*(A.hipHalf+.020);y=A.points['shin.L'].y/H;z=A.legLength/H*.96;
        if(value.x!==x||value.y!==y||value.z!==z)value.set(x,y,z);
        value=this.hands[i];x=hand.x*invH;y=hand.y*invH;z=hand.z*invH;
        if(value.x!==x||value.y!==y||value.z!==z)value.set(x,y,z);
        value=this.elbows[i];x=elbow.x*invH;y=elbow.y*invH;z=elbow.z*invH;
        if(value.x!==x||value.y!==y||value.z!==z)value.set(x,y,z);
        for (const key of PAIR_FIELDS) {
          const target=key==='footPlant'?1:0;if(this[key][i]!==target)this[key][i]=target;
        }
      }

      return this;
    }

    rot(name, x = 0, y = 0, z = 0) {
      this.q[this.rig.index[name]].setFromEuler(this._e.set(x, y, z, 'XYZ'));
    }

    copy(pose) {
      this.hips.copy(pose.hips);
      for(let i=0;i<this.q.length;i++)this.q[i].copy(pose.q[i]);

      for (const key of POSE_VECTOR_FIELDS) {
        const dst=this[key],src=pose[key];
        for(let i=0;i<dst.length;i++)dst[i].copy(src[i]);
      }

      this.prone = pose.prone;
      this.handIK = pose.handIK;
      this.handCurl = pose.handCurl;
      this.gait = pose.gait;

      for (const key of PAIR_FIELDS) {
        for (let i = 0; i < 2; i++) this[key][i] = pose[key][i];
      }

      return this;
    }

    blend(a, b, t) {
      const hipsA=a.hips,hipsB=b.hips,hips=this.hips;
      if(hipsA.x===hipsB.x&&hipsA.y===hipsB.y&&hipsA.z===hipsB.z){
        if(hips.x!==hipsA.x||hips.y!==hipsA.y||hips.z!==hipsA.z)hips.copy(hipsA);
      }else hips.copy(hipsA).lerp(hipsB,t);
      for(let i=0;i<this.q.length;i++){
        const from=a.q[i],to=b.q[i],dst=this.q[i];
        if(from.x===to.x&&from.y===to.y&&from.z===to.z&&from.w===to.w){
          if(dst.x!==from.x||dst.y!==from.y||dst.z!==from.z||dst.w!==from.w)dst.copy(from);
        }else dst.copy(from).slerp(to,t);
      }

      for (const key of POSE_VECTOR_FIELDS) {
        const dst=this[key],from=a[key],to=b[key];
        for(let i=0;i<dst.length;i++){
          const x=from[i],y=to[i],out=dst[i];
          if(x.x===y.x&&x.y===y.y&&x.z===y.z){
            if(out.x!==x.x||out.y!==x.y||out.z!==x.z)out.copy(x);
          }else out.copy(x).lerp(y,t);
        }
      }

      for(const key of POSE_SCALAR_FIELDS){
        const from=a[key],to=b[key];if(from===to){if(this[key]!==from)this[key]=from;}else this[key]=M.mix(from,to,t);
      }

      for (const key of PAIR_FIELDS) {
        for (let i = 0; i < 2; i++){
          const from=a[key][i],to=b[key][i];if(from===to){if(this[key][i]!==from)this[key][i]=from;}else this[key][i]=M.mix(from,to,t);
        }
      }

      return this;
    }
  }

  R.InfantryAnimator = class InfantryAnimator {
    constructor(model, genome, phenotype=null) {
      this.model = model;
      this.rig = model.rig;
      this.H = model.anatomy.height;
      this.boneResponses=this.rig.bones.map(b=>this.boneResponse(b.name));
      this.boneIsArm=this.rig.bones.map(b=>ARM_BONE.test(b.name));
      this._responseDt=NaN;this._responseAlpha=[];
      this.genome = genome;
      this.locomotion = new R.LocomotionController(model.anatomy, phenotype||R.InfantryGenome.express(genome));
      this.bipedFeet = new R.BipedFeet(model.anatomy);
      this.settling = false;

      this.state = 'IDLE';
      this.time = 0;
      this.phase = 0;
      this.distance = 0;

      this.targetPose = new Pose(this.rig);
      this.transitionPose = new Pose(this.rig);
      this.current = new Pose(this.rig);
      this.from = new Pose(this.rig);
      this.previousArmPose = new Pose(this.rig);
      this._previousPhase = 0; this._phaseArms = false;
      this.bipedProfile = {}; this._hipOffset = V();
      this.transition = null;

      this.ik = new R.TwoBoneIK(this.rig);
      this.poseOwner = 'ANIMATION';

      this.footWorld = [V(), V()];
      this.wasPlanted = [false, false];
      this.footGoal = [V(), V()];
      this.footQ = [new THREE.Quaternion(), new THREE.Quaternion()];
      this.footSamples = [this.createFootSample(), this.createFootSample()];
      this.handSamples = [this.createFootSample(), this.createFootSample()];
      this.footContacts = [new R.GroundContact(), new R.GroundContact()];
      this.handContacts = [new R.GroundContact(), new R.GroundContact()];
      this.footContactWorld = [V(), V()];
      this.handContactWorld = [V(), V()];
      this.handGoal = [V(), V()];
      this.handQ = [new THREE.Quaternion(), new THREE.Quaternion()];
      this.groundQ = [new THREE.Quaternion(), new THREE.Quaternion()];
      this.bodyLift = 0;
      this.seatAnchor = null;

      this.motion = {
        speed: 0,
        moveAngle: 0,
        turnRate: 0,
        treadmill: false
      };

      this._v = V();
      this._w = V();
      this._p = V();
      this._q = new THREE.Quaternion();
      this._q2 = new THREE.Quaternion();
      this._e = new THREE.Euler();
      this._rootQ = new THREE.Quaternion();
      this._inverseRoot = new THREE.Quaternion();
      this._shinQ = new THREE.Quaternion();
      this._worldFootQ = new THREE.Quaternion();
      this._ankleQ = new THREE.Quaternion();
      this._oldUpper = new THREE.Quaternion(); this._oldLower = new THREE.Quaternion();
      this._contactLocal = V(); this._offset = V(); this._rawWorld = V(); this._candidate = V();
      this._normal = V(); this._up = V(0,1,0);
      this._sample = { height: 0, normal: V(0, 1, 0) };
      this._frameA = new THREE.Matrix4();
    this._frameB = new THREE.Matrix4();
      this._clearanceMatrix=new THREE.Matrix4();
    this._clearanceBox=new THREE.Box3();
    this._clearancePoint=V();
    this._clearanceHeightCache={terrain:null,heights:null,ix:-1,iz:-1,a:0,b:0,c:0,d:0};
    this._clearanceMinCache={surface:null,revision:-1,body:NaN,gear:NaN,head:NaN,headGear:NaN};
    this._clearanceResultCache=new WeakMap();
    this._poseBoundsCache=new THREE.Box3();this._poseBoundsRevision=-1;this._poseBoundsSurfaceGeometry=null;this._poseBoundsGearGeometry=null;
      this._x = V();
      this._y = V();
      this._z = V();
      // The bind-space palm frame depends only on anatomy. Build it once per
      // rig side; the terrain-aligned frame remains pose-dependent.
      this._handBindFrames = [new THREE.Matrix4(), new THREE.Matrix4()];
      for(let i=0;i<2;i++) {
        const side=i===0?'L':'R';
        this._y.copy(this.rig.anatomy.points['hand.'+side]).sub(this.rig.anatomy.points['foreArm.'+side]).normalize();
        this._z.set(0,0,1);
        this._x.crossVectors(this._y,this._z).normalize();
        this._handBindFrames[i].makeBasis(this._x,this._y,this._z).invert();
      }

      this.supportIndices = this.buildSupports();
      this.metrics = {
        minSole: 0,
        minBody: 0,
        maxIKError: 0, bodyLift: 0, headLift: 0, minBoot: [0,0], footContact: ['SOLE','SOLE'], handContact: ['FREE','FREE'], seat: '—'
      };

      this.samplePose('IDLE', this.current, this.motion);
      this.applyPose(this.current);
    }

    createFootSample() {
      return {
        z: 0,
        lift: 0,
        stance: false,
        plant: 0,
        support: 0,
        footPitch: 0,
        toePitch: 0,
        swing: 0
      };
    }

    isLocomotion(state) {
      return Gait.isMoving(state);
    }

    setState(state, immediate = false) {
      if(!animationStates&&R.AnimationState)animationStates=new Set(Object.values(R.AnimationState));
      if (!(animationStates?animationStates.has(state):Object.values(R.AnimationState).includes(state))) {
        throw new Error('Nieznana animacja ' + state);
      }

      if (state === this.state && !immediate && !this.locomotion.custom) return false;

      const previous = this.state;
      if (Profile.isBiped(state)) this.locomotion.setPreset(state, immediate);
      // Within this family a preset changes targets, not the animation clock.
      if (Profile.isBiped(previous) && Profile.isBiped(state) && !immediate) {
        this.state = state;
        if (!this.transition || this.transition.keepContacts) this.transition = null;
        return true;
      }
      const preserveFootLock = this.isLocomotion(previous) && this.isLocomotion(state);

      this.state = state;
      if (!preserveFootLock || Gait.restState(previous) !== Gait.restState(state)) this.releaseContacts();

      if (immediate || state === 'REST') {
        this.transition = null;
        this.samplePose(state, this.current, this.motion);
        return true;
      }

      const isProne = s => s === 'PRONE' || s === 'PRONE_MOVE';
      let stages;

      if (isProne(previous) !== isProne(state)) {
        stages = isProne(state)
          ? [
              { state: 'CROUCH', duration: 0.26 },
              { state: 'SUPPORT', duration: 0.38 },
              { state, duration: 0.44 }
            ]
          : [
              { state: 'SUPPORT', duration: 0.40 },
              { state: 'CROUCH', duration: 0.36 },
              { state, duration: 0.28 }
            ];
      } else if (this.isLocomotion(previous) && this.isLocomotion(state)) {
        // Keep gait phase across WALK <-> RUN; only the gait profile changes.
        stages = [{ state, duration: (state === 'CROUCH_WALK' || previous === 'CROUCH_WALK') ? 0.40 : 0.20 }];
      } else if (state === 'SITTING' || previous === 'SITTING') {
        stages = [{ state, duration: 0.60 }];
      } else if (state === 'CROUCH' || previous === 'CROUCH' || state === 'CROUCH_WALK' || previous === 'CROUCH_WALK') {
        stages = [{ state, duration: 0.36 }];
      } else {
        stages = [{ state, duration: 0.22 }];
      }

      this.from.copy(this.current);
      this.transition = { stages, index: 0, time: 0,
        movingFrom: this.isLocomotion(previous) ? previous : null,
        keepContacts: Gait.restState(previous) === Gait.restState(state)
      };
      return true;
    }

    cycleFraction(state = this.state) {
      if (Profile.isBiped(state)) return this.locomotion.gait.cycleM / this.H;
      return Gait.cycleFraction(state, this.genome.speedGene)*(this.rig.anatomy.legLength/this.H/.48);
    }

    cycleLength(state = this.state) {
      return this.H * this.cycleFraction(state);
    }

    samplePose(state, pose, context) {
      pose.clear();
      if (state === 'REST') return;

      const a = this.rig.anatomy.armAngle;
      const ctx = context || this.motion;

      // Neutral arm carriage.
      pose.rot('upperArm.L', 0, 0, -a + 0.055);
      pose.rot('upperArm.R', 0, 0, a - 0.055);
      pose.rot('foreArm.L', -0.065);
      pose.rot('foreArm.R', -0.065);

      if (Profile.isBiped(state)) {
        const intermediate=this.transition && state!==this.state;
        const depth=intermediate?Profile.preset(state,this.locomotion.phenotype).crouch:this.locomotion.actualCrouch;
        this.sampleBiped(pose,depth,intermediate?0:ctx.speed,ctx);
        return;
      }

      if (state === 'SITTING') {
        this.sampleSitting(pose, ctx);
        return;
      }

      if (state === 'SUPPORT') {
        this.sampleSupport(pose);
        return;
      }

      if (state === 'PRONE' || state === 'PRONE_MOVE') {
        this.sampleProne(state, pose);
      }
    }

    sampleIdle(pose) {
      const breath = Math.sin(this.time * 1.42);
      const slow = Math.sin(this.time * 0.63 + 0.8);

      pose.hips.x = slow * 0.0015;
      pose.hips.y += breath * 0.0016;

      pose.rot('spineLower', breath * 0.0035, 0, slow * 0.0025);
      pose.rot('spineUpper', breath * 0.0060, 0, -slow * 0.0030);
      pose.rot('chest', breath * 0.0045);
      pose.rot('neck', -breath * 0.0020, slow * 0.0020);
      pose.rot('head', -breath * 0.0015, -slow * 0.0020);
    }

    sampleFreeArms(pose,phase,depth,run,sprint,amplitude) {
      const A=this.rig.anatomy,lower=1-Math.exp(-2*depth);
      // All current weapons are on the back/hip. Owning a weapon does not
      // constrain the hands; a future carried-weapon layer can replace this.
      const bulk=Math.max(0,(A.body.chestWidthScale||1)-1);
      const clearance=M.clamp(bulk*.08,0,.035);
      for(let i=0;i<2;i++) {
        const side=i===0?'L':'R',sign=i===0?1:-1;
        const t=TAU*(phase+i*.5);
        const wave=Math.cos(t)+.05*Math.sin(3*t);
        const forward=Math.max(0,-wave),back=Math.max(0,wave);
        const pitch=wave*M.mix(.34,.66,run)-sprint*(.06+.14*forward);
        const elbow=M.mix(M.mix(.18,.88,run)+forward*M.mix(.18,.24,run),
          1.48+.10*back-.10*forward,sprint);
        const lowPitch=-.12+Math.cos(t)*M.mix(.34,.10,depth);
        const lowElbow=.23+.26*depth+forward*.12;
        pose.rot('upperArm.'+side,
          M.mix(-.12*depth,M.mix(pitch,lowPitch,lower),amplitude),0,
          sign*(-A.armAngle+M.mix(.055+.05*depth,
            M.mix(M.mix(.072,.095,run)+sprint*(.025+clearance),.08+.05*depth,lower),amplitude)));
        pose.rot('foreArm.'+side,-M.mix(.065+.34*depth,M.mix(elbow,lowElbow,lower),amplitude));
        // Pronation around the forearm's own axis leaves the wrist endpoint
        // unchanged and turns the palms inward instead of carrying them up.
        this._v.copy(A.points['hand.'+side]).sub(A.points['foreArm.'+side]).normalize();
        this._q.setFromAxisAngle(this._v,sign*(.85*run+.35*sprint)*amplitude*(1-lower));
        pose.q[this.rig.index['foreArm.'+side]].multiply(this._q);
        pose.rot('clavicle.'+side,0,-wave*M.mix(.03,.065,run)*amplitude*(1-lower),
          -sign*.009*Math.sin(t)*amplitude*(1-lower));
        pose.rot('hand.'+side,-wave*M.mix(.045,.06,run)*amplitude*(1-lower),
          sign*.025*sprint*amplitude);
      }
    }

    sampleBiped(pose,depth,speed,context) {
      const A=this.rig.anatomy,H=this.H,L=A.legLength;
      const k=Profile.sample(depth,A,this.bipedProfile);
      const g=this.locomotion.gait;
      const moving=speed>.008 || this.settling;
      const amplitude=moving?Math.max(this.settling?.35:0,g.amplitude):0;
      const run=g.run,sprint=g.sprint*amplitude,cycle=g.cycleM/H;
      this.sampleFreeArms(pose,this.phase,depth,run,sprint,amplitude);
      pose.handCurl=(.35*run+.50*sprint)*amplitude*(1-Gait.smoothRange(.15,.60,depth));
      if(!this.transition) {
        this.sampleFreeArms(this.previousArmPose,this._previousPhase,depth,run,sprint,amplitude);
        this._phaseArms=true;
      }
      const breath=Math.sin(this.time*1.42);
      const phaseWave=Math.cos(TAU*this.phase);
      let supportBias=0;
      for(let i=0;i<2;i++) {
        const sign=i===0?1:-1,f=this.footSamples[i];
        const t=this.phase+i*.5;
        Gait.sampleLowContact(t,g.duty,cycle,g.liftM/H,f,sprint);
        pose.feet[i].set(sign*(k.stanceHalf/H-.003*sprint),A.ankleHeight/H+(moving?f.lift*amplitude:0),k.footZ/H+(moving?f.z:0));
        pose.footLift[i]=moving?f.lift*amplitude:0;
        pose.footPlant[i]=moving?f.plant:1;pose.support[i]=moving?f.support:1;
        const u=Gait.wrap01(t)/g.duty;
        const toeOff=moving&&f.stance?Gait.smoothRange(.76,1,u):0;
        // Toe push-off flows into recovery, then the ankle returns to neutral
        // before landing. Both boundaries retain a continuous orientation.
        const pushPitch=M.mix(.16+.08*sprint,.05,depth);
        const swingPitch=moving&&!f.stance?pushPitch*(1-Gait.smoothRange(0,.65,f.swing)):0;
        pose.footPitch[i]=toeOff*pushPitch+swingPitch;
        pose.toePitch[i]=-pose.footPitch[i]*.95;
        pose.footYaw[i]=sign*(k.footYaw-.025*sprint);
        pose.footRelative[i]=0;
        pose.knees[i].set(sign*k.kneeHalf/H,A.ankleHeight/H+L/H*.42,L/H*.82+(moving?f.z*.22:0));
      }
      const sum=pose.support[0]+pose.support[1];
      if(moving&&sum>.01)supportBias=(pose.support[0]-pose.support[1])/sum;
      const yaw=phaseWave*M.mix(.034+.020*sprint,.017,depth)*amplitude+(context.moveAngle||0)*.24;
      const roll=-supportBias*M.mix(.038,.018,depth);
      const bob=moving?Math.cos(TAU*(this.phase+.04*run)*2)*H*M.mix(M.mix(.007,.014,run),.002,depth)*amplitude:breath*.0012*H;
      // A softly flexed supporting knee leaves room for the flight arc. Without
      // this, the reach clamp alternately drops/raises a nearly straight leg.
      const compression=L*run*(.045+.03*sprint)*(1-depth)*amplitude;
      pose.hips.set(supportBias*H*M.mix(.016,.006,depth),k.hipY-compression+bob,k.hipZ);
      pose.hips.multiplyScalar(1/H);
      const runningLean=(run*.065+sprint*.045)*(1-depth);
      pose.rot('hips',k.pelvisPitch+runningLean,yaw,roll);
      pose.rot('spineLower',k.lowerPitch+runningLean*.55+breath*.002,-yaw*(.30+.10*sprint),-roll*.35);
      pose.rot('spineUpper',k.upperPitch+runningLean*.35+breath*.003,-yaw*(.38+.22*sprint),-roll*.40);
      pose.rot('chest',k.chestPitch+breath*.002,-yaw*(.20+.10*sprint)-(context.turnRate||0)*.005,-roll*.20);
      const lean=k.pelvisPitch+k.lowerPitch+k.upperPitch+k.chestPitch;
      pose.rot('neck',-lean*.46-runningLean*1.25,-yaw*.07,roll*.12);
      pose.rot('head',-lean*.13-runningLean*.55,-yaw*.05,roll*.10);
      pose.prone=0;pose.handIK=0;pose.gait=moving?1:0;
      this.constrainBipedHips(pose);
    }

    constrainBipedHips(pose) {
      const A=this.rig.anatomy,H=this.H,L=A.legLength;
      // Bound the true hip-joint distance, including rotated pelvis offset.
      for(let i=0;i<2;i++) {
        const side=i===0?'L':'R';
        this._hipOffset.copy(A.points['thigh.'+side]).sub(A.points.hips).multiplyScalar(1/H).applyQuaternion(pose.q[this.rig.index.hips]);
        const dx=pose.feet[i].x-pose.hips.x-this._hipOffset.x;
        const dz=pose.feet[i].z-pose.hips.z-this._hipOffset.z;
        const reach=L/H*.992;
        const maxY=pose.feet[i].y+Math.sqrt(Math.max(.0001,reach*reach-dx*dx-dz*dz))-this._hipOffset.y;
        pose.hips.y=Math.min(pose.hips.y,maxY);
      }
    }

    sampleSitting(pose, context) {
      const A=this.rig.anatomy,leg=A.legLength/this.H,ankle=A.ankleHeight/this.H;
      pose.hips.set(0,ankle+leg*.635,-leg*.202);
      // The old CROUCH is deliberately preserved as a reference SEATED pose.
      // An optional world-space seat surface anchors the pelvis; no chair/AI is spawned.
      if (this.seatAnchor) {
        this._v.copy(this.seatAnchor); this.rig.root.worldToLocal(this._v);
        pose.hips.copy(this._v).multiplyScalar(1/this.H);
        pose.hips.y += .040*A.body.waistDepthScale;
      }
      pose.rot('spineLower', 0.12);
      pose.rot('spineUpper', 0.15);
      pose.rot('chest', 0.06);
      pose.rot('neck', -0.15);

      for (let i = 0; i < 2; i++) {
        const sign = i === 0 ? 1 : -1;
        const side = i === 0 ? 'L' : 'R';
        pose.feet[i].set(sign*(A.hipHalf+.020),ankle,leg*.073);
        pose.knees[i].set(sign*(A.hipHalf+.048),ankle+leg*.32,leg*.96);
        if (this.seatAnchor) {
          pose.feet[i].x += pose.hips.x;
          pose.feet[i].z += pose.hips.z + leg*.202;
          pose.knees[i].x += pose.hips.x;
          pose.knees[i].z += pose.hips.z + leg*.202;
          // A tall seat permits hanging feet, instead of stretching the legs.
          pose.footLift[i] = Math.max(0, pose.hips.y-leg*.70-ankle);
          pose.feet[i].y = ankle + pose.footLift[i];
          pose.footPlant[i] = pose.footLift[i] > .001 ? 0 : 1;
        }
        pose.rot('upperArm.' + side, -0.34, 0, sign * (-this.rig.anatomy.armAngle + 0.09));
        pose.rot('foreArm.' + side, -1.04);
      }
    }

    sampleSupport(pose) {
      const A=this.rig.anatomy,leg=A.legLength/this.H,torso=A.neckY-A.hipY;
      const arm=A.points['upperArm.L'].distanceTo(A.points['hand.L'])/this.H;
      pose.hips.set(0,A.ankleHeight/this.H+leg*.377,-.015);
      pose.rot('hips',1.14);pose.rot('spineUpper',.10);
      pose.rot('neck',-.62);pose.rot('head',-.10);
      pose.handIK=1;pose.prone=.63;
      for(let i=0;i<2;i++) {
        const sign=i===0?1:-1;
        pose.feet[i].set(sign*(A.hipHalf+.055),A.ankleHeight/this.H,-leg*.57);
        pose.knees[i].set(sign*(A.hipHalf+.19),.055,-leg*.16);
        pose.footRelative[i]=.65;pose.ankleYaw[i]=sign*.07;
        pose.hands[i].set(sign*(A.shoulderHalf+.020),.026,torso*.70+arm*.46);
        pose.elbows[i].set(sign*(A.shoulderHalf+.15),.12,torso*.77);
        pose.handPlant[i]=1;
      }
    }

    sampleProne(state, pose) {
      const moving=state==='PRONE_MOVE', A=this.rig.anatomy, H=this.H;
      const leg=A.legLength/H, torso=A.neckY-A.hipY;
      const arm=A.points['upperArm.L'].distanceTo(A.points['hand.L'])/H;
      const cycle=this.cycleFraction('PRONE_MOVE'), C=R.Config.INFANTRY;
      const wave=moving ? Math.sin(TAU*this.phase) : 0;
      pose.hips.set(wave*.002,Math.max(.070*A.body.waistDepthScale,.082*A.body.legThicknessScale)+.012,-.008);
      pose.rot('hips',Math.PI/2,0,wave*.021);
      pose.rot('spineLower',-.025,0,-wave*.012);
      pose.rot('spineUpper',-.055,0,-wave*.018);
      pose.rot('neck',-.58);pose.rot('head',-.18);
      pose.prone=1;pose.handIK=1;pose.gait=moving?1:0;
      for(let i=0;i<2;i++) {
        const sign=i===0?1:-1;
        const f=Gait.sampleLowContact(this.phase+i*.5,C.CRAWL_FOOT_DUTY,cycle,leg*.026,this.footSamples[i]);
        const h=Gait.sampleLowContact(this.phase+i*.5+.5,C.CRAWL_HAND_DUTY,cycle,arm*.030,this.handSamples[i]);
        const recovery=moving?16*f.swing*f.swing*(1-f.swing)*(1-f.swing):0;
        pose.feet[i].set(sign*(A.hipHalf+.050+recovery*.020),A.ankleHeight/H+(moving?f.lift:0),-leg*.685+(moving?f.z:0));
        pose.footLift[i]=moving?f.lift:0;pose.footPlant[i]=moving?f.plant:1;
        pose.support[i]=moving?f.support:1;pose.footRelative[i]=1;
        // Small ankle adjustment only. Most splay comes from the LEG bend plane.
        pose.anklePitch[i]=.055;pose.ankleYaw[i]=sign*.075;
        pose.knees[i].set(sign*(A.hipHalf+.235),.055,-leg*.30);
        pose.hands[i].set(sign*(A.shoulderHalf+.018),.026+(moving?h.lift:0),torso*.94+arm*.43+(moving?h.z:0));
        pose.handLift[i]=moving?h.lift:0;pose.handPlant[i]=moving?h.plant:1;
        pose.elbows[i].set(sign*(A.shoulderHalf+.12),.055,torso*.94);
      }
    }

    boneResponse(name) {
      if (name === 'head' || name === 'neck') return 10;
      if (name === 'hips' || name === 'spineLower' || name === 'spineUpper' || name === 'chest') return 14;
      if (name.startsWith('clavicle.') || name.startsWith('upperArm.')) return 17;
      if (name.startsWith('foreArm.') || name.startsWith('hand.')) return 15;
      if (name.startsWith('toes.')) return 22;
      return 26;
    }

    dampPose(current, desired, dt) {
      if (!(dt > 0)) {
        current.copy(desired);
        return;
      }

      const hipsAlpha = 1 - Math.exp(-18 * dt);
      current.hips.lerp(desired.hips, hipsAlpha);

      if(dt!==this._responseDt) {
        this._responseDt=dt;
        for(let i=0;i<this.boneResponses.length;i++)this._responseAlpha[i]=1-Math.exp(-this.boneResponses[i]*dt);
      }

      for(let i=0;i<current.q.length;i++){
        const q=current.q[i];
        // Transport the cyclic part to this frame's phase before damping.
        // Parameters/transitions still settle normally, but fast arm swings
        // retain their amplitude and timing relative to the unfiltered feet.
        if(this._phaseArms && this.boneIsArm[i]) {
          this._q.copy(this.previousArmPose.q[i]).invert();
          this._q.premultiply(desired.q[i]);
          q.premultiply(this._q);
        }
        q.slerp(desired.q[i], this._responseAlpha[i]);
      }

      // IK targets should remain phase-accurate. Filtering them causes visible
      // foot skating, so only the upper-body pose receives temporal lag.
      for(const key of DAMPED_VECTOR_FIELDS){
        const target=current[key],source=desired[key];
        for(let i=0;i<target.length;i++)target[i].copy(source[i]);
      }

      const scalarAlpha = 1 - Math.exp(-22 * dt);
      current.prone = M.mix(current.prone, desired.prone, scalarAlpha);
      current.handIK = M.mix(current.handIK, desired.handIK, scalarAlpha);
      current.handCurl = M.mix(current.handCurl, desired.handCurl, scalarAlpha);
      current.gait = M.mix(current.gait, desired.gait, scalarAlpha);

      for (const key of PAIR_FIELDS) {
        for (let i = 0; i < 2; i++) current[key][i] = desired[key][i];
      }
    }

    applyPose(pose) {
      // The pose stores the complete hips translation. Resetting hips to rest
      // and immediately overwriting all three components caused two writes per
      // sample, so write only a changed final value.
      const hips=this.rig.byName.hips.position,poseHips=pose.hips,H=this.H;
      const hipsX=poseHips.x*H,hipsY=poseHips.y*H,hipsZ=poseHips.z*H;
      if(hips.x!==hipsX||hips.y!==hipsY||hips.z!==hipsZ)hips.set(hipsX,hipsY,hipsZ);
      // Snapshot channels are the exact write set for body rotations. Fixed
      // lip/cheek controls and expression-owned quaternions need no body write.
      const rotationIndices=this.rig.animationQuaternionIndices;
      for(let i=0;i<rotationIndices.length;i++){
        const index=rotationIndices[i],current=this.rig.bones[index].quaternion,target=pose.q[index];
        if(current.x!==target.x||current.y!==target.y||current.z!==target.z||current.w!==target.w)current.copy(target);
      }
      if(this.weaponLayer)this.weaponLayer.prepareBody();
      const curl=this.model.mesh.morphTargetDictionary.handsRelax;
      if(curl!==undefined&&this.model.mesh.morphTargetInfluences[curl]!==pose.handCurl)this.model.mesh.morphTargetInfluences[curl]=pose.handCurl;
      // Contact solvers query only the needed bone paths. Ground clearance
      // builds a full skin palette after the final corrected pose.
    }

    update(dt, context) {
      if (this.poseOwner !== 'ANIMATION') return;

      this.motion.speed = Math.max(0, context.speed || 0);
      this.motion.moveAngle = M.clamp(context.moveAngle || 0, -0.9, 0.9);
      this.motion.turnRate = context.turnRate || 0;
      this.motion.treadmill = !!context.treadmill;
      this._stepDt=dt;
      this._previousPhase=this.phase;this._phaseArms=false;
      if(Profile.isBiped(this.state)) {
        this.locomotion.actualSpeedMps=this.motion.speed;
        this.locomotion.sampleGait(dt,dt===0);
      }

      this.time += dt;

      const distance = Math.max(0, context.distance || 0);
      this.distance += distance;

      const biped=Profile.isBiped(this.state);
      const phaseState=biped?this.state:(this.isLocomotion(this.state)?this.state:(this.transition&&this.transition.movingFrom));
      if(phaseState && distance>0) {
        this.phase=Gait.wrap01(this.phase+distance/Math.max(.01,this.cycleLength(phaseState)));
      }
      // Finish an airborne foot after the root has stopped. This is a bounded
      // local landing phase, not a permanently running gait clock at speed zero.
      this.settling=false;
      if(biped && dt>0 && this.motion.speed<.03) {
        const duty=Math.max(.62,this.locomotion.gait.duty), half=Gait.wrap01(this.phase*2)*.5;
        if(half>Math.max(.025,duty-.5-.025)) {
          this.settling=true;
          const remaining=.5-half+.018;
          this.phase=Gait.wrap01(this.phase+Math.min(remaining,dt*.90));
        }
      }

      let desired;

      if (this.transition) {
        const tr = this.transition;
        const stage = tr.stages[tr.index];
        tr.time += dt;

        this.samplePose(stage.state, this.targetPose, this.motion);
        this.transitionPose.blend(
          this.from,
          this.targetPose,
          Gait.smooth5(tr.time / stage.duration)
        );
        desired = this.transitionPose;

        if (tr.time >= stage.duration) {
          tr.index++;
          tr.time = 0;
          this.from.copy(this.transitionPose);
          if (tr.index >= tr.stages.length) this.transition = null;
        }
      } else {
        this.samplePose(this.state, this.targetPose, this.motion);
        desired = this.targetPose;
      }

      this.dampPose(this.current, desired, dt);
      this.applyPose(this.current);

      if (this.state === 'REST') {
        this.releaseContacts();
        return;
      }

      this.ik.maxTargetError = 0;
      if(biped && !this.transition) {
        this.bipedFeet.update(this.current,this,context,dt);
        // Recheck after hip damping and world-space swing replanning. A pose
        // that was reachable on the treadmill can otherwise overextend here.
        this.constrainBipedHips(this.current);
        this.rig.byName.hips.position.copy(this.current.hips).multiplyScalar(this.H);
      }
      this.placeFeet(context);
      this.placeHands(context);

      this.correctGround(context, dt);
      this.metrics.maxIKError=this.ik.maxTargetError;
      this.metrics.crouch=Profile.isBiped(this.state)?this.locomotion.actualCrouch:null;
      this.metrics.runWeight=this.locomotion.runWeight;
      this.metrics.sprintWeight=this.locomotion.sprintWeight;
      this.metrics.seat=this.state==='SITTING'?(this.seatAnchor?'SIEDZISKO':'POZA REFERENCYJNA / BEZ SIEDZISKA'):'—';
    }

    releaseContacts() {
      if(this.bipedFeet)this.bipedFeet.reset();
      this.wasPlanted.fill(false);
      for(let i=0;i<this.footContacts.length;i++)this.footContacts[i].release();
      for(let i=0;i<this.handContacts.length;i++)this.handContacts[i].release();
    }

    setSeatAnchor(point) {
      if (point && ![point.x,point.y,point.z].every(Number.isFinite)) throw new RangeError('Niepoprawna kotwica siedziska.');
      this.seatAnchor=point?V(point.x,point.y,point.z):null;
      this.releaseContacts();
    }

    contactsEnabled(context) {
      return !context.treadmill && !context.turning && Math.abs(context.turnRate||0)<.32 &&
        (!this.transition || this.transition.keepContacts);
    }

    correctGround(context,dt) {
      this.model.prepareSkinPalette();
      let paletteReady=true;
      const bodyMin=this.minimumClearance(this.supportIndices.body,context.surface,true);
      const gearMin=this.minimumGearClearance(context.surface,false,true);
      const min=Math.min(bodyMin,gearMin);
      const required=M.clamp(.001-min,0,this.H*.085);
      // Rise immediately to honour collision; relax slowly so a changing support
      // point does not make the torso oscillate. Never move the root or stretch bones.
      const nextLift=required>this.bodyLift?required:M.mix(this.bodyLift,required,dt>0?1-Math.exp(-12*dt):1);
      const hipsMoved=nextLift!==this.bodyLift;
      this.bodyLift=nextLift;
      let limbsMoved=false;
      if(hipsMoved) {
        this.rig.byName.hips.position.y+=this.bodyLift;
        this.solveFeet(context);
        this.placeHands(context);
        paletteReady=false;
        limbsMoved=true;
      }
      for(let i=0;i<2;i++) {
        // Check the rotated shoe, including its edge/instep, NOT just its old sole.
        let minBoot=Infinity,needsFinalCheck=false;
        for(let pass=0;pass<2;pass++) {
          if(!paletteReady){this.model.prepareSkinPalette();paletteReady=true;}
          minBoot=this.minimumClearance(this.supportIndices.boots[i],context.surface,true);
          if(minBoot>=.0006)break;
          this.footContactWorld[i].y+=Math.min(.045*this.H,.001-minBoot);
          this.solveFoot(i,context);
          paletteReady=false;
          limbsMoved=true;
          needsFinalCheck=pass===1;
        }
        if(needsFinalCheck){this.model.prepareSkinPalette();paletteReady=true;this.metrics.minBoot[i]=this.minimumClearance(this.supportIndices.boots[i],context.surface,true);}
        else this.metrics.minBoot[i]=minBoot;
      }
      if(limbsMoved){if(!paletteReady)this.model.prepareSkinPalette();this.metrics.minBody=this.minimumClearance(this.supportIndices.body,context.surface,true);}
      else this.metrics.minBody=bodyMin;
      this.metrics.bodyLift=this.bodyLift;
      this.metrics.replants=this.bipedFeet.replants;
      let reanchors=0;for(let i=0;i<this.footContacts.length;i++)reanchors+=this.footContacts[i].reanchors||0;
      this.metrics.reanchors=reanchors;
    }

    finalizeAfterLook(context, correctionAlreadyApplied=false) {
      // Looking is applied AFTER the body animator; check head + helmet once more.
      if(this.state==='REST')return;
      // The head and helmet scans read the exact same post-look pose. Build the
      // CPU skin palette once and share it across both scans.
      this.model.prepareSkinPalette();
      const surface=context.surface,heights=surface&&surface.heights,revision=this.model.skinSampleRevision,cache=this._clearanceMinCache;
      const stableSurface=surface===R.FlatSurface||!!(R.TerrainSystem&&surface instanceof R.TerrainSystem&&heights);
      if(stableSurface&&cache.headReady&&cache.surface===surface&&cache.heights===heights&&cache.revision===revision)return;
      const headMin=this.minimumClearance(this.supportIndices.head,context.surface,true);
      const gearHeadMin=this.minimumGearClearance(context.surface,true,true);
      const min=Math.min(headMin,gearHeadMin);
      const previousLift=correctionAlreadyApplied?this.metrics.headLift:0,baseClearance=min-previousLift;
      this.metrics.headLift=M.clamp(.0005-baseClearance,0,.030*this.H);
      const liftDelta=this.metrics.headLift-previousLift;
      if(liftDelta!==0) {
        this.rig.byName.hips.position.y+=liftDelta;
        refreshBoneSubtree(this.rig.byName.hips,this.rig.root);
        this.solveFeet(context);this.placeHands(context);
        cache.headReady=false;
      }else if(stableSurface){
        cache.surface=surface;cache.heights=heights;cache.revision=revision;
        cache.head=headMin;cache.headGear=gearHeadMin;cache.headReady=true;
      }
      // Do not certify this palette for the next request: InfantryUnit still
      // restores the temporary head layer and applies weapon IK after this
      // phase. The next palette consumer must see those writes and refresh it.
    }

    rotateXZ(vector, angle) {
      if (Math.abs(angle) < 1e-8) return vector;
      const c = Math.cos(angle);
      const s = Math.sin(angle);
      const x = vector.x;
      const z = vector.z;
      vector.x = c * x + s * z;
      vector.z = -s * x + c * z;
      return vector;
    }

    lowestPoint(points, quaternion, out) {
      let min=Infinity;
      this._worldFootQ.copy(this._rootQ).multiply(quaternion);
      for(const p of points) {
        this._offset.copy(p).applyQuaternion(this._worldFootQ);
        const projected=this._offset.dot(this._sample.normal);
        if(projected<min){min=projected;out.copy(p);}
      }
      return out;
    }

    solveLeg(i,context) {
      const side=i===0?'L':'R',warp=M.clamp(context.moveAngle||0,-.72,.72)*.58;
      this._p.copy(this.current.knees[i]).multiplyScalar(this.H);this.rotateXZ(this._p,warp);
      this.ik.solve('thigh.'+side,'shin.'+side,'foot.'+side,this.footGoal[i],this._p,true);
    }

    orientFoot(i) {
      const side=i===0?'L':'R',pose=this.current;
      this.rig.byName['shin.'+side].getWorldQuaternion(this._shinQ);
      this._shinQ.premultiply(this._inverseRoot);
      this._ankleQ.setFromEuler(this._e.set(pose.anklePitch[i],pose.ankleYaw[i],0,'XYZ'));
      this._shinQ.multiply(this._ankleQ).normalize();
      this.footQ[i].copy(this.groundQ[i]).slerp(this._shinQ,pose.footRelative[i]).normalize();
      this.rig.setModelQuaternion('foot.'+side,this.footQ[i],this._rootQ);
      this.rig.byName['toes.'+side].rotation.x=pose.toePitch[i];
    }

    placeFeet(context) {
      const pose=this.current,rig=this.rig,H=this.H;
      const warp=M.clamp(context.moveAngle||0,-.72,.72)*.78;
      rig.root.getWorldQuaternion(this._rootQ);
      this._inverseRoot.copy(this._rootQ).invert();
      const enabled=this.contactsEnabled(context);
      for(let i=0;i<2;i++) {
        const c=this.footContacts[i],goal=this.footGoal[i];
        this._rawWorld.copy(pose.feet[i]).multiplyScalar(H);this.rotateXZ(this._rawWorld,warp);
        rig.root.localToWorld(this._rawWorld);
        context.surface.sample(this._rawWorld.x,this._rawWorld.z,this._sample);
        this._normal.copy(this._sample.normal).applyQuaternion(this._inverseRoot).normalize();
        this.groundQ[i].setFromUnitVectors(this._up,this._normal);
        this._q.setFromEuler(this._e.set(pose.footPitch[i],warp*.90+pose.footYaw[i],0,'XYZ'));
        this.groundQ[i].multiply(this._q);
        this._rawWorld.y=this._sample.height+pose.feet[i].y*H;
        goal.copy(this._rawWorld);rig.root.worldToLocal(goal);
        this.solveLeg(i,context);this.orientFoot(i);
        if(!c.active || !enabled || pose.footPlant[i]<.001) this.lowestPoint(this.supportIndices.footLocal[i],this.footQ[i],c.local);
        this._worldFootQ.copy(this._rootQ).multiply(this.footQ[i]);
        this._offset.copy(c.local).applyQuaternion(this._worldFootQ);
        this._candidate.copy(this._rawWorld).add(this._offset);
        const contactHeight=context.surface.getHeightAtCached?context.surface.getHeightAtCached(this._candidate.x,this._candidate.z,this._clearanceHeightCache):context.surface.getHeightAt(this._candidate.x,this._candidate.z);
        this._candidate.y=contactHeight+R.Config.INFANTRY.CONTACT_CLEARANCE;
        c.resolve(this._candidate,pose.footPlant[i],enabled,this.rig.anatomy.legLength*.34,this.footContactWorld[i]);
        this.wasPlanted[i]=c.locked;
        c.kind=pose.footRelative[i]>.5?'OUTER_EDGE':(pose.footPitch[i]>.04?'TOE':'SOLE');
        this.metrics.footContact[i]=(pose.footPlant[i]<.01?'FREE':c.kind)+(c.locked?' / LOCK':'');
      }
      this.solveFeet(context);
    }

    solveFoot(i,context) {
      const rig=this.rig,pose=this.current,c=this.footContacts[i];
      const lift=Math.max(0,pose.feet[i].y*this.H-rig.anatomy.ankleHeight);
      // At most two position/orientation iterations. C = ankle + R * localContact.
      for(let pass=0;pass<2;pass++) {
        this._worldFootQ.copy(this._rootQ).multiply(this.footQ[i]);
        this._offset.copy(c.local).applyQuaternion(this._worldFootQ);
        this.footGoal[i].copy(this.footContactWorld[i]).sub(this._offset);
        this.footGoal[i].y+=lift;
        rig.root.worldToLocal(this.footGoal[i]);
        this.solveLeg(i,context);this.orientFoot(i);
        if(pass===0) {
          // If the first IK solve left the oriented contact point at the same
          // target, a second identical solve cannot improve contact placement.
          this._worldFootQ.copy(this._rootQ).multiply(this.footQ[i]);
          this._offset.copy(c.local).applyQuaternion(this._worldFootQ);
          this._candidate.copy(this.footContactWorld[i]).sub(this._offset);
          this._candidate.y+=lift;
          rig.root.worldToLocal(this._candidate);
          if(this._candidate.distanceToSquared(this.footGoal[i])<=1e-12)break;
        }
      }
      // IK refreshed each changed joint path through setModelQuaternion().
      // The complete rig is refreshed once before the next skin/contact scan;
      // walking every bone subtree here after each leg solve was redundant.
    }

    solveFeet(context) {this.solveFoot(0,context);this.solveFoot(1,context);}

    placeHands(context) {
      const pose=this.current,rig=this.rig,H=this.H;
      if(pose.handIK<.001){for(let i=0;i<this.handContacts.length;i++)this.handContacts[i].release();this.metrics.handContact.fill('FREE');return;}
      for(let i=0;i<2;i++) {
        const side=i===0?'L':'R',upper=rig.byName['upperArm.'+side],lower=rig.byName['foreArm.'+side],c=this.handContacts[i];
        if(this.weaponLayer?.ownsHand(side)){c.release();this.metrics.handContact[i]='WEAPON';continue;}
        this._oldUpper.copy(upper.quaternion);this._oldLower.copy(lower.quaternion);
        this._rawWorld.copy(pose.hands[i]).multiplyScalar(H);rig.root.localToWorld(this._rawWorld);
        context.surface.sample(this._rawWorld.x,this._rawWorld.z,this._sample);
        // Bind frame -> palm down / fingers forward. The wrist frame respects
        // the ground normal; the eye/head layer cannot invalidate this contact.
        this._frameA.copy(this._handBindFrames[i]);
        this._normal.copy(this._sample.normal).applyQuaternion(this._inverseRoot).normalize();
        this._z.copy(this._normal).multiplyScalar(-1);
        this._y.set(0,0,1).addScaledVector(this._normal,-this._normal.z).normalize();
        this._x.crossVectors(this._y,this._z).normalize();this._frameB.makeBasis(this._x,this._y,this._z).multiply(this._frameA);
        this.handQ[i].setFromRotationMatrix(this._frameB).normalize();
        if(!c.active || !this.contactsEnabled(context) || pose.handPlant[i]<.001) this.lowestPoint(this.supportIndices.handLocal[i],this.handQ[i],c.local);
        this._worldFootQ.copy(this._rootQ).multiply(this.handQ[i]);
        this._offset.copy(c.local).applyQuaternion(this._worldFootQ);
        this._candidate.copy(this._rawWorld).add(this._offset);
        const contactHeight=context.surface.getHeightAtCached?context.surface.getHeightAtCached(this._candidate.x,this._candidate.z,this._clearanceHeightCache):context.surface.getHeightAt(this._candidate.x,this._candidate.z);
        this._candidate.y=contactHeight+.002;
        c.resolve(this._candidate,pose.handPlant[i],this.contactsEnabled(context),H*.14,this.handContactWorld[i]);
        this.handGoal[i].copy(this.handContactWorld[i]).sub(this._offset);
        this.handGoal[i].y+=pose.handLift[i]*H;
        rig.root.worldToLocal(this.handGoal[i]);
        this._p.copy(pose.elbows[i]).multiplyScalar(H);
        this.ik.solve('upperArm.'+side,'foreArm.'+side,'hand.'+side,this.handGoal[i],this._p,true,this._z);
        rig.setModelQuaternion('hand.'+side,this.handQ[i],this._rootQ);
        // A pole is NOT an elbow contact. Check the actual sleeve after IK and
        // correct the bend plane (bounded), keeping the palm at its support.
        for(let pass=0;pass<2;pass++) {
          const gap=this.minimumClearance(this.supportIndices.sleeves[i],context.surface);
          if(gap>=.001)break;
          this._p.y+=Math.min(.080*H,3.2*(.002-gap));
          this.ik.solve('upperArm.'+side,'foreArm.'+side,'hand.'+side,this.handGoal[i],this._p,true,this._z);
          rig.setModelQuaternion('hand.'+side,this.handQ[i],this._rootQ);
        }
        if(pose.handIK<.999){upper.quaternion.copy(this._oldUpper.slerp(upper.quaternion,pose.handIK));lower.quaternion.copy(this._oldLower.slerp(lower.quaternion,pose.handIK));}
        rig.setModelQuaternion('hand.'+side,this.handQ[i],this._rootQ);
        this.metrics.handContact[i]=(pose.handPlant[i]>.01?'PALM':'FREE')+(c.locked?' / LOCK':'');
      }
    }

    buildSupports() {
      const anatomy=this.rig.anatomy,cache=supportCacheFor(this.model,anatomy),cached=cache.byAnatomy.get(cache.anatomyKey);
      if(cached)return cached;
      const tags=this.model.surface.tags,positions=this.model.surface.geometry.attributes.position;
      const soles=[(tags['sole.L']||[]).slice(),(tags['sole.R']||[]).slice()],boots=[[],[]],footLocal=[[],[]],handLocal=[[],[]],sleeves=[[],[]],body=[],head=[];
      const sample=(list,n=38)=>list.filter((_,i)=>i%Math.max(1,Math.floor(list.length/n))===0);
      for(const name of ['jacket','head','neck']) {
        const list=sample(tags[name]||[]);body.push(...list);if(name==='head'||name==='neck')head.push(...list);
      }
      for(let i=0;i<2;i++) {
        const side=i===0?'L':'R',A=this.rig.anatomy;
        const all=[...soles[i],...sample(tags['boot.'+side]||[],64)];
        boots[i]=all;
        for(const index of all) {
          const p=V().fromArray(positions.array,index*3).sub(A.points['foot.'+side]);
          // Lower shell, sole and instep; the deforming upper shaft is checked
          // with skinVertex instead of pretending it is rigid around the ankle.
          if(p.y<=this.H*.026)footLocal[i].push(p);
        }
        for(const index of (tags['hand.'+side]||[]))handLocal[i].push(V().fromArray(positions.array,index*3).sub(A.points['hand.'+side]));
        sleeves[i]=sample(tags['sleeve.'+side]||[],60);
        body.push(...sample(tags['leg.'+side]||[],28),...sleeves[i]);
        if(!footLocal[i].length)footLocal[i].push(V(0,-A.ankleHeight,0));
        if(!handLocal[i].length)handLocal[i].push(V(0,-.01,0));
      }
      const supports=freezeSupports({soles,boots,footLocal,handLocal,sleeves,body,head});
      cache.byAnatomy.set(cache.anatomyKey,supports);
      return supports;
    }

    minimumClearance(indices, surface, paletteReady=false, broadphase=true, gear=false) {
      const mesh=gear?this.model.gearMesh:this.model.mesh;
      if(!mesh)return gear?Infinity:0;
      if(!paletteReady)this.model.prepareSkinPalette();
      const heights=surface&&surface.heights,revision=this.model.skinSampleRevision;
      const stableSurface=surface===R.FlatSurface||!!(R.TerrainSystem&&surface instanceof R.TerrainSystem&&heights);
      const resultCache=this._clearanceResultCache,cacheable=!!(resultCache&&indices&&broadphase&&stableSurface&&Number.isFinite(revision));
      if(cacheable){
        const entry=resultCache.get(indices);
        if(entry&&entry.surface===surface&&entry.heights===heights&&entry.revision===revision&&entry.mesh===mesh&&
          entry.geometry===mesh.geometry&&entry.segments===surface.segments&&entry.mapSize===surface.mapSize)return entry.value;
      }
      const finish=value=>{
        if(cacheable)resultCache.set(indices,{surface,heights,revision,mesh,geometry:mesh.geometry,
          segments:surface.segments,mapSize:surface.mapSize,value});
        return value;
      };
      const skin=gear?(this.model.skinSupportGearVertex||this.model.skinGearVertex):(this.model.skinSupportVertex||this.model.skinVertex);
      const scan=list=>{
        let result=Infinity;
        for(const i of list){
          skin.call(this.model,i,this._w);this._w.applyMatrix4(mesh.matrixWorld);
          const height=surface.getHeightAtCached?surface.getHeightAtCached(this._w.x,this._w.z,this._clearanceHeightCache):surface.getHeightAt(this._w.x,this._w.z);
          result=Math.min(result,this._w.y-height);
        }
        return result;
      };
      if(!broadphase||!indices||!indices.length)return finish(scan(indices||[]));
      const partition=clearancePartition(mesh,indices);
      if(!partition||!partition.groups.length)return finish(scan(indices));

      // Exact multi-bone vertices seed the minimum. Rigid groups
      // get a conservative transformed box; terrain is bounded by every grid
      // cell touched by that box. A group is skipped only when its lower bound
      // cannot change the exact minimum already found.
      let min=scan(partition.fallback);
      const root=mesh.matrixWorld,bindInv=mesh.bindMatrixInverse,bind=mesh.bindMatrix;
      const geometry=mesh.geometry,morphs=geometry.morphAttributes.position||[],influences=mesh.morphTargetInfluences||[];
      const poseRevision=this.model.skinSampleRevision;
      if(poseRevision===undefined||partition.poseModel!==this.model||partition.poseRevision!==poseRevision){
        let anyBoundsChanged=poseRevision===undefined||partition.poseModel!==this.model;
        for(const group of partition.groups){
          const bone=this.rig.bones[group.bone],inverse=mesh.skeleton.boneInverses[group.bone];
          this._clearanceMatrix.multiplyMatrices(root,bindInv).multiply(bone.matrixWorld).multiply(inverse).multiply(bind);
          const transformElements=this._clearanceMatrix.elements,cachedElements=group.transform.elements;
          let groupChanged=!group.transformReady;
          for(let i=0;i<16;i++)if(transformElements[i]!==cachedElements[i])groupChanged=true;
          const groupMorphs=group.morphedIndices.length>0,morphSnapshot=group.morphSnapshot;
          if(groupMorphs){
            for(let i=0;i<influences.length;i++)if(morphSnapshot[i]!==influences[i]){groupChanged=true;break;}
          }
          if(!groupChanged)continue;
          this._clearanceBox.copy(group.box);
          if(groupMorphs){
            const position=geometry.attributes.position.array,point=this._clearancePoint;
            for(const vertex of group.morphedIndices){
              const offset=vertex*3;point.fromArray(position,offset);const x=point.x,y=point.y,z=point.z;
              for(let m=0;m<morphs.length;m++){
                const weight=influences[m]||0;if(weight===0)continue;
                const delta=morphs[m].array;point.x+=delta[offset]*weight;point.y+=delta[offset+1]*weight;point.z+=delta[offset+2]*weight;
              }
              if(point.x!==x||point.y!==y||point.z!==z)this._clearanceBox.expandByPoint(point);
            }
          }
          this._clearanceBox.applyMatrix4(this._clearanceMatrix);
          group.poseBox.min.copy(this._clearanceBox.min);group.poseBox.max.copy(this._clearanceBox.max);
          group.transform.copy(this._clearanceMatrix);group.transformReady=true;
          if(groupMorphs)for(let i=0;i<influences.length;i++)morphSnapshot[i]=influences[i];
          anyBoundsChanged=true;
        }
        if(anyBoundsChanged)partition.groups.sort((a,b)=>a.poseBox.min.y-b.poseBox.min.y);
        partition.poseModel=this.model;partition.poseRevision=poseRevision;
      }
      for(const group of partition.groups){
        this._clearanceBox.copy(group.poseBox);
        const terrainMax=terrainMaximum(surface,this._clearanceBox.min.x,this._clearanceBox.min.z,
          this._clearanceBox.max.x,this._clearanceBox.max.z);
        group.lowerBound=terrainMax===null?-Infinity:this._clearanceBox.min.y-terrainMax;
      }
      // Terrain can change independently of the rig, so only sort by terrain
      // lower bound when it differs from the cached pose-only order.
      let ordered=true;for(let i=1;i<partition.groups.length;i++)if(partition.groups[i-1].lowerBound>partition.groups[i].lowerBound){ordered=false;break;}
      if(!ordered)partition.groups.sort((a,b)=>a.lowerBound-b.lowerBound);
      for(const group of partition.groups){
        if(group.lowerBound>min+BROADPHASE_MARGIN)break;
        min=Math.min(min,scan(group.indices));
      }
      return finish(min===Infinity?0:min);
    }

    minimumGearClearance(surface, headOnly = false, paletteReady=false, broadphase=true) {
      if(!this.model.gear||!this.model.gearMesh)return Infinity;
      const indices=headOnly?this.model.gear.headContactIndices:this.model.gear.contactIndices;
      return this.minimumClearance(indices||[],surface,paletteReady,broadphase,true);
    }

    poseBounds(out = new THREE.Box3()) {
      this.model.prepareSkinPalette();
      const surfaceGeometry=this.model.mesh.geometry,gearGeometry=this.model.gearMesh&&this.model.gearMesh.geometry,revision=this.model.skinSampleRevision;
      if(Number.isFinite(revision)&&this._poseBoundsRevision===revision&&this._poseBoundsSurfaceGeometry===surfaceGeometry&&this._poseBoundsGearGeometry===gearGeometry)return out.copy(this._poseBoundsCache);
      out.makeEmpty();

      for (let i = 0; i < this.model.surface.vertices; i++) {
        this.model.skinVertex(i, this._w, true);
        out.expandByPoint(this._w);
      }

      if(this.model.gear)for(const i of this.model.gear.contactIndices){
        this.model.skinGearVertex(i,this._w);out.expandByPoint(this._w);
      }
      if(Number.isFinite(revision)){this._poseBoundsCache.copy(out);this._poseBoundsRevision=revision;this._poseBoundsSurfaceGeometry=surfaceGeometry;this._poseBoundsGearGeometry=gearGeometry;}
      return out;
    }
  };
})();
