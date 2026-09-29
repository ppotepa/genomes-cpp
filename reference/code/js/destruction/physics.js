(function(){
  'use strict';
  const R=globalThis.RTS,S=R.DestructionSolid,V=S.V,to=v=>({x:v[0],y:v[1],z:v[2]});
  function rapier(){
    const exported=globalThis.RapierCompat||globalThis.Rapier||globalThis.RAPIER;
    const candidates=[exported,exported?.default,exported?.RapierCompat,exported?.default?.RapierCompat];
    const P=candidates.find(value=>value&&typeof value.World==='function');
    if(P&&P!==exported)globalThis.RapierCompat=P;
    if(!P||typeof P.World!=='function'||typeof P.RigidBodyDesc!=='function'||typeof P.ColliderDesc!=='function')throw new Error('RapierCompat nie został załadowany lub nie został zainicjalizowany przed inicjalizacją fizyki.');
    return P;
  }
  class DestructionPhysics{
    static async init(){
      if(!this.ready)this.ready=(async()=>{
        if(globalThis.RapierReady){await globalThis.RapierReady();return rapier();}
        const P=rapier();await P.init();return P;
      })().catch(e=>{this.ready=null;throw e;});
      await this.ready;
    }
    constructor(){const P=this.P=rapier();this.world=new P.World({x:0,y:-9.80665,z:0});this.world.timestep=1/60;this.static=new Map();this.debris=[];this.cheapDebris=[];this.maxDebris=512;this.maxActiveDebris=128;this.maxCheapDebris=1024;this.settleGraceTicks=60;this.groundHeightAt=()=>0;this.ragdoll=null;this.ragdolls=new Map();this.cheapPacketMesh=null;this.cheapPacketGeometry=null;this.cheapPacketMaterial=null;this.metrics={unrepresentableHulls:0,heroDemotions:0,cheapSpawned:0,cheapSettled:0,rubbleBakes:0,directDeposits:0};
      this.world.createCollider(P.ColliderDesc.cuboid(2200,.5,2200).setTranslation(0,-.5,0));
      this.player=this.world.createRigidBody(P.RigidBodyDesc.kinematicPositionBased().setTranslation(0,.9,10));this.playerCollider=this.world.createCollider(P.ColliderDesc.capsule(.6,.28),this.player);
      this.controller=this.world.createCharacterController(.015);this.controller.enableAutostep(.25,.15,true);this.controller.enableSnapToGround(.2);this.vy=0;
      this.rubble=new R.RubbleField({cellSize:.5,tileSize:4,maxTiles:256,maxChunkInstances:1024});this.rubble.attachPhysics(this.world,this.P);this.rubble.onEject=packet=>this.spawnCheapPacket(packet);
    }
    setGroundHeightProvider(provider){this.groundHeightAt=typeof provider==='function'?provider:()=>0;this.rubble?.setBaseHeightProvider(this.groundHeightAt);return this;}
    representationAccounting(){
      const heroParts=new Set(),cheapParts=new Set(),materials={hero:{},cheap:{},baked:this.rubble.materialVolumes()};let heroVolume=0,cheapVolume=0;
      for(const d of this.debris)for(const v of d.volumes){const p=v.part;if(heroParts.has(p.id))continue;heroParts.add(p.id);const q=Math.max(0,p.volume||0);heroVolume+=q;materials.hero[p.material]=(materials.hero[p.material]||0)+q;}
      for(const d of this.cheapDebris){if(d.packet){cheapVolume+=d.volume||0;materials.cheap[d.material]=(materials.cheap[d.material]||0)+(d.volume||0);continue;}for(const p of d.parts||[]){if(cheapParts.has(p.id))continue;cheapParts.add(p.id);const q=Math.max(0,p.volume||0);cheapVolume+=q;materials.cheap[p.material]=(materials.cheap[p.material]||0)+q;}}
      const bakedVolume=this.rubble.totalVolume();return {heroVolume,cheapVolume,bakedVolume,totalVolume:heroVolume+cheapVolume+bakedVolume,heroBodies:this.debris.length,cheapRecords:this.cheapDebris.length,rubbleTiles:this.rubble.tiles.size,materials};
    }
    attachRubble(model,parent){this.model=model;this.rubble.attachModel(model);this.rubble.attachVisual(parent);if(typeof THREE!=='undefined'&&this.rubble.root&&!this.cheapPacketMesh){this.cheapPacketGeometry=new THREE.BoxGeometry(.18,.12,.16);this.cheapPacketMaterial=new THREE.MeshStandardMaterial({color:0xffffff,roughness:1,vertexColors:true});this.cheapPacketMesh=new THREE.InstancedMesh(this.cheapPacketGeometry,this.cheapPacketMaterial,Math.min(1024,this.maxCheapDebris));this.cheapPacketMesh.count=0;this.cheapPacketMesh.castShadow=true;this.cheapPacketMesh.frustumCulled=false;this.rubble.root.add(this.cheapPacketMesh);}return this.rubble;}
    visualGroup(visual,center){
      if(!visual||typeof THREE==='undefined')return {visual,parentInverse:null,parentRotation:null};const parent=visual.parent,group=new THREE.Group();group.userData.destructionBody=true;group.userData.cheapDebris=false;parent?.add(group);group.position.copy(new THREE.Vector3(...center));if(parent){parent.updateMatrixWorld(true);parent.worldToLocal(group.position);group.quaternion.copy(parent.getWorldQuaternion(new THREE.Quaternion())).invert();}group.updateMatrixWorld(true);group.attach(visual);return {visual:group,parentInverse:group.parent?new THREE.Matrix4().copy(group.parent.matrixWorld).invert():null,parentRotation:group.parent?group.parent.getWorldQuaternion(new THREE.Quaternion()).invert():null};
    }
    combinedBounds(parts){const faces=parts.flatMap(p=>p.faces);return faces.length?S.bounds(faces):{min:[0,0,0],max:[0,0,0]};}
    settleParts(parts,options={}){
      if(!parts?.length)return 0;let deposited=0;for(const p of parts)deposited+=this.rubble.depositPart(p,options);if(deposited>0){this.rubble.flush();this.metrics.rubbleBakes++;}return deposited;
    }
    finalizeSettledParts(parts,meta){if(!parts?.length)return;if(this.onSettle)this.onSettle(parts,meta);else if(this.model)this.model.batch(()=>{for(const p of parts)if(this.model.parts.has(p.id))this.model.remove(p);});}
    directDeposit(parts,visual,options={}){
      const deposited=this.settleParts(parts,options);visual?.parent?.remove(visual);for(const p of parts){p.dynamic=false;p.cheapDebris=false;p.velocity=[0,0,0];}this.metrics.directDeposits++;this.finalizeSettledParts(parts,{deposited,direct:true});return {baked:true,deposited,parts};
    }
    cheapVisualForPacket(){return null;}
    updateCheapPacketMesh(){
      const mesh=this.cheapPacketMesh;if(!mesh||typeof THREE==='undefined')return;const matrix=new THREE.Matrix4(),quat=new THREE.Quaternion(),scale=new THREE.Vector3(),pos=new THREE.Vector3(),colors={brick:0x8b6651,concrete:0x8b8c86,rock:0x73736d,wood:0x66533b,steel:0x555b5d,armor:0x484e52,glass:0xaab9b8};let count=0;
      for(const d of this.cheapDebris){if(!d.packet||count>=mesh.instanceMatrix.count)continue;const size=Math.max(.04,Math.min(.24,Math.cbrt(d.volume)*.75));pos.set(...d.position);quat.setFromEuler(new THREE.Euler(d.age*2.7,d.age*4.1,d.age*1.9));scale.set(size,size*.7,size*.85);matrix.compose(pos,quat,scale);mesh.setMatrixAt(count,matrix);if(mesh.setColorAt)mesh.setColorAt(count,new THREE.Color(colors[d.material]||0x7d756b));count++;}
      mesh.count=count;mesh.instanceMatrix.needsUpdate=true;if(mesh.instanceColor)mesh.instanceColor.needsUpdate=true;
    }
    spawnCheapPacket(packet){
      if(!packet||packet.volume<=0)return null;if(this.cheapDebris.length>=this.maxCheapDebris){this.rubble.deposit(packet.position,packet.volume,packet.material,{impactSpeed:V.length(packet.velocity||[0,0,0])});this.rubble.flush();this.metrics.directDeposits++;return {baked:true};}
      const visual=this.cheapVisualForPacket(packet),record={cheap:true,packet:true,parts:[],visual,position:packet.position.slice(),velocity:(packet.velocity||[0,0,0]).slice(),angular:[0,0,0],volume:packet.volume,material:packet.material,age:0,half:Math.max(.025,Math.cbrt(packet.volume)*.35)};this.cheapDebris.push(record);this.metrics.cheapSpawned++;return record;
    }
    cheapDetach(part,visual,impulse,parts=[part],point=null,kinematics=null){
      if(parts.some(p=>p.preferHeroDebris||p.buildingPanel&&p.category==='foundation'&&!kinematics&&((this.debris.length>=this.maxDebris&&this.debris.some(d=>d.body.isFixed()||d.body.isSleeping()))||this.activeDebrisCount()<this.maxActiveDebris&&this.debris.length<this.maxDebris)||p.buildingPanel&&['wall','party-wall','roof','gable'].includes(p.category)&&!p.damageField?.cells?.length&&!p.damageField?.holes?.length||p.damageField?.cells?.some(c=>(c.crush||0)>50000)))return this.detach(part,visual,impulse,parts,point,true);
      const bounds=this.combinedBounds(parts),center=V.mul(V.add(bounds.min,bounds.max),.5),mass=Math.max(.001,parts.reduce((s,p)=>s+(p.volume||0)*(R.DestructionMaterials[p.material]?.density||1000),0)),velocity=kinematics?.linear?kinematics.linear.slice():V.mul(impulse,1/mass),angular=kinematics?.angular?kinematics.angular.slice():point?V.mul(V.cross(V.sub(point,center),impulse),1/Math.max(.01,mass*.4)): [0,0,0];
      if(this.cheapDebris.length>=this.maxCheapDebris)return this.directDeposit(parts,visual,{impactSpeed:V.length(velocity),spreadRadius:Math.max(.45,Math.cbrt(parts.reduce((s,p)=>s+(p.volume||0),0))*1.4)});
      const wrapped=this.visualGroup(visual,center);if(wrapped.visual)wrapped.visual.userData.cheapDebris=true;for(const p of parts){p.dynamic=true;p.cheapDebris=true;p.velocity=velocity.slice();}
      const record={cheap:true,packet:false,localParts:parts.map(p=>({part:p,faces:p.faces.map(f=>f.map(v=>V.sub(v,center))),frame:p.materialFrame?Object.fromEntries(Object.entries(p.materialFrame).map(([k,v])=>[k,k==='origin'?V.sub(v,center):v.slice()])):null})),orientation:[0,0,0,1],part,parts,visual:wrapped.visual,parentInverse:wrapped.parentInverse,parentRotation:wrapped.parentRotation,position:center.slice(),velocity,angular,age:0,half:Math.max(.03,(bounds.max[1]-bounds.min[1])*.5)};this.cheapDebris.push(record);this.metrics.cheapSpawned++;return record;
    }
    updateCheap(){
      if(!this.cheapDebris.length)return;const dt=this.world.timestep||1/60,settled=[];for(const d of this.cheapDebris){d.age+=dt;d.velocity[1]-=9.80665*dt;for(let i=0;i<3;i++)d.position[i]+=d.velocity[i]*dt;const ground=this.rubble.surfaceHeightAt(d.position[0],d.position[2]);if(d.position[1]-d.half<=ground&&d.velocity[1]<=0){d.position[1]=ground+d.half;settled.push(d);continue;}if(d.localParts){d.orientation=R.ProjectileMath.integrate(d.orientation,d.angular,dt);for(const local of d.localParts){const p=local.part;p.faces=local.faces.map(f=>f.map(v=>V.add(R.ProjectileMath.rotate(d.orientation,v),d.position)));p.planes=S.planes(p.faces);p.bounds=S.bounds(p.faces);p.velocity=d.velocity.slice();p.linearVelocity=d.velocity.slice();p.angularVelocity=d.angular.slice();p.centerOfMass=d.position.slice();if(local.frame)p.materialFrame=Object.fromEntries(Object.entries(local.frame).map(([k,v])=>[k,k==='origin'?V.add(R.ProjectileMath.rotate(d.orientation,v),d.position):R.ProjectileMath.rotate(d.orientation,v)]));}}
        if(d.visual&&typeof THREE!=='undefined'){const pos=new THREE.Vector3(...d.position);if(d.parentInverse)pos.applyMatrix4(d.parentInverse);d.visual.position.copy(pos);if(d.orientation){d.visual.quaternion.fromArray(d.orientation);if(d.parentRotation)d.visual.quaternion.premultiply(d.parentRotation);}}}
      if(this.model)this.model.reindexMoving();
      if(settled.length)this.settleCheapBatch(settled);else this.updateCheapPacketMesh();
    }
    settleCheap(record){return this.settleCheapBatch([record]);}
    settleCheapBatch(records){
      const valid=records.filter(r=>this.cheapDebris.includes(r));if(!valid.length)return 0;const allParts=[];
      for(const record of valid){const speed=V.length(record.velocity),parts=record.parts||[];if(record.packet)this.rubble.deposit(record.position,record.volume,record.material,{impactSpeed:speed});else for(const p of parts)this.rubble.deposit(record.position,p.volume||0,p.material,{impactSpeed:speed,spreadRadius:Math.max(.4,Math.cbrt(p.volume||.01)*1.45)});allParts.push(...parts);}
      this.rubble.flush();
      for(const record of valid){const i=this.cheapDebris.indexOf(record);if(i>=0)this.cheapDebris.splice(i,1);record.visual?.parent?.remove(record.visual);this.metrics.cheapSettled++;this.rubble.metrics.cheapSettles++;for(const p of record.parts||[]){p.dynamic=false;p.cheapDebris=false;p.velocity=[0,0,0];}}
      if(allParts.length)this.finalizeSettledParts(allParts,{cheap:true,batch:valid.length});this.updateCheapPacketMesh();return valid.length;
    }
    settleHero(record){return this.settleHeroBatch([record]);}
    settleHeroBatch(records){
      const valid=records.filter(r=>this.debris.includes(r));if(!valid.length)return 0;const allParts=[];
      for(const record of valid){const parts=record.volumes.map(v=>v.part),v=record.body.linvel(),speed=Math.hypot(v.x,v.y,v.z);for(const p of parts)this.rubble.depositPart(p,{impactSpeed:speed});allParts.push(...parts);}
      // Build the new static pile collision before retiring any old body.
      this.rubble.flush();
      for(const record of valid){const i=this.debris.indexOf(record);if(i>=0)this.debris.splice(i,1);this.world.removeRigidBody(record.body);record.visual?.parent?.remove(record.visual);for(const {part} of record.volumes){part.dynamic=false;part.velocity=[0,0,0];}this.metrics.rubbleBakes++;}
      if(allParts.length)this.finalizeSettledParts(allParts,{hero:true,batch:valid.length});return valid.length;
    }
    demote(record){
      const i=this.debris.indexOf(record);if(i<0)return null;const v=record.body.linvel(),a=record.body.angvel(),position=record.body.translation(),parts=record.volumes.map(x=>x.part);this.debris.splice(i,1);this.world.removeRigidBody(record.body);this.metrics.heroDemotions++;const cheap=this.cheapDetach(record.part,record.visual,[0,0,0],parts,null,{linear:[v.x,v.y,v.z],angular:[a.x,a.y,a.z]});if(cheap?.cheap){cheap.position=[position.x,position.y,position.z];}return cheap;
    }
    impulse(body,impulse,point=null){
      if(point&&typeof body.applyImpulseAtPoint==='function'){body.applyImpulseAtPoint(to(impulse),to(point),true);return;}
      body.applyImpulse(to(impulse),true);
      if(point&&typeof body.applyTorqueImpulse==='function'){const p=body.translation(),r=[point[0]-p.x,point[1]-p.y,point[2]-p.z],torque=V.cross(r,impulse);body.applyTorqueImpulse(to(torque),true);}
    }
    collider(part,center=[0,0,0]){const vertices=[];for(const face of part.faces)for(const p of face)vertices.push(...V.sub(p,center));return this.P.ColliderDesc.convexHull(new Float32Array(vertices));}
    createPartCollider(part,center=[0,0,0],body){const desc=this.collider(part,center);if(!desc)return null;if(body)desc.setDensity(R.DestructionMaterials[part.material].density);
      // Rapier's JS descriptor may exist while its Float32 native hull is null
      // for a near-coplanar remnant. Validate the actual native shape and pass
      // that same allocation to createCollider, which takes care of freeing it.
      const shape=desc.shape,convert=shape.intoRaw,raw=convert.call(shape);if(!raw){this.metrics.unrepresentableHulls++;return null;}shape.intoRaw=function(){this.intoRaw=convert;return raw;};return this.world.createCollider(desc,body);
    }
    sync(model){this.model=model;if(this.rubble.model!==model)this.rubble.attachModel(model);for(const [id,c] of this.static)if(!model.parts.has(id)||model.parts.get(id).detached||model.parts.get(id).dynamic){this.world.removeCollider(c,true);this.static.delete(id);}const dust=[];for(const p of model.parts.values())if(!p.indestructible&&!p.detached&&!p.dynamic&&!this.static.has(p.id)){const collider=this.createPartCollider(p);if(collider)this.static.set(p.id,collider);else dust.push(p);}if(dust.length){for(const p of dust){const center=V.mul(V.add(p.bounds.min,p.bounds.max),.5);this.rubble.depositMicro(center,p.volume||0,p.material||'concrete');}this.rubble.flush();model.batch(()=>{for(const p of dust)model.remove(p);});}}
    activeDebrisCount(){let count=0;for(const d of this.debris)if(d.body.isDynamic()&&!d.body.isSleeping())count++;return count;}
    enforceActiveLimit(){while(this.activeDebrisCount()>this.maxActiveDebris){let awake=0;for(const d of this.debris.slice())if(d.body.isDynamic()&&!d.body.isSleeping()&&++awake>this.maxActiveDebris)this.retire(d);}}
    retire(record){if(!record)return;return this.demote(record);}
    detach(part,visual,impulse,parts=[part],point=null,forceHero=false){if(this.retiring)return null;const c=this.static.get(part.id);if(c){this.world.removeCollider(c,true);this.static.delete(part.id);}
      // Overflow degrades to the cheap airborne representation instead of deleting material.
      if(this.activeDebrisCount()>=this.maxActiveDebris&&!forceHero)return this.cheapDetach(part,visual,impulse,parts,point);
      const center=V.mul(V.add(part.bounds.min,part.bounds.max),.5);
      if(this.debris.length>=this.maxDebris){const settled=this.debris.find(d=>d.body.isFixed()||d.body.isSleeping());if(settled)this.retire(settled);else return forceHero?this.directDeposit(parts,visual,{impactSpeed:V.length(impulse),spreadRadius:Math.max(.4,Math.cbrt(parts.reduce((s,q)=>s+(q.volume||0),0))*1.5)}):this.cheapDetach(part,visual,impulse,parts,point);}
      const body=this.world.createRigidBody(this.P.RigidBodyDesc.dynamic().setTranslation(...center).setCcdEnabled(true));
      for(const p of parts)if(!this.createPartCollider(p,center,body)){this.world.removeRigidBody(body);const mass=Math.max(.001,parts.reduce((s,q)=>s+(q.volume||0)*(R.DestructionMaterials[q.material]?.density||1000),0));return this.directDeposit(parts,visual,{impactSpeed:V.length(V.mul(impulse,1/mass)),spreadRadius:Math.max(.4,Math.cbrt(parts.reduce((s,q)=>s+(q.volume||0),0))*1.5)});}
      let parentInverse=null,parentRotation=null;if(visual){const wrapped=this.visualGroup(visual,center);visual=wrapped.visual;parentInverse=wrapped.parentInverse;parentRotation=wrapped.parentRotation;}
      const volumes=parts.map(p=>{p.dynamic=true;const faces=p.faces.map(f=>f.map(v=>V.sub(v,center)));return {part:p,materialFrame:p.materialFrame?Object.fromEntries(Object.entries(p.materialFrame).map(([k,v])=>[k,k==='origin'?V.sub(v,center):v.slice()])):null,faces,localPlanes:S.planes(faces),worldFaces:faces.map(f=>f.map(()=>[0,0,0])),worldPlanes:faces.map(()=>({n:[0,0,0],d:0}))};});this.impulse(body,impulse,point);
      // The captured target root is stationary. Cache its inverse rather than
      // walking the Three scene hierarchy twice for every rubble body per tick.
      const record={body,visual,part,volumes,transform:new THREE.Matrix4(),rotation:new THREE.Quaternion(),parentInverse,parentRotation,sleepTicks:0};this.debris.push(record);return record;
    }
    move(delta,jump,crouch){const playerPos=this.player.translation(),cost=this.rubble?.movementCostAt(playerPos.x,playerPos.z)||1;delta=[delta[0]/cost,delta[1]/cost,delta[2]/cost];const half=crouch?.32:.6,current=this.playerCollider.halfHeight();if(current!==half){const p=this.player.translation(),position={x:p.x,y:p.y+half-current,z:p.z},blocked=half>current&&this.world.intersectionWithShape(position,{x:0,y:0,z:0,w:1},new this.P.Capsule(half,.28),undefined,undefined,this.playerCollider,this.player);if(!blocked){this.playerCollider.setHalfHeight(half);this.player.setTranslation(position,true);}}
      if(jump&&this.controller.computedGrounded())this.vy=4.5;this.vy-=9.80665/60;
      this.controller.computeColliderMovement(this.playerCollider,{x:delta[0],y:this.vy/60,z:delta[2]},undefined,undefined,c=>c.handle!==this.playerCollider.handle);
      const m=this.controller.computedMovement(),p=this.player.translation();this.player.setNextKinematicTranslation({x:p.x+m.x,y:p.y+m.y,z:p.z+m.z});if(this.controller.computedGrounded())this.vy=Math.max(0,this.vy);
    }
    teleport(position){this.player.setTranslation(to(position),true);this.player.setNextKinematicTranslation(to(position));this.vy=0;}
    removePart(id){const collider=this.static.get(id);if(collider){this.world.removeCollider(collider,true);this.static.delete(id);}const cheapIndex=this.cheapDebris.findIndex(d=>d.parts?.some(p=>p.id===id));if(cheapIndex>=0){const d=this.cheapDebris[cheapIndex];this.cheapDebris.splice(cheapIndex,1);d.visual?.parent?.remove(d.visual);return {linear:{x:d.velocity[0],y:d.velocity[1],z:d.velocity[2]},angular:{x:d.angular[0],y:d.angular[1],z:d.angular[2]}};}const i=this.debris.findIndex(d=>d.part.id===id);if(i<0)return null;const d=this.debris[i],motion={linear:{...d.body.linvel()},angular:{...d.body.angvel()}};this.world.removeRigidBody(d.body);this.debris.splice(i,1);d.visual?.parent?.remove(d.visual);return motion;}
    applyImpulse(id,impulse,point=null){const cheap=this.cheapDebris.find(d=>d.parts?.some(p=>p.id===id));if(cheap){const mass=Math.max(.001,cheap.parts.reduce((sum,p)=>sum+p.volume*(R.DestructionMaterials[p.material]?.density||1000),0));cheap.velocity=V.add(cheap.velocity,V.mul(impulse,1/mass));if(point)cheap.angular=V.add(cheap.angular,V.mul(V.cross(V.sub(point,cheap.position),impulse),1/Math.max(.001,mass*cheap.half*cheap.half)));for(const p of cheap.parts){p.velocity=cheap.velocity.slice();p.linearVelocity=p.velocity.slice();p.angularVelocity=cheap.angular.slice();}return;}const d=this.debris.find(d=>d.volumes.some(v=>v.part.id===id));if(d){if(d.body.isFixed()||d.body.isSleeping()){if(this.activeDebrisCount()>=this.maxActiveDebris)return;if(d.body.isFixed())d.body.setBodyType(this.P.RigidBodyType.Dynamic,true);d.synced=false;}this.impulse(d.body,impulse,point);return;}const body=this.ragdoll?.bodies.get(id)?.body;if(body){this.impulse(body,impulse,point);}}
    extractParts(parts){const ids=new Set(parts.map(p=>p.id)),inherited={linear:[0,0,0],angular:[0,0,0]};
      for(const record of this.debris.slice()){if(!record.volumes.some(v=>ids.has(v.part.id)))continue;const v=record.body.linvel(),a=record.body.angvel();inherited.linear=[v.x,v.y,v.z];inherited.angular=[a.x,a.y,a.z];const remaining=record.volumes.filter(v=>!ids.has(v.part.id)).map(v=>v.part);
        this.world.removeRigidBody(record.body);this.debris.splice(this.debris.indexOf(record),1);
        if(remaining.length){const faces=remaining.flatMap(p=>p.faces),bounds=S.bounds(faces),compound={...remaining[0],id:'compound:'+remaining[0].id,faces:S.box(bounds.min,bounds.max),bounds},restored=this.detach(compound,record.visual,[0,0,0],remaining);if(restored){restored.body.setLinvel(v,true);restored.body.setAngvel(a,true);}}
      }return inherited;
    }
    impulsePart(id,direction,energy,point=null){const d=this.debris.find(d=>d.volumes.some(v=>v.part.id===id));if(d&&d.body.isDynamic())this.applyImpulse(id,V.mul(direction,Math.min(2000,Math.sqrt(Math.max(0,energy))*.1)),point);const body=this.ragdoll?.bodies.get(id)?.body;if(body){const impulse=V.mul(direction,Math.min(100,Math.sqrt(Math.max(0,energy))*.03));this.impulse(body,impulse,point);}}
    startRagdoll(rig,schema,impulse){if(this.ragdoll)return;const P=this.P,bodies=new Map(),links=[];rig.root.updateMatrixWorld(true);
      for(const d of schema.bodies){const matrix=R.RagdollSchema.bodyWorld(rig.byName[d.bone],d),pos=new THREE.Vector3(),q=new THREE.Quaternion();matrix.decompose(pos,q,new THREE.Vector3());const body=this.world.createRigidBody(P.RigidBodyDesc.dynamic().setTranslation(pos.x,pos.y,pos.z).setRotation(q).setCcdEnabled(true));const shape=d.shape==='box'?P.ColliderDesc.cuboid(...d.halfExtents):P.ColliderDesc.capsule(d.halfCylinder,d.radius);shape.setMass(d.massKg);this.world.createCollider(shape,body);bodies.set(d.id,{body,d});}
      for(const j of schema.joints){const a=bodies.get(j.parent).body,b=bodies.get(j.child).body;
        // Derive anchors from the captured pose to prevent a first-frame snap.
        const jointPos=rig.byName[j.bone].getWorldPosition(new THREE.Vector3()),local=body=>jointPos.clone().sub(new THREE.Vector3().copy(body.translation())).applyQuaternion(new THREE.Quaternion().copy(body.rotation()).invert());
        const joint=this.world.createImpulseJoint(P.JointData.spherical(local(a),local(b)),a,b,true);joint.setContactsEnabled(false);
        links.push({a,b,limits:j.limitsRadians,rest:new THREE.Quaternion().copy(a.rotation()).invert().multiply(new THREE.Quaternion().copy(b.rotation()))});
      }
      bodies.get('chest').body.applyImpulse(to(impulse),true);schema.poseOwner='PHYSICS';schema.activePhysics=true;this.ragdoll={rig,schema,bodies,links};
    }
    startUnitRagdoll(unitId,rig,schema,impulse,severedBody=null){if(this.ragdolls.has(unitId))return this.ragdolls.get(unitId);const previous=this.ragdoll;this.ragdoll=null;let removed=null;if(severedBody){removed=new Set([severedBody]);let changed=true;while(changed){changed=false;for(const b of schema.bodies)if(b.parent&&removed.has(b.parent)&&!removed.has(b.id)){removed.add(b.id);changed=true;}}}const filtered=removed?{...schema,bodies:schema.bodies.filter(b=>!removed.has(b.id)),joints:schema.joints.filter(j=>!removed.has(j.parent)&&!removed.has(j.child))}:schema;filtered.unitId=unitId;this.startRagdoll(rig,filtered,impulse);const created=this.ragdoll;this.ragdolls.set(unitId,created);this.ragdoll=previous;return created;}
    correctRagdollJoints(ragdoll){if(!ragdoll)return;for(const l of ragdoll.links){const parent=new THREE.Quaternion().copy(l.a.rotation()),base=parent.clone().multiply(l.rest),relative=base.clone().invert().multiply(new THREE.Quaternion().copy(l.b.rotation()));
      if(relative.w<0)relative.set(-relative.x,-relative.y,-relative.z,-relative.w);
      const hinge=!!l.limits.flexion,axis=hinge?new THREE.Vector3(1,0,0):new THREE.Vector3(0,1,0),projection=axis.clone().multiplyScalar(relative.x*axis.x+relative.y*axis.y+relative.z*axis.z),twist=new THREE.Quaternion(projection.x,projection.y,projection.z,relative.w).normalize(),swing=relative.clone().multiply(twist.clone().invert());
      let angle=2*Math.atan2(projection.dot(axis),relative.w),range=hinge?l.limits.flexion:l.limits.twist;angle=Math.max(range[0],Math.min(range[1],angle));twist.setFromAxisAngle(axis,angle);
      const swingAngle=2*Math.acos(Math.max(-1,Math.min(1,swing.w))),limit=hinge?.08:Math.min(...l.limits.swing);if(swingAngle>limit)swing.slerp(new THREE.Quaternion(),1-limit/swingAngle);
      const corrected=base.multiply(swing).multiply(twist);if(corrected.angleTo(new THREE.Quaternion().copy(l.b.rotation()))>.001){l.b.setRotation(corrected,true);const av=l.b.angvel();l.b.setAngvel({x:av.x*.85,y:av.y*.85,z:av.z*.85},true);}
    }}
    correctJoints(){this.correctRagdollJoints(this.ragdoll);for(const ragdoll of this.ragdolls.values())this.correctRagdollJoints(ragdoll);}
    step(){this.correctJoints();this.world.step();this.enforceActiveLimit();this.updateCheap();
      // Sleeping bodies remain Dynamic: Rapier can wake a stack when the slab
      // beneath it is removed. Fixed bodies would hang forever at that height.
      for(const d of this.debris)if(d.visual){if(d.visualSynced&&d.body.isSleeping())continue;d.visual.position.copy(d.body.translation());d.visual.quaternion.copy(d.body.rotation());if(d.parentInverse)d.visual.position.applyMatrix4(d.parentInverse);if(d.parentRotation)d.visual.quaternion.premultiply(d.parentRotation);d.visualSynced=true;}
      for(const ragdoll of [this.ragdoll,...this.ragdolls.values()])if(ragdoll){const {rig,bodies}=ragdoll;for(const {body,d} of bodies.values()){const matrix=new THREE.Matrix4().compose(new THREE.Vector3().copy(body.translation()),new THREE.Quaternion().copy(body.rotation()),new THREE.Vector3(1,1,1));const bone=rig.byName[d.bone],world=R.RagdollSchema.boneWorld(matrix,d),local=R.RagdollSchema.boneLocal(world,bone.parent.matrixWorld);local.decompose(bone.position,bone.quaternion,bone.scale);bone.updateMatrixWorld(true);}rig.root.updateMatrixWorld(true);}
    }
    syncDynamic(model){let changed=false;const settle=[];for(const d of this.debris){if((d.body.isSleeping()||d.body.isFixed())&&d.synced){for(const {part} of d.volumes)if(part.velocity.some(v=>v!==0)){part.velocity=[0,0,0];part.linearVelocity=[0,0,0];part.angularVelocity=[0,0,0];changed=true;}d.sleepTicks=(d.sleepTicks||0)+1;const center=d.body.translation(),minY=Math.min(...d.volumes.map(v=>v.part.bounds.min[1])),surface=this.rubble.surfaceHeightAt(center.x,center.z);if(d.sleepTicks>=this.settleGraceTicks&&minY<=surface+.18)settle.push(d);continue;}d.sleepTicks=0;const position=d.body.translation(),rotation=d.body.rotation(),matrix=d.transform.makeRotationFromQuaternion(d.rotation.copy(rotation)),m=matrix.elements,velocity=d.body.linvel();m[12]=position.x;m[13]=position.y;m[14]=position.z;
        for(const v of d.volumes){const p=v.part;if(!model.parts.has(p.id))continue;const bounds=p.bounds;bounds.min.fill(Infinity);bounds.max.fill(-Infinity);
          for(let i=0;i<v.faces.length;i++)for(let j=0;j<v.faces[i].length;j++){const a=v.faces[i][j],b=v.worldFaces[i][j],x=a[0],y=a[1],z=a[2];b[0]=m[0]*x+m[4]*y+m[8]*z+m[12];b[1]=m[1]*x+m[5]*y+m[9]*z+m[13];b[2]=m[2]*x+m[6]*y+m[10]*z+m[14];for(let k=0;k<3;k++){bounds.min[k]=Math.min(bounds.min[k],b[k]);bounds.max[k]=Math.max(bounds.max[k],b[k]);}}
          for(let i=0;i<v.localPlanes.length;i++){const a=v.localPlanes[i],b=v.worldPlanes[i],n=a.n;b.n[0]=m[0]*n[0]+m[4]*n[1]+m[8]*n[2];b.n[1]=m[1]*n[0]+m[5]*n[1]+m[9]*n[2];b.n[2]=m[2]*n[0]+m[6]*n[1]+m[10]*n[2];b.d=a.d+b.n[0]*m[12]+b.n[1]*m[13]+b.n[2]*m[14];}if(v.materialFrame)p.materialFrame=Object.fromEntries(Object.entries(v.materialFrame).map(([k,a])=>[k,[m[0]*a[0]+m[4]*a[1]+m[8]*a[2]+(k==='origin'?m[12]:0),m[1]*a[0]+m[5]*a[1]+m[9]*a[2]+(k==='origin'?m[13]:0),m[2]*a[0]+m[6]*a[1]+m[10]*a[2]+(k==='origin'?m[14]:0)]]));p.faces=v.worldFaces;p.planes=v.worldPlanes;p.velocity[0]=velocity.x;p.velocity[1]=velocity.y;p.velocity[2]=velocity.z;
        }for(const v of d.volumes){const p=v.part,p0=p.centerOfMass||p.center||[0,0,0];p.linearVelocity=[velocity.x,velocity.y,velocity.z];p.velocity=p.linearVelocity.slice();const av=d.body.angvel();p.angularVelocity=[av.x,av.y,av.z];p.centerOfMass=[position.x,position.y,position.z];}d.synced=true;changed=true;}
      for(const ragdoll of [this.ragdoll,...this.ragdolls.values()])if(ragdoll)for(const {body,d} of ragdoll.bodies.values()){const p=[...model.parts.values()].find(part=>part.bodyId===d.id&&(!ragdoll.schema.unitId||part.ragdollUnitId===ragdoll.schema.unitId));if(!p)continue;const half=d.halfExtents||[d.radius,d.halfCylinder+d.radius,d.radius],matrix=new THREE.Matrix4().compose(new THREE.Vector3().copy(body.translation()),new THREE.Quaternion().copy(body.rotation()),new THREE.Vector3(1,1,1));p.faces=S.box(half.map(x=>-x),half).map(f=>f.map(v=>new THREE.Vector3(...v).applyMatrix4(matrix).toArray()));p.bounds=S.bounds(p.faces);p.planes=S.planes(p.faces);p.dynamic=true;const v=body.linvel();p.velocity=[v.x,v.y,v.z];for(const armor of model.parts.values())if(armor.bodyId===d.id&&armor.ragdollUnitId===ragdoll.schema.unitId){armor.faces=armor.bodyLocalFaces?armor.bodyLocalFaces.map(f=>f.map(v=>new THREE.Vector3(...v).applyMatrix4(matrix).toArray())):armor.faces;armor.bounds=S.bounds(armor.faces);armor.planes=S.planes(armor.faces);armor.dynamic=true;armor.velocity=p.velocity.slice();}changed=true;}
      if(changed)(model.reindexMoving?model.reindexMoving():model.reindex());if(settle.length)this.settleHeroBatch(settle);else this.rubble.flush();
    }
    dispose(){for(const d of this.cheapDebris)d.visual?.parent?.remove(d.visual);this.cheapDebris.length=0;if(this.cheapPacketMesh)this.cheapPacketMesh.parent?.remove(this.cheapPacketMesh);this.cheapPacketGeometry?.dispose();this.cheapPacketMaterial?.dispose();this.cheapPacketMesh=null;this.rubble?.dispose();this.model=null;this.world.free();this.static.clear();this.debris.length=0;this.ragdoll=null;this.ragdolls.clear();}
  }
  const stepPhysics=DestructionPhysics.prototype.step;
  DestructionPhysics.prototype.step=function(){stepPhysics.call(this);const settle=[];for(const d of this.debris){if(d.volumes.some(v=>v.part.category==='plant'))continue;const p=d.body.translation(),surface=this.rubble.surfaceHeightAt(p.x,p.z),minY=Math.min(...d.volumes.map(v=>v.part.bounds.min[1])),rock=d.volumes.some(v=>v.part.material==='rock');if((!rock||(d.sleepTicks||0)>=this.settleGraceTicks)&&minY<=surface+.18&&d.body.linvel().y<=0)settle.push(d);}if(settle.length)this.settleHeroBatch(settle);};
  R.DestructionPhysics=DestructionPhysics;
})();
