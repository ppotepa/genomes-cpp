(function(){
'use strict';
const B=globalThis.RTS.Buildings;
function scaled(poly,factor){if(factor===1)return poly;const [x0,x1,z0,z1]=B.Polygon.bounds(poly),cx=(x0+x1)/2,cz=(z0+z1)/2;return B.Polygon.canonical({outer:poly.outer.map(p=>[cx+(p[0]-cx)*factor,cz+(p[1]-cz)*factor]),holes:poly.holes.map(h=>h.map(p=>[cx+(p[0]-cx)*factor,cz+(p[1]-cz)*factor]))});}
function storeys(spec,footprint){const result=[];for(let floor=0;floor<spec.storeys.count;floor++){const raw=spec.storeys.setbacks[floor],factor=Number.isFinite(raw)?Math.max(.45,Math.min(1,1-raw/Math.max(spec.width,spec.depth))):(floor===spec.storeys.count-1?spec.storeys.partialTop:1),polygon=scaled(footprint.polygon,factor);result.push({id:`storey-${floor}`,floor,elevation:floor*spec.storeys.floorHeight,height:spec.storeys.floorHeight,polygon,usablePolygon:polygon,voids:[]});}return result;}
function addFacadeOpenings(plan){
  const sizes={TALL:[.95,1.65,.65],SASH:[1.15,1.3,.85],WIDE:[1.7,1.25,.9],SMALL:[.7,.85,1.15],INDUSTRIAL:[1.8,1.5,1.05]};
  for(const wall of plan.walls.filter(w=>!w.internal)){
    const occupied=wall.openings.map(o=>[o.t*wall.length-o.width/2-.18,o.t*wall.length+o.width/2+.18]);
    const count=Math.max(0,Math.floor(wall.length/plan.spec.facade.bayWidth)),requested=sizes[plan.spec.facade.windowType];
    for(let i=0;i<count;i++){
      const t=(i+1)/(count+1),u=t*wall.length,width=Math.min(requested[0],wall.length/(count+1)*.6);
      const bottom=Math.min(requested[2],wall.height*.38),height=Math.min(requested[1],wall.height-bottom-.4);
      if(width<.45||occupied.some(([a,b])=>u+width/2>a&&u-width/2<b))continue;
      const opening={id:wall.id+'/window-'+i,wallId:wall.id,kind:'window',t,width,bottom,height,floor:wall.floor,rooms:[]};
      wall.openings.push(opening);plan.openings.push(opening);
    }
  }
}
function slabsFor(storeys,stairs){
  const slabs=[];
  for(const s of storeys){
    const voids=stairs.filter(st=>st.slabVoid.floor===s.floor).map(st=>st.slabVoid);
    let parts=[s.polygon];
    for(const item of voids)for(const polygon of item.polygons||[item.polygon]){
      parts=parts.flatMap(p=>B.Polygon.fromBackend(B.PolygonBackend.difference(B.Polygon.backendFormat(p),B.Polygon.backendFormat(polygon))));
    }
    for(const [index,polygon]of parts.entries())slabs.push({id:'slab-'+s.floor+(index?'/region-'+index:''),floor:s.floor,y:s.elevation,thickness:.18,polygon,voids,assemblyId:'floor-slab',physicalSolidId:'slab-'+s.floor+'/region-'+index,layers:[{id:'slab-core',material:'concrete',start:0,end:.18,thickness:.18}]});
  }
  return slabs;
}
function validatePlan(plan){const failures=[];if(plan.schema!=='rts.building-plan/4')failures.push('SCHEMA');if(!plan.rooms.length)failures.push('NO_ROOMS');if(!plan.walls.length)failures.push('NO_WALLS');if(plan.stairs.length!==Math.max(0,plan.storeys.length-1))failures.push('STAIR_COUNT');for(const room of plan.rooms){if(!B.Polygon.validate(room.polygon).valid)failures.push('ROOM_POLYGON:'+room.id);}for(const wall of plan.walls){if(!wall.edge||!Number.isFinite(wall.yaw)||wall.length<=0)failures.push('WALL:'+wall.id);for(const opening of wall.openings||[]){const center=opening.center??opening.t*wall.length;if(center-opening.width/2<-.0001||center+opening.width/2>wall.length+.0001||opening.bottom<0||opening.height<=0||opening.bottom+opening.height>wall.height+.0001)failures.push('OPENING_BOUNDS:'+opening.id);}}for(const opening of plan.openings)if(!plan.walls.some(w=>w.id===opening.wallId))failures.push('OPENING_WALL:'+opening.id);if(!plan.navigation?.navmesh)failures.push('NAVMESH');if(failures.length)throw new B.BuildingGenerationError('PLAN_VALIDATION_FAILED','validation',plan.spec,null,failures,plan);return plan;}
B.validateBuildingPlan=validatePlan;
B.createPlan=function(options={}){
  if(!B.buildingBackendsReady())throw new B.BuildingGenerationError('BACKENDS_NOT_INITIALIZED','preload',options?.schema==='rts.building-spec/6'?options:null,null,[],null);
  const spec=options?.schema==='rts.building-spec/6'?options:B.createBuildingSpec(options),footprint=B.generateFootprint(spec),storeyList=storeys(spec,footprint),graphs=storeyList.map(s=>B.createRoomGraph(spec.roomProgram,s.floor));for(let floor=0;floor<graphs.length-1;floor++)graphs[floor].connect(graphs[floor].nodes[1].id,graphs[floor+1].nodes[1].id,'VERTICAL_CONNECT');
  const core=B.createVerticalCore(spec,footprint.usable),solved=B.solveFloors(spec,footprint.polygon,storeyList.map(s=>s.usablePolygon),graphs,core),stairs=B.buildStairs(spec,core),assembly=B.buildWallsAndPortals(spec,storeyList.map(s=>s.polygon),solved.rooms,graphs);
  const plan={schema:'rts.building-plan/4',generatorVersion:'building-6.0.0',spec,preset:B.BUILDING_PRESETS[spec.presetId],footprint:{family:footprint.family,polygon:footprint.polygon,usablePolygon:footprint.usable,grammarTree:footprint.grammarTree},storeys:storeyList,roomGraph:{schema:'rts.room-graph-set/1',floors:graphs.map(g=>g.toJSON()),unitRegions:solved.unitRegions},rooms:solved.rooms,walls:assembly.walls,openings:assembly.openings,connections:assembly.connections,stairs,slabs:[],roof:null,navigation:null,structure:null,features:[],furniture:[],diagnostics:{footprint:footprint.diagnostics||{},floorSolver:solved.diagnostics,stairRejections:[],fallbacks:[]}};
  plan.slabs=slabsFor(storeyList,stairs);addFacadeOpenings(plan);plan.roof=B.buildRoof(spec,storeyList.at(-1).polygon);plan.diagnostics.roof=plan.roof.diagnostics;plan.diagnostics.fallbacks.push(...plan.roof.diagnostics.fallbacks);plan.structure=B.buildStructure(plan);plan.features=B.buildFeatures(spec,footprint.polygon,plan.roof);plan.navigation=B.buildNavigation(plan);plan.furniture=B.buildFurniture(spec,plan.rooms,plan.navigation);plan.navigation=B.buildNavigation(plan);return validatePlan(plan);
};
})();
