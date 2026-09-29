(async function(){
  const game=RTS.game,assert=(ok,message)=>{if(!ok)throw Error(message);};
  cancelAnimationFrame(game.frameId);
  document.getElementById('rockDensity').value='.64';document.getElementById('rockDensity').dispatchEvent(new Event('input'));
  document.getElementById('rockOverlap').value='.52';document.getElementById('rockOverlap').dispatchEvent(new Event('input'));
  let terrainYieldCount=0;
  const originalTerrainBuildAsync=RTS.TerrainSystem.prototype.buildAsync;
  RTS.TerrainSystem.prototype.buildAsync=function(seed,yieldFrame,check){return originalTerrainBuildAsync.call(this,seed,async()=>{terrainYieldCount++;await yieldFrame();},check);};
  await game.startNew(20260924);
  assert(game.battlefield&&game.units.length===50,'50 soldiers');
  assert(terrainYieldCount>=20,'Terrain generation yields between bounded CPU work slices');
  assert(game.sides.SIDE_A.units.length===25&&game.sides.SIDE_B.units.length===25,'25 per side');
  const world=game.battlefield,terrain=game.terrain;
  assert(world.rockSettings.density===.64&&world.rockSettings.overlap===.52,'Map controls reach the seeded rock generator');
  assert(JSON.parse(localStorage.getItem('genomes.rockPlacement.v1')).density===.64,'Rock controls persist');
  assert(terrain.mapSize===200,'Map size');assert(terrain.sectorAt(101,0)===null&&terrain.sectorAt(100,100).id===1599,'Sector limits');
  const terrainGeometry=terrain.mesh.children[0].geometry,positions=terrainGeometry.attributes.position;let low=Infinity,high=-Infinity;
  for(let i=0;i<positions.count;i++){low=Math.min(low,positions.getY(i));high=Math.max(high,positions.getY(i));}
  assert(high-low>2,'Uneven terrain');assert(world.trees.length>=60,'Tree population');assert(world.trees.every(t=>Math.abs(t.z)>=24),'March corridor');
  assert(world.trees.some(t=>t.kind==='shrub'),'Shrub undergrowth');
  assert(world.rocks.length>=80,'Rock population');
  assert(world.rocks.every(r=>Math.abs(r.z)-r.footprint>=24),'Rocks outside march corridor');
  const rockLod=world.instances.find(lod=>lod.levels[0].object.children.some(part=>part.name.endsWith(' rock')));
  assert(rockLod&&rockLod.levels.length===3,'Instanced rocks have three LODs');
  const treeScales=world.trees.filter(t=>t.kind==='tree').map(t=>t.scale);
  assert(Math.max(...treeScales)/Math.min(...treeScales)>2,'Varied tree heights');
  assert(world.prototypes.some(lods=>lods[0].skeleton.nodes.some(n=>n.broken)),'Incomplete branches');
  const treeLod=world.instances[0];treeLod.updateMatrixWorld(true);assert(treeLod.levels.length===3,'Tree LOD levels');
  const sharedInstanceAttribute=treeLod.levels[0].object.children[0].instanceMatrix,sharedTransforms=sharedInstanceAttribute.array;
  for(let level=0;level<treeLod.levels.length;level++)for(const part of treeLod.levels[level].object.children){
    const matrices=part.instanceMatrix.array;assert(part.instanceMatrix===sharedInstanceAttribute,'tree LOD/material parts should share one instance buffer');assert(part.count===treeLod.levels[level].object.children[0].count,'Tree LOD instance count');assert(matrices.length===sharedTransforms.length,'Tree LOD instance buffer length');
    for(let i=0;i<matrices.length;i++)assert(matrices[i]===sharedTransforms[i],'Tree instance transforms stay identical across materials and LODs');
  }
  const nearFoliage=treeLod.levels[0].object.children.find(part=>part.name.endsWith(' foliage')),
    farFoliage=treeLod.levels[2].object.children.find(part=>part.name.endsWith(' foliage')),
    farWood=treeLod.levels[2].object.children.find(part=>part.name.endsWith(' wood'));
  assert(nearFoliage&&nearFoliage.castShadow,'Near foliage must retain shadows');
  assert(farFoliage&&!farFoliage.castShadow&&farWood&&farWood.castShadow,'Far foliage skips low-value shadows while trunks keep them');
  const lodCenter=treeLod.getWorldPosition(new THREE.Vector3()),lodCamera={zoom:1,getWorldPosition(target){return target.copy(lodCenter).add(new THREE.Vector3(distance,0,0));}};
  let distance=65;treeLod.update(lodCamera);assert(treeLod.getCurrentLevel()===0,'LOD should not switch at nominal boundary while moving farther');
  distance=71;treeLod.update(lodCamera);assert(treeLod.getCurrentLevel()===1,'LOD should switch after outer hysteresis threshold');
  distance=65;treeLod.update(lodCamera);assert(treeLod.getCurrentLevel()===1,'LOD should remain stable inside hysteresis band');
  distance=59;treeLod.update(lodCamera);assert(treeLod.getCurrentLevel()===0,'LOD should return after inner hysteresis threshold');
  distance=141;treeLod.update(lodCamera);assert(treeLod.getCurrentLevel()===2,'LOD should enter far tier after outer threshold');
  distance=125;treeLod.update(lodCamera);assert(treeLod.getCurrentLevel()===2,'Far LOD should remain stable inside hysteresis band');
  distance=119;treeLod.update(lodCamera);assert(treeLod.getCurrentLevel()===1,'Far LOD should return after inner threshold');
  const referenceTerrain=new RTS.TerrainSystem(new THREE.Group(),{mapSize:200,segments:160,battlefield:true});
  referenceTerrain.build(world.seed);
  assert(referenceTerrain.heights.length===terrain.heights.length&&referenceTerrain.heights.every((h,i)=>h===terrain.heights[i]),'Sliced terrain preserves exact height samples');
  const referenceGeometry=referenceTerrain.mesh.children[0].geometry,actualGeometry=terrainGeometry;
  assert(referenceGeometry.attributes.position.array.every((v,i)=>v===actualGeometry.attributes.position.array[i]),'Sliced terrain preserves exact positions');
  assert(referenceGeometry.attributes.color.array.every((v,i)=>v===actualGeometry.attributes.color.array[i]),'Sliced terrain preserves exact colors');
  referenceTerrain.dispose();
  const initial=game.units.map(u=>u.position.clone());
  for(let frame=0;frame<90;frame++)world.step(1/60);
  game.units.forEach((u,i)=>{assert((u.position.x-initial[i].x)*u.marchSign>1,'March direction '+u.id);assert(Math.abs(u.position.y-terrain.getHeightAt(u.position.x,u.position.z))<1e-6,'Ground contact');});
  // Test the stopping phase without simulating the whole 51 m approach.
  game.units.forEach(u=>u.setWorldPosition(u.marchTarget-u.marchSign*.65,u.position.z,terrain));
  for(let frame=0;frame<120;frame++)world.step(1/60);
  assert(world.arrived===50,'All units stop');
  game.units.forEach(u=>{assert(Math.abs(u.position.x-u.marchTarget)<1e-6,'Stop position');assert(u.speed===0,'Stopped speed');});
  const oldRoot=world.root,oldUnits=game.units.slice();
  document.getElementById('startNew').click();
  for(let i=0;i<600&&game.starting;i++)await new Promise(r=>setTimeout(r,50));
  assert(!game.starting&&game.battlefield!==world,'START NEW regenerates');
  assert(oldRoot.parent===null&&oldUnits.every(u=>u.root.parent===null),'Old world removed');
  assert(game.units.length===50&&game.sides.SIDE_A.units.length===25&&game.sides.SIDE_B.units.length===25,'No accumulated units');
  assert(!document.getElementById('startNew').disabled,'Button reenabled');
  game.units.forEach(u=>u.render(1));game.renderer.render(game.scene,game.camera);
  const visibleCalls=game.renderer.info.render.calls;
  for(const unit of game.units){assert(!Array.isArray(unit.model.mesh.material)&&unit.model.mesh.geometry.attributes.materialRegion,'Body did not use a single region-aware material');assert(!Array.isArray(unit.model.gearMesh.material)&&unit.model.gearMesh.geometry.attributes.materialRegion,'Gear did not use a single region-aware material');unit.model.mesh.visible=false;unit.model.gearMesh.visible=false;}
  game.renderer.render(game.scene,game.camera);const withoutInfantryCalls=game.renderer.info.render.calls;
  for(const unit of game.units){unit.model.mesh.visible=true;unit.model.gearMesh.visible=true;}
  game.units.forEach(u=>u.render(1));game.renderer.render(game.scene,game.camera);
  const infantryMeshCalls=visibleCalls-withoutInfantryCalls;
  assert(infantryMeshCalls>=game.units.length*2,'Infantry body/gear should each contribute one color-pass draw per unit');
  const programs=game.renderer.info.programs||[],failedPrograms=programs.filter(program=>program.diagnostics&&program.diagnostics.runnable===false);
  assert(failedPrograms.length===0,'Renderer reported un-runnable material shader program: '+JSON.stringify(failedPrograms.map(program=>program.diagnostics)));
  const result={seed:game.battlefield.seed,units:game.units.length,trees:game.battlefield.trees.length,mapSize:game.terrain.mapSize,heightRange:high-low,drawCalls:game.renderer.info.render.calls,infantryDrawCalls:infantryMeshCalls,infantryDrawCallsPerUnit:infantryMeshCalls/game.units.length,shaderPrograms:programs.length,failedShaderPrograms:failedPrograms.length,triangles:game.renderer.info.render.triangles,tests:['50 units / 25 per side','200 m map and sectors','uneven terrain','trees outside corridor','region-aware single-material infantry draw calls','shader compilation','march direction and grounding','arrival without crossing','button restart and cleanup']};
  RTS.TerrainSystem.prototype.buildAsync=originalTerrainBuildAsync;
  game.clock.reset();game.frameId=requestAnimationFrame(game.loop);return result;
})();
