const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const root=path.resolve(__dirname,'..');
global.window=global;
for(const name of ['vendor/buildings-6.0.0','buildings/buildingCatalog','buildings/buildingRandom','buildings/buildingBackends','buildings/buildingPolygon','buildings/buildingSpec','buildings/buildingFootprints','buildings/buildingPrograms','buildings/buildingSolver','buildings/buildingAssemblies','buildings/buildingPlan'])vm.runInThisContext(fs.readFileSync(path.join(root,'js',name+'.js'),'utf8'),{filename:name});
const B=RTS.Buildings;
const area=f=>Math.abs((f.points[1][0]-f.points[0][0])*(f.points[2][2]-f.points[0][2])-(f.points[1][2]-f.points[0][2])*(f.points[2][0]-f.points[0][0]))/2;
const spec=family=>({width:12,depth:8,roofFamily:family,roofCovering:'tile',material:'brick',storeys:{count:2,floorHeight:3},features:{},stairFamily:'SWITCHBACK'});
(async()=>{
  await B.initializeBuildingBackends();
  const shapes=[B.Polygon.rectangle(-6,6,-4,4),B.Polygon.canonical({outer:[[0,0],[12,0],[12,4],[5,4],[5,10],[0,10]],holes:[]}),B.Polygon.canonical({outer:[[0,0],[12,0],[12,12],[0,12]],holes:[[[4,4],[4,8],[8,8],[8,4]]]}),B.Polygon.canonical({outer:[[0,0],[10,1],[11,6],[6,9],[-1,6]],holes:[]})];
  const families=['GABLE','HIP','HALF_HIP','MANSARD','COMPOUND_GABLE','SHED','FLAT'];
  for(const poly of shapes)for(const family of families){
    const input=spec(family),roof=B.buildRoof(input,poly);
    assert.equal(roof.diagnostics.backend,'straight-skeleton',family);
    assert(roof.skeleton.vertices.some(v=>v[2]>0));
    assert.deepEqual(roof,B.buildRoof(input,poly),'deterministic '+family);
    assert(Math.abs(roof.faces.reduce((n,f)=>n+area(f),0)-B.Polygon.area(poly))<1e-4,'coverage '+family);
    for(const f of roof.faces){const [a,b,c]=f.points;assert((b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2])>1e-9,'upward '+family);assert(f.points.flat().every(Number.isFinite));assert.equal(f.material,'tile');const outside=B.Polygon.fromBackend(B.PolygonBackend.difference(B.Polygon.backendFormat({outer:f.points.map(p=>[p[0],p[2]]),holes:[]}),B.Polygon.backendFormat(poly)));assert(outside.reduce((n,p)=>n+B.Polygon.area(p),0)<1e-5,'no faces across holes or outside footprint '+family);}
    if(family==='HIP')for(const p of roof.skeleton.vertices.filter(p=>p[2]>0))assert(roof.faces.some(f=>f.points.some(v=>Math.hypot(v[0]-p[0],v[2]-p[1])<1e-5&&v[1]>roof.base)),'actual skeleton ridge vertex');
    const heights=roof.faces.flatMap(f=>f.points.map(p=>p[1]));assert.equal(Math.min(...heights),6);if(family==='FLAT')assert.equal(Math.max(...heights),6);else assert(Math.max(...heights)>6.1,'rise '+family);
    const restored=JSON.parse(JSON.stringify(roof)),p=roof.faces[0].points.reduce((s,v)=>[s[0]+v[0]/3,s[1]+v[2]/3],[0,0]);assert.equal(B.roofHeightAt(restored,...p),B.roofHeightAt(roof,...p));
    if(poly.holes.length)assert.equal(B.roofHeightAt(roof,6,6),null,'courtyard');
  }
  const rectangle=shapes[0],hip=B.buildRoof(spec('HIP'),rectangle),gable=B.buildRoof(spec('GABLE'),rectangle),half=B.buildRoof(spec('HALF_HIP'),rectangle);
  assert(hip.ridges.length&&hip.hips.length);assert(gable.gables.length);assert(half.gables.length&&half.hips.length);assert.notDeepEqual(gable.faces,half.faces);
  assert(B.roofHeightAt(gable,6,0)>B.roofHeightAt(hip,6,0));
  const square=B.Polygon.rectangle(-4,4,-4,4),squareGable=B.buildRoof(spec('GABLE'),square),squareHip=B.buildRoof(spec('HIP'),square),squareHalf=B.buildRoof(spec('HALF_HIP'),square);
  assert(Math.abs(B.roofHeightAt(squareGable,-4,0)-B.roofHeightAt(squareGable,4,0))<1e-6,'symmetric square gable ends');assert(B.roofHeightAt(squareGable,4,0)>7);assert.equal(B.roofHeightAt(squareHip,4,0),6);assert(B.roofHeightAt(squareHalf,4,0)>6&&B.roofHeightAt(squareHalf,4,0)<B.roofHeightAt(squareGable,4,0));
  const featureSpec=spec('HIP');featureSpec.features={chimney:true,dormer:true,porch:true,balcony:true};const roof=B.buildRoof(featureSpec,rectangle),before=roof.faces.reduce((n,f)=>n+area(f),0),features=B.buildFeatures(featureSpec,rectangle,roof);
  assert.equal(features.length,4);assert.equal(roof.openings.length,2);
  const removed=roof.openings.reduce((n,o)=>n+B.Polygon.area(o.polygon),0);assert(Math.abs(before-roof.faces.reduce((n,f)=>n+area(f),0)-removed)<1e-4,'real cutouts');
  for(const opening of roof.openings){const b=B.Polygon.bounds(opening.polygon);assert.equal(B.roofHeightAt(roof,(b[0]+b[1])/2,(b[2]+b[3])/2),null);}
  assert(features.every(f=>f.solids.length||f.faces.length));
  const dormer=features.find(f=>f.type==='DORMER');assert(dormer.solids.some(s=>s.material==='glass'));const frontArea=dormer.faces.filter(f=>f.id.includes('/front-')).reduce((sum,f)=>{const [a,b,c]=f.points;return sum+Math.abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/2;},0);assert(frontArea>0);const w=dormer.window;for(const f of dormer.faces.filter(f=>f.id.includes('/front-'))){const points=f.points.map(p=>[p[0],p[1]]),overlap=B.Polygon.fromBackend(B.PolygonBackend.intersection(B.Polygon.backendFormat({outer:points,holes:[]}),B.Polygon.backendFormat(B.Polygon.rectangle(w.center[0]-w.width/2,w.center[0]+w.width/2,w.bottom,w.top))));assert(overlap.reduce((n,p)=>n+B.Polygon.area(p),0)<1e-7,'real dormer window hole');}
  const incompatible=spec('FLAT');incompatible.storeys.count=1;incompatible.features={balcony:true,dormer:true};const flat=B.buildRoof(incompatible,rectangle);assert.equal(B.buildFeatures(incompatible,rectangle,flat).length,0);assert.equal(flat.diagnostics.features.length,2);assert(flat.diagnostics.features.every(d=>d.code==='FEATURES_UNPLACEABLE'));
  const backend=RTSBuildingVendors.straightSkeleton.SkeletonBuilder,build=backend.buildFromPolygon;backend.buildFromPolygon=()=>null;
  try{const fallback=B.buildRoof(spec('HIP'),rectangle);assert.equal(fallback.skeleton,null);assert.equal(fallback.diagnostics.fallbacks[0].code,'SKELETON_BACKEND_FAILED');assert(Math.abs(fallback.faces.reduce((n,f)=>n+area(f),0)-96)<1e-5);assert.deepEqual(fallback,B.buildRoof(spec('HIP'),rectangle));}finally{backend.buildFromPolygon=build;}
  for(const family of ['STRAIGHT','QUARTER_TURN','SWITCHBACK','OPEN_WELL']){const input=spec('HIP');input.stairFamily=family;input.storeys.count=3;const stairs=B.buildStairs(input,{width:1,center:[10,15],yaw:.3,polygon:B.Polygon.rectangle(4,16,9,21)});for(const stair of stairs){assert(stair.validation.dimensions);assert.equal(stair.validation.headroom,true);assert(stair.validation.headroomMeasured===null||stair.validation.headroomMeasured>2);for(const p of stair.centerline)assert(stair.slabVoid.polygons.some(poly=>B.Polygon.containsPoint(poly,[p[0],p[2]])),'full flight void '+family);}}
  const low=spec('HIP');low.storeys={count:3,floorHeight:1.5};assert.equal(B.buildStairs(low,{width:1,center:[0,0]})[0].validation.headroom,false,'measured insufficient headroom');
  const poly=B.Polygon.rectangle(0,8,0,6),rooms=[{id:'right',floor:0,role:'BEDROOM',polygon:B.Polygon.rectangle(4,8,0,6)},{id:'left-bottom',floor:0,role:'LIVING',polygon:B.Polygon.rectangle(0,4,0,3)},{id:'left-top',floor:0,role:'KITCHEN',polygon:B.Polygon.rectangle(0,4,3,6)}];
  const wallSpec={...spec('HIP'),structuralSystem:'MASONRY',storeys:{count:1,floorHeight:3}},assembly=B.buildWallsAndPortals(wallSpec,[poly],rooms,[]);
  assert.equal(assembly.walls.filter(w=>w.internal).length,3,'T junction shared edges');
  const entrance=assembly.openings.find(o=>o.rooms.includes('outside')),entranceWall=assembly.walls.find(w=>w.id===entrance.wallId),point=[entranceWall.edge.a[0]+entranceWall.tangent[0]*entrance.center+entranceWall.normal[0]*.2,entranceWall.edge.a[1]+entranceWall.tangent[1]*entrance.center+entranceWall.normal[1]*.2];
  assert(B.Polygon.containsPoint(rooms.find(r=>r.id===entrance.rooms[1]).polygon,point),'entrance actual room');
  const plan={spec:wallSpec,storeys:[{floor:0,polygon:poly}],...assembly,rooms,slabs:[{id:'slab-0',floor:0,y:0,thickness:.18,polygon:poly}],stairs:[],features:[],furniture:[{id:'table',floor:0,polygon:B.Polygon.rectangle(1,2,1,2)}],roof:B.buildRoof(wallSpec,poly)};
  const nav=B.buildNavigation(plan),areaOpen=nav.surfaces.reduce((n,s)=>n+B.Polygon.area(s.polygon),0);assert(nav.blockers.some(b=>b.kind==='furniture'));
  for(const wall of plan.walls)for(const opening of wall.openings)opening.state='CLOSED';
  const closed=B.buildNavigation(plan);assert(closed.surfaces.reduce((n,s)=>n+B.Polygon.area(s.polygon),0)<areaOpen,'closed doors block floor mesh');assert(closed.portals.every(p=>p.state==='CLOSED'));
  const structure=B.buildStructure(plan);assert(structure.beams.every(b=>b.supports.length===2&&b.supports.every(id=>structure.columns.some(c=>c.id===id))),'real beam support IDs');
  const furniture=B.buildFurniture({...wallSpec,furniture:true},rooms,nav);assert(furniture.length);for(const item of furniture){const room=rooms.find(r=>r.id===item.roomId);assert.equal(B.Polygon.fromBackend(B.PolygonBackend.difference(B.Polygon.backendFormat(item.polygon),B.Polygon.backendFormat(room.polygon))).length,0,'furniture containment');}
  for(const stairFamily of ['STRAIGHT','QUARTER_TURN','DOGLEG','OPEN_WELL']){
    const p=B.createPlan({presetId:stairFamily==='OPEN_WELL'?'MULTI_FAMILY':'FAMILY_HOUSE',seed:42,width:16,depth:14,storeyCount:2,footprintFamily:'RECT',roofFamily:stairFamily==='OPEN_WELL'?'HIP':'GABLE',stairFamily}),nav=RTSBuildingVendors.navcat;
    for(const s of p.stairs){const route=nav.findPath(p.navigation.navmesh.data,s.centerline[0].slice(),s.centerline.at(-1).slice(),[.5,.5,.5],nav.DEFAULT_QUERY_FILTER);assert.equal(route.flags,3,'complete stair path '+stairFamily+' '+JSON.stringify(route.path));}
  }
  console.log('PASS roof quality: 28 family/polygon cases, topology, holes, features, fallback, stairs, complete nav paths, walls, supports, furniture');
})().catch(error=>{console.error(error);process.exitCode=1;});
