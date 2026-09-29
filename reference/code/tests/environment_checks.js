(async function(){
  'use strict';
  const R=window.RTS,assert=(condition,message)=>{if(!condition)throw Error(message);},results=[];
  assert(Object.keys(R.EnvironmentCatalog.plants).length===20,'Expected 20 species');
  for(const species of Object.keys(R.EnvironmentCatalog.plants)){
    const dna=R.PlantGenome.create(species,1337),repeat=R.PlantGenome.create(species,1337),other=R.PlantGenome.create(species,1338);
    assert(JSON.stringify(dna)===JSON.stringify(repeat),'Genome determinism '+species);
    assert(JSON.stringify(dna.genes)!==JSON.stringify(other.genes),'Seed variation '+species);
    let structure,summerLeaves;
    for(const season of ['summer','spring','autumn','winter']){
      const m=R.PlantGenerator.create(dna,{season});
      const serial=JSON.stringify(m.skeleton);if(!structure)structure=serial;else assert(serial===structure,'Season changed skeleton '+species);
      for(const mesh of m.root.children){assert(Array.from(mesh.geometry.attributes.position.array).every(Number.isFinite),'NaN '+species);assert(mesh.geometry.boundingSphere.radius>0,'Bounds '+species);}
      assert(m.stats.triangles<180000,'Geometry budget '+species);
      if(season==='summer')summerLeaves=m.stats.leaves;
      if(season==='winter')assert(R.EnvironmentCatalog.plants[species].evergreen?m.stats.leaves===summerLeaves:m.stats.leaves===0,'Winter foliage '+species);
      results.push({species,season,...m.stats});m.dispose();assert(m.root.parent===null,'Dispose '+species);
    }
    const a=R.PlantGenerator.create(dna),b=R.PlantGenerator.create(dna),low=R.PlantGenerator.create(dna,{detail:'world'}),young=R.PlantGenerator.create(dna,{age:.2,season:'autumn'});
    assert(JSON.stringify(Array.from(a.root.children[0].geometry.attributes.position.array))===JSON.stringify(Array.from(b.root.children[0].geometry.attributes.position.array)),'Mesh determinism '+species);
    assert(JSON.stringify(a.skeleton)===JSON.stringify(low.skeleton),'LOD changed branches '+species);
    assert(low.stats.triangles<a.stats.triangles,'LOD budget '+species);
    assert(young.stats.fruits===0&&young.skeleton.height<a.skeleton.height,'Juvenile '+species);
    for(const m of [a,b,low,young])m.dispose();
    for(const extreme of [0,1]){const extremeDNA=R.PlantGenome.create(species,0,Object.fromEntries(Object.keys(dna.genes).map(k=>[k,extreme]))),m=R.PlantGenerator.create(extremeDNA,{age:.05,season:'winter'});assert(m.skeleton.nodes.every(n=>n.r1>0&&n.r2>0&&n.b.every(Number.isFinite)),'Extreme genes '+species);m.dispose();}
  }
  // Exercise actual editor events, retaining edited DNA through season and age changes.
  const $=id=>document.getElementById(id);$('species').value='apple';$('species').dispatchEvent(new Event('change'));$('seed').value='2026';$('seed').dispatchEvent(new Event('change'));
  await environmentLab.rebuild();const saved=JSON.stringify(environmentLab.genome);
  $('season').value='autumn';$('season').dispatchEvent(new Event('change'));assert(JSON.stringify(environmentLab.genome)===saved,'Editor season rerolled DNA');await environmentLab.rebuild();
  $('detail').value='world';$('detail').dispatchEvent(new Event('change'));await environmentLab.rebuild();assert(environmentLab.model.state.detail==='world','Editor detail');
  $('detail').value='high';$('detail').dispatchEvent(new Event('change'));await environmentLab.rebuild();
  const oldRadius=environmentLab.model.skeleton.nodes[0].r1;
  for(const key of ['girth','damage']){$(key).value='1';$(key).dispatchEvent(new Event('input'));$(key).dispatchEvent(new Event('change'));}
  await environmentLab.rebuild();
  assert(environmentLab.model.skeleton.nodes[0].r1>oldRadius,'Girth slider invalidates skeleton');
  assert(environmentLab.model.skeleton.nodes.some(n=>n.broken),'Damage slider invalidates skeleton');
  assert($('species').options.length===20,'UI catalog');assert(document.querySelectorAll('#categories button:disabled').length===1,'Future categories');
  return {results,tests:['20 species × 4 seasons','stable skeleton and mesh','distinct seeds','juvenile fruit suppression','low detail','extreme genomes','editor events']};
})();
