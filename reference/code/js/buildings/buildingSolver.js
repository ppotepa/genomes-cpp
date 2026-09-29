(function(){
'use strict';
const B=globalThis.RTS.Buildings,P=B.Polygon,EPS=1e-5;
class BuildingGenerationError extends Error{
  constructor(code,stage,spec,candidateId,failures,partialPlan){super(`${stage}: ${code}`);this.name='BuildingGenerationError';this.code=code;this.stage=stage;this.seed=spec?.seed;this.presetId=spec?.presetId;this.candidateId=candidateId;this.failures=failures||[];this.partialPlan=JSON.parse(JSON.stringify(partialPlan||null));}
  toJSON(){return {name:this.name,message:this.message,code:this.code,stage:this.stage,seed:this.seed,presetId:this.presetId,candidateId:this.candidateId,failures:this.failures,partialPlan:this.partialPlan};}
}
const area=parts=>parts.reduce((sum,p)=>sum+P.area(p),0),backend=parts=>parts.flatMap(p=>P.backendFormat(p));
function clean(poly){
  const ring=r=>{const reduced=r.filter((b,i)=>{const a=r[(i+r.length-1)%r.length],c=r[(i+1)%r.length];return Math.abs((b[0]-a[0])*(c[1]-b[1])-(b[1]-a[1])*(c[0]-b[0]))>1e-8;});return reduced.length>=3?reduced:r;};
  return P.canonical({outer:ring(poly.outer),holes:poly.holes.map(ring)});
}
const operation=(name,a,b)=>P.fromBackend(B.PolygonBackend[name](backend(a),backend(b))).map(clean),intersect=(a,b)=>operation('intersection',a,b);
function shared(a,b){
  let total=0,max=0;
  for(const ar of [a.outer,...a.holes])for(const br of [b.outer,...b.holes])for(let i=0;i<ar.length;i++)for(let j=0;j<br.length;j++){
    const u=ar[i],v=ar[(i+1)%ar.length],s=br[j],t=br[(j+1)%br.length],dx=v[0]-u[0],dz=v[1]-u[1],length=Math.hypot(dx,dz);
    if(length<EPS||Math.abs(dx*(s[1]-u[1])-dz*(s[0]-u[0]))>EPS*length||Math.abs(dx*(t[1]-u[1])-dz*(t[0]-u[0]))>EPS*length)continue;
    const q=((s[0]-u[0])*dx+(s[1]-u[1])*dz)/length,r=((t[0]-u[0])*dx+(t[1]-u[1])*dz)/length;
    const overlap=Math.max(0,Math.min(length,Math.max(q,r))-Math.max(0,Math.min(q,r)));total+=overlap;max=Math.max(max,overlap);
  }
  return {total,max};
}
function stairBounds(spec){
  const width=1,landing=1,gap=spec.stairFamily==='OPEN_WELL'?.8:.16,pad=width/2+.04;
  let risers=Math.ceil(spec.storeys.floorHeight/.175);if(spec.stairFamily!=='STRAIGHT'&&risers%2)risers++;
  const run=(risers-1)*.28,halfRun=(risers/2-1)*.28;
  return spec.stairFamily==='STRAIGHT'?[-run/2-pad,run/2+pad,-pad,pad]:spec.stairFamily==='QUARTER_TURN'?[-halfRun-width/2-pad,pad,-halfRun/2-width/2-pad,halfRun/2+pad]:[-halfRun/2-pad,halfRun/2+landing,-width-gap/2-.04,width+gap/2+.04];
}
function coreCandidate(spec,poly){
  if(spec.storeys.count<2)return null;
  if(!poly)throw new BuildingGenerationError('NO_CORE_SPACE','stairs',spec,null,[{code:'NO_USABLE_POLYGON'}]);
  const width=1,landing=1,local=stairBounds(spec);
  const [x0,x1,z0,z1]=P.bounds(poly),candidates=[];
  for(const yaw of [0,Math.PI/2])for(let iz=0;iz<=10;iz++)for(let ix=0;ix<=10;ix++){
    const rotated=yaw?[-local[3],-local[2],local[0],local[1]]:local;
    if(rotated[1]-rotated[0]>x1-x0+EPS||rotated[3]-rotated[2]>z1-z0+EPS)continue;
    const ox=x0-rotated[0]+(x1-x0-rotated[1]+rotated[0])*ix/10,oz=z0-rotated[2]+(z1-z0-rotated[3]+rotated[2])*iz/10;
    const polygon=P.rectangle(rotated[0]+ox,rotated[1]+ox,rotated[2]+oz,rotated[3]+oz);
    if(Math.abs(area(intersect([polygon],[poly]))-P.area(polygon))>EPS)continue;
    const distance=Math.hypot(ox-(x0+x1)/2,oz-(z0+z1)/2),boundary=shared(polygon,poly).total;
    candidates.push({center:[ox,oz],yaw,polygon,score:distance*.2-boundary*.3});
  }
  candidates.sort((a,b)=>a.score-b.score||a.center[0]-b.center[0]||a.center[1]-b.center[1]);
  if(!candidates.length)throw new BuildingGenerationError('NO_CORE_SPACE','stairs',spec,null,[{code:'STAIR_ENVELOPE',requiredWidth:local[1]-local[0],requiredDepth:local[3]-local[2]}]);
  return {id:'vertical-core',family:spec.stairFamily,width,yaw:0,...candidates[0],localBounds:local,placementCandidates:candidates.length,landingDepth:landing};
}
function bounds(parts){const boxes=parts.map(P.bounds);return [Math.min(...boxes.map(b=>b[0])),Math.max(...boxes.map(b=>b[1])),Math.min(...boxes.map(b=>b[2])),Math.max(...boxes.map(b=>b[3]))];}
function split(parts,axis,target){
  const [x0,x1,z0,z1]=bounds(parts),boxAt=cut=>axis?P.rectangle(x0-1,x1+1,z0-1,cut):P.rectangle(x0-1,cut,z0-1,z1+1);
  // Clipped area is quadratic between consecutive vertex coordinates (linear
  // for orthogonal polygons). Locate that interval, then solve its polynomial.
  const knots=[...new Set(parts.flatMap(p=>[p.outer,...p.holes].flatMap(r=>r.map(v=>v[axis]))))].sort((a,b)=>a-b);
  let lo=0,hi=knots.length-1,lowArea=0,highArea=area(parts);
  while(hi-lo>1){const mid=(lo+hi)>>1,a=area(intersect(parts,[boxAt(knots[mid])]));if(a<target){lo=mid;lowArea=a;}else{hi=mid;highArea=a;}}
  const middleArea=area(intersect(parts,[boxAt((knots[lo]+knots[hi])/2)])),q=2*(highArea+lowArea-2*middleArea),linear=highArea-lowArea-q,delta=target-lowArea;
  const t=Math.abs(q)<1e-7?delta/(highArea-lowArea):2*delta/(linear+Math.sqrt(Math.max(0,linear*linear+4*q*delta)));
  const cut=knots[lo]+Math.max(0,Math.min(1,t))*(knots[hi]-knots[lo]),box=boxAt(cut);
  return {cut,left:intersect(parts,[box]),right:operation('difference',parts,[box])};
}
// Keep every intermediate polygon part; a disconnected leaf rejects the
// candidate rather than replacing geometry or discarding its smaller parts.
function subdivide(parts,items,rng){
  if(items.length===1){if(parts.length!==1)return null;return {cells:[{index:items[0].index,polygon:parts[0]}],tree:{roomIndex:items[0].index,area:area(parts)}};}
  const [x0,x1,z0,z1]=bounds(parts),axis=rng.next()<.3?(x1-x0>z1-z0?1:0):(x1-x0>z1-z0?0:1),count=1+Math.floor(rng.next()*(items.length-1)),a=items.slice(0,count),b=items.slice(count),cut=split(parts,axis,a.reduce((s,i)=>s+i.target,0));
  if(!cut.left.length||!cut.right.length)return null;
  const l=subdivide(cut.left,a,rng),r=subdivide(cut.right,b,rng);if(!l||!r)return null;
  return {cells:l.cells.concat(r.cells),tree:{axis:axis?'z':'x',cut:cut.cut,children:[l.tree,r.tree]}};
}
const privateRole=role=>/BEDROOM|BATHROOM|WC|APARTMENT_UNIT/.test(role),wetRole=role=>/BATHROOM|WC|KITCHEN/.test(role);
function evaluate(rooms,program,poly,graph,core,previous){
  const failures=[],passage=B.AGENT_PROFILE.shoulderWidth+2*B.AGENT_PROFILE.passageMargin+.3,adj=rooms.map(()=>[]),exposures=rooms.map(r=>shared(r.polygon,poly).max);
  for(let i=0;i<rooms.length;i++){
    const r=rooms[i],d=program.rooms[i];
    if(!P.validate(r.polygon,{minArea:1,minEdge:.2}).valid)failures.push({code:'ROOM_GEOMETRY',roomId:r.id});
    const box=P.bounds(r.polygon),clearWidth=B.AGENT_PROFILE.shoulderWidth+2*B.AGENT_PROFILE.passageMargin;
    if(Math.min(box[1]-box[0],box[3]-box[2])+EPS<clearWidth)failures.push({code:'ROOM_CLEAR_WIDTH',roomId:r.id,required:clearWidth});
    if(d&&r.area+EPS<d.minArea)failures.push({code:'MIN_AREA',roomId:r.id,required:d.minArea,actual:r.area});
    if(d?.exterior&&exposures[i]<.66)failures.push({code:'EXTERIOR_EXPOSURE',roomId:r.id});
    for(let j=0;j<i;j++)if(shared(r.polygon,rooms[j].polygon).max+EPS>=passage){adj[i].push(j);adj[j].push(i);}
  }
  const distances=start=>{const d=rooms.map(()=>Infinity);d[start]=0;const q=[start];for(const i of q)for(const j of adj[i])if(!Number.isFinite(d[j])){d[j]=d[i]+1;q.push(j);}return d;};
  const paths=rooms.map((_,i)=>distances(i)),entrances=exposures.map((e,i)=>e>=passage?i:-1).filter(i=>i>=0);
  if(!entrances.length)failures.push({code:'NO_ENTRANCE'});
  if(paths[0].some(d=>!Number.isFinite(d)))failures.push({code:'DISCONNECTED_ROOMS'});
  let adjacency=0,privacy=0;
  for(const e of graph.edges||[]){
    if(e.type==='VERTICAL_CONNECT')continue;
    const i=rooms.findIndex(r=>r.id===e.from),j=rooms.findIndex(r=>r.id===e.to);
    if(e.from==='outside'||e.to==='outside'){
      const k=i<0?j:i;if(k<0||(e.type==='REQUIRED_TRAVERSABLE'?exposures[k]<passage:!entrances.some(n=>Number.isFinite(paths[n][k]))))failures.push({code:'REQUIRED_ENTRANCE',edge:e});continue;
    }
    if(i<0||j<0||(e.type==='REQUIRED_TRAVERSABLE'?!adj[i].includes(j):!Number.isFinite(paths[i][j])))failures.push({code:'REQUIRED_ADJACENCY',edge:e});
    else adjacency+=Math.max(0,paths[i][j]-1);
  }
  // Removing a private room must ideally leave every public room reachable.
  for(let k=0;k<rooms.length;k++)if(privateRole(rooms[k].role)){
    const start=rooms.findIndex((r,i)=>i!==k&&!privateRole(r.role));if(start<0)continue;
    const seen=new Set([start]),q=[start];for(const i of q)for(const j of adj[i])if(j!==k&&!seen.has(j)){seen.add(j);q.push(j);}
    privacy+=rooms.filter((r,i)=>i!==k&&!privateRole(r.role)&&!seen.has(i)).length;
  }
  let areaDeviation=0,aspect=0,exposure=0,wetStacking=0;
  rooms.forEach((r,i)=>{const [x0,x1,z0,z1]=P.bounds(r.polygon);aspect+=Math.max(x1-x0,z1-z0)/Math.max(.01,Math.min(x1-x0,z1-z0))-1;const d=program.rooms[i];if(d){areaDeviation+=Math.abs(r.area-d.targetArea)/d.targetArea;if(d.exterior)exposure+=1/(1+exposures[i]);}if(previous&&wetRole(r.role)){const below=previous.find(q=>q.role===r.role);wetStacking+=below?1-area(intersect([r.polygon],[below.polygon]))/Math.max(r.area,below.area):1;}});
  const floorArea=P.area(poly),corridorRatio=rooms.filter(r=>/CORRIDOR|CORE/.test(r.role)).reduce((s,r)=>s+r.area,0)/floorArea,congestion=adj.reduce((s,a)=>s+Math.max(0,a.length-3),0),stairArea=core?P.area(core.polygon)/floorArea:0,partitionComplexity=rooms.reduce((s,r)=>s+r.polygon.outer.length+r.polygon.holes.reduce((n,h)=>n+h.length,0),0);
  const covered=area(rooms.map(r=>r.polygon));if(Math.abs(covered-floorArea)>EPS*rooms.length)failures.push({code:'COVERAGE',actual:covered,required:floorArea});
  const score={areaDeviation,adjacency,corridorRatio,aspect,exposure,privacy,wetStacking,congestion,stairArea,partitionComplexity};
  score.total=areaDeviation+adjacency*.6+corridorRatio*2+aspect*.15+exposure+privacy*3+wetStacking*2+congestion*.2+stairArea+partitionComplexity*.01;
  return {hardFailures:failures,score,adjacency:adj};
}
B.solveFloors=function(spec,footprint,storeyPolygons,roomGraphs,core){
  const diagnostics={candidates:[],selectedCandidate:[],capacityRules:[],core:core?{center:core.center,yaw:core.yaw,placementCandidates:core.placementCandidates,score:core.score}:null,limits:{floorCandidates:96,localMoves:0}},byFloor=[],allRooms=[];
  if(!storeyPolygons.length||storeyPolygons.length!==spec.storeys.count||roomGraphs.length!==storeyPolygons.length)throw new BuildingGenerationError('INVALID_FLOOR_INPUT','floor-solver',spec,null,[{code:'FLOOR_COUNT'}],{diagnostics});
  if(spec.storeys.count>1&&!core)throw new BuildingGenerationError('STAIR_CORE_REQUIRED','floor-solver',spec,null,[{code:'MISSING_CORE'}],{diagnostics});
  for(let floor=0;floor<storeyPolygons.length;floor++){
    const poly=storeyPolygons[floor],graph=roomGraphs[floor];
    if(core&&Math.abs(area(intersect([core.polygon],[poly]))-P.area(core.polygon))>EPS)throw new BuildingGenerationError('CORE_OUTSIDE_STOREY','floor-solver',spec,null,[{floor,code:'STAIR_CONTAINMENT'}],{diagnostics});
    const parts=core?operation('difference',[poly],[core.polygon]):[poly],available=area(parts),program=B.resolveRoomProgram(spec,available);
    diagnostics.capacityRules.push({floor,...program.capacityRule});
    if(available+EPS<program.capacityRule.requiredArea)throw new BuildingGenerationError('PROGRAM_CAPACITY_EXCEEDED','floor-solver',spec,null,[{floor,...program.capacityRule}],{diagnostics});
    let best=null;
    for(let candidateId=0;candidateId<diagnostics.limits.floorCandidates;candidateId++){
      const rng=new B.RNG(B.deriveSeed(spec.seeds.program,`floor-${floor}/candidate-${candidateId}`)),weights=program.rooms.map(r=>r.weight*(.75+rng.next()*.5)),sum=weights.reduce((s,w)=>s+w,0),extra=Math.max(0,available-program.capacityRule.requiredArea);
      const nominalWeight=program.rooms.reduce((s,r)=>s+r.weight,0),descriptors=program.rooms.map(r=>({...r,targetArea:r.minArea+extra*r.weight/nominalWeight})),items=descriptors.map((d,index)=>({index,target:d.minArea+extra*weights[index]/sum}));
      for(let i=items.length-1;i>0;i--){const j=Math.floor(rng.next()*(i+1));[items[i],items[j]]=[items[j],items[i]];}
      const partition=subdivide(parts,items,rng);
      if(!partition){diagnostics.candidates.push({floor,candidateId,hardFailures:[{code:'DISCONNECTED_SUBDIVISION'}],score:null,localMoves:0});continue;}
      const rooms=partition.cells.sort((a,b)=>a.index-b.index).map(({index,polygon})=>({id:graph.nodes[index+1]?.id||`f${floor}/room-${index}`,floor,role:descriptors[index].role,polygon,area:P.area(polygon),minArea:descriptors[index].minArea,targetArea:descriptors[index].targetArea,exteriorExposure:shared(polygon,poly).max>=.66}));
      if(core)rooms.push({id:`f${floor}/vertical-core`,floor,role:'VERTICAL_CORE',polygon:core.polygon,area:P.area(core.polygon),exteriorExposure:shared(core.polygon,poly).max>=.66,verticalCore:core});
      const check=evaluate(rooms,{rooms:descriptors},poly,graph,core,byFloor[floor-1]?.rooms),entry={floor,candidateId,hardFailures:check.hardFailures,score:check.score,localMoves:0};diagnostics.candidates.push(entry);
      if(!check.hardFailures.length&&(!best||check.score.total<best.score.total))best={candidateId,rooms,score:check.score,subdivisionTree:partition.tree,adjacency:check.adjacency};
    }
    if(!best)throw new BuildingGenerationError('NO_FEASIBLE_FLOOR_PLAN','floor-solver',spec,null,diagnostics.candidates.filter(c=>c.floor===floor).map(c=>({candidateId:c.candidateId,failures:c.hardFailures})),{diagnostics});
    for(let i=0;i<program.rooms.length;i++){const node=graph.nodes.find(n=>n.id===best.rooms[i].id);if(node){node.nominalMinArea=program.rooms[i].nominalMinArea;node.minArea=program.rooms[i].minArea;}}
    if(core&&!graph.nodes.some(n=>n.id===`f${floor}/vertical-core`))graph.nodes.push({id:`f${floor}/vertical-core`,role:'VERTICAL_CORE',minArea:P.area(core.polygon),exterior:false});
    best.rooms.forEach((r,i)=>{r.neighbors=best.adjacency[i].map(j=>best.rooms[j].id);});byFloor.push(best);allRooms.push(...best.rooms);diagnostics.selectedCandidate.push(best.candidateId);
  }
  return {rooms:allRooms,byFloor,diagnostics,unitRegions:B.ROOM_PROGRAMS[spec.roomProgram].twoLevel?allRooms.filter(r=>r.role==='APARTMENT_UNIT').map(r=>({id:r.id+'/unit',polygon:r.polygon,roomGraph:B.createRoomGraph('APARTMENT_UNIT',r.floor).toJSON()})):[]};
};
B.stairCoreBounds=stairBounds;B.createVerticalCore=coreCandidate;B.BuildingGenerationError=BuildingGenerationError;
})();
