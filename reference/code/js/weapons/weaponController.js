(function () {
  'use strict';
  const R=window.RTS,M=R.Math,V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z),Q=()=>new THREE.Quaternion();
  const smooth=t=>R.GaitProfile.smooth5(t),clamp=(x,a=0,b=1)=>M.clamp(x,a,b);
  const BODY_AIM_BONES=[['spineUpper',.14],['chest',.20]],LONG_STOW_EULER=new THREE.Euler(-Math.PI/2,0,-.24,'XYZ');
  const RENDER_GRIP_BODY_BONES=['hips','spineLower','spineUpper','chest','clavicle.L','clavicle.R'];
  const pose=()=>({p:V(),q:Q()}),task=()=>({p:V(),q:Q(),weight:0,curl:0,profile:'HANDLE',attached:false});
  function copy(a,b){a.p.copy(b.p);a.q.copy(b.q);return a;}
  function mix(out,a,b,t){out.p.copy(a.p).lerp(b.p,t);out.q.copy(a.q).slerp(b.q,t);return out;}
  function sameQuaternion(a,b){return a.x===b.x&&a.y===b.y&&a.z===b.z&&a.w===b.w;}
  function sameGripTask(a,b){return a.p.x===b.p.x&&a.p.y===b.p.y&&a.p.z===b.p.z&&sameQuaternion(a.q,b.q)&&a.weight===b.weight&&a.curl===b.curl&&a.attached===b.attached&&a.profile===b.profile;}
  function spring(s,target,w,dt){if(dt<=0||(s.x===target&&s.v===0))return;const y=s.x-target,j=(s.v+w*y)*dt,e=Math.exp(-w*dt);s.x=target+(y+j)*e;s.v=(s.v-w*j)*e;}
  function bounded(value,min,max,label){if(!Number.isFinite(value))throw new RangeError(label+' wymaga skończonej liczby.');return clamp(value,min,max);}
  function makeSnapshot(){return {objects:{},tasks:[task(),task()],activeSlot:null,shotAge:10};}
  function syncSnapshotSlots(snapshot,instances) {
    for(const slot of Object.keys(snapshot.objects))if(!instances[slot])delete snapshot.objects[slot];
    for(const slot of Object.keys(instances))if(!snapshot.objects[slot])snapshot.objects[slot]=pose();
  }

  R.WeaponController=class WeaponController {
    constructor(unit) {
      this.unit=unit;this.model=unit.model;this.rig=unit.rig;this.A=unit.rig.anatomy;this.H=this.A.height;
      this.instances={};this.instanceSlots=[];this.selectedSlot=null;this.activeSlot=null;this.queuedSlot=null;
      this.state='STOWED';this.actionTime=0;this.phase=0;this.clock=0;this.readiness=0;
      this.requestedReadiness=.75;this.requestedGrip='AUTO';this.yaw=0;this.pitch=0;this.aimTargetWorld=null;
      this.aimYaw=0;this.aimPitch=0;this.supportSpring={x:0,v:0};this.readySpring={x:0,v:0};this.kick={x:0,v:0};this.shift={x:0,v:0};
      this.trigger=false;this.pendingShot=false;this.shotId=0;this.lastShotTime=-10;this.shotAge=10;this.onShot=null;
      this.pendingDraw=false;this.pendingHolster=false;this.pendingBody=null;this.isScrubbing=false;
      this.tasks=[task(),task()];this.starts=[pose(),pose()];this.freeHands=[pose(),pose()];this.startPose=pose();this.endPose=pose();
      this.stow=pose();this.hold=pose();this.sidePose=pose();this.extract=pose();this.output=pose();
      this.ik=new R.TwoBoneIK(this.rig);this.metrics={status:'STOWED',gripError:0,clearance:0,owners:['FREE','FREE'],shots:0};
      this._v=V();this._w=V();this._p=V();this._pole=V();this._normal=V();this._q=Q();this._r=Q();this._rootQ=Q();this._e=new THREE.Euler();this._m=new THREE.Matrix4();this._reachTask=task();this._renderTasks=[task(),task()];
      this._solveHandBases=[{upper:Q(),lower:Q(),hand:Q()},{upper:Q(),lower:Q(),hand:Q()}];
      this._renderGripArms=['L','R'].map(side=>({upper:this.rig.byName['upperArm.'+side],lower:this.rig.byName['foreArm.'+side],hand:this.rig.byName['hand.'+side]}));
      this._renderGripBodyBones=RENDER_GRIP_BODY_BONES.map(name=>this.rig.byName[name]);
      this._renderGripCache={ready:false,item:null,weaponPosition:V(),weaponQuaternion:Q(),rootPosition:V(),rootQuaternion:Q(),rootScale:V(),hipsPosition:V(),poseHipsPosition:V(),
        support:NaN,prone:NaN,surface:null,heights:null,bodyQuaternions:Array.from({length:RENDER_GRIP_BODY_BONES.length},Q),
        tasks:[task(),task()],inputs:[{upper:Q(),lower:Q(),hand:Q()},{upper:Q(),lower:Q(),hand:Q()}],
        outputs:[{upper:Q(),lower:Q(),hand:Q()},{upper:Q(),lower:Q(),hand:Q()}]};
      this._terrainHeightCache={terrain:null,heights:null,ix:-1,iz:-1,a:0,b:0,c:0,d:0};
      this.handling=new R.WeaponHandlingProfiles(this.A);this.profilePose=pose();
      for(let i=0;i<2;i++)this.freeHands[i].p.copy(this.A.points['hand.'+(i?'R':'L')]);
      this.previous=makeSnapshot();this.current=makeSnapshot();this.reconcile(unit.equipment);this.capture();this.syncSnapshots();
    }
    get active(){return this.instances[this.activeSlot]||null;}
    get selected(){return this.instances[this.selectedSlot]||null;}
    get grip(){return this.active?R.WeaponCatalog.grip(this.active.def,this.requestedGrip):null;}
    get handlingProfile(){return R.WeaponHandlingProfiles.resolve(this.active?.def||this.selected?.def,this.grip||'1H');}
    snapGrip(){this.supportSpring.x=this.grip==='2H'?1:0;this.supportSpring.v=0;}
    get busy(){return this.state==='DRAWING'||this.state==='HOLSTERING';}
    get occupiesHands(){return !!this.active && this.state!=='STOWED';}
    ownsHand(side) {
      if(!this.occupiesHands)return false;
      if(side==='R')return this.state!=='HOLSTERING'||this.phase<1;
      if(this.state==='DRAWING')return this.grip==='2H'&&this.phase>.77;
      if(this.state==='HOLSTERING')return this.grip==='2H'&&this.phase<.18;
      return this.grip==='2H'||this.supportSpring.x>.001;
    }
    reconcile(equipment) {
      const old=this.instances,next={};
      for(const [slot,item] of Object.entries(equipment.slots)) {
        if(!item||!R.WeaponCatalog.get(item.definitionId))continue;
        const existing=old[slot];
        if(existing&&existing.item.definitionId===item.definitionId&&existing.item.seed===item.seed)next[slot]=existing;
        else {next[slot]=R.WeaponGeometry.create(item);this.model.root.add(next[slot].root);}
      }
      for(const [slot,instance] of Object.entries(old))if(next[slot]!==instance)instance.dispose();
      const lost=this.activeSlot&&next[this.activeSlot]!==old[this.activeSlot];
      this.instances=next;
      this.instanceSlots=Object.keys(next);
      syncSnapshotSlots(this.previous,next);syncSnapshotSlots(this.current,next);
      if(lost){this.state='STOWED';this.activeSlot=null;this.pendingShot=false;this.trigger=false;this.pendingHolster=false;this.pendingDraw=false;this.queuedSlot=null;this.metrics.status='STOWED';}
      if(!next[this.selectedSlot])this.selectedSlot=Object.keys(next)[0]||null;
      if(this.active&&!this.active.def.profiles.includes(this.requestedGrip))this.requestedGrip='AUTO';
      this.fit=this.model.surface.clothingFit||new R.EquipmentFit(this.A,equipment);this.model.root.updateMatrixWorld(true);
      for(const [slot,instance] of Object.entries(next))if(slot!==this.activeSlot){this.stowPose(slot,this.output);this.assign(instance,this.output);}
      this.capture();this.syncSnapshots();
    }
    select(slot) {
      if(slot!==null&&!this.instances[slot])throw new Error('Jednostka nie posiada broni w tym slocie.');
      this.selectedSlot=slot;
      if(this.activeSlot&&slot!==this.activeSlot){this.queuedSlot=slot;this.holster();}
      if(this.selected&&!this.selected.def.profiles.includes(this.requestedGrip))this.requestedGrip='AUTO';
      return this;
    }
    setGrip(value='AUTO') {
      const item=this.active||this.selected;if(!['AUTO','1H','2H'].includes(value))throw new RangeError('Nieznany chwyt.');
      if(item)R.WeaponCatalog.grip(item.def,value);this.requestedGrip=value;return this;
    }
    setReadiness(value){this.requestedReadiness=bounded(value,0,1,'Podniesienie');return this;}
    setAim(yaw,pitch){this.yaw=bounded(yaw,-.85,.85,'Yaw');this.pitch=bounded(pitch,-.50,.50,'Pitch');this.aimTargetWorld=null;return this;}
    setAimTargetWorld(point) {
      if(point&&!['x','y','z'].every(k=>Number.isFinite(point[k])))throw new RangeError('Niepoprawny cel spojrzenia broni.');
      this.aimTargetWorld=point?V(point.x,point.y,point.z):null;return this;
    }
    draw() {
      if(!this.selected){this.metrics.status='NO_WEAPON';return false;}
      if(this.state==='HELD'&&this.activeSlot===this.selectedSlot)return true;
      if(this.busy){this.pendingDraw=true;return true;}
      if(this.activeSlot){this.queuedSlot=this.selectedSlot;this.holster();return true;}
      this.pendingDraw=true;this.isScrubbing=false;this.metrics.status='DRAW_PENDING';
      if(this.unit.animator.state==='PRONE_MOVE')this.unit.animator.setState('PRONE');
      return true;
    }
    holster(){this.pendingDraw=false;this.trigger=false;this.pendingShot=false;this.isScrubbing=false;if(this.active){this.pendingHolster=true;}return true;}
    setTrigger(value){this.trigger=!!value;}
    fire(){this.isScrubbing=false;this.pendingShot=true;return this.canFire();}
    canFire() {
      const item=this.active;
      return !!item&&item.def.firearm&&this.state==='HELD'&&this.readiness>=.68&&
        !this.unit.animator.transition&&this.unit.animator.state!=='REST'&&this.unit.animator.state!=='PRONE_MOVE'&&
        (this.unit.locomotion.sprintWeight||0)<.35&&this.metrics.gripError<.025;
    }
    guardState(state) {
      if((state==='PRONE_MOVE'||state==='REST')&&this.occupiesHands){this.pendingBody=state;this.holster();return false;}
      return true;
    }
    beginStep(dt,captureSnapshot=true) {
      if(captureSnapshot)this.copySnapshot(this.previous,this.current);
      if(!(dt>0))return;
      this.isScrubbing=false;this.clock+=dt;
      spring(this.supportSpring,this.occupiesHands&&this.grip==='2H'?1:0,12,dt);this.shotAge=this.clock-this.lastShotTime;
      const recovery=this.active?.def.kind==='pistol'?M.mix(19,25,clamp(this.supportSpring.x)):22;
      spring(this.kick,0,recovery,dt);spring(this.shift,0,25,dt);
      const sprint=this.unit.locomotion.sprintWeight||0,transition=!!this.unit.animator.transition;
      const target=this.state==='HELD'?this.requestedReadiness*(1-smooth(sprint/.55)):0;
      spring(this.readySpring,target,10,dt);this.readiness=clamp(this.readySpring.x);
      if(this.aimTargetWorld) {
        this._v.copy(this.aimTargetWorld);this.rig.root.worldToLocal(this._v);
        this.rig.modelPoint('chest',this._w);this._v.sub(this._w);
        this.yaw=clamp(Math.atan2(this._v.x,this._v.z),-.85,.85);
        this.pitch=clamp(Math.atan2(this._v.y,Math.hypot(this._v.x,this._v.z)),-.50,.50);
      }
      if(this.aimYaw!==this.yaw||this.aimPitch!==this.pitch){const turn=1-Math.exp(-10*dt);this.aimYaw=M.mix(this.aimYaw,this.yaw,turn);this.aimPitch=M.mix(this.aimPitch,this.pitch,turn);}
      if(this.state==='STOWED'&&this.pendingBody){const s=this.pendingBody;this.pendingBody=null;this.unit.animator.setState(s);}
      if(this.state==='STOWED'&&this.queuedSlot){this.selectedSlot=this.queuedSlot;this.queuedSlot=null;this.pendingDraw=true;}
      if(this.state==='STOWED'&&this.pendingDraw&&(sprint>=.15||transition))this.metrics.status='DRAW_PENDING / zwolnij sprint lub zakończ zmianę postawy';
      if(this.state==='STOWED'&&this.pendingDraw&&!transition&&sprint<.15&&this.unit.animator.state!=='PRONE_MOVE'&&this.unit.animator.state!=='REST')this.startDraw();
      if(this.state==='HELD'&&this.pendingHolster){this.state='HOLSTERING';this.actionTime=0;this.phase=0;this.pendingHolster=false;copy(this.startPose,{p:this.active.root.position,q:this.active.root.quaternion});}
      if(this.busy&&!transition) {
        const duration=this.state==='DRAWING'?this.active.def.draw:this.active.def.holster;
        this.actionTime+=dt;this.phase=clamp(this.actionTime/duration);
        if(this.phase>=1) {
          if(this.state==='DRAWING'){this.state='HELD';this.metrics.status='HELD';this.readiness=0;this.readySpring.x=0;}
          else {this.state='STOWED';this.metrics.status='STOWED';this.activeSlot=null;this.pendingShot=false;this.trigger=false;this.readiness=0;}
        }
      }
    }
    startDraw() {
      const item=this.selected;if(!item){this.pendingDraw=false;return;}
      this.stowPose(this.selectedSlot,this.stow);this.gripTask(item,this.stow,'R',this.tasks[1]);
      this.rig.modelPoint('upperArm.R',this._v);
      const reach=this.A.points['upperArm.R'].distanceTo(this.A.points['foreArm.R'])+this.A.points['foreArm.R'].distanceTo(this.A.points['hand.R']);
      if(this._v.distanceTo(this.tasks[1].p)>reach*.998){this.metrics.status='DRAW_BLOCKED / poza zasięgiem ręki';this.pendingDraw=false;this.tasks[1].weight=0;return;}
      this.refreshFreeHands();
      for(let i=0;i<2;i++)copy(this.starts[i],this.freeHands[i]);
      this.metrics.status='DRAWING';this.activeSlot=this.selectedSlot;this.state='DRAWING';this.actionTime=0;this.phase=0;this.pendingDraw=false;this.readiness=0;
      const contacts=this.unit.animator.handContacts;
      for(let i=0;i<contacts.length;i++)contacts[i].release();
    }
    boneFrame(name,out) {
      this.rig.modelPoint(name,out.p);this.model.root.getWorldQuaternion(this._rootQ);
      this.rig.byName[name].getWorldQuaternion(out.q);out.q.premultiply(this._rootQ.invert());return out;
    }
    refreshFreeHands() {
      this.boneFrame('hand.L',this.freeHands[0]);
      this.boneFrame('hand.R',this.freeHands[1]);
    }
    stowPose(slot,out) {
      const item=this.instances[slot],kind=item.def.kind;
      const socket=this.fit.socket(kind==='long'?'WEAPON_BACK':kind==='pistol'?'WEAPON_HIP':kind==='knife'?'HIP_R':'WAIST_FRONT');
      this.boneFrame(socket.bone,out);
      this._v.copy(socket.position).multiplyScalar(this.H).sub(this.A.points[socket.bone]).applyQuaternion(out.q);out.p.add(this._v);
      if(kind==='long')out.q.multiply(this._q.setFromEuler(LONG_STOW_EULER));
      else if(kind==='knife')out.q.multiply(this._q.setFromAxisAngle(V(0,0,1),Math.PI));
      else if(kind==='pistol')out.q.multiply(this._q.setFromAxisAngle(V(1,0,0),Math.PI/2));
      return out;
    }
    prepareBody() {
      // Runs before final foot/ground IK. It never resets the locomotion clock.
      if(!this.occupiesHands)return;
      const aim=this.active.def.firearm?this.readiness:0,prone=this.unit.animator.current.prone||0;
      for(const [name,weight] of BODY_AIM_BONES) {
        this._e.set((-this.aimPitch*aim-this.kick.x*.25)*weight*(1-prone*.75),this.aimYaw*weight*aim,0,'YXZ');
        this._q.setFromEuler(this._e);
        this.rig.byName[name].quaternion.multiply(this._q);
      }
      const curl=this.model.mesh.morphTargetDictionary.handsRelax;if(curl!==undefined)this.model.mesh.morphTargetInfluences[curl]=0;
    }
    holdPose(out,ready=this.readiness) {
      const item=this.active,A=this.A,H=this.H;
      const profile=R.WeaponHandlingProfiles.resolve(item.def,'1H');
      if(profile) {
        this.handling.sample(profile,this,out,ready);
        if(item.def.kind==='pistol') {
          this.handling.sample(R.WeaponHandlingProfiles.get('PISTOL_2H'),this,this.profilePose,ready);
          mix(out,out,this.profilePose,clamp(this.supportSpring.x));
        }
        if(item.def.firearm) {
          this._q.setFromAxisAngle(V(1,0,0),-this.kick.x);out.q.multiply(this._q);
          out.p.add(this._v.set(0,0,-this.shift.x).applyQuaternion(out.q));
        }
        this.fitHeldReach(out);return out;
      }
      this.boneFrame('upperArm.R',out);
      const shoulder=out.p.clone(),prone=this.unit.animator.current.prone||0;
      const yaw=this.aimYaw*ready,pitch=this.aimPitch*ready;
      this._e.set((1-ready)*.64-pitch-this.kick.x,yaw,0,'YXZ');out.q.setFromEuler(this._e);
      const arm=A.points['upperArm.R'].distanceTo(A.points['hand.R']);
      if(item.def.kind==='long') {
        out.p.copy(shoulder).add(V(.015,-.042+(1-ready)*-.045,.034));
        out.p.sub(this._v.copy(item.butt).applyQuaternion(out.q));
      } else {
        this._v.set(0,-.08-(1-ready)*arm*.43,arm*M.mix(.42,.82,ready));
        this._v.applyAxisAngle?this._v.applyAxisAngle(V(0,1,0),yaw):this._v.applyQuaternion(this._q.setFromAxisAngle(V(0,1,0),yaw));
        out.p.copy(shoulder).add(this._v);
        if(item.def.kind==='knife'||item.def.kind==='grenade'){this._e.set(.12,0,-.12,'XYZ');out.q.setFromEuler(this._e);}
      }
      if(prone>.5) {
        this.rig.modelPoint('chest',this._v);
        out.p.x=this._v.x-H*.070;out.p.y=Math.max(out.p.y,this._v.y+.04);
        out.p.z=Math.max(out.p.z,this._v.z+arm*.32);
        this._e.set(.08-pitch-this.kick.x,yaw,0,'YXZ');out.q.setFromEuler(this._e);
      }
      // Small coherent idle/gait motion belongs to the weapon rig, not each hand.
      const t=this.unit.animator.time,phase=this.unit.animator.phase*Math.PI*2;
      out.p.y+=Math.sin(t*1.4)*.002+Math.sin(phase*2)*.0015*(this.unit.speed||0);
      out.p.add(this._v.set(0,0,-this.shift.x).applyQuaternion(out.q));
      this.fitHeldReach(out);
      return out;
    }
    gripTask(item,weaponPose,side,out) {
      const grip=side==='R'?item.primary:item.secondary;if(!grip)return;
      const frame=this.A.handFrames[side];
      out.q.copy(weaponPose.q).multiply(grip.quaternion).multiply(this._q.copy(frame.quaternion).invert()).normalize();
      out.p.copy(grip.position).applyQuaternion(weaponPose.q).add(weaponPose.p)
        .sub(this._v.copy(frame.offset).applyQuaternion(out.q));
      out.attached=true;
      const profile=R.WeaponHandlingProfiles.resolve(item.def,'1H');
      out.profile=side==='L'?(item.def.kind==='pistol'?'PISTOL_SUPPORT':'SUPPORT'):(profile?profile.fingers:'PISTOL');
    }
    fitHeldReach(out) {
      // Translate the one rigid weapon, never independently slide its grips.
      const support=clamp(this.supportSpring.x),two=!!this.active.secondary&&support>.001;
      for(let pass=0;pass<3;pass++)for(const side of two?['R','L']:['R']) {
        const t=this._reachTask;this.gripTask(this.active,out,side,t);this.rig.modelPoint('upperArm.'+side,this._w);
        const reach=this.A.points['upperArm.'+side].distanceTo(this.A.points['foreArm.'+side])+this.A.points['foreArm.'+side].distanceTo(this.A.points['hand.'+side]);
        this._p.copy(t.p).sub(this._w);const d=this._p.length();
        if(d>reach*.96)out.p.addScaledVector(this._p,(reach*.96-d)/Math.max(d,.001)*(side==='L'?support:1));
      }
    }
    buildActionPose(out) {
      const item=this.active,p=this.phase,H=this.H;
      this.stowPose(this.activeSlot,this.stow);this.holdPose(this.hold,this.state==='DRAWING'?0:this.readiness);
      const right=this.A.shoulderHalf*H;
      this.boneFrame('chest',this.sidePose);
      this.sidePose.p.add(V(-right-.11,-.025,.04));
      this.sidePose.q.copy(this.hold.q);
      copy(this.extract,this.stow);
      this.extract.p.add(V(0,item.def.kind==='long'?.08:.11,.01));
      if(this.state==='DRAWING') {
        if(p<.34)copy(out,this.stow);
        else if(p<.48)mix(out,this.stow,this.extract,smooth((p-.34)/.14));
        else if(p<.73)mix(out,this.extract,this.sidePose,smooth((p-.48)/.25));
        else mix(out,this.sidePose,this.hold,smooth((p-.73)/.27));
      } else {
        // Release the support hand first; approach the socket before letting go.
        if(p<.16)copy(out,this.startPose);
        else if(p<.43)mix(out,this.startPose,this.sidePose,smooth((p-.16)/.27));
        else if(p<.70)mix(out,this.sidePose,this.extract,smooth((p-.43)/.27));
        else if(p<.86)mix(out,this.extract,this.stow,smooth((p-.70)/.16));
        else copy(out,this.stow);
      }
      return out;
    }
    supportTask(weight) {
      const t=this.tasks[0],item=this.active;
      if(!item.secondary||weight<.001)return;
      this.gripTask(item,this.output,'L',t);
      const end=this._reachTask;copy(end,t);
      // Cartesian approach from the animated FREE hand, then exact socket
      // locking. Joint-rotation blending alone takes the hand through the gun.
      mix(t,this.freeHands[0],end,weight);
      t.p.x+=16*weight*weight*(1-weight)*(1-weight)*this.H*.025;
      t.weight=1;t.curl=weight;t.attached=weight>.9995;
    }
    assign(instance,p){instance.root.position.copy(p.p);instance.root.quaternion.copy(p.q);}
    apply(context,dt=0) {
      const active=this.active;
      // Animator/facial finalization has already refreshed the posed rig.
      // Only held/action poses need fresh hand bases; stowed items read the
      // specific socket bones they use below.
      if(active)this.refreshFreeHands();
      this.metrics.handlingProfile=this.handlingProfile?.id||(this.active?'LONG_GUN_2H':'—');
      this.metrics.supportWeight=clamp(this.supportSpring.x);
      this.metrics.gripError=0;this.metrics.owners=['FREE','FREE'];this.metrics.clearance=0;
      for(const t of this.tasks){t.weight=0;t.curl=0;t.attached=false;}
      for(const slot of this.instanceSlots)if(slot!==this.activeSlot){const item=this.instances[slot];this.stowPose(slot,this.output);this.assign(item,this.output);}
      if(active) {
        if(this.state==='HELD')this.holdPose(this.output);else this.buildActionPose(this.output);
        if(this.state==='HELD') {
          // Bound ground penetration in the CURRENT rigid orientation.
          let gap=Infinity;
          for(const p of this.active.points){this._v.copy(p).applyQuaternion(this.output.q).add(this.output.p);this.rig.root.localToWorld(this._v);const height=context.surface.getHeightAtCached?context.surface.getHeightAtCached(this._v.x,this._v.z,this._terrainHeightCache):context.surface.getHeightAt(this._v.x,this._v.z);gap=Math.min(gap,this._v.y-height);}
          if(gap<.006)this.output.p.y+=Math.min(.10,.006-gap);
          this.metrics.clearance=gap;
          this.fitHeldReach(this.output);
        }
        this.assign(this.active,this.output);
        this.gripTask(this.active,this.output,'R',this.tasks[1]);this.tasks[1].weight=1;this.tasks[1].curl=1;
        if(this.state==='DRAWING'&&this.phase<.34) {
          const t=this.tasks[1],u=smooth(this.phase/.34),end={p:t.p.clone(),q:t.q.clone()};
          mix(t,this.starts[1],end,u);
          // Reach around the torso rather than drawing a chord through it.
          const arch=16*u*u*(1-u)*(1-u);t.p.x-=arch*this.H*.10;
          t.weight=smooth(this.phase/.12);t.curl=smooth((this.phase-.24)/.10);t.attached=false;
        }
        if(this.state==='HOLSTERING'&&this.phase>.86){this.tasks[1].weight=1-smooth((this.phase-.86)/.14);this.tasks[1].curl=this.tasks[1].weight;}
        if(this.active.secondary&&(this.grip==='2H'||this.supportSpring.x>.001)) {
          const weight=this.state==='DRAWING'?smooth((this.phase-.77)/.23):this.state==='HOLSTERING'?1-smooth(this.phase/.18):clamp(this.supportSpring.x);
          this.supportTask(weight);
        }
        this.solveTasks(this.tasks);
        if(!this.isScrubbing&&(this.pendingShot||this.trigger)) {
          if(this.canFire()&&this.clock-this.lastShotTime>=this.active.def.interval)this.acceptShot();
          else if(this.pendingShot)this.metrics.status=this.active.def.firearm?'FIRE_BLOCKED / chwyt, postawa lub gotowość':'NOT_A_FIREARM';
          this.pendingShot=false;
        }
      }
      const relaxed=this.unit.animator.current.prone>.01?0:(this.unit.animator.current.handCurl||0);
      if(active&&this.occupiesHands) {
        const curl=this.model.mesh.morphTargetDictionary.handsRelax;if(curl!==undefined)this.model.mesh.morphTargetInfluences[curl]=0;
        R.HandPose.apply(this.rig,'L',this.tasks[0].profile,this.tasks[0].curl,relaxed,0);
        R.HandPose.apply(this.rig,'R',this.tasks[1].profile,this.tasks[1].curl,relaxed,this.shotAge<.13?1:0);
      }
      this.updateFlash(this.shotAge);this.metrics.shots=this.shotId;
      if(!/BLOCKED|NO_WEAPON|NOT_A_FIREARM|DRAW_PENDING/.test(this.metrics.status))this.metrics.status=this.state;
      if(!active)return;
    }
    solveTasks(tasks) {
      this.ik.maxTargetError=0;
      for(let i=0;i<2;i++) {
        const t=tasks[i];if(t.weight<.001)continue;
        const side=i?'R':'L',sign=i?-1:1,upper=this.rig.byName['upperArm.'+side],lower=this.rig.byName['foreArm.'+side],hand=this.rig.byName['hand.'+side];
        const base=this._solveHandBases[i];
        base.upper.copy(upper.quaternion);base.lower.copy(lower.quaternion);base.hand.copy(hand.quaternion);
        this.rig.modelPoint('upperArm.'+side,this._pole);
        const prone=this.unit.animator.current.prone||0;
        const profile=R.WeaponHandlingProfiles.resolve(this.active?.def,'1H');
        if(profile) {
          const p=profile.pole,L=this.handling.arm,support=clamp(this.supportSpring.x);
          this._v.fromArray(p);
          if(this.active.def.kind==='pistol')this._v.lerp(this._p.fromArray(R.WeaponHandlingProfiles.get('PISTOL_2H').pole),support);
          this._v.x=Math.abs(this._v.x)*sign;this._pole.addScaledVector(this._v,L);
        } else this._pole.add(this._v.set(sign*this.H*.19,-this.H*.18,this.H*.09));
        if(prone>.5)this._pole.y=Math.max(.025,this.unit.animator.current.hips.y*this.H*.6);
        this._normal.set(0,0,1).applyQuaternion(t.q);
        this.ik.solve('upperArm.'+side,'foreArm.'+side,'hand.'+side,t.p,this._pole,true,this._normal);
        this.rig.setModelQuaternion('hand.'+side,t.q);
        if(prone>.5) {
          const surface=this.unit._animContext.surface;
          for(let pass=0;pass<2;pass++) {
            const gap=this.unit.animator.minimumClearance(this.unit.animator.supportIndices.sleeves[i],surface);
            if(gap>=.001)break;
            this._pole.y+=Math.min(this.H*.08,3*(.002-gap));
            this.ik.solve('upperArm.'+side,'foreArm.'+side,'hand.'+side,t.p,this._pole,true,this._normal);
            this.rig.setModelQuaternion('hand.'+side,t.q);
          }
        }
        upper.quaternion.copy(base.upper.slerp(upper.quaternion,t.weight));
        lower.quaternion.copy(base.lower.slerp(lower.quaternion,t.weight));
        hand.quaternion.copy(base.hand.slerp(hand.quaternion,t.weight));
        // Keep the world-space grip measurement precise. Outside that read,
        // the renderer refreshes bone matrices before drawing; traversing the
        // full arm and all finger descendants here duplicated that work.
        if(t.weight>.98&&t.attached){this.rig.modelPoint('hand.'+side,this._w);this.metrics.gripError=Math.max(this.metrics.gripError,this._w.distanceTo(t.p));}
        this.metrics.owners[i]=t.attached?'WEAPON':'REACH';
      }
    }
    acceptShot() {
      this.shotId++;this.lastShotTime=this.clock;this.shotAge=0;this.metrics.status='HELD';
      const strength=this.active.def.kind==='pistol'?M.mix(1,.72,clamp(this.supportSpring.x)):1;
      this.kick.v=Math.min(2,this.kick.v+this.active.def.kick*4*strength);
      this.shift.v=Math.min(.5,this.shift.v+this.active.def.kick*.50*strength);
      // Exactly one semantic event per accepted pulse. No projectile/damage simulation.
      if(typeof this.onShot==='function')this.onShot({id:this.shotId,time:this.clock,slot:this.activeSlot,definitionId:this.active.item.definitionId});
    }
    updateFlash(age) {
      for(const slot of this.instanceSlots) {
        const item=this.instances[slot];
        item.updateVisual(item===this.active?age:-1);
        if(!item.flash)continue;
        const on=item===this.active&&age>=0&&age<.06;
        if(item.flash.visible!==on)item.flash.visible=on;
        const opacity=on?1-age/.06:0,material=item.flash.material[0];
        if(material.opacity!==opacity)material.opacity=opacity;
        const scale=on?.75+.65*Math.sin(Math.PI*clamp(age/.06)):1,currentScale=item.flash.scale.x;
        if(currentScale!==scale||item.flash.scale.y!==scale||item.flash.scale.z!==scale)item.flash.scale.set(scale,scale,scale);
      }
    }
    seekAction(kind,phase) {
      if(!this.selected)throw new Error('Najpierw wybierz posiadaną broń.');
      phase=bounded(phase,0,1,'Faza akcji');if(!['DRAW','HOLSTER','SHOT'].includes(kind))throw new Error('Nieznana akcja.');
      if(kind==='SHOT'&&!this.selected.def.firearm)throw new Error('Ten przedmiot nie strzela.');
      this.isScrubbing=true;this.supportSpring.x=(this.selected.def.profiles[0]==='2H'||this.requestedGrip==='2H')?1:0;this.supportSpring.v=0;this.pendingDraw=false;this.pendingHolster=false;this.pendingShot=false;this.trigger=false;
      this.activeSlot=this.selectedSlot;this.phase=phase;
      if(kind==='SHOT') {
        this.state='HELD';this.readiness=this.requestedReadiness=1;this.readySpring.x=1;
        const age=phase*.5,support=clamp(this.supportSpring.x),pistol=this.active.def.kind==='pistol',strength=pistol?M.mix(1,.72,support):1,recovery=pistol?M.mix(19,25,support):22;this.kick.x=this.active.def.kick*4*strength*age*Math.exp(-recovery*age);this.shift.x=this.active.def.kick*.50*strength*age*Math.exp(-25*age);this.shotAge=age;
      } else {
        this.state=kind==='DRAW'?'DRAWING':'HOLSTERING';
        this.actionTime=phase*(kind==='DRAW'?this.active.def.draw:this.active.def.holster);
        this.refreshFreeHands();
        for(let i=0;i<2;i++)copy(this.starts[i],this.freeHands[i]);
        this.holdPose(this.startPose,this.requestedReadiness);
        this.kick.x=this.shift.x=0;this.shotAge=10;
      }
      this.metrics.status=this.state;
    }
    capture() {
      const s=this.current;s.activeSlot=this.activeSlot;s.shotAge=this.shotAge;
      for(const slot of this.instanceSlots){
        const item=this.instances[slot],target=s.objects[slot];
        target.p.copy(item.root.position);target.q.copy(item.root.quaternion);
      }
      for(let i=0;i<2;i++){copy(s.tasks[i],this.tasks[i]);Object.assign(s.tasks[i],{weight:this.tasks[i].weight,curl:this.tasks[i].curl,attached:this.tasks[i].attached,profile:this.tasks[i].profile});}
    }
    copySnapshot(a,b) {
      a.activeSlot=b.activeSlot;a.shotAge=b.shotAge;
      for(const slot of this.instanceSlots){
        copy(a.objects[slot],b.objects[slot]);
      }
      for(let i=0;i<2;i++){copy(a.tasks[i],b.tasks[i]);Object.assign(a.tasks[i],{weight:b.tasks[i].weight,curl:b.tasks[i].curl,attached:b.tasks[i].attached,profile:b.tasks[i].profile});}
    }
    syncSnapshots(){this.copySnapshot(this.previous,this.current);}
    renderArmMask(solveGrip = true) {
      if(!solveGrip||!this.active||this.current.activeSlot!==this.previous.activeSlot)return 0;
      let mask=0;
      for(let i=0;i<2;i++)if(this.previous.tasks[i].attached&&this.current.tasks[i].attached)mask|=i?2:1;
      return mask;
    }
    render(alpha,solveGrip = true) {
      for(let i=0;i<this.instanceSlots.length;i++){
        const slot=this.instanceSlots[i],item=this.instances[slot];
        const b=this.current.objects[slot],a=this.previous.objects[slot]||b;if(!b)continue;
        if(a.p.x===b.p.x&&a.p.y===b.p.y&&a.p.z===b.p.z&&
          a.q.x===b.q.x&&a.q.y===b.q.y&&a.q.z===b.q.z&&a.q.w===b.q.w)continue;
        mix(this.output,a,b,alpha);this.assign(item,this.output);
      }
      if(solveGrip&&this.active&&this.current.activeSlot===this.previous.activeSlot) {
        const tasks=this._renderTasks,cache=this._renderGripCache,active=this.active,unitRoot=this.unit.root,
          surface=this.unit._animContext.surface,heights=surface&&surface.heights,prone=this.unit.animator.current.prone,
          hips=this.rig.byName.hips.position,poseHips=this.unit.animator.current.hips,weaponPosition=active.root.position,weaponQuaternion=active.root.quaternion;
        const stableSurface=prone<=.5||surface===R.FlatSurface||!!(R.TerrainSystem&&surface instanceof R.TerrainSystem&&heights);
        let unchanged=stableSurface&&cache.ready&&cache.item===active&&cache.support===this.supportSpring.x&&cache.prone===prone&&
          cache.surface===surface&&cache.heights===heights&&
          cache.weaponPosition.x===weaponPosition.x&&cache.weaponPosition.y===weaponPosition.y&&cache.weaponPosition.z===weaponPosition.z&&
          sameQuaternion(cache.weaponQuaternion,weaponQuaternion)&&
          cache.rootPosition.x===unitRoot.position.x&&cache.rootPosition.y===unitRoot.position.y&&cache.rootPosition.z===unitRoot.position.z&&
          cache.rootScale.x===unitRoot.scale.x&&cache.rootScale.y===unitRoot.scale.y&&cache.rootScale.z===unitRoot.scale.z&&
          sameQuaternion(cache.rootQuaternion,unitRoot.quaternion)&&
          cache.hipsPosition.x===hips.x&&cache.hipsPosition.y===hips.y&&cache.hipsPosition.z===hips.z&&
          cache.poseHipsPosition.x===poseHips.x&&cache.poseHipsPosition.y===poseHips.y&&cache.poseHipsPosition.z===poseHips.z;
        for(let i=0;i<this._renderGripBodyBones.length;i++)if(!sameQuaternion(cache.bodyQuaternions[i],this._renderGripBodyBones[i].quaternion))unchanged=false;
        for(let i=0;i<2;i++) {
          const a=this.previous.tasks[i],b=this.current.tasks[i];mix(tasks[i],a,b,alpha);
          tasks[i].weight=M.mix(a.weight,b.weight,alpha);tasks[i].curl=M.mix(a.curl,b.curl,alpha);tasks[i].attached=b.attached;tasks[i].profile=b.profile;
          if(a.attached&&b.attached)this.gripTask(this.active,{p:this.active.root.position,q:this.active.root.quaternion},i?'R':'L',tasks[i]);
          const arms=this._renderGripArms[i],input=cache.inputs[i];
          if(!sameGripTask(cache.tasks[i],tasks[i])||!sameQuaternion(input.upper,arms.upper.quaternion)||
            !sameQuaternion(input.lower,arms.lower.quaternion)||!sameQuaternion(input.hand,arms.hand.quaternion))unchanged=false;
        }
        if(unchanged){
          for(let i=0;i<2;i++){
            const arms=this._renderGripArms[i],output=cache.outputs[i];
            arms.upper.quaternion.copy(output.upper);arms.lower.quaternion.copy(output.lower);arms.hand.quaternion.copy(output.hand);
          }
        }else{
          cache.ready=false;cache.item=active;cache.support=this.supportSpring.x;cache.prone=prone;cache.surface=surface;cache.heights=heights;
          cache.weaponPosition.copy(weaponPosition);cache.weaponQuaternion.copy(weaponQuaternion);
          cache.rootPosition.copy(unitRoot.position);cache.rootQuaternion.copy(unitRoot.quaternion);cache.rootScale.copy(unitRoot.scale);cache.hipsPosition.copy(hips);cache.poseHipsPosition.copy(poseHips);
          for(let i=0;i<this._renderGripBodyBones.length;i++)cache.bodyQuaternions[i].copy(this._renderGripBodyBones[i].quaternion);
          for(let i=0;i<2;i++){
            const arms=this._renderGripArms[i],input=cache.inputs[i];
            copy(cache.tasks[i],tasks[i]);cache.tasks[i].weight=tasks[i].weight;cache.tasks[i].curl=tasks[i].curl;
            cache.tasks[i].attached=tasks[i].attached;cache.tasks[i].profile=tasks[i].profile;
            input.upper.copy(arms.upper.quaternion);input.lower.copy(arms.lower.quaternion);input.hand.copy(arms.hand.quaternion);
          }
          const error=this.metrics.gripError,ownerL=this.metrics.owners[0],ownerR=this.metrics.owners[1];this.solveTasks(tasks);this.metrics.gripError=error;this.metrics.owners[0]=ownerL;this.metrics.owners[1]=ownerR;
          for(let i=0;i<2;i++){
            const arms=this._renderGripArms[i],output=cache.outputs[i];
            output.upper.copy(arms.upper.quaternion);output.lower.copy(arms.lower.quaternion);output.hand.copy(arms.hand.quaternion);
          }
          cache.ready=stableSurface;
        }
      }
      // The shot is an event, not a wrapping scalar to interpolate backwards.
      this.updateFlash(this.current.shotAge<this.previous.shotAge?this.current.shotAge:M.mix(this.previous.shotAge,this.current.shotAge,alpha));
    }
    exportState(){return {selectedSlot:this.selectedSlot,activeSlot:this.activeSlot,state:this.state,phase:this.phase,actionTime:this.actionTime,readiness:this.readiness,requestedReadiness:this.requestedReadiness,requestedGrip:this.requestedGrip,supportWeight:this.supportSpring.x,supportVelocity:this.supportSpring.v,yaw:this.yaw,pitch:this.pitch,aimYaw:this.aimYaw,aimPitch:this.aimPitch};}
    restoreState(data) {
      if(!data||!this.instances[data.selectedSlot])return;
      for(const key of ['selectedSlot','phase','actionTime','readiness','requestedReadiness','yaw','pitch','aimYaw','aimPitch'])if(data[key]!==undefined)this[key]=data[key];
      this.setGrip(data.requestedGrip||'AUTO');
      if(this.instances[data.activeSlot]){this.activeSlot=data.activeSlot;this.state=data.state;}
      this.supportSpring.x=Number.isFinite(data.supportWeight)?clamp(data.supportWeight):(this.grip==='2H'?1:0);this.supportSpring.v=Number.isFinite(data.supportVelocity)?data.supportVelocity:0;
      this.readySpring.x=this.readiness;this.readySpring.v=0;
      for(let i=0;i<2;i++)this.boneFrame('hand.'+(i?'R':'L'),this.starts[i]);
      if(this.active)this.holdPose(this.startPose,this.readiness);
      this.isScrubbing=true;this.trigger=false;this.pendingShot=false;
    }
    dispose(){for(const slot of this.instanceSlots)this.instances[slot].dispose();this.instances={};this.instanceSlots.length=0;this.trigger=false;}
  };
})();
