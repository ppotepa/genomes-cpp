(function(){
'use strict';
const R=globalThis.RTS=globalThis.RTS||{},B=R.Buildings=R.Buildings||{};
let state='idle',promise=null;
const vendors=()=>globalThis.RTSBuildingVendors;
function requireVendors(){const v=vendors();if(!v)throw new Error('Building vendor bundle is not loaded');return v;}
B.initializeBuildingBackends=function(){
  if(state==='ready')return Promise.resolve();
  if(promise)return promise;
  const v=requireVendors();state='loading';
  promise=Promise.resolve(v.straightSkeleton?.SkeletonBuilder?.init?.()).then(()=>{state='ready';}).catch(error=>{state='failed';promise=null;throw error;});
  return promise;
};
B.buildingBackendsReady=()=>state==='ready';
B.PolygonBackend=Object.freeze({
  union(...polygons){return requireVendors().polygonClipping.union(...polygons);},
  intersection(...polygons){return requireVendors().polygonClipping.intersection(...polygons);},
  difference(subject,...clips){return requireVendors().polygonClipping.difference(subject,...clips);},
  xor(...polygons){return requireVendors().polygonClipping.xor(...polygons);}
});
B.Triangulation=Object.freeze({triangulate(vertices,holes,dimensions=2){return Array.from(requireVendors().earcut(vertices,holes,dimensions));}});
B.ConstraintBackend=Object.freeze({get api(){return requireVendors().kiwi;}});
B.RoofSkeletonBackend=Object.freeze({
  build(polygon){if(state!=='ready')throw new Error('RoofSkeletonBackend requires initializeBuildingBackends()');return requireVendors().straightSkeleton.SkeletonBuilder.buildFromPolygon(polygon);}
});
B.NavBackend=Object.freeze({
  build(input,options){const v=requireVendors(),fn=v.navcatBlocks?.generateSoloNavMesh;if(!fn)throw new Error('Navcat generator unavailable');return fn(input,options);},
  serialize(value){return JSON.parse(JSON.stringify(value,(key,item)=>typeof item==='bigint'?item.toString():item));}
});
})();
