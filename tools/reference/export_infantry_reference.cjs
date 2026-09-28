#!/usr/bin/env node
'use strict';

// Offline semantic evidence exporter. It loads only the pinned read-only
// reference repository; native runtime targets never depend on this script.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const positional = process.argv.slice(2).filter(argument => !argument.startsWith('--'));
const referenceRoot = positional[0] ||
  path.resolve(__dirname, '..', '..', '..', 'genomes');
const sourceRoot = fs.existsSync(path.join(referenceRoot, 'reference', 'js'))
  ? path.join(referenceRoot, 'reference') : referenceRoot;
const outputPath = positional[1] || '';
global.window = global;
global.THREE = require(path.join(sourceRoot, 'tests', 'numeric_math.cjs'));
THREE.Quaternion.prototype.dot = function dot(q) {
  return this.x * q.x + this.y * q.y + this.z * q.z + this.w * q.w;
};
THREE.SkinnedMesh.prototype.updateMorphTargets = function updateMorphTargets() {};
global.document = {createElement: () => ({getContext: () => ({})})};

const loader = fs.readFileSync(path.join(sourceRoot, 'js', 'loader.js'), 'utf8');
const modules = vm.runInNewContext(loader.match(/const local=(\[[\s\S]*?\]);/)[1]);
for (const modulePath of modules) {
  vm.runInThisContext(fs.readFileSync(path.join(sourceRoot, modulePath), 'utf8'),
                     {filename: modulePath});
}

const seeds = [0x5EED2026, 0x12345678, 0xCAFEBABE];
const captures = seeds.map(seed => {
  const genome = RTS.InfantryGenome.create(seed);
  const phenotype = RTS.InfantryGenome.express(genome);
  const anatomy = RTS.InfantryAnatomy.create(
    phenotype.height, phenotype.face, phenotype.body);
  const face = anatomy.faceLayout;
  const rig = new RTS.InfantryRig(anatomy, new THREE.Group());
  const levels = face.levels.map(level => ({
    y: level[0] * phenotype.height,
    half_width: level[1] * phenotype.height,
    half_depth: level[2] * phenotype.height,
    center_z: level[3] * phenotype.height,
  }));
  const landmarks = {
    left_eye: face.eyes.L,
    right_eye: face.eyes.R,
    mouth: face.mouth,
    jaw: face.jaw,
  };
  const clean = value => {
    if (value && typeof value === 'object' && !Array.isArray(value)) {
      const result = {};
      for (const [key, entry] of Object.entries(value)) {
        if (key === 'faceLayout' || key === 'root' || key === 'skeleton' ||
            key === 'byName' || key === 'index' || key === 'bones' || key === 'rest') continue;
        result[key] = clean(entry);
      }
      return result;
    }
    if (Array.isArray(value)) return value.map(clean);
    if (value && typeof value.x === 'number' && typeof value.y === 'number' &&
        typeof value.z === 'number') return {x: value.x, y: value.y, z: value.z};
    return value;
  };
  return {
    seed,
    genome,
    phenotype: clean({height: phenotype.height, body: phenotype.body, face: phenotype.face}),
    anatomy: {height: phenotype.height, head_levels: levels, landmarks: clean(landmarks)},
    rig: {bone_count: rig.bones.length, names: rig.bones.map(bone => bone.name)},
    source_adjustments: clean(face.adjustments),
  };
});

const result = {
  schema_version: 1,
  source_repository: 'ppotepa/genomes',
  source_commit: 'da885ca68b2ae63154a004574fed00eb9dfeb458',
  generator_version: 'reference-js-semantic-export-v1',
  evidence_class: 'semantic-reference',
  captures,
};
const compact = process.argv.includes('--compact');
const output = compact ? {
  ...result,
  captures: captures.map(capture => ({
    seed: capture.seed,
    height: capture.phenotype.height,
    shoulder_width_scale: capture.phenotype.body.shoulderWidthScale,
    hip_width_scale: capture.phenotype.body.hipWidthScale,
    eye_y: capture.phenotype.face.eyeY,
    mouth_y: capture.phenotype.face.mouthY,
    hair_style: capture.phenotype.face.hairStyle,
    head_level_count: capture.anatomy.head_levels.length,
    first_head_level: capture.anatomy.head_levels[0],
    last_head_level: capture.anatomy.head_levels.at(-1),
    rig_bone_count: capture.rig.bone_count,
    adjustment_count: capture.source_adjustments.length,
  })),
} : result;
const text = JSON.stringify(output, null, 2) + '\n';
if (outputPath) fs.writeFileSync(outputPath, text);
else process.stdout.write(text);
