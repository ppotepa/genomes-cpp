(function () {
  'use strict';

  const R = window.RTS;
  const M = R.Math;

  function wrap01(value) {
    value %= 1;
    return value < 0 ? value + 1 : value;
  }

  function smooth5(t) {
    t = M.clamp(t, 0, 1);
    return t * t * t * (t * (t * 6 - 15) + 10);
  }

  function smoothRange(edge0, edge1, value) {
    if (edge0 === edge1) return value >= edge1 ? 1 : 0;
    return smooth5((value - edge0) / (edge1 - edge0));
  }

  function bell01(t) {
    t = M.clamp(t, 0, 1);
    return Math.sin(Math.PI * t);
  }

  function strideScale(speedGene) {
    // Keep genetic speed mostly as cadence. Only a small part changes stride length,
    // otherwise fast soldiers would look as if they had elastic legs.
    return M.mix(0.95, 1.05, M.clamp(speedGene, 0, 1));
  }

  function cycleFraction(state, speedGene) {
    const C = R.Config.INFANTRY;
    const scale = strideScale(speedGene);

    if (state === 'RUN') return C.RUN_CYCLE_HEIGHTS * scale;
    if (state === 'PRONE_MOVE') return C.CRAWL_CYCLE_LEGS * .48 * M.mix(0.97, 1.03, speedGene);
    if (state === 'CROUCH_WALK') return C.CROUCH_CYCLE_LEGS * .48 * scale;
    return C.WALK_CYCLE_HEIGHTS * scale;
  }

  /**
   * Samples one foot in a normalized gait cycle.
   *
   * phase: 0..1, heel strike starts at 0.
   * halfStride/lift are in body-height units, not metres.
   *
   * out fields:
   *   z, lift, stance, plant, support, footPitch, toePitch, swing
   */
  function sampleFoot(phase, duty, halfStride, lift, run, out) {
    const t = wrap01(phase);
    out.stance = t < duty;
    out.swing = out.stance ? 0 : (t - duty) / (1 - duty);

    if (out.stance) {
      const u = t / duty;

      // During stance the foot's local Z motion is intentionally linear.
      // Combined with phase driven by travelled distance this makes world-space
      // foot locking continuous at toe-off.
      out.z = M.mix(halfStride, -halfStride, u);
      out.lift = 0;

      // Contact ramps avoid hard IK ownership changes exactly at impact/toe-off.
      const impact = smoothRange(0.00, run ? 0.045 : 0.065, u);
      const release = 1 - smoothRange(run ? 0.82 : 0.86, 1.0, u);
      out.plant = impact * release;
      out.support = out.plant * (0.72 + 0.28 * bell01(u));

      // Heel -> flat -> forefoot progression.
      if (u < 0.18) {
        out.footPitch = M.mix(run ? -0.08 : -0.12, 0, smooth5(u / 0.18));
      } else if (u < 0.74) {
        out.footPitch = 0;
      } else {
        out.footPitch = M.mix(0, run ? 0.13 : 0.16, smooth5((u - 0.74) / 0.26));
      }

      out.toePitch = -M.mix(0, run ? 0.22 : 0.30, smoothRange(0.70, 1.0, u));
      return out;
    }

    const u = out.swing;
    const advance = smooth5(u);
    out.z = M.mix(-halfStride, halfStride, advance);

    // A slightly asymmetric arc: fast clearance after toe-off, calmer descent.
    const arc = Math.pow(Math.max(0, Math.sin(Math.PI * u)), run ? 0.82 : 0.72);
    const earlyBoost = 1 + (1 - u) * (run ? 0.10 : 0.16);
    out.lift = lift * arc * earlyBoost;
    out.plant = 0;
    out.support = 0;

    // Toe points down just after push-off, then dorsiflexes before heel strike.
    if (u < 0.38) {
      out.footPitch = M.mix(run ? 0.11 : 0.09, -0.03, smooth5(u / 0.38));
    } else {
      out.footPitch = M.mix(-0.03, run ? -0.08 : -0.11, smooth5((u - 0.38) / 0.62));
    }
    out.toePitch = M.mix(-0.12, -0.04, smooth5(u));
    return out;
  }

  // One complete cycle contains two alternating steps. Values are normalized
  // to standing height. During contact dz/dphase == -cycleDistance.
  function sampleLowContact(phase, duty, cycleDistance, clearance, out, sprint = 0) {
    const t = wrap01(phase), half = cycleDistance * duty * .5;
    out.stance = t < duty;
    out.swing = out.stance ? 0 : (t - duty) / (1 - duty);
    out.footPitch = 0;
    out.toePitch = 0;
    out.recoveryZ = 0;
    if (out.stance) {
      const u = t / duty;
      out.z = half - cycleDistance * t;
      out.lift = 0;
      out.plant = smoothRange(0, .075, u) * (1 - smoothRange(.86, 1, u));
      out.support = out.plant;
    } else {
      const u = out.swing, u2 = u*u, u3 = u2*u;
      // Cubic Hermite: match the nonzero root-space contact velocity at BOTH
      // ends. The limb therefore arrives at rest in world space, not in root space.
      const tangent = -cycleDistance * (1-duty);
      out.z = (2*u3-3*u2+1)*(-half) + (u3-2*u2+u)*tangent +
              (-2*u3+3*u2)*half + (u3-u2)*tangent;
      const arc=16*u2*(1-u)*(1-u);
      // Fold the heel behind the body before driving the knee through. The
      // time warp has zero offset AND derivative at takeoff and touchdown.
      const v=u-sprint*.14*arc,v2=v*v,v3=v2*v;
      const recovery=(2*v3-3*v2+1)*(-half)+(v3-2*v2+v)*tangent+
        (-2*v3+3*v2)*half+(v3-v2)*tangent;
      out.recoveryZ=recovery-out.z;out.z=recovery;
      out.lift = clearance * arc * (1+sprint*1.6*(.5-u));
      out.plant = 0;
      out.support = 0;
    }
    return out;
  }

  function isMoving(state) {
    return ['WALK','RUN','CROUCH_WALK','PRONE_MOVE'].includes(state);
  }
  function restState(state) {
    if (state === 'CROUCH_WALK' || state === 'CROUCH') return 'CROUCH';
    if (state === 'PRONE_MOVE' || state === 'PRONE') return 'PRONE';
    if (state === 'SITTING') return 'SITTING';
    return 'IDLE';
  }

  R.GaitProfile = Object.freeze({
    wrap01,
    smooth5,
    smoothRange,
    bell01,
    strideScale,
    cycleFraction,
    sampleFoot, sampleLowContact, isMoving, restState
  });
})();
