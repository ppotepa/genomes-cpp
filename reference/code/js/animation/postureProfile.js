(function () {
  'use strict';
  const R = window.RTS, M = R.Math;
  const BIPED = new Set(['IDLE', 'WALK', 'RUN', 'CROUCH', 'CROUCH_WALK']);
  // Art-directed samples, not joint ROM norms. Values between knots are continuous.
  // c, hip-above-ankle / leg, pelvis setback / leg, pelvis, lower/upper/chest pitch
  const KEYS = [
    [0,    .985, .000, .010, .004, .003, .003],
    [.25,  .925, .025, .185, .052, .044, .022],
    [.50,  .810, .060, .385, .085, .071, .030],
    [.75,  .600, .092, .490, .075, .062, .025],
    [1,    .315, .110, .500, .078, .066, .028]
  ];
  const SLOPES = KEYS.map((_, i) => {
    const row = [];
    for (let column = 1; column < KEYS[0].length; column++) {
      if (i === 0) row[column] = (KEYS[1][column] - KEYS[0][column]) * 4;
      else if (i === 4) row[column] = (KEYS[4][column] - KEYS[3][column]) * 4;
      else {
        const left = (KEYS[i][column] - KEYS[i - 1][column]) * 4;
        const right = (KEYS[i + 1][column] - KEYS[i][column]) * 4;
        row[column] = left * right <= 0 ? 0 : 2 * left * right / (left + right);
      }
    }
    return row;
  });
  function smooth(t) { return R.GaitProfile.smooth5(t); }
  function curve(c, column) {
    c = M.clamp(c, 0, 1);
    const index = Math.min(3, Math.floor(c * 4)), a = KEYS[index], b = KEYS[index + 1];
    // Monotone cubic Hermite: no extrema between the authored samples and no
    // zero-velocity dead zone at every intermediate slider position.
    const t = (c - a[0]) * 4, t2 = t*t, t3 = t2*t;
    return (2*t3-3*t2+1)*a[column] + (t3-2*t2+t)*.25*SLOPES[index][column] +
      (-2*t3+3*t2)*b[column] + (t3-t2)*.25*SLOPES[index+1][column];
  }
  function speedLimit(c, phenotype) {
    const v0=phenotype.runSpeed,v1=v0*.68,v2=phenotype.walkSpeed*.86,
      v3=phenotype.crouchSpeed*.68,v4=phenotype.crouchSpeed*.28;
    const j = Math.min(3, Math.floor(M.clamp(c, 0, 1)*4));
    const a=j===0?v0:j===1?v1:j===2?v2:v3;
    const b=j===0?v1:j===1?v2:j===2?v3:v4;
    return M.mix(a,b,smooth((c-j*.25)*4));
  }
  function depthAtSpeed(speed, phenotype) {
    if (speed <= speedLimit(1, phenotype)) return 1;
    if (speed >= speedLimit(0, phenotype)) return 0;
    let lo = 0, hi = 1;
    for (let i=0;i<14;i++) { const mid=(lo+hi)*.5; if(speedLimit(mid,phenotype)>=speed)lo=mid;else hi=mid; }
    return lo;
  }
  function preset(state, p) {
    return {crouch: state.startsWith('CROUCH') ? .60 : 0,
      speedMps: state==='RUN'?p.runSpeed:state==='WALK'?p.walkSpeed:state==='CROUCH_WALK'?p.crouchSpeed:0};
  }
  function sample(c, anatomy, out) {
    const H=anatomy.height, L=anatomy.legLength, ankle=anatomy.ankleHeight;
    const hipOffset=anatomy.points['hips'].y-anatomy.points['thigh.L'].y;
    const pitch=curve(c,3);
    out.pelvisPitch=pitch;
    // Account for the ROTATED hips->hip-joint offset; never scale leg bones.
    out.hipY=ankle+L*curve(c,1)+Math.cos(pitch)*hipOffset;
    out.hipZ=-L*curve(c,2)+Math.sin(pitch)*hipOffset;
    out.lowerPitch=curve(c,4);out.upperPitch=curve(c,5);out.chestPitch=curve(c,6);
    const bulk=Math.max(0,(anatomy.body.legThicknessScale||1)-1);
    out.stanceHalf=anatomy.hipHalf*H + (.004+.025*smooth(c)+bulk*.012)*H;
    out.footZ=L*.025*c;
    out.footYaw=.05+.20*smooth(c);
    out.kneeHalf=out.stanceHalf+H*(.018+.030*c);
    return out;
  }
  function gait(c, speed, anatomy, p, out) {
    const ratio=speed/Math.max(.1,p.speedMultiplier);
    const run=smooth((ratio-1.75)/1.05)*(1-smooth((c-.16)/.40));
    const sprint=run*smooth((speed/Math.max(.1,p.runSpeed)-.70)/.30);
    const walkCycle=anatomy.legLength*1.50,runCycle=anatomy.legLength*2.50;
    const shortening=M.mix(1,.27,smooth(c));
    const maxCycle=M.mix(walkCycle,runCycle,run)*shortening;
    // Cadence is bounded by shortening the step at very low speeds.
    const cadence=M.mix(1.05,1.92,run)*M.mix(1,.82,c);
    out.cycleM=Math.max(.11*anatomy.height,Math.min(maxCycle,Math.max(0,speed)/cadence));
    out.run=run;
    out.sprint=sprint;
    out.duty=M.mix(M.mix(M.mix(.62,.76,c),.42,run),.38,sprint);
    out.liftM=M.mix(M.mix(M.mix(.048,.012,c),.100,run),.180,sprint)*anatomy.height;
    out.amplitude=smooth(speed/.20);
    out.cadence=speed/out.cycleM;
    return out;
  }
  R.PostureProfile=Object.freeze({isBiped:s=>BIPED.has(s),preset,curve,sample,gait,speedLimit,depthAtSpeed});
})();
