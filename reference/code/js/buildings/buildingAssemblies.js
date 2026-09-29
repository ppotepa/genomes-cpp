(function(){
'use strict';
const B=globalThis.RTS.Buildings,round=n=>+n.toFixed(6);
const point3=(p,y)=>[p[0],y,p[1]],edgeId=(a,b)=>[a,b].sort((p,q)=>p[0]-q[0]||p[1]-q[1]).map(p=>p.join(',')).join('|');
function capsulePolygon(a,b,r,segments=12){const angle=Math.atan2(b[1]-a[1],b[0]-a[0]),points=[];for(let i=0;i<=segments;i++){const q=angle-Math.PI/2+Math.PI*i/segments;points.push([b[0]+Math.cos(q)*r,b[1]+Math.sin(q)*r]);}for(let i=0;i<=segments;i++){const q=angle+Math.PI/2+Math.PI*i/segments;points.push([a[0]+Math.cos(q)*r,a[1]+Math.sin(q)*r]);}return B.Polygon.canonical({outer:points,holes:[]});}
function flight(id,start,end,width,risers){const rise=(end[1]-start[1])/risers,run=Math.hypot(end[0]-start[0],end[2]-start[2]),treads=Math.max(1,risers-1);return {id,start,end,width,risers,treads,rise,tread:run/treads,run};}
B.buildStairs=function(spec,core){if(!core)return [];const stairs=[],height=spec.storeys.floorHeight;for(let floor=0;floor<spec.storeys.count-1;floor++){
  let risers=Math.ceil(height/.175);if(spec.stairFamily!=='STRAIGHT'&&risers%2)risers++;const y=floor*height,w=Math.max(.9,core.width),landing=Math.max(w,1),half=risers/2|0,run=(risers-1)*.28,halfRun=(half-1)*.28,flights=[],landings=[];
  if(spec.stairFamily==='STRAIGHT'){flights.push(flight(`stair-${floor}/flight-0`,[-run/2,y,0],[run/2,y+height,0],w,risers));}
  else if(spec.stairFamily==='QUARTER_TURN'){const mid=y+height*half/risers;flights.push(flight(`stair-${floor}/flight-0`,[-halfRun-w/2,y,halfRun/2],[ -w/2,mid,halfRun/2],w,half),flight(`stair-${floor}/flight-1`,[0,mid,halfRun/2-w/2],[0,y+height,-halfRun/2-w/2],w,risers-half));landings.push({id:`stair-${floor}/landing-0`,center:[0,mid,halfRun/2],width:landing,depth:landing,y:mid});}
  else {const gap=spec.stairFamily==='OPEN_WELL'?.8:.16,mid=y+height*half/risers,side=w/2+gap/2;flights.push(flight(`stair-${floor}/flight-0`,[-halfRun/2,y,-side],[halfRun/2,mid,-side],w,half),flight(`stair-${floor}/flight-1`,[halfRun/2,mid,side],[-halfRun/2,y+height,side],w,risers-half));landings.push({id:`stair-${floor}/landing-0`,center:[halfRun/2+landing/2,mid,0],width:landing,depth:w*2+gap,y:mid});}
  const cb=core.polygon?B.Polygon.bounds(core.polygon):[0,0,0,0],center=core.center||[(cb[0]+cb[1])/2,(cb[2]+cb[3])/2],yaw=core.yaw||0,cs=Math.cos(yaw),sn=Math.sin(yaw),world=p=>[round(center[0]+p[0]*cs-p[2]*sn),p[1],round(center[1]+p[0]*sn+p[2]*cs)];
  for(const f of flights){f.start=world(f.start);f.end=world(f.end);}for(const l of landings){l.center=world(l.center);l.yaw=-yaw;const local=[[-l.width/2,-l.depth/2],[l.width/2,-l.depth/2],[l.width/2,l.depth/2],[-l.width/2,l.depth/2]];l.polygon=B.Polygon.canonical({outer:local.map(p=>[l.center[0]+p[0]*cs-p[1]*sn,l.center[2]+p[0]*sn+p[1]*cs]),holes:[]});}
  const centerline=[],appendSegment=(a,b)=>{const steps=Math.max(1,Math.ceil(Math.hypot(...a.map((v,i)=>b[i]-v))/.1));for(let s=0;s<=steps;s++){const t=s/steps;centerline.push(a.map((v,i)=>round(v+(b[i]-v)*t)));}};
  for(const [i,f] of flights.entries()){if(i&&landings[0]){appendSegment(flights[i-1].end,landings[0].center);appendSegment(landings[0].center,f.start);}appendSegment(f.start,f.end);}
  // Sweep every flight and connecting landing; the first/last shortcut misses U turns.
  const sweep=flights.map(f=>capsulePolygon([f.start[0],f.start[2]],[f.end[0],f.end[2]],Math.max(w/2,B.AGENT_PROFILE.radius+B.AGENT_PROFILE.passageMargin)+.04));
  sweep.push(...landings.map(l=>l.polygon));const voidPolygons=B.Polygon.fromBackend(B.PolygonBackend.union(...sweep.map(B.Polygon.backendFormat))),voidPolygon=voidPolygons[0];
  // The top landing bridges the rounded clearance cutout back to the upper slab.
  // It is structural geometry, and must not itself enlarge that cutout.
  const lastFlight=flights.at(-1),ex=(lastFlight.end[0]-lastFlight.start[0])/lastFlight.run,ez=(lastFlight.end[2]-lastFlight.start[2])/lastFlight.run,reach=w/2+.18,end=lastFlight.end,exitCenter=[end[0]+ex*reach/2,end[1],end[2]+ez*reach/2],exitPolygon=B.Polygon.canonical({outer:[[end[0]-ez*w/2,end[2]+ex*w/2],[end[0]+ez*w/2,end[2]-ex*w/2],[end[0]+ex*reach+ez*w/2,end[2]+ez*reach-ex*w/2],[end[0]+ex*reach-ez*w/2,end[2]+ez*reach+ex*w/2]],holes:[]});
  landings.push({id:`stair-${floor}/exit-landing`,kind:'exit',center:exitCenter,width:reach,depth:w,y:end[1],yaw:-Math.atan2(ez,ex),polygon:exitPolygon});
  const stair={id:`stair-${floor}`,floor,type:spec.stairFamily,y,height,width:w,risers,rise:height/risers,tread:.28,landing,flights,landings,treads:flights.flatMap(f=>Array.from({length:f.treads},(_,i)=>({id:`${f.id}/tread-${i}`,flightId:f.id,index:i}))),centerline,walkline:centerline,slabVoid:{id:`stair-${floor}/void`,floor:floor+1,polygon:voidPolygon,polygons:voidPolygons},navigationRamp:{id:`stair-${floor}/ramp`,fromFloor:floor,toFloor:floor+1,width:w,centerline,flights},clearanceVolume:{polygon:voidPolygon,polygons:voidPolygons,bottom:y,top:y+height+B.AGENT_PROFILE.standingHeight},validation:{dimensions:height/risers<=.18&&flights.every(f=>f.tread>=.27),analyticCapsule:true,headroom:false,headroomMeasured:null,lateralClearance:w>=B.AGENT_PROFILE.shoulderWidth+2*B.AGENT_PROFILE.passageMargin,rapierFixture:'required-by-runtime'}};stairs.push(stair);
  }
  // Measure clearance to actual overhead flight undersides and landing slabs.
  for(const stair of stairs){let minimum=Infinity;for(const p of stair.centerline){for(const overhead of stairs){for(const f of overhead.flights){const dx=f.end[0]-f.start[0],dz=f.end[2]-f.start[2],length2=dx*dx+dz*dz,t=((p[0]-f.start[0])*dx+(p[2]-f.start[2])*dz)/length2;if(t<0||t>1||Math.abs(dx*(p[2]-f.start[2])-dz*(p[0]-f.start[0]))>f.width/2*Math.sqrt(length2))continue;const y=f.start[1]+(f.end[1]-f.start[1])*t;if(y>p[1]+.2)minimum=Math.min(minimum,y-.16-p[1]);}for(const l of overhead.landings)if(l.y>p[1]+.2&&B.Polygon.containsPoint(l.polygon,[p[0],p[2]]))minimum=Math.min(minimum,l.y-.18-p[1]);}}
    stair.validation.headroomMeasured=Number.isFinite(minimum)?round(minimum):null;stair.validation.headroom=minimum>=B.AGENT_PROFILE.standingHeight;stair.validation.headroomSource='flight-and-landing-undersides';}
  return stairs;
};
function wallFromEdge(id,floor,a,b,height,thickness,internal,structure,material){const dx=b[0]-a[0],dz=b[1]-a[1],length=Math.hypot(dx,dz),tangent=[dx/length,dz/length],normal=[-tangent[1],tangent[0]],core=/TIMBER/.test(structure)?'wood':/STEEL/.test(structure)?'metal':structure==='MASONRY'?'brick':'concrete';return {id,floor,edge:{a:a.slice(),b:b.slice()},tangent,normal,yaw:Math.atan2(dz,dx),length,height,thickness,internal,structural:!internal,assemblyId:internal?'interior-partition':'exterior-wall',physicalSolidId:id,layers:[{id:'structural-core',material:core,start:0,end:thickness,thickness}],material,openings:[]};}
B.buildWallsAndPortals=function(spec,storeyPolygons,rooms,roomGraphs){const walls=[],openings=[],connections=[];for(let floor=0;floor<storeyPolygons.length;floor++){
  const poly=storeyPolygons[floor],height=spec.storeys.floorHeight,thickness=B.STRUCTURAL_SYSTEMS[spec.structuralSystem]?.wallThickness||.24,outerKeys=new Set();for(const ring of [poly.outer,...poly.holes])ring.forEach((a,i)=>{const b=ring[(i+1)%ring.length],id=`f${floor}/exterior-${outerKeys.size}`;outerKeys.add(edgeId(a,b));walls.push(wallFromEdge(id,floor,a,b,height,thickness,false,spec.structuralSystem,spec.material));});
  // Split collinear overlaps at all room vertices, including T junctions.
  const map=new Map(),floorRooms=rooms.filter(r=>r.floor===floor),vertices=floorRooms.flatMap(r=>r.polygon.outer),on=(p,a,b)=>Math.abs((b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]))<1e-6&&p[0]>=Math.min(a[0],b[0])-1e-6&&p[0]<=Math.max(a[0],b[0])+1e-6&&p[1]>=Math.min(a[1],b[1])-1e-6&&p[1]<=Math.max(a[1],b[1])+1e-6;
  for(const room of floorRooms)for(const ring of [room.polygon.outer,...room.polygon.holes])ring.forEach((a,i)=>{const b=ring[(i+1)%ring.length],dx=b[0]-a[0],dz=b[1]-a[1],len2=dx*dx+dz*dz;if(len2<1e-10)return;const cuts=[a,b,...vertices.filter(p=>on(p,a,b))].sort((p,q)=>((p[0]-q[0])*dx+(p[1]-q[1])*dz)/len2).filter((p,j,all)=>!j||Math.hypot(p[0]-all[j-1][0],p[1]-all[j-1][1])>1e-6);for(let j=0;j<cuts.length-1;j++){const p=cuts[j].map(round),q=cuts[j+1].map(round);if([poly.outer,...poly.holes].some(r=>r.some((v,k)=>on(p,v,r[(k+1)%r.length])&&on(q,v,r[(k+1)%r.length]))))continue;const key=edgeId(p,q),entries=map.get(key)||[];if(!entries.some(e=>e.roomId===room.id))entries.push({a:p,b:q,roomId:room.id});map.set(key,entries);}});
  const coreRoutes=new Map();
  for(const room of floorRooms.filter(r=>r.role==='VERTICAL_CORE'&&r.verticalCore)){
    const stairs=B.buildStairs(spec,room.verticalCore),access=[];
    if(floor>0){const f=stairs[floor-1]?.flights.at(-1);if(f)access.push({point:[f.end[0],f.end[2]],direction:[(f.end[0]-f.start[0])/f.run,(f.end[2]-f.start[2])/f.run]});}
    if(floor<spec.storeys.count-1){const f=stairs[floor]?.flights[0];if(f)access.push({point:[f.start[0],f.start[2]],direction:[(f.start[0]-f.end[0])/f.run,(f.start[2]-f.end[2])/f.run]});}
    coreRoutes.set(room.id,access);
  }
  for(const entries of map.values()){
    if(entries.length<2)continue;const e=entries[0],wall=wallFromEdge(`f${floor}/partition-${walls.length}`,floor,e.a,e.b,height,.13,true,spec.structuralSystem,spec.material);walls.push(wall);
    const width=Math.min(1,wall.length-.3);if(width<B.AGENT_PROFILE.shoulderWidth+2*B.AGENT_PROFILE.passageMargin)continue;
    const core=entries.find(e=>coreRoutes.has(e.roomId)),centers=[];
    if(core){
      for(const access of coreRoutes.get(core.roomId)){const d=access.direction,t=wall.tangent,delta=[wall.edge.a[0]-access.point[0],wall.edge.a[1]-access.point[1]],det=d[0]*t[1]-d[1]*t[0];if(Math.abs(det)<1e-7)continue;const along=(delta[0]*t[1]-delta[1]*t[0])/det,u=(delta[0]*d[1]-delta[1]*d[0])/det;if(along<-.05||along>2||u<-.05||u>wall.length+.05)continue;const center=Math.max(width/2+.1,Math.min(wall.length-width/2-.1,u));if(Math.abs(center-u)>.45||centers.some(v=>Math.abs(v-center)<width+.1))continue;centers.push(center);}
    }else centers.push(wall.length/2);
    for(const u of centers){const id=wall.id+'/door-'+wall.openings.length,center=[e.a[0]+wall.tangent[0]*u,e.a[1]+wall.tangent[1]*u],clear=capsulePolygon([center[0]-wall.normal[0]*.65,center[1]-wall.normal[1]*.65],[center[0]+wall.normal[0]*.65,center[1]+wall.normal[1]*.65],width/2),opening={id,wallId:wall.id,kind:'door',t:u/wall.length,center:u,width,bottom:0,height:2.1,floor,rooms:[entries[0].roomId,entries[1].roomId],clearPolygon:clear,dynamic:true,defaultState:'OPEN'};wall.openings.push(opening);openings.push(opening);connections.push({from:opening.rooms[0],to:opening.rooms[1],type:'REQUIRED_TRAVERSABLE',openingId:id});}
  }
  if(floor===0){
    const candidates=walls.filter(w=>w.floor===floor&&!w.internal).sort((a,b)=>((b.edge.a[1]+b.edge.b[1])-(a.edge.a[1]+a.edge.b[1])));let placed=false;
    for(const front of candidates){const width=Math.min(1.1,front.length-.3);if(width<.66)continue;for(const t of [.5,.35,.65,.2,.8]){
      const u=t*front.length;if(u-width/2<.1||u+width/2>front.length-.1)continue;const c=[front.edge.a[0]+front.tangent[0]*u,front.edge.a[1]+front.tangent[1]*u],room=floorRooms.find(r=>r.role!=='VERTICAL_CORE'&&[-width/2+.05,0,width/2-.05].every(v=>B.Polygon.containsPoint(r.polygon,[c[0]+front.normal[0]*.2+front.tangent[0]*v,c[1]+front.normal[1]*.2+front.tangent[1]*v])));if(!room)continue;
      const id=front.id+'/entrance',opening={id,wallId:front.id,kind:'door',t,center:u,width,bottom:0,height:2.2,floor,rooms:['outside',room.id],clearPolygon:capsulePolygon([c[0]-front.normal[0],c[1]-front.normal[1]],[c[0]+front.normal[0],c[1]+front.normal[1]],width/2),dynamic:true,defaultState:'OPEN'};front.openings.push(opening);openings.push(opening);connections.push({from:'outside',to:room.id,type:'REQUIRED_TRAVERSABLE',openingId:id});placed=true;break;
    }if(placed)break;}
  }
  }
  return {walls,openings,connections};
};
function navcatMesh(slabs,stairs=[]){
  const positions=[],indices=[];
  function triangle(points){const offset=positions.length/3;const [a,b,c]=points;if((b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2])<0)[points[1],points[2]]=[points[2],points[1]];positions.push(...points.flat());indices.push(offset,offset+1,offset+2);}
  for(const slab of slabs){const tri=B.Polygon.triangulate(slab.polygon);for(let i=0;i<tri.indices.length;i+=3)triangle(tri.indices.slice(i,i+3).map(j=>[tri.vertices[j*2],slab.y+.02,tri.vertices[j*2+1]]));}
  for(const stair of stairs){
    for(const f of stair.flights){const dx=f.end[0]-f.start[0],dz=f.end[2]-f.start[2],len=Math.hypot(dx,dz),nx=-dz/len*f.width/2,nz=dx/len*f.width/2;
      const p=[f.start,f.end].flatMap(v=>[[v[0]+nx,v[1]+.02,v[2]+nz],[v[0]-nx,v[1]+.02,v[2]-nz]]);triangle([p[0],p[2],p[3]]);triangle([p[0],p[3],p[1]]);
    }
    const last=stair.flights.at(-1),dx=(last.end[0]-last.start[0])/last.run,dz=(last.end[2]-last.start[2])/last.run,e=last.end,r=stair.width/2+.15;
    const exit=B.Polygon.canonical({outer:[[e[0]-dz*r,e[2]+dx*r],[e[0]+dz*r,e[2]-dx*r],[e[0]+dx*r+dz*r,e[2]+dz*r-dx*r],[e[0]+dx*r-dz*r,e[2]+dz*r+dx*r]],holes:[]});
    for(const landing of stair.landings.some(l=>l.kind==='exit')?stair.landings:[...stair.landings,{polygon:exit,y:e[1]}]){const tri=B.Polygon.triangulate(landing.polygon);for(let i=0;i<tri.indices.length;i+=3)triangle(tri.indices.slice(i,i+3).map(j=>[tri.vertices[j*2],landing.y+.02,tri.vertices[j*2+1]]));}
  }
  // At 20 cm, ceil(radius/cellSize) plus ledge filtering erases metre-wide
  // ramps. Ten-centimetre cells preserve clearance without shrinking the agent.
  const cs=.1,ch=.05,profile=B.AGENT_PROFILE,options={cellSize:cs,cellHeight:ch,walkableRadiusVoxels:Math.ceil(profile.radius/cs),walkableRadiusWorld:profile.radius,walkableClimbVoxels:6,walkableClimbWorld:.3,walkableHeightVoxels:Math.ceil(profile.standingHeight/ch),walkableHeightWorld:profile.standingHeight,walkableSlopeAngleDegrees:45,borderSize:0,minRegionArea:4,mergeRegionArea:20,maxSimplificationError:.5,maxEdgeLength:12,maxVerticesPerPoly:6,detailSampleDistance:6,detailSampleMaxError:1};
  const result=B.NavBackend.build({positions,indices},options);return {backend:'navcat-0.4.1',input:{positions,indices},data:B.NavBackend.serialize(result.navMesh)};
}
function wallObstacle(wall,start,end){const a=wall.edge.a,t=wall.tangent,n=wall.normal,r=wall.thickness/2;return B.Polygon.canonical({outer:[[a[0]+t[0]*start+n[0]*r,a[1]+t[1]*start+n[1]*r],[a[0]+t[0]*end+n[0]*r,a[1]+t[1]*end+n[1]*r],[a[0]+t[0]*end-n[0]*r,a[1]+t[1]*end-n[1]*r],[a[0]+t[0]*start-n[0]*r,a[1]+t[1]*start-n[1]*r]],holes:[]});}
B.buildNavigation=function(plan){
  const blockers=[];
  for(const wall of plan.walls){
    const open=(wall.openings||[]).filter(o=>o.kind==='door'&&(o.state||o.defaultState)==='OPEN'&&o.bottom<.2&&o.height>=B.AGENT_PROFILE.standingHeight).map(o=>[Math.max(0,(o.center??o.t*wall.length)-o.width/2),Math.min(wall.length,(o.center??o.t*wall.length)+o.width/2)]).sort((a,b)=>a[0]-b[0]);let start=0;
    for(const [lo,hi] of [...open,[wall.length,wall.length]]){if(lo-start>1e-6)blockers.push({id:'nav/blocker/'+wall.id+'/'+start,floor:wall.floor,kind:'wall',polygon:wallObstacle(wall,start,lo)});start=Math.max(start,hi);}
  }
  for(const item of plan.furniture||[])blockers.push({id:'nav/blocker/'+item.id,floor:item.floor,kind:'furniture',polygon:item.polygon});
  for(const feature of plan.features||[])for(const solid of feature.solids||[])if(solid.kind==='chimney'&&solid.polygon)for(const slab of plan.slabs)if(solid.bottom<=slab.y+.2&&solid.top>=slab.y+B.AGENT_PROFILE.standingHeight)blockers.push({id:'nav/blocker/'+solid.id+'/'+slab.floor,floor:slab.floor,kind:'chimney',polygon:solid.polygon});
  const surfaces=plan.slabs.flatMap(s=>{const obstacles=blockers.filter(b=>b.floor===s.floor);const polygons=obstacles.length?B.Polygon.fromBackend(B.PolygonBackend.difference(B.Polygon.backendFormat(s.polygon),...obstacles.map(b=>B.Polygon.backendFormat(b.polygon)))):[s.polygon];return polygons.map((polygon,i)=>({id:'nav/'+s.id+'/'+i,floor:s.floor,polygon,y:s.y,roomIds:plan.rooms.filter(r=>r.floor===s.floor).map(r=>r.id)}));});
  const portals=plan.openings.filter(o=>o.kind==='door').map(o=>({id:'nav/'+o.id,openingId:o.id,from:o.rooms[0],to:o.rooms[1],dynamic:o.dynamic,state:o.state||o.defaultState,clearPolygon:o.clearPolygon}));
  for(const stair of plan.stairs){let minimum=stair.validation.headroomMeasured??Infinity;for(const p of stair.centerline){for(const slab of plan.slabs)if(slab.y>p[1]+.2&&B.Polygon.containsPoint(slab.polygon,[p[0],p[2]]))minimum=Math.min(minimum,slab.y-slab.thickness-p[1]);const roofY=plan.roof&&B.roofHeightAt(plan.roof,p[0],p[2]);if(roofY!==null&&roofY>p[1]+.2)minimum=Math.min(minimum,roofY-.12-p[1]);}stair.validation.headroomMeasured=Number.isFinite(minimum)?round(minimum):null;stair.validation.headroom=minimum>=B.AGENT_PROFILE.standingHeight;stair.validation.headroomSource='slabs-roof-flight-and-landing-undersides';}
  const verticalLinks=plan.stairs.map(s=>({id:'nav/'+s.id,type:'STAIR_RAMP',fromFloor:s.floor,toFloor:s.floor+1,centerline:s.centerline,width:s.width,clearPolygons:s.clearanceVolume.polygons,enabled:s.validation.headroom&&s.validation.dimensions}));
  let navmesh;try{navmesh=navcatMesh(surfaces,plan.stairs.filter(s=>s.validation.headroom&&s.validation.dimensions));}catch(error){throw new B.BuildingGenerationError('NAVMESH_FAILED','navigation',plan.spec,null,[{message:error.message}],plan);}
  const query=globalThis.RTSBuildingVendors.navcat;
  for(const [i,stair] of plan.stairs.entries()){
    const link=verticalLinks[i];if(!link.enabled){stair.validation.navigationConnected=false;continue;}
    // findPath may project inputs in place; preserve the geometry's walkline.
    const route=query.findPath(navmesh.data,stair.centerline[0].slice(),stair.centerline.at(-1).slice(),[.5,.5,.5],query.DEFAULT_QUERY_FILTER),complete=!!(route.flags&query.FindPathResultFlags.COMPLETE_PATH)&&!(route.flags&query.FindPathResultFlags.PARTIAL_PATH);
    link.pathValidation={complete,flags:route.flags};link.enabled=complete;stair.validation.navigationConnected=complete;
    if(!complete)throw new B.BuildingGenerationError('STAIR_NAVIGATION_DISCONNECTED','navigation',plan.spec,stair.id,[{code:'INCOMPLETE_STAIR_PATH',flags:route.flags}],plan);
  }
  return {agentProfile:B.AGENT_PROFILE,surfaces,portals,verticalLinks,blockers,breachLinks:[],navmesh};
};
B.buildStructure=function(plan){
  const columns=[],beams=[],roofAnchors=[];
  plan.storeys.forEach(storey=>{
    const byPoint=new Map();
    function support(point){const key=point.map(round).join(',');if(byPoint.has(key))return byPoint.get(key);const id=`support/f${storey.floor}/column-${byPoint.size}`;byPoint.set(key,id);columns.push({id,floor:storey.floor,point:point.slice(),height:plan.spec.storeys.floorHeight,section:B.STRUCTURAL_SYSTEMS[plan.spec.structuralSystem]?.section||.2,physicalSolidId:id});return id;}
    for(const ring of [storey.polygon.outer,...storey.polygon.holes])ring.forEach(support);
    plan.walls.filter(w=>w.floor===storey.floor&&!w.internal).forEach(w=>beams.push({id:'beam/'+w.id,floor:w.floor,edge:w.edge,supports:[support(w.edge.a),support(w.edge.b)],physicalSolidId:'beam/'+w.id}));
    if(storey===plan.storeys.at(-1))for(const ring of [storey.polygon.outer,...storey.polygon.holes])ring.forEach(point=>roofAnchors.push({id:`roof-anchor-${roofAnchors.length}`,point:point.slice(),supportId:support(point)}));
  });
  return {system:plan.spec.structuralSystem,bearingWallIds:plan.walls.filter(w=>w.structural).map(w=>w.id),columns,beams,slabSupports:columns.map(c=>c.id),roofAnchors};
};
// SkeletonBuilder returns {vertices:[[x,z,time]], polygons:[[vertexIndex,...]]}.
// Keep this source topology intact; family shaping is a separate, reproducible map.
const cross2=(a,b,c)=>(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
function roofTriangle(points,id,material,regionId){
  const p=points.map(v=>v.map(round));
  const area=cross2(p[0].filter((_,i)=>i!==1),p[1].filter((_,i)=>i!==1),p[2].filter((_,i)=>i!==1));
  if(Math.abs(area)<1e-9)return null;
  if(area>0)[p[1],p[2]]=[p[2],p[1]];
  return {id,regionId,points:p,kind:'roof',category:'roof',assemblyId:'roof-assembly',physicalSolidId:id,thickness:.12,material,layers:[{id:'roof-covering',material,start:0,end:.12,thickness:.12}]};
}
function splitTime(ring,time){
  const clip=above=>{const out=[];for(let i=0;i<ring.length;i++){const a=ring[i],b=ring[(i+1)%ring.length],ia=above?a[2]>=time:a[2]<=time,ib=above?b[2]>=time:b[2]<=time;if(ia)out.push(a);if(ia!==ib){const t=(time-a[2])/(b[2]-a[2]);out.push([a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t,time]);}}return out;};
  return [clip(false),clip(true)].filter(r=>r.length>=3);
}
function clipLinear(ring,value){const out=[];for(let i=0;i<ring.length;i++){const a=ring[i],b=ring[(i+1)%ring.length],va=value(a),vb=value(b),ia=va<=1e-9,ib=vb<=1e-9;if(ia)out.push(a);if(ia!==ib){const t=va/(va-vb);out.push(a.map((v,j)=>v+(b[j]-v)*t));}}return out;}
function surfaceTriangles(ring,height,id,material){
  const clean=ring.filter((p,i)=>!i||Math.hypot(p[0]-ring[i-1][0],p[1]-ring[i-1][1])>1e-8);
  if(clean.length<3||Math.abs(B.Polygon.signedArea(clean))<1e-8)return [];
  const tri=B.Polygon.triangulate({outer:clean,holes:[]}),points=[];
  for(let i=0;i<tri.vertices.length;i+=2){const x=tri.vertices[i],z=tri.vertices[i+1];points.push([x,height(x,z),z]);}
  const result=[];for(let i=0;i<tri.indices.length;i+=3){const f=roofTriangle(tri.indices.slice(i,i+3).map(j=>points[j]),`${id}/face-${i/3}`,material,id);if(f)result.push(f);}return result;
}
function triangleHeight(points,x,z){
  const [a,b,c]=points,det=(b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2]);
  if(Math.abs(det)<1e-10)return null;
  const u=((b[2]-c[2])*(x-c[0])+(c[0]-b[0])*(z-c[2]))/det,v=((c[2]-a[2])*(x-c[0])+(a[0]-c[0])*(z-c[2]))/det;
  return u>=-1e-6&&v>=-1e-6&&u+v<=1+1e-6?u*a[1]+v*b[1]+(1-u-v)*c[1]:null;
}
B.roofHeightAt=function(roof,x,z){let height=null;for(const face of roof.faces){const y=triangleHeight(face.points,x,z);if(y!==null&&(height===null||y>height))height=y;}return height;};
function roofEdges(roof){
  const edges=new Map(),key=p=>p.map(round).join(',');
  for(const f of roof.faces)for(let i=0;i<3;i++){const a=f.points[i],b=f.points[(i+1)%3],k=[key(a),key(b)].sort().join('|');if(!edges.has(k))edges.set(k,{a,b,faces:[]});edges.get(k).faces.push(f);}
  roof.ridges=[];roof.hips=[];roof.valleys=[];
  for(const edge of edges.values()){
    if(edge.faces.length!==2||edge.faces[0].regionId===edge.faces[1].regionId)continue;
    const {a,b}=edge,m=[(a[0]+b[0])/2,(a[1]+b[1])/2,(a[2]+b[2])/2],dx=b[0]-a[0],dz=b[2]-a[2],len=Math.hypot(dx,dz);if(len<1e-6)continue;
    const sample=sign=>B.roofHeightAt(roof,m[0]+sign*dz/len*.001,m[2]-sign*dx/len*.001),left=sample(1),right=sample(-1);if(left===null||right===null)continue;
    if(Math.abs(left+right-2*m[1])<1e-7)continue;
    const kind=left+right>2*m[1]?'valleys':Math.abs(a[1]-b[1])<1e-6?'ridges':'hips';roof[kind].push({id:`${kind}-${roof[kind].length}`,a,b});
  }
}
B.buildRoof=function(spec,input){
  const poly=B.Polygon.canonical(input),bounds=B.Polygon.bounds(poly),base=spec.storeys.count*spec.storeys.floorHeight,family=spec.roofFamily,material=spec.roofCovering||spec.material;
  const roof={family,base,polygon:poly,regions:[],faces:[],ridges:[],valleys:[],hips:[],eaves:[],gables:[],openings:[],skeleton:null,diagnostics:{backend:'straight-skeleton',fallbacks:[]}};
  let skeleton;
  try{
    skeleton=B.RoofSkeletonBackend.build([poly.outer,...poly.holes].map(r=>r.concat([r[0]])));
    if(!skeleton?.vertices?.length||!skeleton?.polygons?.length||skeleton.vertices.some(v=>v.length!==3||v.some(n=>!Number.isFinite(n))))throw new Error('Skeleton backend returned no valid face topology');
    roof.skeleton={vertices:skeleton.vertices.map(v=>v.slice()),polygons:skeleton.polygons.map(p=>p.slice())};
  }catch(error){
    roof.diagnostics.backend='compound-region';roof.diagnostics.fallbacks.push({code:'SKELETON_BACKEND_FAILED',message:error.message});
    // Explicit deterministic fallback: triangulated compound cells with an interior apex.
    const tri=B.Polygon.triangulate(poly);for(let i=0;i<tri.indices.length;i+=3){const ring=tri.indices.slice(i,i+3).map(j=>[tri.vertices[j*2],tri.vertices[j*2+1]]),c=ring.reduce((s,p)=>[s[0]+p[0]/3,s[1]+p[1]/3],[0,0]),rise=family==='FLAT'?0:Math.sqrt(Math.abs(B.Polygon.signedArea(ring)))*.22,id=`compound-region-${i/3}`;roof.regions.push({id,polygon:{outer:ring,holes:[]},fallback:true});for(let j=0;j<3;j++){const f=roofTriangle([point3(ring[j],base),point3(ring[(j+1)%3],base),point3(c,base+rise)],`${id}/face-${j}`,material,id);if(f)roof.faces.push(f);}}
  }
  if(roof.skeleton){
    const maxTime=Math.max(...skeleton.vertices.map(p=>p[2])),rise=Math.min(bounds[1]-bounds[0],bounds[3]-bounds[2])*.22,slope=rise/Math.max(maxTime,1e-6),longX=bounds[1]-bounds[0]>=bounds[3]-bounds[2],ends=[];
    if(['GABLE','COMPOUND_GABLE','HALF_HIP'].includes(family))for(const indices of skeleton.polygons){
      const boundary=indices.filter(i=>skeleton.vertices[i][2]<1e-7),inner=indices.filter(i=>skeleton.vertices[i][2]>=1e-7);
      if(boundary.length!==2||inner.length!==1)continue;
      const a=skeleton.vertices[boundary[0]],b=skeleton.vertices[boundary[1]],p=skeleton.vertices[inner[0]],dx=b[0]-a[0],dz=b[1]-a[1];
      if(family!=='COMPOUND_GABLE'&&(longX?Math.abs(dz)<Math.abs(dx):Math.abs(dx)<Math.abs(dz)))continue;
      // Only outer boundary terminal faces become gables; courtyard eaves stay intact.
      if(!poly.outer.some((v,i)=>Math.abs(cross2(v,poly.outer[(i+1)%poly.outer.length],a))<1e-6&&Math.abs(cross2(v,poly.outer[(i+1)%poly.outer.length],b))<1e-6))continue;
      ends.push({index:inner[0],time:p[2],dx:(a[0]+b[0])/2-p[0],dz:(a[1]+b[1])/2-p[1]});
    }
    // A skeleton event shared by two terminal faces (a square) must choose one axis.
    const uniqueEnds=ends.filter((e,i)=>ends.findIndex(q=>q.index===e.index)===i);
    // At a square's simultaneous collapse event both gable ends share one source
    // vertex. Split its source faces by the roof planes instead of choosing one end.
    const square=ends.length>uniqueEnds.length&&poly.outer.length===4&&!poly.holes.length&&Math.abs(bounds[1]-bounds[0]-(bounds[3]-bounds[2]))<1e-6&&poly.outer.every(p=>[bounds[0],bounds[1]].some(x=>Math.abs(p[0]-x)<1e-6)&&[bounds[2],bounds[3]].some(z=>Math.abs(p[1]-z)<1e-6));
    const squarePlanes=square?[p=>base+slope*(p[1]-bounds[2]),p=>base+slope*(bounds[3]-p[1]),...(family==='HALF_HIP'?[p=>base+rise*.5+slope*(p[0]-bounds[0]),p=>base+rise*.5+slope*(bounds[1]-p[0])]:[])]:null;
    const transform=(p,indices)=>{let x=p[0],z=p[1];for(const e of uniqueEnds){if(!indices.includes(e.index))continue;const event=skeleton.vertices[e.index];
      // Barycentric interpolation of a displacement field preserves shared edges.
      const original=indices.map(i=>skeleton.vertices[i]),flat=original.flatMap(v=>[v[0],v[1]]),ix=B.Triangulation.triangulate(flat,[],2);let weight=0;
      for(let j=0;j<ix.length;j+=3){const t=ix.slice(j,j+3).map(k=>[original[k][0],indices[k]===e.index?1:0,original[k][1]]),w=triangleHeight(t,p[0],p[1]);if(w!==null){weight=w;break;}}
      if(family==='HALF_HIP')weight=Math.min(weight,.5);
      x+=e.dx*weight;z+=e.dz*weight;
    }return [x,z,p[2]];};
    skeleton.polygons.forEach((indices,i)=>{
      const id=`roof-region-${i}`,original=indices.map(j=>skeleton.vertices[j]),edge=original.find((p,j)=>p[2]<1e-7&&original[(j+1)%original.length][2]<1e-7),edgeIndex=original.indexOf(edge),other=edge&&original[(edgeIndex+1)%original.length];
      let rings=[original];const breaks=family==='MANSARD'?[maxTime*.35]:family==='HALF_HIP'?uniqueEnds.filter(e=>indices.includes(e.index)).map(e=>e.time*.5):[];
      for(const level of breaks)rings=rings.flatMap(r=>splitTime(r,level));
      roof.regions.push({id,skeletonFace:i,sourceVertexIndices:indices.slice(),polygon:{outer:original.map(p=>p.slice(0,2)),holes:[]}});
      if(squarePlanes){for(const [j,plane] of squarePlanes.entries()){let ring=original;for(const other of squarePlanes)ring=clipLinear(ring,p=>plane(p)-other(p));if(ring.length<3||Math.abs(B.Polygon.signedArea(ring))<1e-8)continue;const partId=id+'/slope-'+j;roof.regions.push({id:partId,parentId:id,skeletonFace:i,polygon:{outer:ring.map(p=>p.slice(0,2)),holes:[]}});roof.faces.push(...surfaceTriangles(ring,(x,z)=>plane([x,z]),partId,material));}if(edge&&other)roof.eaves.push({id:`eave-${i}`,a:point3(edge,base),b:point3(other,base)});return;}
      for(const [part,ring] of rings.entries()){
        const shaped=ring.map(p=>transform(p,indices)),heightAt=p=>family==='FLAT'?base:family==='SHED'?base+(p[0]-bounds[0])/(bounds[1]-bounds[0])*rise:base+slope*(family==='MANSARD'?(p[2]<=maxTime*.35?p[2]*2:p[2]*.3+maxTime*.595):p[2]);
        const flat=shaped.flatMap(p=>[p[0],p[1]]);if(Math.abs(B.Polygon.signedArea(shaped))<1e-8)continue;
        const ix=B.Triangulation.triangulate(flat,[],2);for(let j=0;j<ix.length;j+=3){const face=roofTriangle(ix.slice(j,j+3).map(k=>[shaped[k][0],heightAt(shaped[k]),shaped[k][1]]),`${id}/part-${part}/face-${j/3}`,material,id);if(face)roof.faces.push(face);}
      }
      if(edge&&other)roof.eaves.push({id:`eave-${i}`,a:point3(edge,base),b:point3(other,base)});
    });
  }
  // Boundary profiles also locate the fascia/rake beams at their actual heights.
  roof.eaves=[];
  // Boundary profiles are actual vertical closure geometry, including raised shed edges.
  for(const [ri,ring] of [poly.outer,...poly.holes].entries())for(let i=0;i<ring.length;i++){
    const a=ring[i],b=ring[(i+1)%ring.length],dx=b[0]-a[0],dz=b[1]-a[1],len2=dx*dx+dz*dz,points=[];
    for(const face of roof.faces)for(const p of face.points){const t=((p[0]-a[0])*dx+(p[2]-a[1])*dz)/len2;if(t>=-1e-6&&t<=1+1e-6&&Math.abs(dx*(p[2]-a[1])-dz*(p[0]-a[0]))<1e-5)points.push({t,p});}
    points.sort((p,q)=>p.t-q.t);const unique=points.filter((p,j)=>!j||Math.abs(p.t-points[j-1].t)>1e-6);
    for(let j=0;j<unique.length-1;j++)roof.eaves.push({id:`eave-${ri}-${i}-${j}`,a:unique[j].p,b:unique[j+1].p});
    for(let j=0;j<unique.length-1;j++){const p=unique[j].p,q=unique[j+1].p;if(Math.max(p[1],q[1])<=base+1e-6)continue;const id=`gable-${ri}-${i}-${j}`,normal=[dz/Math.sqrt(len2),0,-dx/Math.sqrt(len2)];for(const [k,triangle] of [[point3([p[0],p[2]],base),p,q],[point3([p[0],p[2]],base),q,point3([q[0],q[2]],base)]].entries()){const u=triangle[1].map((v,k)=>v-triangle[0][k]),v=triangle[2].map((v,k)=>v-triangle[0][k]);if(Math.hypot(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])<1e-7)continue;roof.gables.push({id:id+'-'+k,points:triangle,normal,thickness:.18,material:spec.material,category:'wall'});}}
  }
  roofEdges(roof);return roof;
};
function cutRoof(roof,polygon,id,featureId){
  const faces=[];for(const face of roof.faces){const subject={outer:face.points.map(p=>[p[0],p[2]]),holes:[]},parts=B.Polygon.fromBackend(B.PolygonBackend.difference(B.Polygon.backendFormat(subject),B.Polygon.backendFormat(polygon)));
    for(const [part,poly] of parts.entries()){const tri=B.Polygon.triangulate(poly);for(let i=0;i<tri.indices.length;i+=3){const points=tri.indices.slice(i,i+3).map(j=>{const x=tri.vertices[j*2],z=tri.vertices[j*2+1];return [x,triangleHeight(face.points,x,z),z];});const f=roofTriangle(points,`${face.id}/cut-${part}-${i/3}`,face.material,face.regionId);if(f)faces.push(f);}}
  }roof.faces=faces;roof.openings.push({id,featureId,polygon});roofEdges(roof);
}
B.buildFeatures=function(spec,footprint,roof){
  const result=[],base=roof.base,material=spec.material,covering=spec.roofCovering||material,[x0,x1,z0,z1]=B.Polygon.bounds(footprint);
  const solid=(id,polygon,bottom,top,kind='wall',mat=material)=>({id,polygon,bottom,top,kind,category:kind,material:mat,structural:true});
  const box=(id,x,y,z,w,h,d,kind='wall',mat=material)=>({id,center:[x,y,z],size:[w,h,d],yaw:0,kind,category:kind,material:mat,structural:true});
  function roofSite(w,d,preferX){const candidates=roof.faces.map(f=>{const p=f.points.reduce((s,v)=>[s[0]+v[0]/3,s[1]+v[2]/3],[0,0]);return {p,score:Math.abs(p[0]-preferX)+Math.abs(p[1]-(z0+z1)/2)*.2};}).sort((a,b)=>a.score-b.score);
    for(const {p} of candidates){const polygon=B.Polygon.rectangle(p[0]-w/2,p[0]+w/2,p[1]-d/2,p[1]+d/2),samples=[...polygon.outer,p],heights=samples.map(v=>B.roofHeightAt(roof,...v));if(heights.some(y=>y===null))continue;const area=roof.faces.reduce((sum,f)=>sum+B.Polygon.fromBackend(B.PolygonBackend.intersection(B.Polygon.backendFormat({outer:f.points.map(v=>[v[0],v[2]]),holes:[]}),B.Polygon.backendFormat(polygon))).reduce((n,q)=>n+B.Polygon.area(q),0),0);if(Math.abs(area-w*d)<1e-5)return {p,polygon,heights};}return null;
  }
  if(spec.features.chimney){const id='feature/chimney',site=roofSite(.7,.7,(x0+x1)/2);if(site){const [x,z]=site.p,top=Math.max(...site.heights)+1.1,outer=site.polygon,inner=B.Polygon.rectangle(x-.18,x+.18,z-.18,z+.18),shaft={polygon:B.Polygon.canonical({outer:outer.outer,holes:[inner.outer]}),bottom:0,top};result.push({id,type:'CHIMNEY',shaft,roofOpeningId:id+'/roof-opening',solids:[solid(id+'/shaft',shaft.polygon,0,top,'chimney','brick'),solid(id+'/cap',B.Polygon.canonical({outer:B.Polygon.rectangle(x-.43,x+.43,z-.43,z+.43).outer,holes:[inner.outer]}),top,top+.12,'chimney','concrete')],faces:[]});cutRoof(roof,outer,id+'/roof-opening',id);}}
  // Attach exterior features to a real outer edge, avoiding courtyard and concave notches.
  const edges=footprint.outer.map((a,i)=>({a,b:footprint.outer[(i+1)%footprint.outer.length]})).sort((a,b)=>(b.a[1]+b.b[1])-(a.a[1]+a.b[1])),front=edges.find(e=>Math.hypot(e.b[0]-e.a[0],e.b[1]-e.a[1])>=2)||edges[0],dx=front.b[0]-front.a[0],dz=front.b[1]-front.a[1],len=Math.hypot(dx,dz),t=[dx/len,dz/len],n=[t[1],-t[0]],c=[(front.a[0]+front.b[0])/2,(front.a[1]+front.b[1])/2],width=Math.min(3,len*.8);
  const at=(u,v)=>[c[0]+u*t[0]+v*n[0],c[1]+u*t[1]+v*n[1]],deck=(depth)=>B.Polygon.canonical({outer:[at(-width/2,0),at(width/2,0),at(width/2,depth),at(-width/2,depth)],holes:[]});
  if(spec.features.porch){const id='feature/porch',depth=1.7,polygon=deck(depth),solids=[solid(id+'/deck',polygon,-.12,.06,'slab','concrete')],faces=[];for(const [i,u] of [-width/2+.1,width/2-.1].entries()){const p=at(u,depth-.1);solids.push(box(id+'/post-'+i,p[0],1.15,p[1],.15,2.3,.15,'column','wood'));}const points=[at(-width/2-.1,0),at(width/2+.1,0),at(width/2+.1,depth+.15),at(-width/2-.1,depth+.15)].map((p,i)=>point3(p,i<2?2.65:2.35));for(const [i,ix] of [[0,1,2],[0,2,3]].entries())faces.push(roofTriangle(ix.map(j=>points[j]),id+'/roof-'+i,covering,id));result.push({id,type:'PORCH',floor:0,solids,faces});}
  if(spec.features.balcony&&spec.storeys.count>1){const id='feature/balcony',y=spec.storeys.floorHeight,depth=1.25,polygon=deck(depth),solids=[solid(id+'/deck',polygon,y-.18,y,'slab','concrete')];for(let i=1;i<4;i++){const a=polygon.outer[i],b=polygon.outer[(i+1)%4],mid=[(a[0]+b[0])/2,(a[1]+b[1])/2],length=Math.hypot(b[0]-a[0],b[1]-a[1]);for(const h of [.12,1.05]){const rail=box(id+`/rail-${i}-${h}`,mid[0],y+h,mid[1],length,.06,.06,'railing','metal');rail.yaw=-Math.atan2(b[1]-a[1],b[0]-a[0]);solids.push(rail);}for(let j=0;j<=Math.ceil(length/.15);j++){const f=j/Math.ceil(length/.15);solids.push(box(id+`/baluster-${i}-${j}`,a[0]+(b[0]-a[0])*f,y+.55,a[1]+(b[1]-a[1])*f,.035,1.05,.035,'railing','metal'));}}result.push({id,type:'BALCONY',floor:1,solids,faces:[]});}
  if(spec.features.dormer&&spec.roofFamily!=='FLAT'){
    const id='feature/dormer',site=roofSite(1.2,1.4,x0+(x1-x0)*.25);
    if(site){
      const [x,z]=site.p,[a,b,c,d]=site.polygon.outer.map((p,i)=>point3(p,site.heights[i])),top=Math.max(...site.heights)+.75,ridge=top+.4;
      const points=[[x-.6,top,z-.7],[x+.6,top,z-.7],[x+.6,top,z+.7],[x-.6,top,z+.7],[x,ridge,z-.7],[x,ridge,z+.7]],faces=[],solids=[];
      for(const [i,ix] of [[0,4,5],[0,5,3],[4,1,2],[4,2,5]].entries())faces.push(roofTriangle(ix.map(j=>points[j]),id+'/roof-'+i,covering,id));
      for(const [i,pair] of [[a,b],[b,c],[c,d],[d,a]].entries()){
        if(i===2)continue;const [p,q]=pair;
        for(const [j,ps] of [[p,q,[q[0],top,q[2]]],[p,[q[0],top,q[2]],[p[0],top,p[2]]]].entries())faces.push({id:id+`/cheek-${i}-${j}`,points:ps,thickness:.12,material,category:'wall'});
      }
      const windowBottom=Math.max(c[1],d[1])+.12,windowTop=top-.12,windowWidth=.72,windowHeight=windowTop-windowBottom;
      // Triangulate the front wall in its own x/y plane with a true window hole.
      const front={outer:[[d[0],d[1]],[c[0],c[1]],[c[0],top],[d[0],top]],holes:[[[x-windowWidth/2,windowBottom],[x-windowWidth/2,windowTop],[x+windowWidth/2,windowTop],[x+windowWidth/2,windowBottom]]]},tri=B.Polygon.triangulate(front);
      for(let i=0;i<tri.indices.length;i+=3)faces.push({id:id+'/front-'+i/3,points:tri.indices.slice(i,i+3).map(j=>[tri.vertices[j*2],tri.vertices[j*2+1],z+.7]),normal:[0,0,1],thickness:.12,material,category:'wall'});
      const pane=box(id+'/window/glass',x,(windowBottom+windowTop)/2,z+.705,windowWidth-.08,windowHeight-.08,.025,'window','glass');pane.structural=false;solids.push(pane);
      for(const [i,xx] of [x-windowWidth/2,x+windowWidth/2].entries())solids.push(box(id+'/window/jamb-'+i,xx,(windowBottom+windowTop)/2,z+.72,.06,windowHeight+.06,.14,'window-frame','wood'));
      for(const [i,y] of [windowBottom,windowTop].entries())solids.push(box(id+'/window/rail-'+i,x,y,z+.72,windowWidth,.06,.14,'window-frame','wood'));
      faces.push({id:id+'/gable-front',points:[points[3],points[2],points[5]],normal:[0,0,1],thickness:.12,material,category:'wall'},{id:id+'/gable-back',points:[points[0],points[4],points[1]],normal:[0,0,-1],thickness:.12,material,category:'wall'});
      result.push({id,type:'DORMER',roofOpeningId:id+'/opening',window:{id:id+'/window',center:[x,(windowBottom+windowTop)/2,z+.7],width:windowWidth,height:windowHeight,bottom:windowBottom,top:windowTop},solids,faces});
      cutRoof(roof,site.polygon,id+'/opening',id);
    }
  }
  const diagnostics=roof.diagnostics.features=roof.diagnostics.features||[];
  for(const [flag,type] of [['chimney','CHIMNEY'],['porch','PORCH'],['balcony','BALCONY'],['dormer','DORMER']]){
    if(!spec.features[flag]||result.some(f=>f.type===type))continue;
    const reason=flag==='dormer'&&spec.roofFamily==='FLAT'?'Dormer requires a pitched roof':flag==='balcony'&&spec.storeys.count<2?'Balcony requires an upper storey':'No roof region can fully contain the requested feature';
    diagnostics.push({code:'FEATURES_UNPLACEABLE',featureId:'feature/'+flag,feature:type,reason});
  }
  return result;
};
B.buildFurniture=function(spec,rooms,navigation){
  if(!spec.furniture)return [];
  const reserved=navigation.portals.map(p=>p.clearPolygon).filter(Boolean);
  const overlaps=(a,b)=>B.Polygon.fromBackend(B.PolygonBackend.intersection(B.Polygon.backendFormat(a),B.Polygon.backendFormat(b))).reduce((n,p)=>n+B.Polygon.area(p),0)>1e-7;
  return rooms.map(r=>{
    if(/CORE|STAIR|CORRIDOR|CIRCULATION/.test(r.role))return null;
    const [x0,x1,z0,z1]=B.Polygon.bounds(r.polygon),w=Math.min(1.2,(x1-x0)*.3),d=Math.min(.8,(z1-z0)*.3);
    const stairClear=(navigation.verticalLinks||[]).filter(s=>s.fromFloor===r.floor||s.toFloor===r.floor).flatMap(s=>s.clearPolygons||s.centerline.slice(1).map((p,i)=>capsulePolygon([s.centerline[i][0],s.centerline[i][2]],[p[0],p[2]],s.width/2+.1)));
    for(const [x,z] of [[x0+.2,z0+.2],[x1-w-.2,z0+.2],[x0+.2,z1-d-.2],[x1-w-.2,z1-d-.2]]){
      const polygon=B.Polygon.rectangle(x,x+w,z,z+d),outside=B.Polygon.fromBackend(B.PolygonBackend.difference(B.Polygon.backendFormat(polygon),B.Polygon.backendFormat(r.polygon)));
      if(outside.some(p=>B.Polygon.area(p)>1e-7)||reserved.some(p=>overlaps(p,polygon))||stairClear.some(p=>overlaps(p,polygon)))continue;
      return {id:`furniture/${r.id}`,roomId:r.id,floor:r.floor,kind:r.role.includes('BED')?'bed':'table',polygon,height:r.role.includes('BED')?.5:.75};
    }
    return null;
  }).filter(Boolean);
};
})();
