(function () {
  'use strict';

  const R = window.RTS;
  const M = R.Math;
  const DEG = Math.PI / 180;
  const FACE_SIDES=['L','R'],FACE_SIGNS=[1,-1];

  const CHANNELS = [
    'eyeOpen','eyeSquint','browInnerUp','browOuterUp','browDown',
    'jawOpen','mouthOpen','mouthStretch','mouthCornerUp','mouthCornerDown',
    'lipPress','cheekRaise'
  ];
  const EXPRESSION_NAMES=new Set(Object.values(R.ExpressionState));

  const PRESETS = {
    NEUTRAL: {},
    ALERT: { eyeOpen:.18, browInnerUp:.22, browOuterUp:.13, lipPress:.025 },
    FEAR: { eyeOpen:.58, browInnerUp:.58, browOuterUp:.31, jawOpen:.18, mouthOpen:.36, mouthStretch:.15 },
    ANGER: { eyeSquint:.34, browDown:.58, jawOpen:.008, lipPress:.62, mouthCornerDown:.18 },
    PAIN: { eyeSquint:.64, browInnerUp:.43, browDown:.14, jawOpen:.12, mouthOpen:.17, mouthStretch:.13, mouthCornerDown:.38, cheekRaise:.24 },
    FATIGUE: { eyeOpen:-.72, eyeSquint:.16, browOuterUp:.06, jawOpen:.05, mouthOpen:.07, mouthCornerDown:.13 },
    EYES_CLOSED: {}
  };
  // Presets are immutable. Keep only authored channels so each facial blend
  // visits meaningful contributions instead of probing every channel.
  const PRESET_CHANNELS=Object.fromEntries(Object.entries(PRESETS).map(([name,preset])=>{
    const flat=[];for(const channel of Object.keys(preset))flat.push(channel,preset[channel]);return[name,flat];
  }));

  const CHANNEL_LIMITS={
    eyeOpen:[-.78,.82],eyeSquint:[0,.72],browInnerUp:[-.28,.62],browOuterUp:[-.24,.42],browDown:[0,.72],
    jawOpen:[0,.42],mouthOpen:[0,.66],mouthStretch:[0,.58],mouthCornerUp:[0,.48],mouthCornerDown:[0,.58],lipPress:[0,.72],cheekRaise:[0,.42]
  };
  const CHANNEL_OMEGAS=Object.fromEntries(CHANNELS.map(name=>[name,name.startsWith('eye')?12:10]));
  const SPRING_OMEGAS=[4.4,4.2,7.8,27,25,22,20,8.5,12,10],SPRING_DECAYS=new Float64Array(SPRING_OMEGAS.length);
  let springDecayDt=NaN;
  function springDecay(dt,omega){
    if(dt!==springDecayDt){springDecayDt=dt;SPRING_DECAYS.fill(NaN);}
    for(let i=0;i<SPRING_OMEGAS.length;i++)if(SPRING_OMEGAS[i]===omega){
      let decay=SPRING_DECAYS[i];if(Number.isNaN(decay))SPRING_DECAYS[i]=decay=Math.exp(-omega*dt);return decay;
    }
    return Math.exp(-omega*dt);
  }

  function springStep(state, target, dt, omega) {
    if (!(dt > 0)) {
      state.value = target;
      state.velocity = 0;
      return;
    }
    const y = state.value - target;
    const e = springDecay(dt,omega);
    const temp = (state.velocity + omega * y) * dt;
    state.value = target + (y + temp) * e;
    state.velocity = (state.velocity - omega * temp) * e;
  }
  function settledSpring(state,target){return Math.abs(state.value-target)<1e-8&&Math.abs(state.velocity)<1e-8;}

  function headPart(angle,deadZone,maxAngle,factor){
    const a=Math.abs(angle);
    if(a<=deadZone)return 0;
    return Math.sign(angle)*Math.min(maxAngle,(a-deadZone)*factor);
  }

  R.FaceAnimator = class FaceAnimator {
    constructor(model, genome, seed) {
      this.model = model;
      this.rig = model.rig;
      this.controlBindings=Object.create(null);
      const bindControl=name=>{const index=this.rig.index[name],binding={bone:this.rig.byName[name],rest:this.rig.rest[index]};this.controlBindings[name]=binding;return binding;};
      this.neckControl=bindControl('neck');this.headControl=bindControl('head');
      this.sideControls=FACE_SIDES.map(side=>({
        eye:bindControl('eye.'+side),upperLid:bindControl('lidUpper.'+side),lowerLid:bindControl('lidLower.'+side),
        innerBrow:bindControl('browInner.'+side),outerBrow:bindControl('browOuter.'+side),
        cheek:bindControl('cheek.'+side),mouthCorner:bindControl('mouthCorner.'+side)
      }));
      this.jawControl=bindControl('jaw');this.upperLipControl=bindControl('mouthUpper');this.lowerLipControl=bindControl('mouthLower');
      this.face = model.anatomy.face;
      this.layout = model.anatomy.faceLayout;
      this._bodyHead = new THREE.Quaternion();
      this._appliedHead = new THREE.Quaternion();
      this._bodyNeck = new THREE.Quaternion();
      this._appliedNeck = new THREE.Quaternion();
      this._hasHeadLayer = false;
      this._headLayerYaw=NaN;this._headLayerPitch=NaN;
      this.genome = genome.face;
      this.seed = seed >>> 0;

      this.random = new R.SeededRandom((this.seed ^ 0x9e3779b9) >>> 0);
      this.state = R.ExpressionState.NEUTRAL;
      this.intensity = 1;
      this.forcedEyesClosed = false;

      this.weights = {};
      this.weightTargets = {};
      for (const name of Object.values(R.ExpressionState)) {
        if (name === 'NEUTRAL' || name === 'EYES_CLOSED') continue;
        this.weights[name] = { value:0, velocity:0 };
        this.weightTargets[name] = 0;
      }
      this.expressionNames=Object.keys(this.weights);
      this._expressionsSettled=true;

      this.channels = {};
      CHANNELS.forEach(name => this.channels[name] = { value:0, velocity:0 });
      this.expressionTargets={};
      for(const name of CHANNELS)this.expressionTargets[name]=0;

      this.blink = 0;
      this.blinkTime = 0;
      this.blinkActive = false;
      this.nextBlink = this.randomBlinkDelay();

      this.lookTargetWorld = null;

      // Attention is the slow, visible head/neck direction. Gaze adds the
      // fast eye component and tiny saccades on top. Keeping those layers
      // separate prevents the head from twitching with every eye movement.
      this.attentionYaw = { value:0, velocity:0 };
      this.attentionPitch = { value:0, velocity:0 };
      this.attentionTargetYaw = 0;
      this.attentionTargetPitch = 0;
      this.attentionTime = 0;
      this.nextAttention = .55 + this.random.next()*1.15;

      this.saccadeYaw = { value:0, velocity:0 };
      this.saccadePitch = { value:0, velocity:0 };
      this.saccadeTargetYaw = 0;
      this.saccadeTargetPitch = 0;
      this.saccadeTime = 0;
      this.nextSaccade = this.randomSaccadeDelay();

      this.gazeYaw = { value:0, velocity:0 };
      this.gazePitch = { value:0, velocity:0 };
      this.gazeTargetYaw = 0;
      this.gazeTargetPitch = 0;
      this._gazeSettled=false;

      this._headWorld = new THREE.Vector3();
      this._direction = new THREE.Vector3();
      this._rootQ = new THREE.Quaternion();
      this._e = new THREE.Euler();
      this._q = new THREE.Quaternion();
      this._rest = new THREE.Vector3();

      this.metrics = {
        eyeOpen: this.face.neutralEyeOpen,
        blink: 0,
        gazeYaw: 0,
        gazePitch: 0
      };
    }

    randomBlinkDelay() {
      const variation = .72 + this.random.next() * .62;
      return this.face.blinkInterval * variation;
    }

    randomAttentionDelay() {
      return (1.45 + this.random.next()*3.1) / Math.max(.72, this.face.gazeRestlessness);
    }

    chooseAttentionTarget() {
      // Most glances stay near the forward hemisphere, with occasional
      // clearly visible side looks. This is intentionally larger than a
      // micro-saccade so the head can be read from RTS camera distances.
      const restlessness=M.clamp(this.face.gazeRestlessness,.65,1.45);
      const centered=this.random.next()<.18;
      let yaw=0;
      if(!centered){
        const magnitude=(7+this.random.next()*31)*DEG*restlessness;
        yaw=(this.random.next()<.5?-1:1)*magnitude;
      }
      const pitch=(this.random.next()*2-1)*(4.5+this.random.next()*4.5)*DEG;
      this.attentionTargetYaw=M.clamp(yaw,-42*DEG,42*DEG);
      this.attentionTargetPitch=M.clamp(pitch,-10*DEG,9*DEG);
    }

    randomSaccadeDelay() {
      return (.55 + this.random.next()*1.55) / this.face.gazeRestlessness;
    }

    setExpression(name, intensity = 1) {
      if (!EXPRESSION_NAMES.has(name)) {
        throw new Error('Nieznana mimika ' + name);
      }
      const nextIntensity=M.clamp(Number(intensity)||0,0,1);
      if(name===this.state&&nextIntensity===this.intensity&&this.forcedEyesClosed===(name==='EYES_CLOSED'))return;
      this.state = name;
      this.intensity = nextIntensity;
      Object.keys(this.weightTargets).forEach(key => this.weightTargets[key] = 0);
      this.forcedEyesClosed = name === 'EYES_CLOSED';
      if (name !== 'NEUTRAL' && name !== 'EYES_CLOSED') {
        this.weightTargets[name] = this.intensity;
      }
      this._expressionsSettled=false;
    }

    setExpressionWeight(name, value) {
      if (!(name in this.weightTargets)) throw new Error('Nieznana mimika ' + name);
      const target=M.clamp(Number(value)||0,0,1);
      if(this.state==='BLEND'&&!this.forcedEyesClosed&&this.weightTargets[name]===target)return;
      this.weightTargets[name] = target;
      this.state = 'BLEND';
      this.forcedEyesClosed = false;
      this._expressionsSettled=false;
    }

    setEyesClosed(value) {
      const closed=!!value;if(closed===this.forcedEyesClosed)return;this.forcedEyesClosed=closed;
    }

    setLookTargetWorld(target) {
      this.lookTargetWorld = target ? target.clone() : null;
      this._gazeSettled=false;
    }

    clearLookTarget() {
      this.lookTargetWorld = null;
      this._gazeSettled=false;
      this.attentionTime = 0;
      this.nextAttention = .25 + this.random.next()*.45;
    }

    updateBlink(dt) {
      if (this.forcedEyesClosed) {
        this.blink = 1;
        return;
      }

      if (!this.blinkActive) {
        // Between blinks the authored blink channel is exactly zero. Keep
        // advancing only its event clock; do not rewrite eyelid state or
        // recompute a value that cannot change until the next event.
        const changed=this.blink!==0;
        this.blink=0;
        this.nextBlink -= dt;
        if (this.nextBlink <= 0) {
          this.blinkActive = true;
          this.blinkTime = 0;
          return true;
        }
        return changed;
      }

      this.blinkTime += dt;
      const duration = this.face.blinkDuration;
      const close = duration * .34;
      const hold = duration * .12;
      const open = Math.max(.001, duration-close-hold);

      if (this.blinkTime < close) {
        this.blink = R.GaitProfile.smooth5(this.blinkTime/close);
      } else if (this.blinkTime < close+hold) {
        this.blink = 1;
      } else if (this.blinkTime < duration) {
        this.blink = 1-R.GaitProfile.smooth5((this.blinkTime-close-hold)/open);
      } else {
        this.blink = 0;
        this.blinkActive = false;
        this.nextBlink = this.randomBlinkDelay();
        return true;
      }
      return true;
    }

    updateGaze(dt) {
      const sleeping=this._gazeSettled&&!this.lookTargetWorld&&dt>0;
      if(sleeping){
        this.attentionTime+=dt;this.saccadeTime+=dt;
        if(this.attentionTime<this.nextAttention&&this.saccadeTime<this.nextSaccade)return;
      }
      if (this.lookTargetWorld) {
        // World target is measured against the body-driven head pose before
        // the gaze layer is applied. Head/neck then take the large component;
        // the eyes keep the residual angle.
        this.headControl.bone.getWorldPosition(this._headWorld);
        this._direction.copy(this.lookTargetWorld).sub(this._headWorld).normalize();
        this.headControl.bone.getWorldQuaternion(this._rootQ);
        this._direction.applyQuaternion(this._rootQ.invert());
        const horizontal = Math.hypot(this._direction.x, this._direction.z);
        this.attentionTargetYaw = M.clamp(Math.atan2(this._direction.x, this._direction.z), -55*DEG, 55*DEG);
        this.attentionTargetPitch = M.clamp(-Math.atan2(this._direction.y, horizontal), -24*DEG, 22*DEG);
        this.saccadeTargetYaw = 0;
        this.saccadeTargetPitch = 0;
      } else {
        if(!sleeping)this.attentionTime += dt;
        if (this.attentionTime >= this.nextAttention) {
          this.attentionTime = 0;
          this.nextAttention = this.randomAttentionDelay();
          this.chooseAttentionTarget();
        }

        if(!sleeping)this.saccadeTime += dt;
        if (this.saccadeTime >= this.nextSaccade) {
          this.saccadeTime = 0;
          this.nextSaccade = this.randomSaccadeDelay();
          const amp=(1.0+this.random.next()*1.6)*DEG*this.face.gazeRestlessness;
          this.saccadeTargetYaw=(this.random.next()*2-1)*amp;
          this.saccadeTargetPitch=(this.random.next()*2-1)*amp*.55;
        }
      }

      springStep(this.attentionYaw,this.attentionTargetYaw,dt,this.lookTargetWorld?7.8:4.4);
      springStep(this.attentionPitch,this.attentionTargetPitch,dt,this.lookTargetWorld?7.8:4.2);
      springStep(this.saccadeYaw,this.saccadeTargetYaw,dt,27);
      springStep(this.saccadePitch,this.saccadeTargetPitch,dt,25);

      this.gazeTargetYaw=this.attentionYaw.value+this.saccadeYaw.value;
      this.gazeTargetPitch=this.attentionPitch.value+this.saccadePitch.value;
      springStep(this.gazeYaw,this.gazeTargetYaw,dt,22);
      springStep(this.gazePitch,this.gazeTargetPitch,dt,20);
      this._gazeSettled=!this.lookTargetWorld&&
        settledSpring(this.attentionYaw,this.attentionTargetYaw)&&settledSpring(this.attentionPitch,this.attentionTargetPitch)&&
        settledSpring(this.saccadeYaw,this.saccadeTargetYaw)&&settledSpring(this.saccadePitch,this.saccadeTargetPitch)&&
        settledSpring(this.gazeYaw,this.gazeTargetYaw)&&settledSpring(this.gazePitch,this.gazeTargetPitch);
    }

    updateExpressions(dt) {
      if(this._expressionsSettled&&dt>0)return;
      for (const name of this.expressionNames) {
        springStep(this.weights[name], this.weightTargets[name], dt, 8.5);
      }

      const targets=this.expressionTargets;
      for(const channel of CHANNELS)targets[channel]=0;
      for (const expression of this.expressionNames) {
        const weightState=this.weights[expression];
        const weight = weightState.value;
        if(weight===0)continue;
        const contributions=PRESET_CHANNELS[expression];
        for(let i=0;i<contributions.length;i+=2)targets[contributions[i]]+=contributions[i+1]*weight;
      }

      const f=this.face;
      targets.eyeOpen *= f.expressionScale*f.eyeExpressionScale;
      targets.eyeSquint *= f.expressionScale*f.eyeExpressionScale;
      targets.browInnerUp *= f.expressionScale*f.browExpressionScale;
      targets.browOuterUp *= f.expressionScale*f.browExpressionScale;
      targets.browDown *= f.expressionScale*f.browExpressionScale;
      targets.jawOpen *= f.expressionScale*f.mouthExpressionScale;
      targets.mouthOpen *= f.expressionScale*f.mouthExpressionScale;
      targets.mouthStretch *= f.expressionScale*f.mouthExpressionScale;
      targets.mouthCornerUp *= f.expressionScale*f.mouthExpressionScale;
      targets.mouthCornerDown *= f.expressionScale*f.mouthExpressionScale;
      targets.lipPress *= f.expressionScale*f.mouthExpressionScale;
      targets.cheekRaise *= f.expressionScale;

      let settled=true;
      for (const channel of CHANNELS) {
        const [lo,hi]=CHANNEL_LIMITS[channel];
        targets[channel]=M.clamp(targets[channel],lo,hi);
        const state=this.channels[channel],target=targets[channel];
        springStep(state,target,dt,CHANNEL_OMEGAS[channel]);
        if(Math.abs(state.value-target)>1e-6||Math.abs(state.velocity)>1e-6)settled=false;
      }
      for(const name of this.expressionNames){const state=this.weights[name];if(Math.abs(state.value-this.weightTargets[name])>1e-6||Math.abs(state.velocity)>1e-6){settled=false;break;}}
      this._expressionsSettled=settled;
    }

    resetControl(name) {
      const binding=typeof name==='string'?this.controlBindings[name]:name;
      const bone=binding.bone,rest=binding.rest;
      bone.position.copy(rest.position);bone.quaternion.copy(rest.quaternion);bone.scale.copy(rest.scale);
      return bone;
    }

    restoreHeadLayer() {
      if(!this._hasHeadLayer)return;
      const neck=this.neckControl.bone,head=this.headControl.bone;
      if(Math.abs(neck.quaternion.dot(this._appliedNeck))>1-1e-10)neck.quaternion.copy(this._bodyNeck);
      if(Math.abs(head.quaternion.dot(this._appliedHead))>1-1e-10)head.quaternion.copy(this._bodyHead);
      this._hasHeadLayer=false;
    }

    applyHeadLayer(yaw=this.attentionYaw.value,pitch=this.attentionPitch.value,stableBodyPose=false) {
      // Compose attention from the current body pose. Restoring first is safe
      // both after a prior render composition and after a body pose rewrite.
      const neck=this.neckControl.bone,head=this.headControl.bone,nq=neck.quaternion,hq=head.quaternion;
      if(stableBodyPose&&this._hasHeadLayer&&yaw===this._headLayerYaw&&pitch===this._headLayerPitch&&
        nq.x===this._appliedNeck.x&&nq.y===this._appliedNeck.y&&nq.z===this._appliedNeck.z&&nq.w===this._appliedNeck.w&&
        hq.x===this._appliedHead.x&&hq.y===this._appliedHead.y&&hq.z===this._appliedHead.z&&hq.w===this._appliedHead.w)return neck;
      this.restoreHeadLayer();
      const headFollowYaw=headPart(yaw,5*DEG,31*DEG,.78);
      const headFollowPitch=headPart(pitch,3*DEG,15*DEG,.66);
      this._bodyNeck.copy(neck.quaternion);this._bodyHead.copy(head.quaternion);
      this._q.setFromEuler(this._e.set(headFollowPitch*.34,headFollowYaw*.40,0,'XYZ'));
      neck.quaternion.multiply(this._q);
      this._q.setFromEuler(this._e.set(headFollowPitch*.66,headFollowYaw*.60,0,'XYZ'));
      head.quaternion.multiply(this._q);
      this._appliedNeck.copy(neck.quaternion);this._appliedHead.copy(head.quaternion);this._hasHeadLayer=true;
      this._headLayerYaw=yaw;this._headLayerPitch=pitch;
      this.metrics.headYaw=headFollowYaw;this.metrics.headPitch=headFollowPitch;
      return neck;
    }

    applyInterpolatedHeadLayer(yaw,pitch,stableBodyPose=false) {
      const neck=this.applyHeadLayer(yaw,pitch,stableBodyPose);
      const dict=this.model.mesh.morphTargetDictionary||{},values=this.model.mesh.morphTargetInfluences||[];
      if(dict.neckFlex!==undefined){const index=dict.neckFlex,value=M.clamp(Math.abs(neck.rotation.x)/.72,0,1);if(values[index]!==value)values[index]=value;}
    }

    apply() {
      const f=this.face, c=this.channels, H=this.rig.anatomy.height;
      const eyeOpen=M.clamp(
        f.neutralEyeOpen + c.eyeOpen.value*.24 - c.eyeSquint.value*.42,
        0.05,1
      )*(1-this.blink);
      const closure=1-eyeOpen;

      // Large attention changes rotate the neck and head. A small dead zone
      // leaves micro-saccades in the eyes only, while a 30-40 degree glance
      // becomes an obvious head turn instead of a barely visible eye twitch.
      const neck=this.applyHeadLayer();
      const headFollowYaw=this.metrics.headYaw,headFollowPitch=this.metrics.headPitch;

      const eyeYaw=M.clamp(this.gazeYaw.value-headFollowYaw,-30*DEG,30*DEG);
      const eyePitch=M.clamp(this.gazePitch.value-headFollowPitch,-20*DEG,20*DEG);

      for(let sideIndex=0;sideIndex<FACE_SIDES.length;sideIndex++){
        const controls=this.sideControls[sideIndex],sign=FACE_SIGNS[sideIndex];
        const eye=this.resetControl(controls.eye);
        eye.quaternion.setFromEuler(this._e.set(eyePitch,eyeYaw,0,'XYZ'));

        // The aperture contour is deformed on the GPU; rigid lid translations
        // cannot preserve a spherical globe or stationary eye corners.
        const upperLid=this.resetControl(controls.upperLid);
        const lowerLid=this.resetControl(controls.lowerLid);
        const lidFollow=(1-closure)*(1-M.clamp(Math.abs(eyeYaw)/(32*DEG),0,.45));
        upperLid.position.y += -eyePitch*.025*H*lidFollow;
        lowerLid.position.y += -eyePitch*.012*H*lidFollow;

        const inner=this.resetControl(controls.innerBrow);
        const outer=this.resetControl(controls.outerBrow);
        const neutral=f.neutralBrow*.0018*H;
        inner.position.y += neutral + H*(c.browInnerUp.value*.0058-c.browDown.value*.0047);
        outer.position.y += neutral*.75 + H*(c.browOuterUp.value*.0048-c.browDown.value*.0034);
        inner.position.z += Math.max(0,-(inner.position.y-controls.innerBrow.rest.position.y))*.35;
        inner.rotation.z += sign*(c.browInnerUp.value*.06+c.browDown.value*.08);
        outer.position.z += Math.max(0,-(outer.position.y-controls.outerBrow.rest.position.y))*.30;
        outer.rotation.z += sign*(-c.browOuterUp.value*.05+c.browDown.value*.05);

        const cheek=this.resetControl(controls.cheek);
        cheek.position.y += c.cheekRaise.value*.0028*H;

        const corner=this.resetControl(controls.mouthCorner);
        corner.position.x += sign*M.clamp(c.mouthStretch.value,0,1)*this.layout.mouth.w*.20*H;
        corner.position.y += H*(c.mouthCornerUp.value*.0035-c.mouthCornerDown.value*.0038+c.cheekRaise.value*.0010);
      }

      const jaw=this.resetControl(this.jawControl);
      const jawAmount=M.clamp(c.jawOpen.value+c.mouthOpen.value*.34,0,.72);
      jaw.rotation.x += jawAmount*.26;

      const upperLip=this.resetControl(this.upperLipControl);
      const lowerLip=this.resetControl(this.lowerLipControl);
      const mouthOpen=M.clamp(c.mouthOpen.value*.90+c.jawOpen.value*.28,0,.78);
      const lipPress=c.lipPress.value*(1-M.clamp(jawAmount*1.6,0,.90));
      upperLip.position.y += H*(mouthOpen*.00105-lipPress*.00014);
      lowerLip.position.y -= H*(mouthOpen*.00125-lipPress*.00014);
      upperLip.position.z += lipPress*.00006*H;
      lowerLip.position.z += lipPress*.00006*H;

      const dict=this.model.mesh.morphTargetDictionary||{},values=this.model.mesh.morphTargetInfluences||[];
      const set=(name,value)=>{if(dict[name]!==undefined)values[dict[name]]=value;};
      set('eyelidsClose',closure);
      set('eyelidsArc',4*closure*(1-closure));
      set('neckFlex',M.clamp(Math.abs(this.neckControl.bone.rotation.x)/.72,0,1));
      // Face and body pose are synchronized together by InfantryUnit.render;
      // clearance scans prepare their own CPU skin palette before reading it.

      this.metrics.eyeOpen=eyeOpen;
      this.metrics.blink=this.blink;
      this.metrics.gazeYaw=this.gazeYaw.value;
      this.metrics.gazePitch=this.gazePitch.value;
    }

    update(dt) {
      const previousBlink=this.blink;
      const blinkChanged=this.updateBlink(dt);
      const attentionYaw=this.attentionYaw.value,attentionPitch=this.attentionPitch.value,
        gazeYaw=this.gazeYaw.value,gazePitch=this.gazePitch.value;
      this.updateGaze(dt);
      const gazeChanged=attentionYaw!==this.attentionYaw.value||attentionPitch!==this.attentionPitch.value||
        gazeYaw!==this.gazeYaw.value||gazePitch!==this.gazePitch.value;
      const expressionWasSettled=this._expressionsSettled;
      this.updateExpressions(dt);
      // While expression springs and blink are both stable, applying the
      // same face would reset/copy the same controls on every visual sample.
      // Gaze still advances independently and is composed by the render path.
      if(!expressionWasSettled||!this._expressionsSettled||blinkChanged||this.blinkActive||this.blink!==previousBlink||gazeChanged){
        this.apply();
        return true;
      }
      return false;
    }
  };
})();
