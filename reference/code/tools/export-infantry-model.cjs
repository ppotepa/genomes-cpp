// Exports the authoritative procedural infantry buffers without WebGL.
// The native port consumes this output as a golden parity fixture.
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const crypto = require('crypto');

const referenceRoot = path.resolve(__dirname, '..');
const fixtureManifestPath = path.resolve(__dirname, '../../../reference/fixtures/manifest.json');
const fixtureManifest = JSON.parse(fs.readFileSync(fixtureManifestPath, 'utf8'));
if (fixtureManifest.schema !== 'genomes.fixture-manifest.v1' ||
    fixtureManifest.generator !== 'tools/reference/export_infantry_reference.cjs') {
  throw new Error(`unsupported fixture manifest: ${fixtureManifestPath}`);
}
global.window = global;
global.THREE = require('../tests/numeric_math.cjs');
THREE.Quaternion.prototype.dot = function dot(q) {
  return this.x * q.x + this.y * q.y + this.z * q.z + this.w * q.w;
};
THREE.SkinnedMesh.prototype.updateMorphTargets = function updateMorphTargets() {
  this.morphTargetInfluences = [];
  this.morphTargetDictionary = {};
  for (const [index, attribute] of (this.geometry.morphAttributes.position || []).entries()) {
    this.morphTargetInfluences.push(0);
    this.morphTargetDictionary[attribute.name] = index;
  }
};
global.document = {
  createElement: () => ({
    getContext: () => ({
      createImageData: (width, height) => ({
        data: new Uint8ClampedArray(width * height * 4),
      }),
      putImageData: () => {},
    }),
  }),
};

const loader = fs.readFileSync(path.join(referenceRoot, 'js/loader.js'), 'utf8');
const moduleNames = vm
  .runInNewContext(loader.match(/const local=(\[[\s\S]*?\]);/)[1])
  .map(file => file.replace(/^js\//, '').replace(/\.js$/, ''));
for (const moduleName of moduleNames) {
  const source = fs.readFileSync(path.join(referenceRoot, 'js', `${moduleName}.js`), 'utf8');
  vm.runInThisContext(source, { filename: moduleName });
}

function argumentsOf(argv) {
  const result = { mode: 'full-avatar', seed: 8841, detail: 'high', variation: 0,
    equipment: 'default', loadout: 'RIFLEMAN', wear: 0.25, count: 1 };
  const positional = [];
  for (let index = 0; index < argv.length; ++index) {
    const argument = argv[index];
    if (!argument.startsWith('--')) { positional.push(argument); continue; }
    const key = argument.slice(2);
    if (!['mode', 'seed', 'detail', 'variation', 'output', 'weapon', 'equipment',
      'loadout', 'wear', 'count', 'slot', 'item', 'hair-style'].includes(key)) {
      throw new Error(`unknown option --${key}`);
    }
    if (++index >= argv.length) throw new Error(`missing value for --${key}`);
    result[key] = argv[index];
  }
  if (positional.length) result.seed = positional[0];
  if (positional.length > 1) result.detail = positional[1];
  if (positional.length > 2) result.output = positional[2];
  result.seed = Number(result.seed);
  result.variation = Number(result.variation);
  result.wear = Number(result.wear);
  result.count = Number(result.count);
  if (result['hair-style'] !== undefined) result['hair-style'] = Number(result['hair-style']);
  return result;
}

const request = argumentsOf(process.argv.slice(2));
const { seed, detail } = request;
const destination = request.output;
if (!Number.isSafeInteger(seed) || seed < 0) throw new RangeError('seed must be a non-negative integer');
if (!Number.isFinite(request.variation) || request.variation < 0 || request.variation > 1.75) {
  throw new RangeError('variation must be in [0, 1.75]');
}
if (!Number.isFinite(request.wear) || request.wear < 0 || request.wear > 1) {
  throw new RangeError('wear must be in [0, 1]');
}
if (!Number.isSafeInteger(request.count) || request.count < 1 || request.count > 65536) {
  throw new RangeError('count must be an integer in [1, 65536]');
}
if (request['hair-style'] !== undefined &&
    (!Number.isSafeInteger(request['hair-style']) || request['hair-style'] < 0 || request['hair-style'] > 6)) {
  throw new RangeError('hair-style must be an integer in [0, 6]');
}
if (!['neutral', 'default', 'full'].includes(request.equipment)) {
  throw new RangeError('equipment must be neutral, default, or full');
}
if (!['far', 'world', 'high'].includes(detail)) throw new RangeError('invalid detail');
if (!['genome', 'surface', 'equipment', 'weapon', 'animation', 'full-avatar'].includes(request.mode)) {
  throw new RangeError('invalid export mode');
}
if (!destination) throw new Error('usage: node export-infantry-model.cjs --mode MODE --seed N --detail LOD --output PATH');

const side = new RTS.Side('SIDE_A', RTS.Config.SIDES.SIDE_A);
let genome = RTS.InfantryGenome.create(seed);
genome = RTS.InfantryGenome.applyVariation(genome, request.variation);
if (request['hair-style'] !== undefined) genome = RTS.InfantryGenome.withOverrides(
  genome, { 'face.hairStyleGene': (request['hair-style'] + 0.5) / 7 });
const phenotype = RTS.InfantryGenome.express(genome);
const loadout = request.equipment === 'neutral' ? null :
  request.equipment === 'full' ? 'HEAVY_SUPPORT' : request.loadout;
if (loadout && !RTS.InfantryLoadouts[loadout]) throw new RangeError(`unknown loadout ${loadout}`);
if (Boolean(request.slot) !== Boolean(request.item)) {
  throw new RangeError('--slot and --item must be provided together');
}
const overrides = request.slot ? { [request.slot]: request.item } : {};
const equipment = new RTS.Equipment({ unitSeed: seed, slotSchema: RTS.EquipmentSlots,
  loadout: request.slot ? null : loadout, overrides, wear: request.wear });
const model = RTS.InfantryFactory.createModel(side, phenotype, detail, equipment);

function attribute(geometry, name) {
  const value = geometry.attributes[name];
  return value ? Array.from(value.array) : [];
}

function meshFixture(mesh, metadata) {
  const geometry = mesh.geometry;
  const materials = (Array.isArray(mesh.material) ? mesh.material : [mesh.material]).map(material => ({
    name: material.name || '', type: material.type,
    color: material.color ? [material.color.r, material.color.g, material.color.b] : null,
    roughness: material.roughness ?? null, metalness: material.metalness ?? null,
    opacity: material.opacity ?? 1, transparent: Boolean(material.transparent),
    vertexColors: Boolean(material.vertexColors), side: material.side,
  }));
  const bounds = geometry.boundingBox && geometry.boundingSphere ? {
    minimum: geometry.boundingBox.min.toArray(), maximum: geometry.boundingBox.max.toArray(),
    sphereCenter: geometry.boundingSphere.center.toArray(), sphereRadius: geometry.boundingSphere.radius,
  } : null;
  return {
    positions: attribute(geometry, 'position'),
    normals: attribute(geometry, 'normal'),
    colors: attribute(geometry, 'color'),
    uvs: attribute(geometry, 'uv'),
    skinIndices: attribute(geometry, 'skinIndex'),
    skinWeights: attribute(geometry, 'skinWeight'),
    indices: Array.from(geometry.index.array),
    groups: geometry.groups.map(group => ({
      start: group.start,
      count: group.count,
      material: group.materialIndex,
    })),
    morphs: (geometry.morphAttributes.position || []).map((position, index) => ({
      name: position.name,
      positions: Array.from(position.array),
      normals: Array.from(geometry.morphAttributes.normal[index].array),
    })),
    tags: metadata.tags,
    records: metadata.records || [],
    faceMetadata: metadata.faceMetadata || null,
    bounds,
    materials,
  };
}

const sourceLock = JSON.parse(fs.readFileSync(path.join(__dirname, 'infantry-source-lock.json'), 'utf8'));
const sourceFiles = Object.entries(sourceLock.files).map(([relative, expected]) => {
  const bytes = fs.readFileSync(path.join(referenceRoot, relative));
  const sha256 = crypto.createHash('sha256').update(bytes).digest('hex');
  if (sha256 !== expected) throw new Error(`reference source hash mismatch: ${relative}`);
  return { path: relative, sha256 };
});
const provenance = {
  sourceCommit: sourceLock.sourceCommit,
  sourceTree: sourceLock.sourceTree,
  generatorVersion: 'genomes-infantry-exporter-v10',
  schemaVersion: 1,
  files: sourceFiles,
};
const gearFit = model.gear && model.gear.fit;
const equipmentFit = gearFit ? {
  height: gearFit.H,
  hipY: gearFit.A.hipY,
  armorGap: gearFit.armorGap(),
  jacket: gearFit.jacket,
  packDimensions: gearFit.packDimensions,
  sockets: Object.fromEntries(Object.entries(gearFit.sockets).map(([name, socket]) =>
    [name, { bone: socket.bone, position: socket.position.toArray(),
      normal: socket.normal.toArray() }])),
  headBottom: gearFit.headgear ? [-Math.PI, -Math.PI * .5, 0, Math.PI * .5]
    .map(theta => gearFit.headBottom(theta)) : [],
} : null;
const fullOutput = {
  schema: 'genomes.infantry.reference.v1',
  provenance,
  request: { mode: request.mode, seed, variation: request.variation, detail,
    equipment: request.equipment, loadout, wear: request.wear,
    hairStyle: request['hair-style'] ?? phenotype.face.hairStyle },
  equipmentState: {
    schema: 'EQUIPMENT/v1', seed: equipment.seed, palette: equipment.palette,
    wear: equipment.wear,
    slots: Object.fromEntries(Object.entries(equipment.slots).map(([slot, item]) =>
      [slot, item ? { definitionId: item.definitionId, seed: item.seed,
        variant: item.variant } : null])),
  },
  equipmentFit,
  phenotype,
  rig: model.rig.bones.map((bone, index) => ({
    index,
    name: bone.name,
    parent: bone.parent && bone.parent.name ? bone.parent.name : null,
    position: [bone.position.x, bone.position.y, bone.position.z],
    inverseBind: Array.from(model.rig.skeleton.boneInverses[index].elements),
  })),
  anatomy: {
    height: model.rig.anatomy.height,
    face: {
      neck: model.rig.anatomy.faceLayout.neck,
      chinY: model.rig.anatomy.faceLayout.chinY,
      headPivotY: model.rig.anatomy.faceLayout.headPivotY,
      levels: model.rig.anatomy.faceLayout.levels,
      sectionSamples: model.rig.anatomy.faceLayout.levels.slice(0, -1).flatMap((level, index) =>
        [.25, .5, .75].map(t => {
          const y = level[0] + (model.rig.anatomy.faceLayout.levels[index + 1][0] - level[0]) * t;
          return model.rig.anatomy.faceLayout.section(y);
        })),
      mouth: model.rig.anatomy.faceLayout.mouth,
      eyes: model.rig.anatomy.faceLayout.eyes,
      brows: model.rig.anatomy.faceLayout.brows,
    },
    fingers: model.rig.anatomy.fingers,
  },
  body: meshFixture(model.mesh, model.surface),
  gear: model.gearMesh ? meshFixture(model.gearMesh, model.gear) : null,
};

let weaponModel = null;
if (request.mode === 'weapon') {
  const definitionId = request.weapon || 'rifle';
  if (!RTS.WeaponCatalog.get(definitionId)) throw new RangeError(`unknown weapon ${definitionId}`);
  weaponModel = RTS.WeaponGeometry.create({
    definitionId,
    slot: 'PrimaryWeapon',
    variant: { size: 1, shade: 1, detail: 0 },
  });
}

let output = fullOutput;
if (request.mode === 'genome') {
  const names = ['heightGene', 'speedGene',
    ...Object.keys(genome.body).map(name => `body.${name}`),
    ...Object.keys(genome.face).map(name => `face.${name}`)];
  const catalog = [];
  const phenotypeBodyNames = Object.keys(phenotype.body);
  const phenotypeFaceNames = Object.keys(phenotype.face);
  for (let offset = 0; offset < request.count; ++offset) {
    const catalogSeed = seed + offset;
    if (catalogSeed > 0xffffffff) throw new RangeError('catalog seed range exceeds uint32');
    let entry = RTS.InfantryGenome.create(catalogSeed);
    entry = RTS.InfantryGenome.applyVariation(entry, request.variation);
    const expressed = RTS.InfantryGenome.express(entry);
    catalog.push({ seed: catalogSeed, genes: [entry.heightGene, entry.speedGene,
      ...Object.values(entry.body), ...Object.values(entry.face)],
      body: Object.values(expressed.body), face: Object.values(expressed.face) });
  }
  output = { schema: fullOutput.schema, provenance,
    request: { ...fullOutput.request, count: request.count }, geneNames: names,
    phenotypeBodyNames, phenotypeFaceNames, catalog };
}
if (request.mode === 'surface') output = { schema: fullOutput.schema, provenance, request: fullOutput.request, body: fullOutput.body, rig: fullOutput.rig };
if (request.mode === 'equipment') {
  const slots = Object.entries(RTS.EquipmentSlots).map(([id, definition]) => ({
    id, required: definition.required || null, socket: definition.socket || null,
  }));
  const items = Object.values(RTS.EquipmentCatalog.items).map(item => ({
    id: item.id, slots: Array.from(item.slots), kind: item.kind,
    weightKg: item.weightKg, visual: item.visual,
  }));
  const loadouts = Object.values(RTS.InfantryLoadouts).map(profile => ({
    id: profile.id, slots: profile.slots,
  }));
  output = { schema: fullOutput.schema, provenance, request: fullOutput.request,
    equipmentSchema: 'EQUIPMENT/v1', equipmentState: fullOutput.equipmentState,
    equipmentFit: fullOutput.equipmentFit, slots, items, loadouts, gear: fullOutput.gear };
}
if (request.mode === 'weapon') output = {
  schema: fullOutput.schema,
  provenance,
  request: { ...fullOutput.request, weapon: weaponModel.def.id },
  weapon: {
    definition: weaponModel.def,
    primary: meshFixture(weaponModel.mesh, { tags: {} }),
    slide: weaponModel.slide ? meshFixture(weaponModel.slide, { tags: {} }) : null,
    muzzleFlash: weaponModel.flash ? meshFixture(weaponModel.flash, { tags: {} }) : null,
    grips: {
      primary: [weaponModel.primary.position.x, weaponModel.primary.position.y,
        weaponModel.primary.position.z, weaponModel.primary.quaternion.x,
        weaponModel.primary.quaternion.y, weaponModel.primary.quaternion.z,
        weaponModel.primary.quaternion.w],
      secondary: weaponModel.secondary ? [weaponModel.secondary.position.x,
        weaponModel.secondary.position.y, weaponModel.secondary.position.z,
        weaponModel.secondary.quaternion.x, weaponModel.secondary.quaternion.y,
        weaponModel.secondary.quaternion.z, weaponModel.secondary.quaternion.w] : null,
      muzzle: weaponModel.muzzle ? [weaponModel.muzzle.x, weaponModel.muzzle.y,
        weaponModel.muzzle.z] : null,
    },
  },
};
if (request.mode === 'animation') {
  const animation = [];
  if (request.count > 1) {
    const speeds = { WALK: 1.4, RUN: 3.2, CROUCH_WALK: .7, PRONE_MOVE: .35 };
    for (const state of Object.values(RTS.AnimationState)) {
      const animator = new RTS.InfantryAnimator(model, genome, phenotype);
      animator.setState(state, true);
      const translations = [], rotations = [], morphWeights = [], locomotion = [], posture = [],
        bipedTargets = [], targetRotations = [], poseChannels = [], limbTargets = [], footGoals = [], previousArmRotations = [];
      const speed = speeds[state] || 0;
      const context = { speed, distance: speed / 60, moveAngle: 0, turnRate: 0,
        treadmill: true, surface: RTS.FlatSurface };
      for (let frame = 0; frame < request.count; ++frame) {
        animator.update(1 / 60, context);
        const gait = animator.locomotion.gait;
        locomotion.push(animator.locomotion.actualCrouch,
          animator.locomotion.targetSpeedMps, gait.cycleM, gait.duty, gait.run,
          gait.sprint, gait.liftM, gait.amplitude, gait.cadence, animator.phase);
        const profile = animator.bipedProfile;
        posture.push(profile.pelvisPitch || 0, profile.hipY || 0, profile.hipZ || 0,
          profile.lowerPitch || 0, profile.upperPitch || 0, profile.chestPitch || 0,
          profile.stanceHalf || 0, profile.footZ || 0, profile.footYaw || 0,
          profile.kneeHalf || 0);
        const target = animator.targetPose;
        poseChannels.push(target.hips.x, target.hips.y, target.hips.z, target.handCurl || 0);
        for (const goal of animator.footGoal) footGoals.push(goal.x, goal.y, goal.z);
        for (const quaternion of target.q) targetRotations.push(
          quaternion.x, quaternion.y, quaternion.z, quaternion.w);
        for (const quaternion of animator.previousArmPose.q) previousArmRotations.push(
          quaternion.x, quaternion.y, quaternion.z, quaternion.w);
        for (let side = 0; side < 2; ++side) {
          const foot = target.feet[side], knee = target.knees[side];
          bipedTargets.push(foot.x, foot.y, foot.z, knee.x, knee.y, knee.z,
            target.footPlant[side], target.support[side], target.footPitch[side],
            target.toePitch[side], target.footYaw[side]);
          const hand = target.hands[side], elbow = target.elbows[side];
          limbTargets.push(hand.x, hand.y, hand.z, elbow.x, elbow.y, elbow.z,
            target.handPlant[side], target.handLift[side], target.footRelative[side],
            target.anklePitch[side], target.ankleYaw[side]);
        }
        for (const bone of model.rig.bones) {
          translations.push(bone.position.x, bone.position.y, bone.position.z);
          rotations.push(bone.quaternion.x, bone.quaternion.y, bone.quaternion.z,
            bone.quaternion.w);
        }
        const influences = model.mesh.morphTargetInfluences || [];
        for (let index = 0; index < 4; ++index) morphWeights.push(influences[index] || 0);
      }
      animation.push({ state, frames: request.count, translations, rotations, morphWeights,
        locomotion, posture, bipedTargets, targetRotations, poseChannels, limbTargets, footGoals,
        previousArmRotations });
    }
  }
  output = { schema: fullOutput.schema, provenance,
    request: { ...fullOutput.request, count: request.count }, rig: fullOutput.rig,
    anatomy: fullOutput.anatomy, animation };
}

function gnifStream(name, values, type) {
  const array = type === 4 ? Uint32Array.from(values) : Float32Array.from(values);
  return { name, type, array, bytes: Buffer.from(array.buffer, array.byteOffset, array.byteLength) };
}

function streamTolerance(name, type) {
  if (type !== 1) return 1;
  if (name.includes('.normals')) return 2e-5;
  if (name.includes('rotations')) return 2e-4;
  return 2e-6;
}

function quantizedFNV1a64(stream) {
  const tolerance = streamTolerance(stream.name, stream.type);
  let hash = 0xcbf29ce484222325n;
  const prime = 0x100000001b3n;
  for (const value of stream.array) {
    let quantized = typeIsInteger(stream.type)
      ? BigInt(value) : BigInt(Math.round(Number(value) / tolerance));
    if (quantized < 0n) quantized += 1n << 64n;
    for (let byte = 0; byte < 8; ++byte) {
      hash ^= (quantized >> BigInt(byte * 8)) & 0xffn;
      hash = (hash * prime) & 0xffffffffffffffffn;
    }
  }
  return hash.toString(16).padStart(16, '0');
}

function typeIsInteger(type) { return type === 3 || type === 4; }

function extrema(values, stride) {
  if (!values.length) return null;
  const minimum = Array(stride).fill(Infinity), maximum = Array(stride).fill(-Infinity);
  for (let index = 0; index < values.length; index += stride) {
    for (let component = 0; component < stride; ++component) {
      const value = values[index + component];
      minimum[component] = Math.min(minimum[component], value);
      maximum[component] = Math.max(maximum[component], value);
    }
  }
  return { minimum, maximum };
}

function writeGnif(filename, fixture) {
  const streams = [];
  const addMesh = (prefix, mesh) => {
    if (!mesh) return;
    streams.push(gnifStream(`${prefix}.positions`, mesh.positions, 1));
    streams.push(gnifStream(`${prefix}.normals`, mesh.normals, 1));
    streams.push(gnifStream(`${prefix}.colors`, mesh.colors, 1));
    streams.push(gnifStream(`${prefix}.uvs`, mesh.uvs, 1));
    streams.push(gnifStream(`${prefix}.skinIndices`, mesh.skinIndices, 1));
    streams.push(gnifStream(`${prefix}.skinWeights`, mesh.skinWeights, 1));
    streams.push(gnifStream(`${prefix}.indices`, mesh.indices, 4));
    for (const morph of mesh.morphs) {
      streams.push(gnifStream(`${prefix}.morph.${morph.name}.positions`, morph.positions, 1));
      streams.push(gnifStream(`${prefix}.morph.${morph.name}.normals`, morph.normals, 1));
    }
  };
  addMesh('body', fixture.body);
  addMesh('gear', fixture.gear);
  if (fixture.weapon) {
    addMesh('weapon.primary', fixture.weapon.primary);
    addMesh('weapon.slide', fixture.weapon.slide);
    addMesh('weapon.muzzleFlash', fixture.weapon.muzzleFlash);
  }
  for (const state of fixture.animation || []) {
    streams.push(gnifStream(`animation.${state.state}.translations`, state.translations, 1));
    streams.push(gnifStream(`animation.${state.state}.rotations`, state.rotations, 1));
    streams.push(gnifStream(`animation.${state.state}.morphWeights`, state.morphWeights, 1));
    streams.push(gnifStream(`animation.${state.state}.locomotion`, state.locomotion, 1));
    streams.push(gnifStream(`animation.${state.state}.posture`, state.posture, 1));
    streams.push(gnifStream(`animation.${state.state}.bipedTargets`, state.bipedTargets, 1));
    streams.push(gnifStream(`animation.${state.state}.targetRotations`, state.targetRotations, 1));
    streams.push(gnifStream(`animation.${state.state}.poseChannels`, state.poseChannels, 1));
    streams.push(gnifStream(`animation.${state.state}.limbTargets`, state.limbTargets, 1));
    streams.push(gnifStream(`animation.${state.state}.footGoals`, state.footGoals, 1));
    streams.push(gnifStream(`animation.${state.state}.previousArmRotations`, state.previousArmRotations, 1));
  }
  const header = Buffer.alloc(12);
  header.write('GNIF', 0, 'ascii');
  header.writeUInt32LE(1, 4);
  header.writeUInt32LE(streams.length, 8);
  const parts = [header];
  const descriptions = [];
  for (const stream of streams) {
    const name = Buffer.from(stream.name, 'utf8');
    const descriptor = Buffer.alloc(14);
    descriptor.writeUInt16LE(name.length, 0);
    descriptor.writeUInt8(stream.type, 2);
    descriptor.writeBigUInt64LE(BigInt(stream.array.length), 6);
    parts.push(descriptor.subarray(0, 2), name, descriptor.subarray(2), stream.bytes);
    descriptions.push({ name: stream.name, type: stream.type, elements: stream.array.length,
      bytes: stream.bytes.length, quantizedFNV1a64: quantizedFNV1a64(stream) });
  }
  fs.writeFileSync(filename, Buffer.concat(parts));
  const meshManifest = mesh => mesh ? {
    groups: mesh.groups, tags: mesh.tags, records: mesh.records,
    vertices: mesh.positions.length / 3, triangles: mesh.indices.length / 3,
    morphs: mesh.morphs.map(morph => morph.name),
    extrema: { positions: extrema(mesh.positions, 3), normals: extrema(mesh.normals, 3),
      indices: extrema(mesh.indices, 1) },
    faceMetadata: mesh.faceMetadata, bounds: mesh.bounds, materials: mesh.materials,
  } : null;
  const weaponManifest = fixture.weapon ? {
    definition: fixture.weapon.definition,
    primary: meshManifest(fixture.weapon.primary),
    slide: meshManifest(fixture.weapon.slide),
    muzzleFlash: meshManifest(fixture.weapon.muzzleFlash),
    grips: fixture.weapon.grips,
  } : undefined;
  const manifest = { ...fixture, body: meshManifest(fixture.body),
    gear: meshManifest(fixture.gear), weapon: weaponManifest, streams: descriptions };
  if (fixture.animation) manifest.animation = fixture.animation.map(state => ({
    state: state.state, frames: state.frames, bones: fixture.rig.length,
  }));
  fs.writeFileSync(`${filename}.json`, `${JSON.stringify(manifest, null, 2)}\n`);
}

fs.mkdirSync(path.dirname(path.resolve(destination)), { recursive: true });
if (path.extname(destination).toLowerCase() === '.gnif') writeGnif(destination, output);
else fs.writeFileSync(destination, `${JSON.stringify(output)}\n`);
model.dispose();
if (weaponModel) weaponModel.dispose();
console.log(JSON.stringify({
  destination: path.resolve(destination),
  mode: request.mode,
  bodyVertices: fullOutput.body.positions.length / 3,
  bodyTriangles: fullOutput.body.indices.length / 3,
  bones: fullOutput.rig.length,
}));
