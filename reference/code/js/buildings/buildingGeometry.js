(function(){
'use strict';
const B=globalThis.RTS.Buildings;
const colors={stucco:0xd2c5aa,brick:0x985f4b,concrete:0xa1a39b,metal:0x586369,wood:0x806047,stone:0x8c887b,glass:0x93b5c2,trim:0xe5ddca,frame:0x443d34,clay:0x9e5440,slate:0x50585e,shingle:0x76624e,corrugated:0x657174,membrane:0x454a49,thatch:0xaa9261,mattress:0xd9d2bd,fabric:0x6f827d};
function wallSegments(wall){
  const cuts=[0,wall.length],out=[],openings=wall.openings||[];
  for(const o of openings){const c=o.center??o.t*wall.length;cuts.push(Math.max(0,c-o.width/2),Math.min(wall.length,c+o.width/2));}
  cuts.sort((a,b)=>a-b);
  for(let i=0;i<cuts.length-1;i++){
    const a=cuts[i],b=cuts[i+1];if(b-a<1e-6)continue;
    const mid=(a+b)/2,active=openings.filter(o=>{const c=o.center??o.t*wall.length;return mid>c-o.width/2+1e-6&&mid<c+o.width/2-1e-6;}),ys=[0,wall.height];
    for(const o of active)ys.push(o.bottom,o.bottom+o.height);ys.sort((a,b)=>a-b);
    for(let j=0;j<ys.length-1;j++){const bottom=ys[j],top=ys[j+1];if(top-bottom<1e-6||active.some(o=>(bottom+top)/2>o.bottom&&(bottom+top)/2<o.bottom+o.height))continue;out.push({lo:a,hi:b,bottom,top});}
  }return out;
}
// Closed convex pieces preserve courtyard/shaft holes in rendering AND destruction.
function prismGeometry(points,offset){
  const a=points.map(p=>p.slice()),b=a.map(p=>p.map((v,i)=>v+offset[i]));
  const n=new THREE.Vector3().subVectors(new THREE.Vector3(...a[1]),new THREE.Vector3(...a[0])).cross(new THREE.Vector3().subVectors(new THREE.Vector3(...a[2]),new THREE.Vector3(...a[0])));
  if(n.dot(new THREE.Vector3(...offset))>0){a.reverse();b.reverse();}
  const faces=[a,b.slice().reverse()];for(let i=0;i<a.length;i++){const j=(i+1)%a.length;faces.push([a[j],a[i],b[i],b[j]]);}
  const pos=[];for(const f of faces)for(let i=1;i<f.length-1;i++)pos.push(...f[0],...f[i],...f[i+1]);
  const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(pos,3));g.computeVertexNormals();g.computeBoundingBox();g.computeBoundingSphere();return g;
}
B.buildGeometry=function(plan,options={}){
  if(plan?.schema!=='rts.building-plan/4')throw new TypeError('buildGeometry requires BuildingPlan/4');
  const root=new THREE.Group(),components=[],materials=new Map(),tint=.96+new B.RNG(plan.spec.seeds.facade).next()*.08;
  root.name='Building '+plan.spec.presetId;
  function mat(id){
    id=id||'concrete';
    if(!materials.has(id)){const color=new THREE.Color(colors[id]??0x92958e);if(id===plan.spec.material)color.multiplyScalar(tint);
      materials.set(id,new THREE.MeshStandardMaterial({color,roughness:id==='glass'?.2:/metal|corrugated/.test(id)?.65:.92,metalness:/metal|corrugated/.test(id)?.2:0,transparent:id==='glass',opacity:id==='glass'?.45:1,depthWrite:id!=='glass'}));}
    const material=materials.get(id);
    if(!material.userData.proceduralSurface&&['brick','wood','stone','stucco','concrete','clay','slate','shingle','metal','corrugated'].includes(id)){
      material.userData.proceduralSurface=id;
      material.customProgramCacheKey=()=> 'building-surface-6/'+id;
      material.onBeforeCompile=shader=>{
        shader.vertexShader=shader.vertexShader.replace('#include <common>','#include <common>\nvarying vec3 buildingPosition;')
          .replace('#include <begin_vertex>','#include <begin_vertex>\nbuildingPosition=(modelMatrix*vec4(position,1.0)).xyz;');
        const pattern=id==='brick'||id==='stone'?
          'float row=floor(buildingPosition.y/0.23);vec2 uv=fract(vec2((buildingPosition.x+buildingPosition.z)/0.48+mod(row,2.0)*0.5,buildingPosition.y/0.23));float mortar=step(0.045,min(min(uv.x,1.0-uv.x),min(uv.y,1.0-uv.y)));diffuseColor.rgb=mix(diffuseColor.rgb*1.23,diffuseColor.rgb,mortar);':
          ['clay','slate','shingle'].includes(id)?
          'vec2 tile=fract(buildingPosition.xz/vec2(0.32,0.45));float seam=step(0.025,min(min(tile.x,1.0-tile.x),min(tile.y,1.0-tile.y)));diffuseColor.rgb*=mix(0.72,1.0,seam);':
          id==='wood'?'diffuseColor.rgb*=0.94+0.06*sin((buildingPosition.x+buildingPosition.z)*95.0+sin(buildingPosition.y*8.0));':
          id==='metal'||id==='corrugated'?'diffuseColor.rgb*=0.9+0.1*smoothstep(0.025,0.06,fract((buildingPosition.x+buildingPosition.z)*3.0));':
          'diffuseColor.rgb*=0.96+0.04*fract(sin(dot(floor(buildingPosition*70.0),vec3(12.9898,78.233,37.719)))*43758.5453);';
        shader.fragmentShader=shader.fragmentShader.replace('#include <common>','#include <common>\nvarying vec3 buildingPosition;')
          .replace('#include <color_fragment>','#include <color_fragment>\n'+pattern);
      };
    }
    return material;
  }
  function add(mesh,source,category,extra={}){
    const material=extra.material||source?.material||plan.spec.material;
    mesh.castShadow=category!=='window';mesh.receiveShadow=true;mesh.geometry.computeBoundingBox();
    const s=new THREE.Vector3();mesh.geometry.boundingBox.getSize(s);
    const c={id:mesh.name,mesh,category,floor:source?.floor??null,sourceId:source?.id||null,structural:source?.structural!==false,maxHealth:100,health:100,surfaceArea:Math.max(.01,2*(s.x*s.y+s.x*s.z+s.y*s.z)),...extra};
    mesh.userData.building={id:c.id,category,floor:c.floor,sourceId:c.sourceId,structural:c.structural,material,assemblyId:source?.assemblyId||category+'-assembly',physicalSolidId:source?.physicalSolidId||mesh.name,wallId:c.wallId,roofId:c.roofId,featureId:c.featureId,layers:source?.layers||[]};
    mesh.userData.id=mesh.name;mesh.userData.physicalSolidId=mesh.userData.building.physicalSolidId;root.add(mesh);components.push(c);return mesh;
  }
  function box(id,center,size,material,source,category,extra={}){
    const m=new THREE.Mesh(new THREE.BoxGeometry(...size),mat(material));m.name=id;m.position.set(...center);m.rotation.y=extra.yaw||0;return add(m,source,category,{...extra,material});
  }
  function beam(id,a,b,width,material,source,category='detail',extra={}){
    const dir=new THREE.Vector3(...b).sub(new THREE.Vector3(...a));if(dir.length()<1e-6)return;
    const m=new THREE.Mesh(new THREE.BoxGeometry(width,dir.length(),width),mat(material));m.name=id;m.position.copy(new THREE.Vector3(...a).add(new THREE.Vector3(...b)).multiplyScalar(.5));m.quaternion.setFromUnitVectors(new THREE.Vector3(0,1,0),dir.normalize());return add(m,source,category,{structural:false,...extra,material});
  }
  function face(source,category,extra={}){
    const p=source.points,material=source.material||plan.spec.roofCovering||plan.spec.material;
    const n=new THREE.Vector3().subVectors(new THREE.Vector3(...p[1]),new THREE.Vector3(...p[0])).cross(new THREE.Vector3().subVectors(new THREE.Vector3(...p[2]),new THREE.Vector3(...p[0]))).normalize();
    const offset=Math.abs(n.y)>.05?[0,-(source.thickness||.12),0]:n.multiplyScalar(-(source.thickness||.18)).toArray();
    const m=new THREE.Mesh(prismGeometry(p,offset),mat(material));m.name=source.id;return add(m,source,category,{...extra,material});
  }
  function polygon(source,category,extra={}){
    const t=B.Polygon.triangulate(source.polygon),y=source.top??source.y,thickness=source.bottom!==undefined?y-source.bottom:source.thickness||.18;
    for(let i=0;i<t.indices.length;i+=3){const points=t.indices.slice(i,i+3).map(n=>[t.vertices[n*2],y,t.vertices[n*2+1]]);face({...source,id:source.id+':triangle-'+i/3,points,thickness},category,{sourceId:source.id,...extra});}
  }
  for(const wall of plan.walls){
    const ux=(wall.edge.b[0]-wall.edge.a[0])/wall.length,uz=(wall.edge.b[1]-wall.edge.a[1])/wall.length,yaw=-wall.yaw,elevation=plan.storeys[wall.floor].elevation;
    const at=(u,y,out=0)=>[wall.edge.a[0]+ux*u+uz*out,elevation+y,wall.edge.a[1]+uz*u-ux*out];
    for(const [i,s]of wallSegments(wall).entries())box(wall.id+':solid-'+i,at((s.lo+s.hi)/2,(s.bottom+s.top)/2),[s.hi-s.lo,s.top-s.bottom,wall.thickness],wall.material,wall,wall.internal?'partition':'wall',{wallId:wall.id,yaw});
    if(!wall.internal&&wall.floor===0)box('foundation/'+wall.id,at(wall.length/2,-.22),[wall.length+.04,.44,wall.thickness+.16],'stone',{...wall,id:'foundation/'+wall.id,layers:[]},'foundation',{wallId:wall.id,yaw});
    const detail=(id,u,y,w,h,d,material='trim',out=wall.thickness/2+.035,category='detail')=>box(wall.id+'/'+id,at(u,y,out),[w,h,d],material,wall,category,{wallId:wall.id,structural:false,yaw});
    for(const o of wall.openings){
      const u=o.center??o.t*wall.length,h=o.height,w=o.width,b=o.bottom,frame=plan.spec.facade.style==='MODERN'?'metal':'frame',s=.065;
      for(const sign of [-1,1])detail(o.id+'/jamb-'+sign,u+sign*(w/2-s/2),b+h/2,s,h,.12,frame);
      detail(o.id+'/head',u,b+h-s/2,w,s,.12,frame);
      if(o.kind==='window'){
        detail(o.id+'/sill',u,b-.025,w+.2,.09,.26);detail(o.id+'/bottom',u,b+s/2,w,s,.12,frame);
        detail(o.id+'/glass',u,b+h/2,w-2*s,h-2*s,.025,'glass',0,'window');
        const divisions=plan.spec.facade.windowType==='INDUSTRIAL'?3:plan.spec.facade.windowType==='WIDE'?2:1;
        for(let n=1;n<=divisions;n++)detail(o.id+'/mullion-'+n,u-w/2+w*n/(divisions+1),b+h/2,.035,h-.1,.13,frame);
        if(plan.spec.facade.style!=='MODERN')detail(o.id+'/transom',u,b+h*.6,w-.1,.035,.13,frame);
        if(plan.spec.facade.style==='CLASSIC')detail(o.id+'/lintel',u,b+h+.09,w+.25,.12,.2);
        if(plan.spec.facade.style==='RUSTIC')for(const sign of [-1,1])detail(o.id+'/shutter-'+sign,u+sign*(w*.75+.08),b+h/2,w*.4,h,.075,'wood');
      }else{
        const width=w-.13,hinge=new THREE.Vector3(...at(u-w/2+.065,b+h/2)),angle=yaw-Math.PI*.48,center=new THREE.Vector3(width/2,0,0).applyAxisAngle(new THREE.Vector3(0,1,0),angle).add(hinge);
        box(o.id+'/leaf',center.toArray(),[width,h-.1,.065],plan.spec.facade.doorType==='STEEL'?'metal':'wood',wall,'door',{wallId:wall.id,sourceId:o.id,structural:false,yaw:angle,openingId:o.id});
      }
    }
    if(!wall.internal){
      const style=plan.spec.facade.style;
      detail('cornice',wall.length/2,wall.height-.12,wall.length,.16,.18,style==='MODERN'?'metal':'trim');
      for(const s of wallSegments({...wall,height:.31,openings:wall.openings.filter(o=>o.bottom<.31).map(o=>({...o,height:.31-o.bottom}))}))detail('plinth-'+s.lo,(s.lo+s.hi)/2,(s.bottom+s.top)/2,s.hi-s.lo,s.top-s.bottom,.08,'stone');
      if(style==='RUSTIC'||style==='INDUSTRIAL'){
        const bays=Math.max(1,Math.ceil(wall.length/(style==='RUSTIC'?2.8:4.5)));
        for(let i=0;i<=bays;i++){const u=wall.length*i/bays;if(wall.openings.some(o=>Math.abs(o.t*wall.length-u)<o.width/2+.12))continue;detail('bay-'+i,u,wall.height/2,.1,wall.height-.28,.09,style==='RUSTIC'?'wood':'metal');}
      }
    }
  }
  if(!B.STRUCTURAL_SYSTEMS[plan.spec.structuralSystem]?.loadBearingWalls){
    const material=plan.spec.structuralSystem==='TIMBER_FRAME'?'wood':plan.spec.structuralSystem==='STEEL_FRAME'?'metal':'concrete';
    for(const column of plan.structure.columns){const y=plan.storeys[column.floor].elevation;
      box(column.id,[column.point[0],y+column.height/2,column.point[1]],[column.section,column.height,column.section],material,column,'structure');}
    for(const girder of plan.structure.beams){const y=plan.storeys[girder.floor].elevation+plan.spec.storeys.floorHeight-.14;
      beam(girder.id,[girder.edge.a[0],y,girder.edge.a[1]],[girder.edge.b[0],y,girder.edge.b[1]],.2,material,girder,'structure',{structural:true});}
  }
  for(const slab of plan.slabs)polygon({...slab,material:slab.layers?.[0]?.material||'concrete'},'slab');
  for(const stair of plan.stairs){
    for(const flight of stair.flights){
      const n=Math.max(1,flight.treads),dx=(flight.end[0]-flight.start[0])/n,dz=(flight.end[2]-flight.start[2])/n,yaw=-Math.atan2(dz,dx),tread=Math.hypot(dx,dz),rise=(flight.end[1]-flight.start[1])/flight.risers;
      for(let i=0;i<n;i++){const t=(i+.5)/n;box(flight.id+'/tread-'+i,[flight.start[0]+(flight.end[0]-flight.start[0])*t,flight.start[1]+rise*(i+1)-.045,flight.start[2]+(flight.end[2]-flight.start[2])*t],[tread+.015,.09,stair.width],'wood',stair,'stair',{yaw,structural:false});}
      const axis=new THREE.Vector3(flight.end[0]-flight.start[0],0,flight.end[2]-flight.start[2]).normalize(),side=[-axis.z,axis.x];
      for(const sign of [-1,1]){
        const point=(p,y)=>[p[0]+side[0]*stair.width/2*sign,p[1]+y,p[2]+side[1]*stair.width/2*sign];
        beam(flight.id+'/stringer-'+sign,point(flight.start,-.13),point(flight.end,-.13),.13,'metal',stair,'stair');
        beam(flight.id+'/handrail-'+sign,point(flight.start,.95),point(flight.end,.95),.055,'wood',stair);
        for(let i=0;i<=3;i++){const p=flight.start.map((v,k)=>v+(flight.end[k]-v)*i/3);beam(flight.id+'/baluster-'+sign+'-'+i,point(p,0),point(p,.95),.035,'metal',stair);}
      }
    }
    for(const l of stair.landings)box(l.id,[l.center[0],l.y-.06,l.center[2]],[l.width,.12,l.depth],'wood',stair,'stair',{structural:false,yaw:l.yaw||0});
  }
  for(const f of plan.roof.faces)face(f,'roof',{roofId:f.id});
  for(const g of plan.roof.gables||[])if(g.points)face({...g,material:plan.spec.material},'gable',{roofId:g.id});
  for(const e of [...(plan.roof.eaves||[]),...(plan.roof.ridges||[]),...(plan.roof.hips||[])])if(e.a&&e.b)beam(e.id,e.a,e.b,e.id.includes('eave')?.12:.09,plan.spec.roofCovering,{id:e.id},'roof-detail',{roofId:e.faceId||plan.roof.faces[0]?.id});
  for(const feature of plan.features||[]){
    const featureFloor=feature.floor??(feature.type==='PORCH'?0:feature.type==='BALCONY'?1:plan.storeys.length-1);
    const extra={sourceId:feature.id,featureId:feature.id,wallId:feature.wallId,roofId:feature.roofFaceId,floor:featureFloor};
    for(const s of feature.solids||[]){const source={...s,assemblyId:feature.id,physicalSolidId:s.id,floor:s.floor??featureFloor},category=({chimney:'wall',column:'structure',railing:'detail'})[s.category]||s.category||'structure';if(s.polygon)polygon(source,category,extra);else if(s.center&&s.size)box(s.id,s.center,s.size,s.material,source,category,{...extra,yaw:s.yaw||0,floor:source.floor});}
    for(const f of feature.faces||[])face({...f,assemblyId:feature.id,floor:f.floor??featureFloor},f.category||'roof',{sourceId:feature.id,featureId:feature.id,roofId:f.id,floor:f.floor??featureFloor});
  }
  for(const item of plan.furniture||[]){
    const b=B.Polygon.bounds(item.polygon),x=(b[0]+b[1])/2,z=(b[2]+b[3])/2,w=b[1]-b[0],d=b[3]-b[2],y=plan.storeys[item.floor].elevation;
    const part=(suffix,center,size,material)=>box(item.id+'/'+suffix,center,size,material,item,'furniture',{structural:false});
    if(item.kind==='bed'){part('frame',[x,y+.18,z],[w,.25,d],'wood');part('mattress',[x,y+.37,z],[w*.94,.18,d*.96],'mattress');part('pillow',[x,y+.49,z-d*.32],[w*.65,.1,d*.22],'fabric');}
    else{part('top',[x,y+.72,z],[w,.07,d],'wood');for(const a of [-1,1])for(const c of [-1,1])part('leg-'+a+'-'+c,[x+a*w*.38,y+.35,z+c*d*.35],[.07,.7,.07],'wood');}
  }
  const overlay=new THREE.Group();overlay.name='Navigation overlay';overlay.visible=!!options.showNavigation;
  const navmat=new THREE.MeshBasicMaterial({color:0x32d6a0,transparent:true,opacity:.18,depthWrite:false,side:THREE.DoubleSide});materials.set('navigation',navmat);
  for(const s of plan.navigation.surfaces||[]){const t=B.Polygon.triangulate(s.polygon),pos=[];for(let i=0;i<t.vertices.length;i+=2)pos.push(t.vertices[i],s.y+.045,t.vertices[i+1]);const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(pos,3));g.setIndex(t.indices);const m=new THREE.Mesh(g,navmat);m.name=s.id;overlay.add(m);}
  for(const l of plan.navigation.verticalLinks||[]){const g=new THREE.BufferGeometry().setFromPoints(l.centerline.map(p=>new THREE.Vector3(p[0],p[1]+.08,p[2]))),m=new THREE.LineBasicMaterial({color:0x32d6a0});materials.set(l.id,m);overlay.add(new THREE.Line(g,m));}
  root.add(overlay);
  const manifest={schema:'rts.building-manifest/6',generatorVersion:plan.generatorVersion,seed:plan.spec.seed,spec:plan.spec,footprint:plan.footprint,storeys:plan.storeys,walls:plan.walls,openings:plan.openings,stairs:plan.stairs,roof:plan.roof,navigation:plan.navigation,features:plan.features,furniture:plan.furniture,components:components.map(c=>({id:c.id,category:c.category,floor:c.floor,sourceId:c.sourceId,structural:c.structural,physicalSolidId:c.mesh.userData.building.physicalSolidId})),damage:{damagePercent:0,destroyed:0,total:components.length}};
  const instance=new B.BuildingInstance(root,manifest,[...materials.values()],components,overlay);instance.plan=plan;return instance;
};
B.wallSolidSegments=wallSegments;B.buildPrismGeometry=prismGeometry;
})();
