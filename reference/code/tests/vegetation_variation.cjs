'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
global.window=global;global.THREE=require('./numeric_math.cjs');
THREE.Color.prototype.multiplyScalar=function(s){this.r*=s;this.g*=s;this.b*=s;return this;};
for(const file of ['js/core/seededRandom.js','js/environment/plantCatalog.js','js/environment/plantGenerator.js','js/world/battlefield.js'])vm.runInThisContext(fs.readFileSync(path.join(__dirname,'..',file),'utf8'),{filename:file});
const R=RTS,terrain={mapSize:400,sample(x,z,out){out.height=.1*Math.sin(x)*Math.cos(z);out.normal.set(0,1,0);}};
const layout=seed=>Array.from(R.Battlefield.vegetationLayout(seed,terrain)).filter(Boolean);
for(let seed=0;seed<64;seed++){
  const plants=layout(seed),trees=plants.filter(p=>p.kind==='tree');
  assert.deepEqual(plants,layout(seed),'seed must reproduce all placements');
  assert.equal(trees.length,90);assert.equal(plants.length,205);
  assert.ok(plants.every(p=>Math.abs(p.x)<=184&&Math.abs(p.z)<=184));
  let close=0;const nearest=[];
  for(let i=0;i<trees.length;i++){
    let distance=Infinity;
    for(let j=0;j<trees.length;j++)if(i!==j)distance=Math.min(distance,Math.hypot(trees[i].x-trees[j].x,trees[i].z-trees[j].z));
    nearest.push(distance);if(distance<4)close++;
  }
  assert.ok(close>0,'must allow closely spaced trees');
  assert.ok(Math.max(...nearest)>7,'must also retain gaps');
  assert.ok(Math.max(...trees.map(p=>p.scale))/Math.min(...trees.map(p=>p.scale))>2);
  for(let i=0;i<plants.length;i++)for(let j=i+1;j<plants.length;j++)assert.ok(Math.hypot(plants[i].x-plants[j].x,plants[i].z-plants[j].z)>=plants[i].footprint+plants[j].footprint,'trunks overlap');
}
assert.notDeepEqual(layout(1),layout(2));
for(const species of Object.keys(R.EnvironmentCatalog.plants)){
  const dna=R.PlantGenome.create(species,734),intact=R.PlantGenerator.skeleton(dna,.7);
  const thick=R.PlantGenerator.skeleton(R.PlantGenome.create(species,734,{...dna.genes,girth:1}),.7);
  const damagedDNA=R.PlantGenome.create(species,734,{...dna.genes,damage:1});
  const damaged=R.PlantGenerator.skeleton(damagedDNA,.7);
  assert.deepEqual(damaged,R.PlantGenerator.skeleton(damagedDNA,.7));
  assert.ok(thick.nodes[0].r1>intact.nodes[0].r1);
  assert.equal(thick.height,intact.height,'girth must be independent of height');
  assert.ok(damaged.nodes.some(n=>n.broken),'damage must create bare stubs');
  assert.ok(damaged.nodes.every(n=>n.r1>0&&n.r2>0&&n.b.every(Number.isFinite)));
  const a=R.PlantGenerator.create(dna,{age:.7,detail:'far'}),b=R.PlantGenerator.create(damagedDNA,{age:.7,detail:'far'});
  assert.notEqual(a.skeleton,b.skeleton,'damage must invalidate skeleton cache');
  assert.notEqual(a.root.children[0].geometry,b.root.children[0].geometry,'damage must invalidate wood cache');
  a.dispose();b.dispose();
}
console.log('PASS 64 seeded layouts: clusters, gaps, shrubs, scale range, trunk clearance; 20 species: girth, damage and cache isolation');
