const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),root=path.resolve(__dirname,'..');
// Reuse the authoritative v6 loader, without running its async smoke suite.
const loader=fs.readFileSync(path.join(__dirname,'building_v6.cjs'),'utf8').split('(async()=>')[0];
new Function('require','__dirname',loader)(require,__dirname);
(async()=>{
  const B=RTS.Buildings;await B.initializeBuildingBackends();
  const solve=options=>{const spec=B.createBuildingSpec(options),fp=B.generateFootprint(spec),core=B.createVerticalCore(spec,fp.usable),graphs=Array.from({length:spec.storeys.count},(_,i)=>B.createRoomGraph(spec.roomProgram,i));return {spec,fp,core,graphs,result:B.solveFloors(spec,fp.polygon,Array(spec.storeys.count).fill(fp.polygon),graphs,core)};};
  const polygonArea=multi=>B.Polygon.fromBackend(multi).reduce((s,p)=>s+B.Polygon.area(p),0);
  if(process.argv.includes('--defaults-250')||process.argv.includes('--explicit-250')){
    const explicit=process.argv.includes('--explicit-250'),failures=[],expectedRejections=[],presets=Object.keys(B.BUILDING_PRESETS),start=Date.now();
    for(let seed=0;seed<250;seed++){const presetId=presets[seed%presets.length],p=B.BUILDING_PRESETS[presetId],options={presetId,seed};if(explicit)Object.assign(options,{footprintFamily:p.footprints[seed%p.footprints.length],roofFamily:p.roofs[seed%p.roofs.length],storeyCount:p.storeys[0]+seed%(p.storeys[1]-p.storeys[0]+1),width:p.width[0]+(seed%17)/16*(p.width[1]-p.width[0]),depth:p.depth[0]+(seed%19)/18*(p.depth[1]-p.depth[0])});try{solve(options);}catch(e){const record={seed,presetId,code:e.code,failures:e.failures?.slice(0,1)};if(explicit&&e instanceof B.BuildingGenerationError&&['PROGRAM_CAPACITY_EXCEEDED','NO_CORE_SPACE'].includes(e.code))expectedRejections.push(record);else failures.push(record);}}
    console.log(JSON.stringify({cases:250,milliseconds:Date.now()-start,expectedRejections,failures},null,2));assert.equal(failures.length,0);return;
  }
  let count=0;
  for(const presetId of Object.keys(B.BUILDING_PRESETS))for(const seed of [0,1,42,0xffffffff]){
    const spec=B.createBuildingSpec({presetId,seed}),fp=B.generateFootprint(spec);
    let core,solved;
    try{core=B.createVerticalCore(spec,fp.usable);solved=B.solveFloors(spec,fp.polygon,Array(spec.storeys.count).fill(fp.polygon),Array.from({length:spec.storeys.count},(_,i)=>B.createRoomGraph(spec.roomProgram,i)),core);}
    catch(e){console.error('FAILED',presetId,seed,JSON.stringify(spec),e.code,JSON.stringify(e.failures).slice(0,1500));throw e;}
    for(const floor of solved.byFloor){
      assert(!floor.relaxed);assert(floor.subdivisionTree);assert(floor.rooms.every(r=>B.Polygon.validate(r.polygon).valid));
      assert(Math.abs(floor.rooms.reduce((s,r)=>s+r.area,0)-B.Polygon.area(fp.polygon))<1e-4);
      for(let i=0;i<floor.rooms.length;i++)for(let j=0;j<i;j++){
        const overlap=B.Polygon.fromBackend(B.PolygonBackend.intersection(B.Polygon.backendFormat(floor.rooms[i].polygon),B.Polygon.backendFormat(floor.rooms[j].polygon)));
        assert(overlap.reduce((s,p)=>s+B.Polygon.area(p),0)<1e-5);
      }
      assert(floor.rooms.every(r=>r.role==='VERTICAL_CORE'||r.area+1e-5>=r.minArea));
    }
    count++;
  }
  const options={presetId:'FAMILY_HOUSE',seed:42,footprintFamily:'RECT',width:14,depth:13,storeyCount:1},a=solve(options),b=solve(options);
  assert.deepEqual(a.result,b.result,'identical seed reproduces complete solver diagnostics');
  assert.notDeepEqual(a.result.rooms,solve({...options,seed:43}).result.rooms);
  const candidates=a.result.diagnostics.candidates.filter(c=>!c.hardFailures.length);
  assert(candidates.length>2);assert(new Set(candidates.map(c=>c.score.total.toFixed(5))).size>2,'search evaluates genuinely distinct candidates');
  const selected=a.result.byFloor[0];assert.equal(selected.score.total,Math.min(...candidates.map(c=>c.score.total)));
  assert(selected.score.areaDeviation>0);assert(candidates.some(c=>c.score.adjacency>0));assert(candidates.some(c=>c.score.privacy>0));
  const axes=new Set();const visit=node=>{if(node.axis)axes.add(node.axis);for(const child of node.children||[])visit(child);};visit(selected.subdivisionTree);assert.equal(axes.size,2,'subdivision is not repeated parallel strips');
  assert.throws(()=>solve({...options,width:3,depth:3}),e=>e instanceof B.BuildingGenerationError&&e.code==='PROGRAM_CAPACITY_EXCEEDED'&&e.toJSON().failures.length>0);
  const shed=solve({presetId:'RURAL_SHED',seed:0,width:2.5,depth:2.5,footprintFamily:'RECT'});assert.equal(shed.result.diagnostics.capacityRules[0].id,'SINGLE_PURPOSE_SHED_2M2');assert.equal(shed.result.rooms[0].minArea,2);
  const invalidGraphs=[B.createRoomGraph(a.spec.roomProgram,0)];invalidGraphs[0].connect('f0/missing','f0/room-0');
  assert.throws(()=>B.solveFloors(a.spec,a.fp.polygon,[a.fp.polygon],invalidGraphs,null),e=>e.code==='NO_FEASIBLE_FLOOR_PLAN'&&e.failures.some(c=>c.failures.some(f=>f.code==='REQUIRED_ADJACENCY')));
  const stack=solve({...options,storeyCount:2,stairFamily:'DOGLEG'});
  assert(stack.core.placementCandidates>1);assert(stack.result.byFloor[1].score.stairArea>0);assert(stack.result.byFloor[1].score.wetStacking>=0);
  const away=B.Polygon.rectangle(50,60,50,60);
  assert.throws(()=>B.solveFloors(stack.spec,stack.fp.polygon,[stack.fp.polygon,away],[B.createRoomGraph(stack.spec.roomProgram,0),B.createRoomGraph(stack.spec.roomProgram,1)],stack.core),e=>e.code==='CORE_OUTSIDE_STOREY');
  assert.throws(()=>B.solveFloors(stack.spec,stack.fp.polygon,[stack.fp.polygon,stack.fp.polygon],[B.createRoomGraph(stack.spec.roomProgram,0),B.createRoomGraph(stack.spec.roomProgram,1)],null),e=>e.code==='STAIR_CORE_REQUIRED');
  const rotatedSpec=B.createBuildingSpec({presetId:'BARN',seed:103,width:6,depth:14,storeyCount:2,floorHeight:5,stairFamily:'STRAIGHT'}),translated=B.Polygon.rectangle(20,26,30,44),rotatedCore=B.createVerticalCore(rotatedSpec,translated);
  assert.equal(rotatedCore.yaw,Math.PI/2);assert(rotatedCore.center[0]>20&&rotatedCore.center[1]>30);
  assert(polygonArea(B.PolygonBackend.difference(B.Polygon.backendFormat(rotatedCore.polygon),B.Polygon.backendFormat(translated)))<1e-6);
  for(const family of ['COMPOSITE','IRREGULAR_ORTHO']){
    const fingerprints=new Set();
    for(let seed=0;seed<8;seed++){
      const spec={...B.createBuildingSpec({presetId:'WORKSHOP_HALL',seed,footprintFamily:'COMPOSITE',width:24,depth:20,storeyCount:1}),footprintFamily:family},fp=B.generateFootprint(spec);assert.deepEqual(fp,B.generateFootprint(spec));
      const nodes=[];const walk=(node,depth=0)=>{assert(depth<=2);nodes.push(node);for(const child of node.children){assert(polygonArea(B.PolygonBackend.intersection(B.Polygon.backendFormat(node.polygon),B.Polygon.backendFormat(child.polygon)))>0);walk(child,depth+1);}};walk(fp.grammarTree);
      assert(nodes.length>=3&&nodes.length<=5);if(family==='IRREGULAR_ORTHO')assert(nodes.length>3);
      const union=B.PolygonBackend.union(...nodes.map(n=>B.Polygon.backendFormat(n.polygon)));
      assert(polygonArea(B.PolygonBackend.difference(union,B.Polygon.backendFormat(fp.polygon)))<1e-6);
      assert(polygonArea(B.PolygonBackend.difference(B.Polygon.backendFormat(fp.polygon),union))<1e-6);
      for(const point of fp.polygon.outer)assert(Math.abs(point[0])<=12+1e-6&&Math.abs(point[1])<=10+1e-6);
      fingerprints.add(JSON.stringify(fp.polygon));
    }
    assert(fingerprints.size>=6,'grammar must vary by seed');
  }
  // Courtyard cuts may yield multiple parts. No area or original hole is lost.
  const court=solve({presetId:'MULTI_FAMILY',seed:4,footprintFamily:'COURTYARD',storeyCount:1,width:24,depth:20});
  assert(Math.abs(court.result.rooms.reduce((s,r)=>s+r.area,0)-B.Polygon.area(court.fp.polygon))<1e-4);
  for(const room of court.result.rooms)assert(polygonArea(B.PolygonBackend.difference(B.Polygon.backendFormat(room.polygon),B.Polygon.backendFormat(court.fp.polygon)))<1e-5);
  const diagonal=solve({...options,footprint:{outer:[[-8,-8],[8,-8],[5,8],[-5,8]],holes:[]}});
  assert(Math.abs(diagonal.result.rooms.reduce((s,r)=>s+r.area,0)-208)<1e-4,'quadratic cut interpolation preserves non-orthogonal area');
  console.log('PASS building solver quality',count,'default plans; deterministic search, capacity errors, stairs, coverage, hierarchy');
})().catch(e=>{console.error(e.stack||e);process.exitCode=1;});
