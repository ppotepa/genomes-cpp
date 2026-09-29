(function () {
  'use strict';
  const R = window.RTS;

  const clamp01 = v => Math.max(0, Math.min(1, v));
  const signed = g => g * 2 - 1;
  // A single-draw, deterministic central bias avoids piling anatomy genes
  // onto their geometry clamps while keeping both ends reachable.
  const varied = random => {
    const x=random.next()*2-1;
    return clamp01(.5+.5*Math.sign(x)*Math.pow(Math.abs(x),1.30));
  };
  const centred = random => (random.next() + random.next()) * .5;

  function validate01(value, name) {
    if (!Number.isFinite(value) || value < 0 || value > 1) throw new RangeError('Niepoprawny gen ' + name);
  }
  function hexToRgb(hex) { return [(hex >> 16) & 255, (hex >> 8) & 255, hex & 255]; }
  function rgbToHex(rgb) { return ((Math.round(rgb[0]) & 255) << 16) | ((Math.round(rgb[1]) & 255) << 8) | (Math.round(rgb[2]) & 255); }
  function mixHex(a, b, t) {
    const ca=hexToRgb(a),cb=hexToRgb(b);return rgbToHex([R.Math.mix(ca[0],cb[0],t),R.Math.mix(ca[1],cb[1],t),R.Math.mix(ca[2],cb[2],t)]);
  }
  function shadeHex(hex, factor) { return rgbToHex(hexToRgb(hex).map(v=>R.Math.clamp(v*factor,0,255))); }
  function palette(gene, colors) {
    const p=R.Math.clamp(gene,0,.999999)*(colors.length-1),i=Math.floor(p);return mixHex(colors[i],colors[Math.min(colors.length-1,i+1)],p-i);
  }
  function deepFreeze(obj) {
    Object.values(obj).forEach(v=>{if(v&&typeof v==='object'&&!Object.isFrozen(v))deepFreeze(v);});return Object.freeze(obj);
  }
  function clone(obj) { return JSON.parse(JSON.stringify(obj)); }
  function setPath(obj,path,value) {
    const parts=path.split('.');let at=obj;for(let i=0;i<parts.length-1;i++)at=at[parts[i]];at[parts[parts.length-1]]=clamp01(value);
  }
  function applyAroundHalf(value, scale) { return clamp01(.5 + (value-.5)*scale); }

  function makeBody(seed) {
    const random=new R.SeededRandom((seed^0x8b7a3d11)>>>0);
    return {
      frameGene: varied(random), massGene: varied(random), musculatureGene: varied(random), adiposityGene: varied(random),
      shoulderBreadthGene: varied(random), hipBreadthGene: varied(random), torsoLegRatioGene: varied(random),
      armLengthGene: varied(random), legLengthGene: varied(random), chestDepthGene: varied(random), waistWidthGene: varied(random),
      limbThicknessGene: varied(random), neckThicknessGene: varied(random), headScaleGene: varied(random),
      handScaleGene: varied(random), footScaleGene: varied(random), skinToneGene: varied(random)
    };
  }

  function makeFace(seed) {
    const random=new R.SeededRandom((seed^0x51f15e37)>>>0);
    return {
      headWidthGene: varied(random), headDepthGene: varied(random), headLengthGene: varied(random),
      foreheadWidthGene: varied(random), foreheadSlopeGene: varied(random), templeWidthGene: varied(random), browRidgeGene: varied(random),
      jawWidthGene: varied(random), jawLengthGene: varied(random), jawAngleGene: varied(random),
      chinWidthGene: varied(random), chinHeightGene: varied(random), chinProjectionGene: varied(random),
      cheekboneWidthGene: varied(random), cheekboneHeightGene: varied(random), cheekFullnessGene: varied(random), midfaceProjectionGene: varied(random),

      eyeSpacingGene: varied(random), eyeWidthGene: varied(random), eyeHeightGene: varied(random), eyeRoundnessGene: varied(random),
      eyeDepthGene: varied(random), eyeTiltGene: varied(random), eyeVerticalGene: varied(random), eyeColorGene: random.next(),

      browHeightGene: varied(random), browThicknessGene: varied(random), browTiltGene: varied(random), browSpacingGene: varied(random),

      noseWidthGene: varied(random), noseLengthGene: varied(random), noseProjectionGene: varied(random), noseBridgeGene: varied(random),
      noseTipWidthGene: varied(random), noseTipRotationGene: varied(random), nostrilWidthGene: varied(random),

      mouthWidthGene: varied(random), upperLipGene: varied(random), lowerLipGene: varied(random), mouthHeightGene: varied(random),
      earSizeGene: varied(random), earAngleGene: varied(random),

      hairColorGene: random.next(), hairBrightnessGene: centred(random), hairStyleGene: random.next(),
      hairDensityGene: varied(random), hairThicknessGene: varied(random), hairVolumeGene: varied(random),
      hairlineGene: varied(random), templeRecessionGene: varied(random), widowPeakGene: varied(random),

      restingEyeOpennessGene: centred(random), restingBrowGene: centred(random), restingMouthGene: centred(random),
      eyeHeightAsymmetryGene: centred(random), browHeightAsymmetryGene: centred(random), mouthCornerAsymmetryGene: centred(random), earAsymmetryGene: centred(random),
      blinkRateGene: centred(random), blinkSpeedGene: centred(random), gazeRestlessnessGene: centred(random),
      expressionScaleGene: centred(random), eyeExpressionGene: centred(random), mouthExpressionGene: centred(random), browExpressionGene: centred(random)
    };
  }

  R.InfantryGenome = {
    create(seed) {
      if (!Number.isInteger(seed)||seed<0||seed>0xffffffff) throw new RangeError('Seed: liczba całkowita 0–4294967295.');
      const base=new R.SeededRandom(seed);
      return deepFreeze({heightGene:base.next(),speedGene:base.next(),body:makeBody(seed),face:makeFace(seed)});
    },

    withOverrides(genome, overrides={}) {
      const out=clone(genome);Object.keys(overrides).forEach(path=>setPath(out,path,overrides[path]));return deepFreeze(out);
    },

    applyVariation(genome, scale=1) {
      scale=Number(scale);scale=R.Math.clamp(Number.isFinite(scale)?scale:1,0,1.75);const out=clone(genome);
      const walk=(obj,prefix)=>Object.keys(obj).forEach(key=>{
        const path=prefix?prefix+'.'+key:key,val=obj[key];
        if(typeof val==='number'&&/Gene$/.test(key)) setPath(out,path,applyAroundHalf(val,scale));
        else if(val&&typeof val==='object')walk(val,path);
      });
      walk(genome,'');return deepFreeze(out);
    },

    express(genome) {
      validate01(genome.heightGene,'heightGene');validate01(genome.speedGene,'speedGene');
      if(!genome.body||!genome.face)throw new Error('Brak body/face genome.');
      Object.keys(genome.body).forEach(k=>validate01(genome.body[k],'body.'+k));
      Object.keys(genome.face).forEach(k=>validate01(genome.face[k],'face.'+k));

      const C=R.Config.INFANTRY,b=genome.body,g=genome.face;
      const height=R.Math.mix(C.MIN_HEIGHT,C.MAX_HEIGHT,genome.heightGene);
      const speedMultiplier=R.Math.mix(C.MIN_SPEED_MULTIPLIER,C.MAX_SPEED_MULTIPLIER,genome.speedGene);

      const frame=R.Math.mix(.90,1.11,b.frameGene),mass=R.Math.mix(.84,1.18,b.massGene),muscle=R.Math.mix(.84,1.20,b.musculatureGene),fat=R.Math.mix(.82,1.20,b.adiposityGene);
      const shoulderWidthScale=R.Math.clamp(R.Math.mix(.82,1.22,b.shoulderBreadthGene)*R.Math.mix(.96,1.06,b.frameGene)*R.Math.mix(.96,1.07,b.musculatureGene),.78,1.28);
      const hipWidthScale=R.Math.clamp(R.Math.mix(.86,1.17,b.hipBreadthGene)*R.Math.mix(.97,1.05,b.frameGene)*R.Math.mix(.97,1.05,b.adiposityGene),.82,1.23);
      const chestWidthScale=R.Math.clamp(R.Math.mix(.86,1.18,(b.massGene+b.musculatureGene+b.shoulderBreadthGene)/3),.82,1.24);
      const chestDepthScale=R.Math.clamp(R.Math.mix(.82,1.22,b.chestDepthGene)*R.Math.mix(.96,1.08,b.massGene),.80,1.28);
      const waistWidthScale=R.Math.clamp(R.Math.mix(.78,1.22,b.waistWidthGene)*R.Math.mix(.93,1.12,b.adiposityGene),.74,1.30);
      const waistDepthScale=R.Math.clamp(R.Math.mix(.82,1.18,(b.adiposityGene+b.massGene)/2),.78,1.24);
      const limbBase=R.Math.mix(.76,1.25,b.limbThicknessGene);
      const armThicknessScale=R.Math.clamp(limbBase*R.Math.mix(.92,1.12,b.musculatureGene)*R.Math.mix(.97,1.05,b.massGene),.72,1.36);
      const legThicknessScale=R.Math.clamp(limbBase*R.Math.mix(.94,1.12,b.musculatureGene)*R.Math.mix(.97,1.06,b.massGene),.72,1.36);
      const neckScale=R.Math.clamp(R.Math.mix(.79,1.25,b.neckThicknessGene)*R.Math.mix(.96,1.08,b.frameGene),.76,1.32);
      const legLengthScale=R.Math.mix(.94,1.07,b.legLengthGene),armLengthScale=R.Math.mix(.93,1.09,b.armLengthGene);
      const torsoLegBias=signed(b.torsoLegRatioGene);
      const hipY=R.Math.clamp(.54+(legLengthScale-1)*.42-torsoLegBias*.020,.502,.579);
      const headScale=R.Math.mix(.93,1.07,b.headScaleGene),handScale=R.Math.mix(.88,1.13,b.handScaleGene),footScale=R.Math.mix(.89,1.14,b.footScaleGene);
      const skinColor=palette(b.skinToneGene,[0x6f4a37,0x8f6048,0xae795e,0xc79576,0xd6ad8e,0xe0bd9f]);
      const body=Object.freeze({frame,mass,muscle,fat,torsoLegBias,shoulderWidthScale,hipWidthScale,chestWidthScale,chestDepthScale,waistWidthScale,waistDepthScale,armThicknessScale,legThicknessScale,neckScale,legLengthScale,armLengthScale,hipY,headScale,handScale,footScale,skinColor});

      const hairBase=palette(g.hairColorGene,[0x11100f,0x1a1512,0x2c1c14,0x452a1a,0x684125,0x8e663b,0xb58f58,0xd1b578,0x8b4829]);
      const hairColor=shadeHex(hairBase,R.Math.mix(.76,1.16,g.hairBrightnessGene));
      const eyeColor=palette(g.eyeColorGene,[0x2d2018,0x4b3522,0x6a4c2f,0x6c6341,0x596755,0x687477,0x71879b,0x526a82]);
      const headWidthScale=R.Math.mix(.86,1.16,g.headWidthGene)*R.Math.mix(.97,1.03,b.headScaleGene);
      const headWidthFit=R.Math.clamp((headWidthScale-.86)/.30,0,1);
      const jawWidthScale=R.Math.clamp(R.Math.mix(.74,1.29,g.jawWidthGene)*R.Math.mix(.92,1.08,headWidthFit),.70,1.34);
      const chinHeight=R.Math.mix(-.005,.006,g.chinHeightGene);
      const eyeWidthScale=R.Math.mix(.78,1.24,g.eyeWidthGene),eyeHeightScale=R.Math.mix(.73,1.28,g.eyeHeightGene);
      const eyeY=R.Math.mix(.935,.945,g.eyeVerticalGene);
      const browY=eyeY+.0034*eyeHeightScale+.008+R.Math.mix(-.0005,.0015,g.browHeightGene);
      const noseWidthScale=R.Math.mix(.72,1.30,g.noseWidthGene)*R.Math.mix(.91,1.09,headWidthFit);
      const jawWidthFit=R.Math.clamp((jawWidthScale-.70)/.64,0,1);
      const face=Object.freeze({
        headWidthScale,
        headDepthScale:R.Math.mix(.88,1.14,g.headDepthGene),headLengthScale:R.Math.mix(.92,1.10,g.headLengthGene),
        foreheadWidthScale:R.Math.mix(.84,1.16,g.foreheadWidthGene),foreheadSlope:R.Math.mix(-.006,.007,g.foreheadSlopeGene),templeWidthScale:R.Math.mix(.86,1.15,g.templeWidthGene),browRidge:R.Math.mix(-.002,.0045,g.browRidgeGene),
        jawWidthScale,jawLengthScale:R.Math.mix(.89,1.12,g.jawLengthGene),jawAngle:R.Math.mix(.80,1.18,g.jawAngleGene),
        chinWidthScale:R.Math.clamp(R.Math.mix(.68,1.34,g.chinWidthGene)*R.Math.mix(.91,1.09,jawWidthFit),.64,1.39),chinHeight,chinProjection:R.Math.mix(-.006,.009,g.chinProjectionGene),
        cheekboneScale:R.Math.mix(.82,1.20,g.cheekboneWidthGene),cheekboneY:R.Math.mix(-.006,.006,g.cheekboneHeightGene),cheekFullness:R.Math.mix(-.004,.006,g.cheekFullnessGene),midfaceProjection:R.Math.mix(-.004,.0065,g.midfaceProjectionGene),
        eyeSpacing:R.Math.mix(.0164,.0250,g.eyeSpacingGene)*R.Math.mix(.91,1.09,headWidthFit),eyeWidthScale,eyeHeightScale,eyeSizeScale:R.Math.mix(.76,1.26,(g.eyeWidthGene+g.eyeHeightGene)*.5),eyeRoundness:R.Math.mix(.72,1.28,g.eyeRoundnessGene),
        eyeDepth:R.Math.mix(-.0025,.0030,g.eyeDepthGene),eyeTilt:R.Math.mix(-.12,.12,g.eyeTiltGene),eyeY,eyeColor,
        browY,browThickness:R.Math.mix(.00055,.00128,g.browThicknessGene),browTilt:R.Math.mix(-.15,.16,g.browTiltGene),browSpacing:R.Math.mix(-.003,.0035,g.browSpacingGene),
        noseWidthScale,noseLengthScale:R.Math.mix(.80,1.22,g.noseLengthGene),noseProjectionScale:R.Math.mix(.76,1.29,g.noseProjectionGene),noseBridgeScale:R.Math.mix(.70,1.30,g.noseBridgeGene),noseTipWidthScale:R.Math.mix(.72,1.31,g.noseTipWidthGene),noseTipRotation:R.Math.mix(-.14,.16,g.noseTipRotationGene),nostrilWidthScale:R.Math.mix(.76,1.28,g.nostrilWidthGene),
        mouthWidth:R.Math.mix(.017,.030,g.mouthWidthGene)*R.Math.mix(.92,1.08,jawWidthFit),upperLip:R.Math.mix(.0012,.0031,g.upperLipGene),lowerLip:R.Math.mix(.0013,.0034,g.lowerLipGene),mouthY:R.Math.mix(.899,.904,g.mouthHeightGene)+chinHeight*.30,
        earScale:R.Math.mix(.78,1.24,g.earSizeGene),earAngle:R.Math.mix(-.18,.24,g.earAngleGene),
        hairColor,hairStyle:Math.min(6,Math.floor(g.hairStyleGene*7)),hairDensity:R.Math.mix(.76,1.18,g.hairDensityGene),hairThickness:R.Math.mix(.0025,.0070,g.hairThicknessGene),hairVolume:R.Math.mix(.003,.016,g.hairVolumeGene),hairline:R.Math.mix(.943,.961,g.hairlineGene),templeRecession:R.Math.mix(0,.012,g.templeRecessionGene),widowPeak:R.Math.mix(0,.009,g.widowPeakGene),
        neutralEyeOpen:R.Math.mix(.67,.93,g.restingEyeOpennessGene),neutralBrow:R.Math.mix(-.09,.09,g.restingBrowGene),neutralMouth:R.Math.mix(-.12,.10,g.restingMouthGene),
        eyeAsymmetry:signed(g.eyeHeightAsymmetryGene)*.0018,browAsymmetry:signed(g.browHeightAsymmetryGene)*.0020,mouthAsymmetry:signed(g.mouthCornerAsymmetryGene)*.0016,earAsymmetry:signed(g.earAsymmetryGene)*.0019,
        blinkInterval:R.Math.mix(2.6,6.4,g.blinkRateGene),blinkDuration:R.Math.mix(.105,.185,g.blinkSpeedGene),gazeRestlessness:R.Math.mix(.65,1.55,g.gazeRestlessnessGene),
        expressionScale:R.Math.mix(.84,1.16,g.expressionScaleGene),eyeExpressionScale:R.Math.mix(.84,1.18,g.eyeExpressionGene),mouthExpressionScale:R.Math.mix(.82,1.20,g.mouthExpressionGene),browExpressionScale:R.Math.mix(.84,1.18,g.browExpressionGene)
      });

      return Object.freeze({height,standingHeight:height,speedMultiplier,walkSpeed:C.WALK_BASE_SPEED*speedMultiplier,runSpeed:C.RUN_BASE_SPEED*speedMultiplier,proneSpeed:C.PRONE_BASE_SPEED*speedMultiplier,crouchSpeed:C.CROUCH_BASE_SPEED*speedMultiplier,body,face,generatorVersion:R.Config.GENERATOR_VERSION});
    }
  };
})();
