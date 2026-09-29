const {spawnSync}=require('node:child_process'),path=require('node:path');
const root=path.resolve(__dirname,'../..');
const three=path.join(__dirname,'node_modules','three');
const tests=[
  ['Building v6','tests/building_v6.cjs'],
  ['Building roof quality','tests/building_roof_quality.cjs'],
  ['Building solver quality','tests/building_solver_quality.cjs'],
  ['Building geometry quality','tests/building_geometry_quality.cjs'],
  ['Building fuzz v6','tests/building_fuzz_v6.cjs'],
  ['Building collision proxy v6','tests/building_collision_proxy.cjs'],
  ['Building damage chunks v6','tests/building_damage_chunks.cjs'],
  ['Building world representation v6','tests/building_world_representation.cjs'],
  ['Building destruction adapter v6','tests/building_destruction_v6.cjs'],
  ['Building destruction quality','tests/building_destruction_quality.cjs'],
  ['Destruction core','tests/destruction_core.cjs'],
  ['Projectile state and contacts','tests/destruction_projectile.cjs'],
  ['Projectile trace recorder','tests/destruction_trace.cjs'],
  ['Spatial index and materials','tests/destruction_spatial.cjs'],
  ['Persistent rubble field','tests/destruction_rubble.cjs'],
  ['Destruction effects','tests/destruction_effects.cjs'],
  ['Destruction lifecycle','tests/destruction_lifecycle.cjs'],
  ['World building spatial index','tests/world_building_spatial_index.cjs'],
  ['Combat AI registry and perception','tests/combat_ai.cjs'],
  ['Infantry damage runtime topology','tests/infantry_damage_runtime.cjs'],
];
let passed=0;
for(const [name,file,...args] of tests){
  process.stdout.write('\n=== '+name+' ===\n');
  const result=spawnSync(process.execPath,[path.join(root,file),...args],{cwd:root,stdio:'inherit',env:process.env});
  if(result.error)throw result.error;
  if(result.status!==0){console.error('\nFAILED: '+name);process.exit(result.status||1);}
  passed++;
}
console.log('\nPASS '+passed+' building/destruction test groups');
