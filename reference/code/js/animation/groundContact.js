(function () {
  'use strict';
  const R = window.RTS;
  /** A position constraint, not friction/physics. Owns one WORLD-space support
   * point. The root may move while this point stays on the terrain. */
  R.GroundContact = class GroundContact {
    constructor() {
      this.point = new THREE.Vector3();
      this.local = new THREE.Vector3();
      this.active = false;
      this.locked = false;
      this.weight = 0;
      this.kind = 'FREE';
      this.reanchors = 0;
    }
    release() { this.active = false; this.locked = false; this.weight = 0; }
    resolve(candidate, strength, enabled, maxDrift, out) {
      const w = R.GaitProfile.smoothRange(.02, .82, strength);
      if (!enabled || strength <= .001) {
        this.release();
        return out.copy(candidate);
      }
      if (!this.active) { this.point.copy(candidate); this.active = true; }
      // A teleport, an incompatible posture or a turn must never stretch a limb.
      if (Math.hypot(candidate.x-this.point.x, candidate.z-this.point.z) > maxDrift) {
        this.point.copy(candidate); this.reanchors++;
      }
      this.weight = w;
      this.locked = w > .98;
      return out.copy(candidate).lerp(this.point, w);
    }
  };
})();
