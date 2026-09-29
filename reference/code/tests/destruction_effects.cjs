// Real Three r128, without WebGL: visual pools, clock, determinism and ownership.
// Shader compilation and appearance are verified separately in the browser fixture.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
global.THREE = require('../tools/destruction/node_modules/three');
global.RTS = {};
vm.runInThisContext(fs.readFileSync(path.join(__dirname, '../js/destruction/effects.js'), 'utf8'));

let checks = 0;
function check(name, run) { run(); checks++; console.log('PASS ' + name); }
function create() { const scene = new THREE.Scene(); return {scene, effects: new RTS.DestructionEffects(scene)}; }
const explosion = {point: [0, 1, 0], normal: [0, 0, 1], material: 'brick', explosiveMass: 2.1, seed: 2026};
const hit = {point: [0, 1, 0], normal: [0, 0, 1], material: 'concrete', diameter: .02, lost: 1000, result: 'stopped', part: 'wall', seed: 42};

check('HE has a brief emissive flash, moving chips and a longer dust tail', () => {
  const {scene, effects} = create();
  assert.equal(scene.children.length, 4);
  assert(scene.children.every(mesh => !mesh.isLight && !mesh.castShadow));
  effects.explosion(explosion);
  assert.equal(effects.flash.count, 2);
  assert(effects.dust.count > 10 && effects.chips.count > 30);
  assert.equal(effects.flash.mesh.material.blending, THREE.AdditiveBlending);
  assert(effects.texture.image.data.some((v, i) => i % 4 === 3 && v > 0));
  const firstChip = effects.chips.data.slice(2, 5);
  effects.update(.1);
  assert.notDeepEqual(Array.from(effects.chips.data.slice(2, 5)), Array.from(firstChip));
  for (let i = 0; i < 6; i++) effects.update(.1);
  assert.equal(effects.flash.count, 0);
  assert(effects.dust.count > 0, 'smoke must outlast the short flash');
  for (let i = 0; i < 50; i++) effects.update(.1);
  assert.equal(effects.diagnostics.drawCalls, 0);
  effects.dispose();
});

check('impact uses absorbed work, material response and actual penetration outcome', () => {
  const {effects} = create();
  effects.impact({...hit, lost: 0, energy: 1000000});
  assert.equal(effects.diagnostics.drawCalls, 0, 'residual AP energy must not drive impact strength');
  effects.impact({...hit, material: 'steel'});
  assert(effects.flash.count > 0 && effects.chips.count > 0);
  assert.equal(effects.dust.count, 0);
  assert.equal(effects.marks.count, 1);
  assert(effects.chips.data[12] > 1, 'metal sparks should be emissive');
  effects.reset();
  effects.impact({...hit, result: 'penetrated'});
  assert.equal(effects.marks.count, 0, 'flat decal must not cover a perforated hole');
  assert(effects.chips.count > 0 && effects.dust.count > 0);
  effects.reset();
  effects.impact({...hit, geometryChanged: true});
  assert.equal(effects.marks.count, 0, 'a crater should not get a floating flat mark');
  effects.dispose();
});

check('material damage modes produce distinct penetration presentation', () => {
  const masonry=create(), concrete=create(), steel=create(), wood=create();
  masonry.effects.impact({...hit,material:'brick',result:'penetrated',materialMode:'masonry-break',damageEnergy:5000,frontRadius:.09,stressRadius:.28,exitRadius:.14,exitPoint:[0,1,.3],outgoingDirection:[0,0,1],masonry:{newlyBroken:4},geometryChanged:true});
  concrete.effects.impact({...hit,material:'concrete',result:'penetrated',materialMode:'concrete-scab',damageEnergy:5000,frontRadius:.08,stressRadius:.24,exitRadius:.12,exitPoint:[0,1,.3],outgoingDirection:[0,0,1],geometryChanged:true});
  steel.effects.impact({...hit,material:'steel',result:'penetrated',materialMode:'petal',damageEnergy:3500,frontRadius:.03,stressRadius:.05,exitRadius:.05,exitPoint:[0,1,.2],outgoingDirection:[0,0,1],geometryChanged:true});
  wood.effects.impact({...hit,material:'wood',result:'penetrated',materialMode:'grain-split',damageEnergy:3000,frontRadius:.05,stressRadius:.18,exitRadius:.08,exitPoint:[0,1,.25],outgoingDirection:[0,0,1],grainDirection:[0,1,0],geometryChanged:true});
  assert(masonry.effects.dust.count>concrete.effects.dust.count-1);assert(masonry.effects.marks.count>0);assert(concrete.effects.marks.count>0);
  assert(steel.effects.flash.count>0);assert.equal(steel.effects.dust.count,0);assert(steel.effects.chips.count>0);
  assert(wood.effects.dust.count>0&&wood.effects.chips.count>0&&wood.effects.marks.count>0);
  masonry.effects.dispose();concrete.effects.dispose();steel.effects.dispose();wood.effects.dispose();
});

check('burst storms stay inside fixed pools and quality reduction compacts immediately', () => {
  const {scene, effects} = create();
  const geometries = scene.children.map(mesh => mesh.geometry);
  for (let i = 0; i < 200; i++) {
    effects.explosion({...explosion, seed: i});
    effects.impact({...hit, seed: i});
  }
  effects.update(1 / 60);
  const high = effects.diagnostics;
  assert(high.flash <= 16 && high.dust <= 128 && high.chips <= 256 && high.marks <= 128);
  assert.equal(high.capacity, 528);
  assert.equal(high.drawCalls, 4);
  effects.update(1 / 60, true);
  const low = effects.diagnostics;
  assert(low.flash <= 4 && low.dust <= 40 && low.chips <= 80 && low.marks <= 32);
  assert.deepEqual(scene.children.map(mesh => mesh.geometry), geometries);
  assert(effects.pools.every(pool => [...pool.data].every(Number.isFinite)));
  effects.dispose();
});

check('same visual seed gives the same spread without a material model or simulation RNG', () => {
  const a = create(), b = create();
  a.effects.explosion(explosion); b.effects.explosion(explosion);
  a.effects.update(1 / 60); b.effects.update(1 / 60);
  for (let i = 0; i < a.effects.pools.length; i++) {
    assert.equal(a.effects.pools[i].count, b.effects.pools[i].count);
    assert.deepEqual(a.effects.pools[i].data, b.effects.pools[i].data);
  }
  a.effects.dispose(); b.effects.dispose();
});

check('removed or detached surfaces retire every impact mark on the next frame without raycasts', () => {
  const {effects} = create();
  for (let i = 0; i < 128; i++) effects.impact({...hit, part: 'removed-' + i});
  let probes = 0;
  const model = {parts: new Map(), trace() { probes++; return null; }};
  effects.update(1 / 60, false, null, model);
  assert.equal(effects.marks.count, 0);
  assert.equal(probes, 0);
  model.parts.set('wall', {detached: false});
  effects.impact(hit);
  effects.update(1 / 60, false, null, model);
  assert.equal(effects.marks.count, 1);
  model.parts.get('wall').detached = true;
  effects.update(1 / 60, false, null, model);
  assert.equal(effects.marks.count, 0);
  effects.dispose();
});

check('expired and reset pools stop GPU uploads and dispose every owned resource once', () => {
  const {scene, effects} = create();
  effects.explosion(explosion);
  effects.update(.1);
  effects.reset();
  const matrixVersion = effects.chips.mesh.instanceMatrix.version;
  const dustVersion = effects.dust.mesh.geometry.attributes.iPosition.version;
  for (let i = 0; i < 10; i++) effects.update(.1);
  assert.equal(effects.chips.mesh.instanceMatrix.version, matrixVersion);
  assert.equal(effects.dust.mesh.geometry.attributes.iPosition.version, dustVersion);
  assert.equal(effects.chips.mesh.count, 0);
  assert.equal(effects.dust.mesh.geometry.instanceCount, 0);
  let geometryDisposals = 0, materialDisposals = 0, textureDisposals = 0;
  for (const mesh of scene.children) {
    mesh.geometry.addEventListener('dispose', () => geometryDisposals++);
    mesh.material.addEventListener('dispose', () => materialDisposals++);
  }
  effects.texture.addEventListener('dispose', () => textureDisposals++);
  effects.dispose(); effects.dispose();
  assert.equal(scene.children.length, 0);
  assert.equal(geometryDisposals, 4);
  assert.equal(materialDisposals, 4);
  assert.equal(textureDisposals, 1);
});

console.log(checks + ' destruction effects checks passed');
