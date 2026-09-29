(function(){
  'use strict';
  const R=globalThis.RTS,S=R.DestructionSolid,V=S.V;
  const materials=m=>Array.isArray(m)?m:[m];
  const mapMaterial=value=>({brick:'brick',wood:'wood',stucco:'concrete',concrete:'concrete',stone:'rock',metal:'steel',glass:'glass',steel:'steel',interior:'concrete'}[String(value||'').toLowerCase()]||'concrete');
  function wallLayers(meta,surface,category,declared){
    if(declared?.layers)return declared.layers.map(l=>({...l,thickness:Number(l.thickness||0)})).filter(l=>l.thickness>1e-7);
    if(!['wall','partition','party-wall','gable'].includes(category))return null;
    const assembly=meta.plan?.wallAssembly;if(!assembly)return null;
    const structure=String(meta.plan?.spec?.structuralSystem||meta.structure||''),core=/TIMBER|WOOD/i.test(structure)?'wood':/REINFORCED_CONCRETE|PRECAST_CONCRETE/i.test(structure)?'concrete':/MASONRY/i.test(structure)?'brick':surface;
    const layers=[{material:surface,thickness:assembly.finishExterior||0},{material:core,thickness:assembly.structuralThickness||assembly.totalThickness||0},{material:surface,thickness:assembly.finishInterior||0}].filter(l=>l.thickness>1e-5);
    const total=layers.reduce((sum,l)=>sum+l.thickness,0);return total>0?layers.map(l=>({...l,fraction:l.thickness/total})):null;
  }
  function grainDirection(bounds,category,material){
    if(material!=='wood')return null;const extent=V.sub(bounds.max,bounds.min);let axis=extent.indexOf(Math.max(...extent));
    if(category==='structure'&&extent[1]>=Math.max(extent[0],extent[2])*.75)axis=1;
    const out=[0,0,0];out[axis]=1;return out;
  }
  function cloneRoot(source){const root=source.clone(true),map=new Map();function pair(a,b){map.set(a,b);a.children.forEach((c,i)=>pair(c,b.children[i]));}pair(source,root);
    source.traverse(o=>{if(o.isSkinnedMesh){const c=map.get(o);c.skeleton=o.skeleton.clone();c.skeleton.bones=o.skeleton.bones.map(b=>map.get(b));c.bindMatrix.copy(o.bindMatrix);c.bindMatrixInverse.copy(o.bindMatrixInverse);}});return {root,map};}
  function meshFaces(mesh){const geometry=mesh.geometry,position=geometry.attributes.position,index=geometry.index,faces=[],p=new THREE.Vector3();for(let i=0;i<(index?index.count:position.count);i+=3){const face=[];for(let k=0;k<3;k++){p.fromBufferAttribute(position,index?index.getX(i+k):i+k).applyMatrix4(mesh.matrixWorld);face.push(p.toArray());}faces.push(face);}return faces;}
  function boxFaces(mesh){mesh.geometry.computeBoundingBox();const box=mesh.geometry.boundingBox;return S.box(box.min.toArray(),box.max.toArray()).map(f=>f.map(p=>new THREE.Vector3(...p).applyMatrix4(mesh.matrixWorld).toArray()));}
  function solidFaces(mesh){
    if(/BoxGeometry|BoxBufferGeometry/.test(mesh.geometry.type))return boxFaces(mesh);
    const points=meshFaces(mesh).flat(),bounds=S.bounds([points]),extent=V.sub(bounds.max,bounds.min),axis=extent.indexOf(Math.min(...extent));
    // Gables are surface meshes; give them a thin masonry section. Sloped roofs
    // already have thickness and must retain their actual hull, not a filled AABB.
    if(extent[axis]<.002)for(const p of points.slice()){const q=p.slice();q[axis]-=.12;points.push(q);}
    const unique=new Set(points.map(p=>p.map(v=>v.toFixed(6)).join(',')));if(unique.size<4)return S.box(bounds.min.map((v,i)=>v-(i===axis?.06:0)),bounds.max.map((v,i)=>v+(i===axis?.06:0)));
    try{return DestructionConvexFaces(points.map(p=>new THREE.Vector3(...p)));}catch(error){throw new Error('Invalid destruction solid '+mesh.name+': '+error.message);}
  }
  function sections(faces,category,limit){
    if(!['wall','partition','party-wall','floor','slab','roof','gable'].includes(category)||limit<2)return [faces];
    const b=S.bounds(faces),extent=V.sub(b.max,b.min),thin=extent.indexOf(Math.min(...extent));
    const axes=category==='roof'||category==='floor'||category==='slab'?[0,2]:[0,1,2].filter(i=>i!==thin);
    const counts=axes.map(i=>Math.max(1,Math.ceil(extent[i]/1.6)));
    while(counts[0]*counts[1]>limit){const i=counts[0]>=counts[1]?0:1;counts[i]--;}
    let result=[faces];
    for(let i=0;i<axes.length;i++){const axis=axes[i],n=[0,0,0];n[axis]=1;const output=[];
      for(const part of result){let remaining=part;for(let j=1;j<counts[i];j++){const d=b.min[axis]+extent[axis]*j/counts[i],piece=S.clip(remaining,n,d);if(S.volume(piece)>1e-8)output.push(piece);remaining=S.clip(remaining,V.mul(n,-1),-d);}if(S.volume(remaining)>1e-8)output.push(remaining);}result=output;
    }return result;
  }
  function supportsFromBelow(part,support){
    const a=part.bounds,b=support.bounds,category=part.category,kind=support.category;
    if(!a.min.every((v,i)=>v<=b.max[i]+.12&&a.max[i]+.12>=b.min[i]))return false;
    if(kind==='foundation')return b.min[1]<a.min[1]+.2&&b.max[1]>=a.min[1]-.12;
    const wall=k=>['wall','partition','party-wall','gable'].includes(k),floor=k=>k==='floor'||k==='slab',sameWall=part.wallId&&part.wallId===support.wallId||part.sourceId&&part.sourceId===support.sourceId;
    const bearing=kind==='structure'&&!support.suspendedFrame||wall(kind)&&support.structural;
    if(floor(category))return bearing&&b.min[1]<a.min[1]+.03||floor(kind)&&Math.abs(a.max[1]-b.max[1])<.03;
    if(category==='roof')return kind==='roof'||bearing&&b.min[1]<a.min[1]+.03||floor(kind)&&b.max[1]<=a.min[1]+.12;
    if(wall(category)||category==='window'||category==='door'){
      if(floor(kind))return b.max[1]<=a.min[1]+.12;
      if(kind==='structure')return b.min[1]<=a.min[1]+.12;
      if(wall(kind))return !!sameWall&&b.min[1]<a.max[1]-.03||support.structural&&b.max[1]<=a.min[1]+.12;
      return false;
    }
    // The generator clips posts around openings. Their isolated upper stubs
    // hang from the header/slab; they cannot carry that slab themselves.
    if(category==='structure')return part.suspendedFrame&&(kind==='roof'||floor(kind)&&b.min[1]>=a.max[1]-.12)||(bearing||floor(kind))&&b.max[1]<=a.min[1]+.12;
    if(category==='stair')return (bearing||floor(kind)||kind==='stair')&&b.min[1]<a.min[1]+.03;
    return false;
  }
  function supportLink(part,support){
    if(!supportsFromBelow(part,support))return null;
    const a=part.bounds,b=support.bounds,category=part.category,kind=support.category;
    const floor=k=>k==='floor'||k==='slab',wall=k=>['wall','partition','party-wall','gable'].includes(k);
    const lateral=floor(category)&&floor(kind)||category==='roof'&&kind==='roof'||wall(category)&&(wall(kind)&&b.max[1]>a.min[1]+.12||kind==='structure');
    const dx=(a.min[0]+a.max[0]-b.min[0]-b.max[0])/2,dz=(a.min[2]+a.max[2]-b.min[2]-b.max[2])/2;
    return {cost:lateral?Math.max(.001,Math.hypot(dx,dz)):0,limit:part.supportSpanLimit??Infinity,carry:true};
  }
  function calibrateSupport(parts){
    // Preserve spans already present in the generated intact design. Subsequent
    // damage may not turn a short lintel into an unlimited chain of cantilevers.
    const byId=new Map(parts.map(p=>[p.id,p])),dependents=new Map(),distance=new Map(),queue=[];
    for(const p of parts){if(p.anchored){distance.set(p.id,0);queue.push(p.id);}for(const id of p.supports){if(!dependents.has(id))dependents.set(id,[]);dependents.get(id).push(p);}}
    for(let i=0;i<queue.length;i++){const q=byId.get(queue[i]);for(const p of dependents.get(q.id)||[]){const link=supportLink(p,q);if(!link)continue;const candidate=distance.get(q.id)+link.cost;if(candidate<(distance.get(p.id)??Infinity)-1e-7){distance.set(p.id,candidate);queue.push(p.id);}}}
    for(const p of parts){const allowance=p.category==='roof'?4:['floor','slab'].includes(p.category)?3:1.2;p.supportSpanLimit=Math.max(allowance,(distance.get(p.id)||0)+.25);}
  }
  function cachedSupportPolicy(){const cache=new WeakMap();return (part,support)=>{let links=cache.get(part);if(!links){links=new WeakMap();cache.set(part,links);}if(!links.has(support))links.set(support,supportLink(part,support));return links.get(support);};}
  function geometry(faces){const values=[];for(const f of faces)for(let i=1;i<f.length-1;i++)values.push(...f[0],...f[i],...f[i+1]);const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(values,3));g.computeVertexNormals();g.computeBoundingSphere();return g;}
  function subset(source,start,count){const g=new THREE.BufferGeometry();for(const [name,a] of Object.entries(source.attributes)){const array=new a.array.constructor(a.array.slice(start*a.itemSize,(start+count)*a.itemSize));g.setAttribute(name,new THREE.BufferAttribute(array,a.itemSize,a.normalized));}g.computeBoundingSphere();return g;}
  class Target{
    constructor(snapshot,kind,meta,defer=false){this.kind=kind;this.meta=meta;this.root=snapshot.root;this.map=snapshot.map;this.ownedGeometry=new Set();this.ownedMaterial=new Set();this.visual=new Map();this.attachments=new Map();this.jobs=[];this.disposed=false;this.seed=meta.seed||1;this.root.updateMatrixWorld(true);this.originals=[];this.parts=[];
      this.build=this.buildSteps();if(!defer){for(const ignored of this.build){/* synchronous API for non-rendering callers */}this.finish();}
    }
    *buildSteps(){if(this.kind==='building')yield* this.building();else if(this.kind==='plant')yield* this.plant();else if(this.kind==='infantry')this.infantry();else this.rock();}
    finish(){this.meta.adapterContractVersion='building-adapter-2';
      this.model=new R.MaterialModel(this.parts,this.seed);if(this.kind==='building'){const initialParts=this.model.parts.size;this.model.maxParts=Math.max(initialParts+1,Math.min(2048,Math.max(256,initialParts*3)));this.model.supportPolicy=cachedSupportPolicy();}this.model.listeners.add(e=>this.event(e));
    }
    register(part,visual){this.parts.push(part);this.visual.set(part.id,visual);part.renderMaterial=visual?.material;}
    makeVisual(faces,source){const g=geometry(faces),inverse=new THREE.Matrix4().copy(this.root.matrixWorld).invert();g.applyMatrix4(inverse);this.ownedGeometry.add(g);
      let m=source?.material?materials(source.material)[0]:null;if(!m||m.vertexColors){m=m?m.clone():new THREE.MeshStandardMaterial({color:this.kind==='rock'?0x77776e:0xaaa397,roughness:1});m.vertexColors=false;this.ownedMaterial.add(m);}const mesh=new THREE.Mesh(g,m);mesh.castShadow=true;mesh.receiveShadow=true;this.root.add(mesh);return mesh;
    }
    releaseVisual(id){const mesh=this.visual.get(id);this.visual.delete(id);if(!mesh)return;mesh.visible=false;const parent=mesh.parent;parent?.remove(mesh);if(parent?.userData.destructionBody&&!parent.children.length)parent.parent?.remove(parent);
      mesh.traverse(o=>{if(this.ownedGeometry.delete(o.geometry))o.geometry.dispose();for(const m of materials(o.material))if(m&&this.ownedMaterial.has(m)&&![...this.visual.values()].some(v=>v?.material===m)){this.ownedMaterial.delete(m);m.dispose();}});
    }
    removeAttachments(id){const attached=this.attachments.get(id);if(attached)for(const mesh of attached){mesh.visible=false;mesh.parent?.remove(mesh);}this.attachments.delete(id);}
    transferAttachments(part,pieces){const attached=this.attachments.get(part.id);if(!attached)return;this.attachments.delete(part.id);for(const mesh of attached){const center=new THREE.Box3().setFromObject(mesh).getCenter(new THREE.Vector3()).toArray(),owner=pieces.find(p=>p.bounds.min.every((x,i)=>center[i]>=x-.2&&center[i]<=p.bounds.max[i]+.2));if(owner){if(!this.attachments.has(owner.id))this.attachments.set(owner.id,new Set());this.attachments.get(owner.id).add(mesh);}else{mesh.visible=false;mesh.parent?.remove(mesh);}}}
    *building(){const structural=new Set(['foundation','wall','partition','party-wall','structure','floor','slab','roof','stair','gable']),physical=new Set([...structural,'siding','window','door']),meshes=[],decorations=[];
      this.root.traverse(mesh=>{if(mesh.isMesh)meshes.push(mesh);});let extraSections=0;
      for(const mesh of meshes){const data=mesh.userData.building;if(!data||!mesh.visible)continue;if(!physical.has(data.category)){decorations.push(mesh);continue;}
    const category=data.category==='siding'?'wall':data.category,spec=this.meta.plan?.spec||{},wallMaterial=String(data.category==='siding'?(this.meta.material||spec.material||data.material):data.material||this.meta.material||spec.material||'concrete').toLowerCase(),structure=String(spec.structuralSystem||this.meta.structure||''),declared=mapMaterial(wallMaterial),material=category==='window'?'glass':category==='door'?(declared==='steel'?'steel':'wood'):['foundation','floor','slab','stair'].includes(category)?'concrete':category==='structure'?(/TIMBER|WOOD/i.test(structure)?'wood':/STEEL/i.test(structure)?'steel':'concrete'):category==='roof'?({metal:'steel',corrugated:'steel',slate:'rock',shingle:'wood',thatch:'wood',clay:'brick'}[spec.roofCovering||this.meta.roofCovering]||'concrete'):declared;
        const faces=solidFaces(mesh),id=String(data.id??data.componentId??('component-'+this.parts.length)),pieces=sections(faces,category,Math.min(16,Math.max(1,1024-extraSections))),split=pieces.length>1;extraSections+=pieces.length-1;
        if(split)mesh.visible=false;
        for(let i=0;i<pieces.length;i++){const piece=pieces[i],bounds=S.bounds(piece),part={id:split?id+'/panel-'+i:id,faces:piece,material,layers:wallLayers(this.meta,material,category,data),assemblyId:data.assemblyId||`${category}-${id}`,physicalSolidId:data.physicalSolidId||id,materialFrame:data.localMaterialFrame?Object.fromEntries(Object.entries(data.localMaterialFrame).map(([key,value])=>[key,key==='origin'?new THREE.Vector3(...value).applyMatrix4(mesh.matrixWorld).toArray():new THREE.Vector3(...value).transformDirection(mesh.matrixWorld).toArray()])):null,materialField:data.materialField,grainDirection:grainDirection(bounds,category,material),category,buildingPanel:true,structuralGraph:data.category!=='siding',anchored:category==='foundation',supports:[],sourceId:data.sourceId,wallId:data.wallId,roofId:data.roofId,floor:data.floor,structural:data.category==='siding'||!!data.structural,suspendedFrame:category==='structure'&&!!this.meta.plan&&bounds.min[1]>(data.floor||0)*this.meta.plan.spec.storeys.floorHeight+.2};validateAssembly(part,{seed:this.seed});this.register(part,split?this.makeVisual(piece,mesh):mesh);part.bounds=bounds;yield;}
      }
      for(const p of this.parts)p.bounds=S.bounds(p.faces);
      for(const p of this.parts){if(p.anchored)continue;for(const q of this.parts)if(p!==q&&supportsFromBelow(p,q))p.supports.push(q.id);yield;}
      calibrateSupport(this.parts);
      // Small visual details follow their nearest structural owner. They never
      // remain as floating facade skins after that section is destroyed.
      const details=new Map((this.meta.plan?.details||[]).map(d=>[d.id,d]));
      for(const mesh of decorations){const data=mesh.userData.building,detail=details.get(data.sourceId),wallId=data.wallId||detail?.wallId,roofId=data.roofId||detail?.roofFaceId,box=new THREE.Box3().setFromObject(mesh),center=box.getCenter(new THREE.Vector3()).toArray();
        let candidates=this.parts.filter(p=>wallId?p.wallId===wallId:roofId?p.roofId===roofId:p.sourceId===data.sourceId);if(!candidates.length)candidates=this.parts.filter(p=>p.category!=='foundation'&&(!Number.isFinite(data.floor)||p.floor===data.floor));if(!candidates.length)candidates=this.parts;const structuralCandidates=candidates.filter(p=>p.category==='wall'||p.category==='party-wall'||p.category==='roof'||p.category==='gable');if(!wallId&&!roofId&&structuralCandidates.length)candidates=structuralCandidates;
        const distance=p=>p.bounds.min.reduce((sum,x,i)=>sum+Math.max(x-center[i],0,center[i]-p.bounds.max[i])**2,0),owner=candidates.reduce((best,p)=>!best||distance(p)<distance(best)?p:best,null);
        if(owner){if(!this.attachments.has(owner.id))this.attachments.set(owner.id,new Set());this.attachments.get(owner.id).add(mesh);}yield;
      }
      // Windows and doors remain physical collision parts, but their visual meshes
      // also belong to the enclosing facade so replacement/removal retires them.
      for(const detailPart of this.parts.filter(p=>p.category==='window'||p.category==='door')){
        const visual=this.visual.get(detailPart.id),walls=this.parts.filter(p=>p.category==='wall'||p.category==='party-wall'),center=detailPart.bounds.min.map((x,i)=>(x+detailPart.bounds.max[i])*.5),distance=p=>p.bounds.min.reduce((sum,x,i)=>sum+Math.max(x-center[i],0,center[i]-p.bounds.max[i])**2,0),owner=(walls.find(p=>detailPart.wallId&&p.wallId===detailPart.wallId)||walls.sort((a,b)=>distance(a)-distance(b))[0]);
        if(visual&&owner){if(!this.attachments.has(owner.id))this.attachments.set(owner.id,new Set());this.attachments.get(owner.id).add(visual);}
      }
      if(!this.parts.some(p=>(p.category==='wall'||p.category==='party-wall')&&this.attachments.has(p.id))){const wall=this.parts.find(p=>p.category==='wall'||p.category==='party-wall'),visual=[...this.visual.entries()].map(([id,m])=>[this.parts.find(p=>p.id===id),m]).find(([p])=>p&&p.category!=='wall'&&p.category!=='party-wall'&&p.category!=='foundation'&&p.category!=='floor')||[wall,wall&&this.visual.get(wall.id)];if(wall&&visual?.[1]){if(!this.attachments.has(wall.id))this.attachments.set(wall.id,new Set());this.attachments.get(wall.id).add(visual[1]);}}
      this.simplified=!this.meta.plan;
    }
    *plant(){const nodes=this.meta.skeleton?.nodes||[];if(!nodes.length){this.root.traverse(m=>{if(m.isMesh)this.register({id:'clump-'+this.parts.length,faces:boxFaces(m),material:'foliage',category:'plant'},m);});return;}
      for(const n of nodes){const axis=new THREE.Vector3(...V.sub(n.b,n.a)),length=axis.length(),rotation=new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0,1,0),axis.normalize()),center=V.mul(V.add(n.a,n.b),.5),matrix=new THREE.Matrix4().compose(new THREE.Vector3(...center),rotation,new THREE.Vector3(1,1,1));
        const r=Math.max(.003,n.r1),faces=S.box([-r,-length/2,-r],[r,length/2,r]).map(f=>f.map(p=>new THREE.Vector3(...p).applyMatrix4(matrix).applyMatrix4(this.root.matrixWorld).toArray()));this.register({id:'branch-'+n.id,parent:n.parent===null?null:'branch-'+n.parent,faces,material:'wood',grainDirection:V.unit(V.sub(n.b,n.a)),category:'plant'},null);yield;}
      this.root.traverse(m=>{if(m.isMesh)this.originals.push(m);});
    }
    *expandPlant(){if(this.expanded)return;this.expanded=true;const groups=new Map();for(const p of this.model.parts.values()){const g=new THREE.Group();g.visible=false;this.root.add(g);groups.set(p.id,g);this.visual.set(p.id,g);yield;}
      for(const source of this.originals){const ranges=source.geometry.userData.branchOwnership||[{id:0,start:0,count:source.geometry.attributes.position.count}];for(const range of ranges){if(!range.count)continue;const group=groups.get('branch-'+range.id);if(!group)continue;const g=subset(source.geometry,range.start,range.count);this.ownedGeometry.add(g);const mesh=new THREE.Mesh(g,source.material);mesh.applyMatrix4(source.matrix);group.add(mesh);yield;}}
      for(const source of this.originals)source.visible=false;for(const group of groups.values())group.visible=true;
    }
    rock(){this.root.traverse(mesh=>{if(!mesh.isMesh)return;const faces=DestructionConvexFaces(meshFaces(mesh).flat().map(p=>new THREE.Vector3(...p)));this.register({id:'rock-'+this.parts.length,faces,material:'rock',strengthScale:({basalt:1.4,granite:1.2,limestone:.55,sandstone:.4,slate:.65})[this.meta.species]||1,layering:this.meta.layering||0,assemblyId:'rock-'+this.parts.length,physicalSolidId:'rock-'+this.parts.length},mesh);});}
    infantry(){const {rig,schema}=this.meta;this.rig={root:this.map.get(rig.root),byName:Object.fromEntries(Object.entries(rig.byName).map(([id,bone])=>[id,this.map.get(bone)])),anatomy:rig.anatomy};this.schema={...schema,poseOwner:'ANIMATION',activePhysics:false};
      for(const d of schema.bodies){const matrix=R.RagdollSchema.bodyWorld(this.rig.byName[d.bone],d),half=d.halfExtents||[d.radius,d.halfCylinder+d.radius,d.radius],faces=S.box(half.map(v=>-v),half).map(f=>f.map(p=>new THREE.Vector3(...p).applyMatrix4(matrix).toArray()));this.register({id:d.id,faces,material:'tissue',category:'infantry'},null);}
      // Separate game-model armor shells; never count the body's interior as armor.
      const slots=this.meta.slots||{};for(const [slot,bodyId] of [['head','head'],['torsoArmor','chest']]){const item=slots[slot];if(!item)continue;const id=item.definitionId;if(slot==='head'&&!id.startsWith('helmet'))continue;const d=schema.byId[bodyId],half=d.halfExtents||[d.radius,d.halfCylinder+d.radius,d.radius],thickness=slot==='head'?.008:id==='heavy_armor'?.029:id==='plate_carrier'?.02:.01,matrix=R.RagdollSchema.bodyWorld(this.rig.byName[d.bone],d),outer=S.box(half.map(x=>-x-thickness),half.map(x=>x+thickness)),inner=S.planes(S.box(half.map(x=>-x),half));
        S.subtract(outer,inner).forEach((faces,i)=>this.register({id:'armor-'+slot+'-'+i,bodyId,bodyLocalFaces:faces,faces:faces.map(f=>f.map(p=>new THREE.Vector3(...p).applyMatrix4(matrix).toArray())),material:'armor',category:'equipment'},null));}
    }
    attach(physics){this.physics=physics;const retire=parts=>this.model.batch(()=>{for(const p of parts)if(this.model.parts.has(p.id))this.model.remove(p);});physics.onRetire=retire;physics.onSettle=retire;physics.attachRubble(this.model,this.root);physics.sync(this.model);}
    *replaceVisual(e,source){for(const p of e.pieces){if(!this.model.parts.has(p.id))continue;const mesh=this.makeVisual(p.faces,source);mesh.userData.destructionPartId=p.id;this.visual.set(p.id,mesh);if(p.detached&&p.pendingDetach)this.detachVisual(p,p.pendingDetach);yield;}}
    event(e){if(e.type==='replace'){
        if(e.part.dynamic){const motion=this.physics?.removePart(e.part.id);for(const p of e.pieces)p.inheritedMotion=motion;}
        const source={material:e.part.renderMaterial||this.visual.get(e.part.id)?.material};if(e.part.buildingPanel&&e.part.category==='wall')this.removeAttachments(e.part.id);else this.transferAttachments(e.part,e.pieces);this.releaseVisual(e.part.id);
        this.jobs.push(this.replaceVisual(e,source));
      }else if(e.type==='remove'){this.physics?.removePart(e.part.id);this.releaseVisual(e.part.id);this.removeAttachments(e.part.id);
      }else if(e.type==='detach'){if(e.part.buildingPanel)e.part.terminalDebris=true;this.removeAttachments(e.part.id);e.part.pendingDetach=e;if(this.visual.get(e.part.id))this.detachVisual(e.part,e);
      }else if(e.type==='branch'){if(this.originals.length)this.jobs.push(this.detachPlant(e));else for(const p of e.parts)this.detachVisual(p,e);
      }else if(e.type==='ragdoll'&&!this.physics.ragdoll){for(const p of this.model.parts.values())p.detached=true;this.model.reindex();this.physics.sync(this.model);this.physics.startRagdoll(this.rig,this.schema,V.mul(e.direction,Math.min(180,Math.sqrt(e.energy)*.15)));}
      else if(e.type==='contact-impulse'&&e.impulse){if(e.part.dynamic)this.physics.applyImpulse(e.part.bodyId||e.part.id,e.impulse,e.point);else{e.part.blastImpulse=e.impulse;e.part.blastPoint=e.point;}}
      else if(e.type==='impact'&&e.part.dynamic&&!e.resolvedImpulse)this.physics.impulsePart(e.part.bodyId||e.part.id,e.direction,e.energy,e.point);
      else if(e.type==='blast'){const impulse=V.mul(e.direction,e.impulse);if(e.part.dynamic)this.physics.applyImpulse(e.part.bodyId||e.part.id,impulse,e.point);else{e.part.blastImpulse=impulse;e.part.blastPoint=e.point?.slice?.()||null;}}
      if(['replace','detach','branch','remove'].includes(e.type))this.physicsDirty=true;
    }
    detachVisual(part,e){const visual=this.visual.get(part.id);if(!visual||part.physicsStarted||!this.physics)return;part.physicsStarted=true;if(part.buildingPanel)visual.traverse(o=>{if(o.isMesh)o.raycast=()=>{};});const impulse=part.blastImpulse||V.mul(e.direction,Math.min(2000,Math.sqrt(2*e.energy))),point=part.blastPoint||e.point||null,useCheap=part.buildingPanel&&e.energy<100000,record=useCheap?this.physics.cheapDetach(part,visual,impulse,[part],point):this.physics.detach(part,visual,impulse,[part],point);part.blastImpulse=null;part.blastPoint=null;if(record&&part.inheritedMotion){const inherited=part.inheritedMotion;if(record.body){const body=record.body,v=body.linvel();body.setLinvel({x:v.x+inherited.linear.x,y:v.y+inherited.linear.y,z:v.z+inherited.linear.z},true);body.setAngvel({x:v.x+inherited.angular.x,y:v.y+inherited.angular.y,z:v.z+inherited.angular.z},true);}else if(record.cheap){record.velocity[0]+=inherited.linear.x??inherited.linear[0]??0;record.velocity[1]+=inherited.linear.y??inherited.linear[1]??0;record.velocity[2]+=inherited.linear.z??inherited.linear[2]??0;record.angular=[inherited.angular.x??inherited.angular[0]??0,inherited.angular.y??inherited.angular[1]??0,inherited.angular.z??inherited.angular[2]??0];}part.inheritedMotion=null;}else if(!record)this.model.remove(part);}
    *detachPlant(e){yield* this.expandPlant();const group=new THREE.Group();this.root.add(group);for(const p of e.parts){const visual=this.visual.get(p.id);if(visual)group.attach(visual);yield;}const bounds=new THREE.Box3().setFromObject(group);if(bounds.isEmpty())return;const inherited=this.physics.extractParts(e.parts),compound={...e.part,faces:S.box(bounds.min.toArray(),bounds.max.toArray()),bounds:{min:bounds.min.toArray(),max:bounds.max.toArray()}},record=this.physics.detach(compound,group,V.mul(e.direction,Math.min(1000,Math.sqrt(2*e.energy))),e.parts,e.point||null);if(record?.body){const body=record.body,velocity=body.linvel();body.setLinvel({x:velocity.x+inherited.linear[0],y:velocity.y+inherited.linear[1],z:velocity.z+inherited.linear[2]},true);body.setAngvel({x:inherited.angular[0],y:inherited.angular[1],z:inherited.angular[2]},true);}else if(record?.cheap){record.velocity[0]+=inherited.linear[0];record.velocity[1]+=inherited.linear[1];record.velocity[2]+=inherited.linear[2];record.angular=inherited.angular.slice();}else{group.parent?.remove(group);for(const p of e.parts)this.visual.delete(p.id);}for(const p of e.parts)p.damage=0;this.model.reindex();}
    update(budget=2){const start=performance.now();while(this.jobs.length&&performance.now()-start<budget){const job=this.jobs[0];if(typeof job==='function'){this.jobs.shift();job();}else if(job.next().done)this.jobs.shift();}if(this.physicsDirty&&this.physics){this.physicsDirty=false;this.physics.sync(this.model);}this.physics?.enforceActiveLimit();this.physics?.rubble?.flush();}
    dispose(){if(this.disposed)return;this.disposed=true;this.jobs.length=0;this.build?.return();this.model?.dispose();for(const g of this.ownedGeometry)g.dispose();for(const m of this.ownedMaterial)m.dispose();this.ownedGeometry.clear();this.ownedMaterial.clear();this.visual.clear();this.attachments.clear();if(this.physics){this.physics.onRetire=null;this.physics.onSettle=null;}this.root.traverse(o=>{if(o.isSkinnedMesh&&o.skeleton)o.skeleton.dispose();});this.root.parent?.remove(this.root);}
  }
  function validateAssembly(part,context={}){const frame=part?.materialFrame||part?.localMaterialFrame;if(!part?.assemblyId||!part?.physicalSolidId)throw new Error(`Missing assembly: seed ${context.seed??'-'} component ${part?.sourceId||part?.id||'-'} assembly ${part?.assemblyId||'-'} category ${part?.category||'-'}`);if(frame){for(const key of ['origin','normal','tangentU','tangentV'])if(!Array.isArray(frame[key])||frame[key].length!==3||!frame[key].every(Number.isFinite))throw new Error(`Invalid material frame: seed ${context.seed??'-'} component ${part.sourceId||part.id} assembly ${part.assemblyId} category ${part.category}`);const dot=(a,b)=>Math.abs(a[0]*b[0]+a[1]*b[1]+a[2]*b[2]),len=a=>Math.hypot(...a);if([frame.normal,frame.tangentU,frame.tangentV].some(v=>Math.abs(len(v)-1)>1e-4)||dot(frame.normal,frame.tangentU)>1e-4||dot(frame.normal,frame.tangentV)>1e-4||dot(frame.tangentU,frame.tangentV)>1e-4)throw new Error(`Non-orthogonal material frame: seed ${context.seed??'-'} component ${part.sourceId||part.id} assembly ${part.assemblyId} category ${part.category}`);}let sum=0;for(const layer of part.layers||[]){if(!(layer.thickness>0))throw new Error(`Invalid layer: seed ${context.seed??'-'} component ${part.sourceId||part.id} assembly ${part.assemblyId} category ${part.category}`);sum+=layer.thickness;}if(part.thickness&&Math.abs(sum-part.thickness)>1e-4)throw new Error(`Layer sum mismatch: seed ${context.seed??'-'} component ${part.sourceId||part.id} assembly ${part.assemblyId} category ${part.category}`);return true;}
  // Capture transforms synchronously. Clone resources only on the first modification.
  R.DestructionAdapters={
    capture(source,kind,meta={}){source.updateMatrixWorld(true);const snapshot=cloneRoot(source);return {kind,meta,create(defer=false){return new Target(cloneRoot(snapshot.root),kind,{...meta,rig:meta.rig?{...meta.rig,root:snapshot.map.get(meta.rig.root),byName:Object.fromEntries(Object.entries(meta.rig.byName).map(([id,b])=>[id,snapshot.map.get(b)]))}:undefined,gear:meta.gear?snapshot.map.get(meta.gear):undefined},defer);},async createAsync(cancelled){const target=this.create(true);try{let done=false;while(!done){if(cancelled()){const e=Error('Target preparation cancelled');e.name='AbortError';throw e;}const start=performance.now();do{done=target.build.next().done;}while(!done&&performance.now()-start<2);if(!done)await new Promise(resolve=>setTimeout(resolve,0));}target.finish();return target;}catch(e){target.dispose();throw e;}},dispose(){snapshot.root.traverse(o=>{if(o.isSkinnedMesh)o.skeleton.dispose();});}};},
    infantry(unit){return this.capture(unit.root,'infantry',{seed:unit.seed,rig:unit.rig,schema:unit.physicsSchema,gear:unit.model.gearMesh,slots:JSON.parse(JSON.stringify(unit.equipment.slots))});},
    plant(model){return this.capture(model.root,'plant',{seed:model.genome.seed,skeleton:model.skeleton});},
    rock(model){return this.capture(model.root,'rock',{seed:model.genome.seed,layering:model.genome.genes.layering,species:model.genome.species});},
    building(model){return this.capture(model.root,'building',{seed:model.manifest.seed,plan:model.plan,material:model.manifest.material,structure:model.manifest.structure?.id,roofCovering:model.manifest.roofCovering});},
    buildingRuntime(runtime){
      if(!runtime?.plan)throw new TypeError('buildingRuntime requires a BuildingRuntime');
      return {kind:'building-runtime',runtime,plan:runtime.plan,create(){return runtime;},createAsync(){return Promise.resolve(runtime);},dispose(){runtime.dispose?.();}};
    },validateAssembly
  };
  R.DestructionTarget=Target;
})();
