'use strict';
const assert = require('assert');
const fs = require('fs');
const vm = require('vm');

class Vector3 {
  constructor(x = 0, y = 0, z = 0) { this.set(x, y, z); }
  set(x, y, z) { this.x = x; this.y = y; this.z = z; return this; }
  copy(v) { return this.set(v.x, v.y, v.z); }
  clone() { return new Vector3(this.x, this.y, this.z); }
}
class Quaternion {
  constructor() { this.x = 0; this.y = 0; this.z = 0; this.w = 1; }
  copy(q) { Object.assign(this, q); return this; }
  clone() { return new Quaternion().copy(this); }
}
class Euler {
  constructor(x = 0, y = 0, z = 0, order = 'XYZ') { this.set(x, y, z, order); }
  set(x, y, z, order = 'XYZ') { this.x = x; this.y = y; this.z = z; this.order = order; return this; }
}

let schemaCreates = 0;
class Animator {
  constructor() { this.locomotion = {}; this.state = 'IDLE'; }
  setState(state) { this.state = state; }
  update() {}
  finalizeAfterLook() {}
  releaseContacts() {}
  buildSupports() { return []; }
}
class FaceAnimator {
  setExpression() {}
  update() {}
}
class WeaponController {
  constructor() { this.trigger = false; this.pendingShot = false; }
  apply() {}
  capture() {}
  syncSnapshots() {}
  beginStep() {}
  render() {}
  dispose() {}
}

const root = {
  position: new Vector3(), quaternion: new Quaternion(), rotation: { set() {} },
  updateMatrixWorld() {}, localToWorld(v) { return v; }
};
const rig = { root, bones: [], skeleton: { update() {} } };
const model = { root, rig, mesh: { morphTargetInfluences: [] }, dispose() {} };
const R = {
  Unit: class {}, EquipmentSlots: {}, FlatSurface: { getHeightAt() { return 0; } },
  InfantryGenome: { create() { return {}; }, express() { return {}; } },
  InfantryFactory: { createModel() { return model; }, setEquipment() {} },
  InfantryAnimator: Animator, FaceAnimator, WeaponController,
  PostureProfile: { isBiped() { return false; } },
  RagdollSchema: { create(actualRig) { schemaCreates++; assert.strictEqual(actualRig, rig); return { id: schemaCreates }; } }
};
const window = { RTS: R };
const THREE = { Vector3, Quaternion, Euler };
vm.runInNewContext(fs.readFileSync(require.resolve('../js/units/infantryUnit.js'), 'utf8'), { window, THREE }, { filename: 'infantryUnit.js' });

const unit = new R.InfantryUnit({ id: 'lazy-physics', side: {}, seed: 1 });
assert.strictEqual(schemaCreates, 0, 'routine construction must not build the diagnostic ragdoll schema');
unit.setWorldPosition(3, 4, R.FlatSurface);
assert.strictEqual(schemaCreates, 0, 'positioning a routine unit must not build the diagnostic schema');
const first = unit.physicsSchema;
assert.strictEqual(schemaCreates, 1, 'first explicit physicsSchema access builds the schema');
assert.strictEqual(unit.physicsSchema, first, 'subsequent accesses reuse the same schema');
assert.strictEqual(schemaCreates, 1, 'schema is not rebuilt on repeated reads');
console.log('PASS infantry lazy physics schema: constructor and positioning skip diagnostics; explicit access builds once');
