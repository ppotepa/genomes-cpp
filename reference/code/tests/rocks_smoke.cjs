'use strict';
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict');
global.window=global;global.THREE=require('./numeric_math.cjs');
for(const file of ['js/core/seededRandom.js','js/environment/rockGenerator.js','js/world/battlefield.js'])vm.runInThisContext(fs.readFileSync(path.join(__dirname,'..',file),'utf8'),{filename:file});
const R=RTS,positions=m=>m.root.children[0].geometry.attributes.position.array;
(async()=>{
  for(const species of Object.keys(R.RockCatalog)){
    const dna=R.RockGenome.create(species,734),a=R.RockGenerator.create(dna),b=R.RockGenerator.create(dna);
    assert.equal(a.root.children[0].geometry,b.root.children[0].geometry);
    const expected=Array.from(positions(a));a.dispose();a.dispose();assert.deepEqual(Array.from(positions(b)),expected);b.dispose();
    const different=R.RockGenerator.create(R.RockGenome.create(species,735,dna.genes));
    assert.notDeepEqual(Array.from(positions(different)),expected,'seed changes the actual silhouette');different.dispose();
    const repeat=R.RockGenerator.create(dna);assert.deepEqual(Array.from(positions(repeat)),expected);repeat.dispose();
    for(const extreme of [0,1]){
      const model=R.RockGenerator.create(R.RockGenome.create(species,734,Object.fromEntries(Object.keys(dna.genes).map(k=>[k,extreme]))));
      const geo=model.root.children[0].geometry;assert.ok(positions(model).every(Number.isFinite));assert.ok(geo.boundingSphere.radius>0);
      const edges=new Map(),p=positions(model);
      for(let i=0;i<p.length;i+=9){const points=[0,3,6].map(j=>Array.from(p.slice(i+j,i+j+3)).join(','));for(let j=0;j<3;j++){const key=[points[j],points[(j+1)%3]].sort().join('|');edges.set(key,(edges.get(key)||0)+1);}}
      assert.ok([...edges.values()].every(count=>count===2),'closed manifold mesh '+species);model.dispose();
    }
    for(const key of Object.keys(dna.genes)){
      const variant=R.RockGenerator.create(R.RockGenome.create(species,734,{...dna.genes,[key]:1}));
      const ref=R.RockGenerator.create(dna);assert.notEqual(variant.root.children[0].geometry,ref.root.children[0].geometry);variant.dispose();ref.dispose();
    }
    let yields=0;const lods=await R.RockGenerator.createLodsAsync(dna,{},async()=>{yields++;});assert.equal(yields,3);
    assert.deepEqual(lods.map(l=>l.stats.triangles),[80,20,20]);
    assert.equal(expected.length/9,320,'close-up triangle budget');
    const vertexKeys=array=>{const keys=new Set();for(let i=0;i<array.length;i+=3)keys.add(array.slice(i,i+3).join(','));return keys;};
    const highVertices=vertexKeys(expected);
    for(const lod of lods)for(const key of vertexKeys(Array.from(positions(lod))))assert.ok(highVertices.has(key),'LOD must retain the exact shared cage and anchor');
    for(const m of lods){const sync=R.RockGenerator.create(dna,m.state);assert.deepEqual(positions(m),positions(sync));sync.dispose();m.dispose();}
  }
  let calls=0;await assert.rejects(R.RockGenerator.createLodsAsync(R.RockGenome.create('granite',1),{},async()=>{},()=>{if(++calls===4)throw Error('cancel');}),/cancel/);
  assert.equal(R.RockGenerator.cacheSize(),0,'cancelled builds must release retained geometry');
  const terrain={sample(x,z,out){out.height=Math.sin(x)*.3;out.normal.set(0,1,0);}},plants=[{x:15,z:40,footprint:3}];
  const layout=seed=>Array.from(R.Battlefield.rockLayout(seed,terrain,plants)).filter(Boolean);
  for(let seed=0;seed<32;seed++){const rocks=layout(seed);assert.deepEqual(rocks,layout(seed));assert.equal(rocks.length,100);assert.ok(rocks.every(r=>Math.abs(r.z)-r.footprint>=24&&Math.hypot(r.x-15,r.z-40)>=3+r.footprint));}
  assert.notDeepEqual(layout(1),layout(2));
  const dense=layout(42);
  const sparse=Array.from(R.Battlefield.rockLayout(42,terrain,[],{clusterSpacing:1,clusterRadius:0,density:0,overlap:0,burial:0,uniformity:0,formationMix:0})).filter(Boolean);
  assert.equal(sparse.length,60,'density controls world rock count');
  const heavy=Array.from(R.Battlefield.rockLayout(42,terrain,[],{clusterSpacing:0,clusterRadius:1,density:1,overlap:.8,burial:.65,uniformity:1,formationMix:1})).filter(Boolean);
  assert.equal(heavy.length,140,'dense profile fills wider cluster budgets');
  assert.ok(new Set(heavy.map(r=>r.cluster)).size>new Set(sparse.map(r=>r.cluster)).size,'cluster spacing controls group count');
  assert.ok(heavy.every(r=>r.burial>=.35),'burial controls per-rock embed depth');
  assert.ok(heavy.some(r=>r.formation==='outcrop')&&heavy.some(r=>r.formation==='talus'),'formation mix enables rock formations');
  const minGap=rocks=>Math.min(...rocks.flatMap((a,i)=>rocks.slice(i+1).map(b=>Math.hypot(a.x-b.x,a.z-b.z)-a.footprint-b.footprint)));
  assert.ok(minGap(heavy)<0&&minGap(heavy)<minGap(sparse),'overlap setting permits actual rock intersections');
  console.log('PASS 8 rock types: closed finite meshes, deterministic DNA, cache ownership, LOD parity/cancellation; 32 seeded world layouts');
})().catch(error=>{console.error(error);process.exitCode=1;});
