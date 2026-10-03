#!/usr/bin/env node
'use strict';

// Diagnostic-only companion to the pinned exporter.  It loads the same
// reference modules but emits no fixture and never changes reference data.
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const root = path.resolve(__dirname, '../../reference/code');
const seed = Number(process.argv[2] ?? 0);
if (!Number.isSafeInteger(seed) || seed < 0) throw new RangeError('seed must be a non-negative integer');

global.window = global;
global.THREE = require('../../reference/code/tests/numeric_math.cjs');
global.document = { createElement: () => ({ getContext: () => ({}) }) };
const loader = fs.readFileSync(path.join(root, 'js/loader.js'), 'utf8');
const modules = vm.runInNewContext(loader.match(/const local=(\[[\s\S]*?\]);/)[1])
  .map(file => file.replace(/^js\//, '').replace(/\.js$/, ''));
for (const moduleName of modules) {
  vm.runInThisContext(fs.readFileSync(path.join(root, 'js', `${moduleName}.js`), 'utf8'),
    { filename: moduleName });
}

let genome = RTS.InfantryGenome.create(seed);
genome = RTS.InfantryGenome.applyVariation(genome, 0);
const phenotype = RTS.InfantryGenome.express(genome);
const layout = new RTS.FaceAnatomy(phenotype.face, phenotype.body);
const anatomy = RTS.InfantryAnatomy.create(phenotype.height, phenotype.face, phenotype.body);
const equipment = new RTS.Equipment({unitSeed: seed, slotSchema: RTS.EquipmentSlots,
  loadout: 'RIFLEMAN', wear: .25});
const fit = new RTS.EquipmentFit(anatomy, equipment);
const rig = new RTS.InfantryRig(anatomy, new THREE.Group());
const tracedGearVertices = [];
const tracedBackVertices = [];
const originalGearVertex = RTS.GearGeometry.prototype.vertex;
RTS.GearGeometry.prototype.vertex = function tracedVertex(point, ...args) {
  if (this.currentTag === 'gear.torsoArmor') tracedGearVertices.push([point.x, point.y, point.z]);
  if (this.currentTag === 'gear.back') tracedBackVertices.push([point.x, point.y, point.z]);
  return originalGearVertex.call(this, point, ...args);
};
const gear = RTS.InfantryGear.build(rig, 'blue', equipment, 'high', fit);
RTS.GearGeometry.prototype.vertex = originalGearVertex;
function baseProfileAt(y) {
  const jacket = [[.504,.112,.071],[.519,.113,.071],[.552,.110,.067],[.595,.094,.056],
    [.640,.100,.059],[.698,.113,.066],[.735,.122,.066],[.759,.129,.064],
    [.785,.134,.062],[.809,.127,.057],[.825,.112,.051],[.841,.070,.040],
    [.852,.045,.036],[.859,.042,.034]];
  for (let i=0;i<jacket.length-1;i++) if (y<=jacket[i+1][0]) {
    const a=jacket[i],b=jacket[i+1],t=RTS.Math.clamp((y-a[0])/(b[0]-a[0]),0,1);
    return [RTS.Math.mix(a[1],b[1],t),RTS.Math.mix(a[2],b[2],t)];
  }
  return jacket[jacket.length-1].slice(1);
}
function clothPoint(x,y,depth) {
  const yy=fit.mapTorsoY(y),base=baseProfileAt(y),d=fit.profile(yy);
  const xx=x*(d[0]/Math.max(.001,base[0]));
  return new THREE.Vector3(xx,yy,d[1]*Math.sqrt(Math.max(0,1-xx*xx/(d[0]*d[0])))+depth);
}
const tubePoints=Array.from({length:6},(_,i)=>clothPoint(.054-.021,.697+i*.009,.003));
const tubeDirection=tubePoints[1].clone().sub(tubePoints[0]).normalize();
const tubeGuide=Math.abs(tubeDirection.z)<.9?new THREE.Vector3(0,0,1):new THREE.Vector3(0,1,0);
const tubeU=tubeDirection.clone().cross(tubeGuide).normalize();
const tubeFirst=tubePoints[0].clone().addScaledVector(tubeU,.00065);
const output = {
  seed,
  face: phenotype.face,
  body: phenotype.body,
  levels: layout.levels,
  mouth: layout.mouth,
  eyes: layout.eyes,
  brows: layout.brows,
  torsoJoints: {
    hip: phenotype.body.hipY,
    spineLower: RTS.Math.mix(phenotype.body.hipY, layout.neck.jointY, .25),
    spineUpper: RTS.Math.mix(phenotype.body.hipY, layout.neck.jointY, .56),
    chest: RTS.Math.mix(phenotype.body.hipY, layout.neck.jointY, .76),
    neck: layout.neck.jointY,
  },
  firstTailoringTube: {
    center: tubePoints[0], direction: tubeDirection, u: tubeU, point: tubeFirst,
    weights: fit.torsoWeights(tubeFirst),
  },
  armorProfile: {
    y0: fit.mapTorsoY(.635), y1: fit.mapTorsoY(.793),
    atY0: fit.profile(fit.mapTorsoY(.635)), atY1: fit.profile(fit.mapTorsoY(.793)),
  },
  gearVertex2991: {
    position: Array.from(gear.geometry.attributes.position.array.slice(2991*3,2991*3+3)),
    normal: Array.from(gear.geometry.attributes.normal.array.slice(2991*3,2991*3+3)),
  },
  backVertexCount: tracedBackVertices.length,
  backVertex: tracedBackVertices[Number(process.argv[3] ?? 0)] ?? null,
  armorAuthoringVertex222: tracedGearVertices[222],
  sampledSections: [0.89515, 0.8984, 0.90165, 0.905].map(y => ({
    y, section: layout.section(y), front: layout.frontZ(0, y),
  })),
  sampledFront: [
    [0, layout.mouthY],
    [0.016946930070997963, layout.mouthY],
    [0.016946930070997963, 1.6004236936569214 / 1.775],
  ].map(([x, y]) => ({x, y, z: layout.frontZ(x, y)})),
};
process.stdout.write(`${JSON.stringify(output, null, 2)}\n`);
