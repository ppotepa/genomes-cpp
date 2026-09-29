// Shared, real Three/Rapier building fixture for integration checks and CPU measurements.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.resolve(__dirname,'../..');
function load(){
  global.window=global;global.THREE=require('./node_modules/three');global.RTS={};
  const evaluate=file=>vm.runInThisContext(fs.readFileSync(path.join(root,file),'utf8'),{filename:file});
  evaluate('js/core/seededRandom.js');
  for(const match of fs.readFileSync(path.join(root,'building-lab/index.html'),'utf8').matchAll(/src="\.\.\/(js\/buildings\/[^\"]+)"/g))evaluate(match[1]);
  for(const name of ['catalogs','solid','impactProfiles','projectileMath','materialImpact','rubbleField','projectileState','impactSolver','surfaceDamage','materialModel','ballistics','physics','adapters'])evaluate('js/destruction/'+name+'.js');
  require('../../js/vendor/rapier-0.19.3.js');require('../../js/vendor/convex-hull-r128.js');
  return RTS;
}
function frontWall(target){
  const walls=[...target.model.parts.values()].filter(p=>p.category==='wall'&&!p.detached),front=Math.max(...walls.map(p=>p.bounds.max[2]));
  return walls.filter(p=>p.bounds.max[2]>front-.05&&p.bounds.max[2]-p.bounds.min[2]<1&&p.bounds.max[1]-p.bounds.min[1]>.5).sort((a,b)=>b.volume-a.volume)[0];
}
function create(options={}){
  const source=new RTS.ProceduralBuildingGenerator().generate({seed:2026,...options}),adapter=RTS.DestructionAdapters.building(source),target=adapter.create(),physics=new RTS.DestructionPhysics();target.attach(physics);
  const world=new RTS.BallisticsWorld({model:target.model,ground:true});
  return {source,adapter,target,physics,world,dispose(){world.dispose();target.dispose();physics.dispose();adapter.dispose();source.dispose();}};
}
function drain(target){let steps=0;while(target.jobs.length){target.update(1000);if(++steps>10000)throw Error('Rebuild queue stalled');}target.update(1000);}
module.exports={root,load,create,frontWall,drain};
