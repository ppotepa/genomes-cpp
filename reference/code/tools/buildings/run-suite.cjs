'use strict';
const {spawnSync}=require('node:child_process'),path=require('node:path');
const root=path.resolve(__dirname,'../..');
for(const file of ['building_v6','building_roof_quality','building_solver_quality','building_geometry_quality','building_fuzz_v6','building_collision_proxy','building_world_representation','building_damage_chunks','building_destruction_v6','building_destruction_quality']){
  const result=spawnSync(process.execPath,[path.join(root,'tests',file+'.cjs')],{cwd:root,stdio:'inherit',timeout:180000});
  if(result.error)throw result.error;
  if(result.status!==0)process.exit(result.status||1);
}
console.log('PASS building acceptance suite');
