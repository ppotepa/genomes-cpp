'use strict';
const assert=require('node:assert/strict');
global.window=global;global.RTS={};
global.THREE=require('./numeric_math.cjs');
require('../js/core/seededRandom.js');
require('../js/world/settlementGenerator.js');
require('../js/world/fenceGenerator.js');
require('../js/world/battlefield.js');
const {WorldGeneration:W,FenceGenerator:F}=RTS,variants=new Set();
let parcels=0,zones=0,fences=0,chamfers=0;
for(const size of [400,600,800,1200])for(let seed=0;seed<18;seed++){
  const input={seed,size,buildings:.75,fencedParcels:1},world=W.create(input),again=W.create(input);
  variants.add(world.variant);
  assert.deepEqual(world.roads,again.roads);
  assert.deepEqual(world.parcels,again.parcels);
  assert.deepEqual(world.zones,again.zones);
  assert(world.buildings.length>=5&&world.buildings.length<=34);
  assert(world.zones.length>0);
  for(let i=1;i<world.roads.length;i++){
    const route=world.roads[i],connected=world.roads.slice(0,i).some(previous=>route.points.some(p=>previous.points.some((end,j)=>j&&W.distanceToSegment(p.x,p.z,previous.points[j-1],end)<.01)));
    assert(connected,'road must connect to the existing network: '+route.id);
  }
  for(const zone of world.zones){
    const center={x:zone.polygon.reduce((s,p)=>s+p.x,0)/4,z:zone.polygon.reduce((s,p)=>s+p.z,0)/4};
    assert.equal(world.zoneAt(center.x,center.z),zone.type);
    zones++;
  }
  for(const parcel of world.parcels){
    const fence=F.plan(parcel,world.roads,world.parcels);
    assert(parcel.polygon.length>=4);
    if(parcel.polygon.length>4)chamfers++;
    for(let i=0;i<parcel.polygon.length;i++)for(let j=i+2;j<parcel.polygon.length;j++){
      if(i===0&&j===parcel.polygon.length-1)continue;
      assert(W.segmentsDistance(parcel.polygon[i],parcel.polygon[(i+1)%parcel.polygon.length],parcel.polygon[j],parcel.polygon[(j+1)%parcel.polygon.length])>0);
    }
    assert(world.roads.some(r=>r.id===parcel.driveway));
    assert(world.buildings.some(b=>b.parcel===parcel));
    if(fence.gate){
      fences++;
      assert(Math.hypot((fence.gate.left.x+fence.gate.right.x)/2-parcel.gate.x,(fence.gate.left.z+fence.gate.right.z)/2-parcel.gate.z)<1e-6);
      for(const panel of fence.panels){
        assert(Math.hypot(panel.a.x-panel.b.x,panel.a.z-panel.b.z)<=2.401);
        for(const road of world.roads)for(let i=1;i<road.points.length;i++)
          assert(W.segmentsDistance(panel.a,panel.b,road.points[i-1],road.points[i])>=road.width*.5+.119);
        for(let i=0;i<parcel.buildingFootprint.length;i++)
          assert(W.segmentsDistance(panel.a,panel.b,parcel.buildingFootprint[i],parcel.buildingFootprint[(i+1)%parcel.buildingFootprint.length])>=.799);
      }
    }
    parcels++;
  }
  for(let i=0;i<world.parcels.length;i++)for(let j=i+1;j<world.parcels.length;j++)
    assert(!W.polygonOverlap(world.parcels[i].polygon,world.parcels[j].polygon));
  if(seed<3){
    const terrain={mapSize:size,sample(x,z,out){out.height=0;out.normal.set(0,1,0);}};
    const plants=Array.from(RTS.Battlefield.vegetationLayout(seed,terrain,world,input.vegetation||.62)).filter(Boolean);
    assert(plants.every(p=>!world.isReserved(p.x,p.z,1.5)&&!['field','fallow','pasture','meadow'].includes(world.zoneAt(p.x,p.z))));
    const rocks=Array.from(RTS.Battlefield.rockLayout(seed,terrain,plants,{},world)).filter(Boolean);
    assert(rocks.every(p=>!world.isReserved(p.x,p.z,p.footprint+.3)&&!['field','orchard','pasture'].includes(world.zoneAt(p.x,p.z))));
  }
}
assert.deepEqual([...variants].sort(),['crossroads','hamlets','winding']);
assert(fences>parcels*.3,'too many parcels lost their safe fence');
assert(chamfers>parcels*.2,'irregular boundaries are missing');
console.log('PASS '+parcels+' parcels, '+fences+' fences and '+zones+' zones across three seeded village layouts');
