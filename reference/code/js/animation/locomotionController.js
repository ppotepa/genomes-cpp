(function () {
  'use strict';
  const R=window.RTS,M=R.Math,P=R.PostureProfile;
  function spring(state,target,omega,dt) {
    if(!(dt>0))return;
    const offset=state.value-target, t=(state.velocity+omega*offset)*dt, e=Math.exp(-omega*dt);
    state.value=target+(offset+t)*e;
    state.velocity=(state.velocity-omega*t)*e;
  }
  function filter(state,target,omega,dt,immediate) {
    if(immediate){state.value=target;state.velocity=0;}else spring(state,target,omega,dt);
  }
  R.LocomotionController=class LocomotionController {
    constructor(anatomy,phenotype) {
      this.anatomy=anatomy;this.phenotype=phenotype;
      this.requestedCrouch=0;this.requestedSpeedMps=0;
      this.actualCrouch=0;this.actualSpeedMps=0;this.targetSpeedMps=0;
      this.runWeight=0;this.sprintWeight=0;this.limitReason='';this.custom=false;
      this.depth={value:0,velocity:0};this.gait={};this.profile={};
      this._cycle={value:anatomy.legLength*1.5,velocity:0};
      this._duty={value:.62,velocity:0};this._run={value:0,velocity:0};
      this._sprint={value:0,velocity:0};this._lift={value:0,velocity:0};
      this.maxCrouch=1;this.depthReason='';this.constraintDepth=1;
      this.profileSample={};this.recalculateLimits();this.sampleGait(0,true);
      this._settledUpdateValid=false;
      this._gaitCacheValid=false;
    }
    recalculateLimits() {
      const b=this.anatomy.body;
      // Conservative visual clearance envelope for bulky thighs/abdomen. This
      // is NOT a collision solver or a biological flexibility gene.
      const bulkyLegs=Math.max(0,(b.legThicknessScale||1)-1.13);
      const bulkyWaist=Math.max(0,(b.waistDepthScale||1)-1.13);
      this.maxCrouch=M.clamp(1-bulkyLegs*.22-bulkyWaist*.16,.88,1);
      this.depthReason=this.maxCrouch<.999?'BODY_CLEARANCE':'';
    }
    set(values) {
      if(!values||typeof values!=='object')throw new TypeError('Oczekiwano parametrów lokomocji.');
      for(const key of ['crouch','speedMps'])if(values[key]!==undefined&&!Number.isFinite(values[key]))throw new RangeError(key+' musi być skończoną liczbą.');
      if(values.crouch!==undefined)this.requestedCrouch=M.clamp(values.crouch,0,1);
      if(values.speedMps!==undefined)this.requestedSpeedMps=M.clamp(values.speedMps,0,this.phenotype.runSpeed);
      this.custom=true;return this;
    }
    setPreset(state,immediate=false) {
      if(!P.isBiped(state))return;
      this.set(P.preset(state,this.phenotype));this.custom=false;
      if(immediate)this.snap();
    }
    snap() {
      this._settledUpdateValid=false;
      this.actualCrouch=Math.min(this.requestedCrouch,this.maxCrouch,this.constraintDepth);
      this.depth.value=this.actualCrouch;this.depth.velocity=0;
      this.actualSpeedMps=Math.min(this.requestedSpeedMps,P.speedLimit(this.actualCrouch,this.phenotype));
      this.targetSpeedMps=this.actualSpeedMps;this.sampleGait(0,true);this.describeLimit();
    }
    update(dt,actualSpeed) {
      this.actualSpeedMps=Math.max(0,actualSpeed);
      const wanted=Math.min(this.requestedCrouch,this.maxCrouch,this.constraintDepth);
      // Once the depth spring has landed exactly at its target, repeating the
      // same fixed-step update only re-evaluates a zero-offset spring and the
      // unchanged diagnostic limit. Keep an exact input signature so public
      // property edits and all normal set()/constraint changes wake the path.
      if(dt>0&&this._settledUpdateValid&&this.actualSpeedMps===this._settledSpeed&&
        wanted===this._settledWanted&&this.actualCrouch===this._settledActualCrouch&&
        this.depth.value===wanted&&this.depth.velocity===0&&
        this.requestedCrouch===this._settledRequestedCrouch&&
        this.requestedSpeedMps===this._settledRequestedSpeed&&
        this.maxCrouch===this._settledMaxCrouch&&this.constraintDepth===this._settledConstraintDepth) return;
      this._settledUpdateValid=false;
      // Brake before descending; when standing up, unlock speed progressively.
      const target=wanted>this.actualCrouch
        ?Math.min(wanted,Math.max(this.actualCrouch,P.depthAtSpeed(this.actualSpeedMps,this.phenotype)))
        :wanted;
      spring(this.depth,target,9,dt);
      this.depth.value=M.clamp(this.depth.value,0,this.maxCrouch);
      this.actualCrouch=this.depth.value;
      this.targetSpeedMps=Math.min(this.requestedSpeedMps,
        P.speedLimit(wanted,this.phenotype),P.speedLimit(this.actualCrouch,this.phenotype));
      this.describeLimit();
      if(dt>0&&this.depth.value===wanted&&this.depth.velocity===0){
        this._settledUpdateValid=true;this._settledSpeed=this.actualSpeedMps;this._settledWanted=wanted;this._settledActualCrouch=this.actualCrouch;
        this._settledRequestedCrouch=this.requestedCrouch;this._settledRequestedSpeed=this.requestedSpeedMps;
        this._settledMaxCrouch=this.maxCrouch;this._settledConstraintDepth=this.constraintDepth;
      }
    }
    describeLimit() {
      this.limitReason=this.requestedCrouch>Math.min(this.maxCrouch,this.constraintDepth)+.001
        ?(this.constraintDepth<this.maxCrouch?'LEG_REACH':this.depthReason)
        :this.requestedSpeedMps>P.speedLimit(Math.min(this.requestedCrouch,this.actualCrouch),this.phenotype)+.01?'POSTURE_SPEED'
        :this.requestedCrouch>this.actualCrouch+.025&&this.actualSpeedMps>P.speedLimit(this.requestedCrouch,this.phenotype)+.03?'BRAKING':'';
    }
    sampleGait(dt,immediate=false) {
      const a=this.anatomy,p=this.phenotype,b=a.body,hipOffset=a.points.hips.y-a.points['thigh.L'].y;
      const bulk=b.legThicknessScale||1;
      const stable=this._gaitCacheValid&&this._gaitCrouch===this.actualCrouch&&this._gaitSpeed===this.actualSpeedMps&&
        this._gaitHeight===a.height&&this._gaitLegLength===a.legLength&&this._gaitAnkle===a.ankleHeight&&
        this._gaitHipOffset===hipOffset&&this._gaitHipHalf===a.hipHalf&&this._gaitBulk===bulk&&
        this._gaitSpeedMultiplier===p.speedMultiplier&&this._gaitRunSpeed===p.runSpeed&&
        this._cycle.value===this.profile.cycleM&&this._cycle.velocity===0&&
        this._duty.value===this.profile.duty&&this._duty.velocity===0&&
        this._run.value===this.profile.run&&this._run.velocity===0&&
        this._sprint.value===this.profile.sprint&&this._sprint.velocity===0&&
        this._lift.value===this.profile.liftM&&this._lift.velocity===0;
      if(!immediate&&stable)return this.gait;
      this._gaitCacheValid=false;
      P.gait(this.actualCrouch,this.actualSpeedMps,this.anatomy,this.phenotype,this.profile);
      filter(this._cycle,this.profile.cycleM,10,dt,immediate);
      filter(this._duty,this.profile.duty,8,dt,immediate);
      filter(this._run,this.profile.run,8,dt,immediate);
      filter(this._sprint,this.profile.sprint,10,dt,immediate);
      filter(this._lift,this.profile.liftM,10,dt,immediate);
      Object.assign(this.gait,this.profile,{cycleM:this._cycle.value,duty:M.clamp(this._duty.value,.36,.78),
        run:M.clamp(this._run.value,0,1),sprint:M.clamp(this._sprint.value,0,1),liftM:Math.max(0,this._lift.value)});
      this.gait.cadence=this.actualSpeedMps/this.gait.cycleM;
      this.runWeight=this.gait.run;this.sprintWeight=this.gait.sprint;
      P.sample(this.actualCrouch,this.anatomy,this.profileSample);
      this._gaitCacheValid=true;this._gaitCrouch=this.actualCrouch;this._gaitSpeed=this.actualSpeedMps;
      this._gaitHeight=a.height;this._gaitLegLength=a.legLength;this._gaitAnkle=a.ankleHeight;
      this._gaitHipOffset=hipOffset;this._gaitHipHalf=a.hipHalf;this._gaitBulk=bulk;
      this._gaitSpeedMultiplier=p.speedMultiplier;this._gaitRunSpeed=p.runSpeed;
      return this.gait;
    }
  };
})();
