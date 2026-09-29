(function(){
  'use strict';
  const R=window.RTS;
  const MARCH_MAX_SPEED=1.55,MARCH_MAX_SPEED_BRAKE_DISTANCE=MARCH_MAX_SPEED*MARCH_MAX_SPEED/3;
  const VEGETATION_SPECIES=['oak','birch','pine','spruce','beech','larch','oak','birch','pine','spruce','beech','larch','hazel','hawthorn','rose'];
  // The playable centre stays readable for marching troops. The settlement is
  // deliberately authored as a sparse layout, while every individual building
  // remains procedurally generated from the match seed.
  function settlementLayout(seed,terrain){
    const random=new R.SeededRandom(seed^0x51e77e),templates=['EURO_HOUSE','EURO_HOUSE','EURO_HOUSE','EURO_APARTMENT','EURO_HOUSE','EURO_HOUSE'];
    const slots=[[-58,-18],[ -18,-25],[24,-24],[55,-16],[-38,23],[27,24]];
    return slots.map(([x,z],index)=>({id:'settlement-'+index,x,z,rotation:(index%2?Math.PI:0)+(random.next()-.5)*.18,seed:(seed+Math.imul(index+1,0x9e3779b1))>>>0,presetId:templates[index]==='EURO_APARTMENT'?'MULTI_FAMILY':templates[index]==='EURO_OFFICE'?'OFFICE':'FAMILY_HOUSE',roofFamily:index===3?'FLAT':index%2?'GABLE':'HIP'}));
  }
  function ribbon(terrain,road,material){
    const points=road.points,vertices=[],indices=[],half=road.width*.5,offset=road.boundary?.07:.015;
    for(let i=0;i<points.length;i++){const before=points[Math.max(0,i-1)],after=points[Math.min(points.length-1,i+1)],dx=after.x-before.x,dz=after.z-before.z,l=Math.hypot(dx,dz)||1,nx=-dz/l,nz=dx/l,p=points[i];for(const side of [-1,1]){const x=p.x+nx*half*side,z=p.z+nz*half*side;vertices.push(x,terrain.getHeightAt(x,z)+offset,z);}}
    for(let i=1;i<points.length;i++){const a=(i-1)*2,b=a+1,c=i*2,d=c+1;indices.push(a,c,b,b,c,d);}
    const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(vertices,3));g.setIndex(indices);g.computeVertexNormals();const mesh=new THREE.Mesh(g,material);mesh.receiveShadow=true;return mesh;
  }
  function appendRibbon(terrain,road,bucket){
    const points=road.points,half=road.width*.5,offset=road.boundary?.07:.015,base=bucket.vertices.length/3;
    for(let i=0;i<points.length;i++){const before=points[Math.max(0,i-1)],after=points[Math.min(points.length-1,i+1)],dx=after.x-before.x,dz=after.z-before.z,l=Math.hypot(dx,dz)||1,nx=-dz/l,nz=dx/l,p=points[i];for(const side of [-1,1]){const x=p.x+nx*half*side,z=p.z+nz*half*side;bucket.vertices.push(x,terrain.getHeightAt(x,z)+offset,z);}}
    for(let i=1;i<points.length;i++){const a=base+(i-1)*2,b=a+1,c=base+i*2,d=c+1;bucket.indices.push(a,c,b,b,c,d);}
  }
  function batchedRoads(terrain,plan,materials){
    const buckets=new Map(),chunkSize=128;
    const add=(road,materialId)=>{for(let i=1;i<road.points.length;i++){const a=road.points[i-1],b=road.points[i],cx=Math.floor(((a.x+b.x)*.5)/chunkSize),cz=Math.floor(((a.z+b.z)*.5)/chunkSize),key=materialId+':'+cx+':'+cz;let bucket=buckets.get(key);if(!bucket){bucket={materialId,cx,cz,vertices:[],indices:[]};buckets.set(key,bucket);}appendRibbon(terrain,{...road,points:[a,b]},bucket);}};
    for(const road of plan.roads){add(road,road.kind==='asphalt'?'asphalt':road.kind==='track'?'gravel':'dirt');if(road.sidewalk)for(const sign of [-1,1]){const shifted={...road,width:road.sidewalk,points:road.points.map((p,i)=>{const a=road.points[Math.max(0,i-1)],b=road.points[Math.min(road.points.length-1,i+1)],dx=b.x-a.x,dz=b.z-a.z,l=Math.hypot(dx,dz)||1;return {x:p.x-dz/l*sign*(road.width*.5+road.sidewalk*.5),z:p.z+dx/l*sign*(road.width*.5+road.sidewalk*.5)};})};add(shifted,'sidewalk');}}
    const root=new THREE.Group();root.name='Batched roads and sidewalks';for(const bucket of buckets.values()){const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(bucket.vertices,3));g.setIndex(bucket.indices);g.computeVertexNormals();const mesh=new THREE.Mesh(g,materials[bucket.materialId]);mesh.name='Road chunk '+bucket.cx+','+bucket.cz+' '+bucket.materialId;mesh.frustumCulled=true;mesh.receiveShadow=true;root.add(mesh);}return root;
  }
  function createSettlementVisuals(terrain,plan){
    const root=new THREE.Group();root.name='Village roads and fences';const asphalt=new THREE.MeshStandardMaterial({color:0x353638,roughness:.92}),dirt=new THREE.MeshStandardMaterial({color:0x74634c,roughness:1}),gravel=new THREE.MeshStandardMaterial({color:0x817967,roughness:1}),sidewalk=new THREE.MeshStandardMaterial({color:0x8a887f,roughness:.95});
    root.add(batchedRoads(terrain,plan,{asphalt,dirt,gravel,sidewalk}));
    return root;
  }
  function createLandscapeVisuals(terrain,plan){
    const root=new THREE.Group();root.name='Agricultural landscape';
    const colors={field:0x927849,fallow:0x81765a,meadow:0x68834b,pasture:0x779255,orchard:0x668349,grove:0x536d42,forest:0x46653e};
    const materials=new Map(),cropItems=[],cropZones=[],hayItems=[],boundary=new THREE.MeshStandardMaterial({color:0x7b714b,roughness:1});
    for(const zone of plan.zones){
      const polygon=zone.polygon,minX=Math.min(...polygon.map(p=>p.x)),maxX=Math.max(...polygon.map(p=>p.x)),minZ=Math.min(...polygon.map(p=>p.z)),maxZ=Math.max(...polygon.map(p=>p.z));
      const vertices=[],indices=[],step=7;
      for(let z=minZ;z<maxZ;z+=step)for(let x=minX;x<maxX;x+=step){
        const cx=x+step*.5,cz=z+step*.5;
        if(!R.WorldGeneration.inside(cx,cz,polygon)||plan.isReserved(cx,cz,5)||[[x,z],[x+step,z],[x+step,z+step],[x,z+step]].some(([px,pz])=>!R.WorldGeneration.inside(px,pz,polygon)||plan.isReserved(px,pz,.5)))continue;
        const base=vertices.length/3;for(const [px,pz] of [[x,z],[x+step,z],[x+step,z+step],[x,z+step]])vertices.push(px,terrain.getHeightAt(px,pz)+.035,pz);
        indices.push(base,base+2,base+1,base,base+3,base+2);
      }
      if(vertices.length){const geometry=new THREE.BufferGeometry();geometry.setAttribute('position',new THREE.Float32BufferAttribute(vertices,3));geometry.setIndex(indices);geometry.computeVertexNormals();let material=materials.get(zone.type);if(!material){material=new THREE.MeshStandardMaterial({color:colors[zone.type],roughness:1,side:THREE.DoubleSide});materials.set(zone.type,material);}const mesh=new THREE.Mesh(geometry,material);mesh.name='Zone '+zone.type;mesh.receiveShadow=true;root.add(mesh);}
      if(zone.type==='field'||zone.type==='fallow'){const outline=[...polygon,polygon[0]];root.add(ribbon(terrain,{points:outline,width:.6,boundary:true},boundary));}
      if(zone.type==='field'||zone.type==='fallow'){
        const zoneCrops=[];
        for(let z=minZ+3;z<maxZ-2;z+=2.8)for(let x=minX+3;x<maxX-2;x+=3.3){
          if(cropItems.length>=8000)break;
          if(R.WorldGeneration.inside(x,z,polygon)&&!plan.isReserved(x,z,1.4)){const crop={x,z,y:terrain.getHeightAt(x,z),height:zone.type==='field'?.8:.4};cropItems.push(crop);zoneCrops.push(crop);}
        }
        if(zoneCrops.length)cropZones.push({x:(minX+maxX)*.5,z:(minZ+maxZ)*.5,items:zoneCrops});
      }
      if(zone.type==='meadow'||zone.type==='pasture')for(let x=minX+13;x<maxX-8;x+=27)for(let z=minZ+10;z<maxZ-8;z+=31)if(R.WorldGeneration.inside(x,z,polygon)&&!plan.isReserved(x,z,2))hayItems.push({x,z,y:terrain.getHeightAt(x,z)});
    }
    function instances(name,items,geometry,material,scale){
      if(!items.length){geometry.dispose();material.dispose();return;}
      const mesh=new THREE.InstancedMesh(geometry,material,items.length),dummy=new THREE.Object3D();mesh.name=name;mesh.castShadow=false;
      items.forEach((p,i)=>{dummy.position.set(p.x,p.y+p.height*.5||p.y+.4,p.z);dummy.scale.set(...scale(p));dummy.updateMatrix();mesh.setMatrixAt(i,dummy.matrix);});mesh.instanceMatrix.needsUpdate=true;root.add(mesh);
    }
    if(cropZones.length){
      const geometry=new THREE.ConeGeometry(.22,1,4),material=new THREE.MeshStandardMaterial({color:0x9caa54,roughness:1}),dummy=new THREE.Object3D();
      for(const zone of cropZones){
        const lod=new THREE.LOD(),mesh=new THREE.InstancedMesh(geometry,material,zone.items.length);lod.position.set(zone.x,0,zone.z);lod.name='Crop rows LOD';mesh.name='Crop rows';mesh.castShadow=false;
        zone.items.forEach((p,i)=>{dummy.position.set(p.x-zone.x,p.y+p.height*.5,p.z-zone.z);dummy.scale.set(1,p.height,1);dummy.updateMatrix();mesh.setMatrixAt(i,dummy.matrix);});
        mesh.instanceMatrix.needsUpdate=true;lod.addLevel(mesh,0);lod.addLevel(new THREE.Group(),130);root.add(lod);
      }
    }
    instances('Hay bales',hayItems,new THREE.CylinderGeometry(.7,.7,1.2,8),new THREE.MeshStandardMaterial({color:0xb49a56,roughness:1}),()=>[1,1,1]);
    if(!root.children.some(mesh=>mesh.material===boundary))boundary.dispose();
    root.userData.cropCount=cropItems.length;
    return root;
  }
  // A bounded prototype palette keeps procedural variation compatible with instancing.
  function* vegetationLayout(seed,terrain,plan=null,amount=.62){
    const random=new R.SeededRandom(seed^0xa72fb941),patches=[],plants=[],buckets=new Map();
    const cellSize=4,sample={height:0,normal:new THREE.Vector3()};
    const span=terrain.mapSize*.46,target=Math.round(50+amount*250),patchCount=Math.round(6+amount*16);
    if(plan){
      const limit=Math.min(target,Math.round(amount*70));
      for(const zone of plan.zones)if(zone.type==='orchard'&&plants.length<limit){
        const xs=zone.polygon.map(p=>p.x),zs=zone.polygon.map(p=>p.z);
        for(let x=Math.ceil(Math.min(...xs)/9)*9;x<Math.max(...xs)&&plants.length<limit;x+=9)
          for(let z=Math.ceil(Math.min(...zs)/9)*9;z<Math.max(...zs)&&plants.length<limit;z+=9){
            if(!R.WorldGeneration.inside(x,z,zone.polygon)||plan.isReserved(x,z,2))continue;
            terrain.sample(x,z,sample);if(sample.normal.y<.9)continue;
            const scale=.6+random.next()*.2,plant={x,z,y:sample.height-.08,variant:Math.floor(random.next()*6),angle:random.next()*Math.PI*2,scale,widthScale:scale,footprint:scale*.95,kind:'tree'};
            plants.push(plant);const cx=Math.floor(x/cellSize),cz=Math.floor(z/cellSize),key=(cx+256)*512+cz+256,cell=buckets.get(key)||[];cell.push(plant);buckets.set(key,cell);yield plant;
          }
      }
    }
    for(let i=0;i<patchCount;i++)patches.push({x:(random.next()-.5)*span*2,z:(random.next()-.5)*span*2,radius:7+random.next()*19,stretch:.55+random.next()*.85});
    for(let attempt=0;attempt<target*40&&plants.length<target;attempt++){
      if((attempt&127)===127)yield null;
      const shrub=plants.length>=90,clustered=random.next()<(shrub?.88:.76),patch=patches[Math.floor(random.next()*patches.length)];
      const angle=random.next()*Math.PI*2,radius=Math.sqrt(random.next())*patch.radius;
      let x=clustered?patch.x+Math.cos(angle)*radius:(random.next()-.5)*span*2;
      let z=clustered?patch.z+Math.sin(angle)*radius*patch.stretch:(random.next()-.5)*span*2;
      if(plan&&plan.zoneAt(x,z)==='orchard'){x=Math.round(x/8)*8;z=Math.round(z/8)*8;}
      if(Math.abs(x)>span||Math.abs(z)>span||(plan&&plan.isReserved(x,z,1.5)))continue;
      const zone=plan&&plan.zoneAt(x,z);
      if(zone==='field'||zone==='fallow'||zone==='pasture'||zone==='meadow')continue;
      const scale=shrub?.55+random.next()*.75:.52+Math.pow(random.next(),.8)*.93;
      const widthScale=scale*(.78+random.next()*.47),footprint=shrub?.45*widthScale:.95*widthScale;
      const cellX=Math.floor(x/cellSize),cellZ=Math.floor(z/cellSize);let tooClose=false;
      for(let dz=-1;dz<=1&&!tooClose;dz++)for(let dx=-1;dx<=1&&!tooClose;dx++){
        const nearby=buckets.get((cellX+dx+256)*512+cellZ+dz+256);
        if(nearby)for(const other of nearby){const ox=other.x-x,oz=other.z-z,min=footprint+other.footprint;if(ox*ox+oz*oz<min*min){tooClose=true;break;}}
      }
      if(tooClose)continue;
      terrain.sample(x,z,sample);if(sample.normal.y<.90)continue;
      const variant=zone==='orchard'?Math.floor(random.next()*6):shrub?12+Math.floor(random.next()*3):Math.floor(random.next()*12);
      const plant={x,z,y:sample.height-.08,variant,angle:random.next()*Math.PI*2,scale,widthScale,footprint,kind:shrub?'shrub':'tree'};
      plants.push(plant);const key=(cellX+256)*512+cellZ+256,cell=buckets.get(key)||[];cell.push(plant);buckets.set(key,cell);
      yield plant;
    }
  }
  const ROCK_PLACEMENT_DEFAULTS=Object.freeze({clusterSpacing:.55,clusterRadius:.55,density:.5,overlap:.18,burial:.2,uniformity:.72,formationMix:.7});
  function normalizeRockSettings(input={}){const out={};for(const [key,value] of Object.entries(ROCK_PLACEMENT_DEFAULTS))out[key]=Number.isFinite(input[key])?Math.max(0,Math.min(1,input[key])):value;return out;}
  function* rockLayout(seed,terrain,plants=[],inputSettings={},plan=null){
    const settings=normalizeRockSettings(inputSettings),random=new R.SeededRandom(seed^0x7ac41f03),sample={height:0,normal:new THREE.Vector3()},clusters=[],placed=[];
    const sizes=[.35,1.1,3,2.5,3.5,3,2,6],spacing=9+settings.clusterSpacing*25,radius=5+settings.clusterRadius*13,target=Math.round(60+settings.density*80),clusterCount=Math.round(5+(1-settings.clusterSpacing)*8);
    // Poisson-style centers keep broad gaps between groups while bounded jitter
    // prevents a mechanical grid. Keep each center away from the march corridor.
    for(let attempt=0;attempt<500&&clusters.length<clusterCount;attempt++){
      const x=(random.next()-.5)*156,z=(random.next()<.5?-1:1)*(29+random.next()*61);
      if(clusters.some(p=>Math.hypot(p.x-x,p.z-z)<spacing*(.68+random.next()*.34)))continue;
      const formation=random.next()*settings.formationMix,typeRoll=random.next();
      const type=formation<.18?'outcrop':formation<.4?'talus':formation<.62?'ridge':'family';
      const dominant=typeRoll<.48?2+Math.floor(random.next()*6):typeRoll<.75?1:0;
      clusters.push({x,z,radius:radius*(.65+random.next()*.7),type,dominant,angle:random.next()*Math.PI*2});
    }
    const maxAttempts=target*65;
    for(let attempt=0;attempt<maxAttempts&&placed.length<target;attempt++){
      if((attempt&63)===63)yield null;
      const cluster=clusters[Math.floor(random.next()*clusters.length)],angle=random.next()*Math.PI*2,r=Math.sqrt(random.next());
      let dx, dz;
      if(cluster.type==='ridge'){const along=(random.next()-.5)*2,across=(random.next()-.5)*.65;dx=along;dz=across;}
      else if(cluster.type==='talus'){const along=(random.next()-.5)*2;dx=along;dz=(random.next()-.5)*(.38+Math.abs(along)*.7);}
      else{dx=Math.cos(angle)*r;dz=Math.sin(angle)*r*(cluster.type==='outcrop'?.62:1);}
      const cs=Math.cos(cluster.angle),sn=Math.sin(cluster.angle),x=cluster.x+(dx*cs-dz*sn)*cluster.radius,z=cluster.z+(dx*sn+dz*cs)*cluster.radius;
      if(Math.abs(x)>91||Math.abs(z)>91||Math.abs(z)<25)continue;
      const rank=random.next();let variant;
      if(rank<.12)variant=0;else if(rank<.29)variant=1;else if(settings.uniformity>.45&&random.next()<settings.uniformity)variant=cluster.dominant;else variant=2+Math.floor(random.next()*6);
      if(cluster.type==='outcrop')variant=cluster.dominant;
      const tier=rank<.12?.24+random.next()*.25:rank<.29?.42+random.next()*.28:.72+random.next()*.58;
      const scale=tier*(.8+random.next()*.4),widthScale=scale*(.82+random.next()*.38),footprint=sizes[variant]*widthScale*.72;
      const minDistance=(a,b)=>(a.footprint+b.footprint)*(1-settings.overlap);
      if(Math.abs(x)+footprint>terrain.mapSize*.47||Math.abs(z)-footprint<24||(plan&&plan.isReserved(x,z,footprint+.3)))continue;
      if(plan&&['field','orchard','pasture'].includes(plan.zoneAt(x,z)))continue;
      if(plants.some(p=>Math.hypot(p.x-x,p.z-z)<footprint+p.footprint))continue;
      if(placed.some(p=>Math.hypot(p.x-x,p.z-z)<minDistance({footprint},p)))continue;
      terrain.sample(x,z,sample);if(sample.normal.y<.86)continue;
      const burial=settings.burial*(.55+random.next()*.9),rock={x,z,y:sample.height-sizes[variant]*scale*burial,variant,scale,widthScale,angle:random.next()*Math.PI*2,tiltX:Math.max(-.15,Math.min(.15,sample.normal.z*.12)),tiltZ:Math.max(-.15,Math.min(.15,-sample.normal.x*.12)),footprint,burial,cluster:clusters.indexOf(cluster),formation:cluster.type,kind:'rock'};
      placed.push(rock);yield rock;
    }
    return placed;
  }
  function mergeSphere(target,source,initialized,offset){
    if(!initialized)return target.copy(source);
    offset.subVectors(source.center,target.center);const distance=offset.length();
    if(distance+source.radius<=target.radius)return target;
    if(distance+target.radius<=source.radius)return target.copy(source);
    const radius=(target.radius+distance+source.radius)*.5;
    if(distance>1e-9)target.center.addScaledVector(offset,(radius-target.radius)/distance);
    target.radius=radius;return target;
  }
  function chunkGeometry(source,bound){
    const geometry=new THREE.BufferGeometry();
    geometry.index=source.index;geometry.attributes=source.attributes;geometry.groups=source.groups;
    geometry.morphAttributes=source.morphAttributes;geometry.morphTargetsRelative=source.morphTargetsRelative;
    geometry.drawRange=source.drawRange;geometry.boundingBox=source.boundingBox;geometry.boundingSphere=bound;
    return geometry;
  }
  R.Battlefield=class Battlefield{
    constructor(seed,sides,rockSettings={},generation={}){this.generation=R.WorldGeneration.normalize({...generation,seed});this.seed=this.generation.seed;this.sides=sides;this.unitsPerSide=Number.isInteger(generation.unitsPerSide)?Math.max(1,Math.min(25,generation.unitsPerSide)):25;this.aiModelIds=generation.aiModelIds||{};this.rockSettings=normalizeRockSettings(rockSettings);this.root=new THREE.Group();this.root.name='Battlefield';this.units=[];this.prototypes=[];this.trees=[];this.rocks=[];this.instances=[];this.buildings=[];this.damageRuntimes=new Map();this._weaponCallbacks=new Map();this.buildingRepresentationManager=new R.BuildingRepresentationManager(null);this.unitPresentationScheduler=new R.UnitPresentationScheduler();const rubble=R.RubbleField?new R.RubbleField({cellSize:.5,tileSize:4,maxTiles:256,maxChunkInstances:1024}):null;this.worldDestruction=new R.WorldDestructionHost(this,{rubble});if(rubble){rubble.attachModel(this.worldDestruction.model);rubble.attachVisual(this.root);}this.ballistics=new R.WorldBallisticsBridge(this);this.ballisticsWorld=new R.BallisticsWorld({model:this.worldDestruction.model,worldBridge:this.ballistics,ground:true});this.ai=new R.BattlefieldAIManager(this,{defaultModelId:generation.aiModelId||'simple-combat-v1'});this.elapsed=0;this.arrived=0;this.terrain=new R.TerrainSystem(this.root,{mapSize:this.generation.size,segments:240,battlefield:true});}
    async build(progress,signal=null){
      const abortError=()=>{const error=new Error('Generowanie świata zostało anulowane.');error.name='AbortError';return error;};
      const check=()=>{if(signal&&signal.aborted)throw abortError();};
      const yieldFrame=()=>new Promise((resolve,reject)=>{
        check();let id;
        const cleanup=()=>signal&&signal.removeEventListener('abort',onAbort);
        const onAbort=()=>{cancelAnimationFrame(id);cleanup();reject(abortError());};
        id=requestAnimationFrame(()=>{cleanup();if(signal&&signal.aborted)reject(abortError());else resolve();});
        if(signal){signal.addEventListener('abort',onAbort,{once:true});if(signal.aborted)onAbort();}
      });
      try{
        check();
        if(progress)progress(0,'terrain');
        // Construct terrain in deterministic, row-sized slices so generation can
        // paint progress and respond to cancellation during the expensive CPU passes.
        await this.terrain.buildAsync(this.seed,yieldFrame,check,signal);
        if(this.generation.skipEnvironment){
          this.plan=R.WorldGeneration.create(this.generation);
          for(let army=0;army<2;army++)for(let i=0;i<this.unitsPerSide;i++){
            check();
            const sign=army===0?-1:1,rank=Math.floor(i/5),file=i%5,side=this.sides[army===0?'SIDE_A':'SIDE_B'];
            const unit=await R.InfantryUnit.createAsync({id:(army===0?'A_':'B_')+String(i+1).padStart(3,'0'),side,seed:(this.seed+army*100003+i*101)>>>0,state:'WALK',detail:'far',equipmentOptions:{loadout:i===0?'SQUAD_LEADER':'RIFLEMAN'}},yieldFrame,check,4);
            this.units.push(unit);unit.heading=-sign*Math.PI/2;unit.setWorldPosition(sign*(55+rank*3.5),(file-2)*4,this.terrain);unit.marchLocomotionInput={speedMps:1.55};unit.setLocomotion(unit.marchLocomotionInput);unit.marchTarget=sign*(4+rank*3.5);unit.marchSign=-sign;unit.marchArrived=false;this.root.add(unit.root);
            unit.alive=true;unit.aiModelId=this.aiModelIds[unit.id]||unit.aiModelId||this.ai.defaultModelId;this.damageRuntimes.set(unit.id,new R.InfantryDamageRuntime(unit,this.worldDestruction.model,this.worldDestruction.physics));this.ai.add(unit,unit.aiModelId);this.connectUnitWeapon(unit);
            if(progress)progress(this.units.length,'army');
            await yieldFrame();
          }
          return this;
        }
        // Build a small settlement before decoration and units. Building roots
        // are independent instances, so a later destruction/gameplay adapter
        // can attach to an individual house without rebuilding the map.
        this.plan=R.WorldGeneration.create(this.generation);const generator=new R.ProceduralBuildingGenerator(),settlement=this.plan.buildings;
        this.terrain.flattenRoads(this.plan.roads);
        for(const spec of settlement){
          check();const plan=generator.createPlan({seed:spec.seed,presetId:spec.presetId||undefined,furniture:false}),bounds=R.Buildings.Polygon.bounds(plan.footprint.polygon),dimensions={width:bounds[1]-bounds[0],depth:bounds[3]-bounds[2]};spec.width=dimensions.width;spec.depth=dimensions.depth;spec.parcel.buildingFootprint=R.WorldGeneration.rectangle(spec.x,spec.z,dimensions.width,dimensions.depth,spec.rotation);if(spec.parcel.buildingFootprint.some(p=>!R.WorldGeneration.inside(p.x,p.z,spec.parcel.polygon)))spec.parcel.fenced=false;const runtime=new R.BuildingRuntime(plan,{id:spec.id,position:[spec.x,0,spec.z],rotation:spec.rotation,chunkSize:R.Config.WORLD_BUILDING_RUNTIME.chunkSize});this.buildings.push(runtime);this.buildingRepresentationManager.add(runtime);this.worldDestruction.add(runtime);
          if(progress)progress(this.buildings.length,'buildings');await yieldFrame();
        }
        this.terrain.flattenPads(settlement);this.settlementVisuals=createSettlementVisuals(this.terrain,this.plan);this.root.add(this.settlementVisuals);
        this.settlementVisuals.add(R.FenceGenerator.create(this.terrain,this.plan.parcels,this.plan.roads));
        this.settlementVisuals.add(createLandscapeVisuals(this.terrain,this.plan));
        for(let i=0;i<this.buildings.length;i++){const building=this.buildings[i],spec=settlement[i],y=this.terrain.getHeightAt(spec.x,spec.z);building.root.name='Settlement building '+spec.id;building.root.position.y=y;building.root.userData.settlement=spec;R.WorldShadowPolicy.apply(building.root);this.root.add(building.root);}
        const species=VEGETATION_SPECIES;
        await yieldFrame();
        for(let i=0;i<species.length;i++){
          check();
          const mature=i>=6&&i<12;
          const genome=R.PlantGenome.create(species[i],(this.seed+i*7919)>>>0,{girth:mature?.85:.28,damage:mature?.65:.12});
          this.prototypes.push(await R.PlantGenerator.createLodsAsync(genome,{age:mature?.78:.48,season:'summer'},yieldFrame,check));
          if(progress)progress(i+1,'trees');
          await yieldFrame();
        }
        const treeChunks=species.map(()=>[[],[],[],[]]);
        for(const tree of vegetationLayout(this.seed,this.terrain,this.plan,this.generation.vegetation)){
          check();if(!tree){await yieldFrame();continue;}
          this.trees.push(tree);treeChunks[tree.variant][(tree.z>=0?2:0)+(tree.x>=0?1:0)].push(tree);
        }
        this.rocks=[];
        const rockSpecies=Object.keys(R.RockCatalog),names=[...species,...rockSpecies];
        for(let i=0;i<rockSpecies.length;i++){
          check();this.prototypes.push(await R.RockGenerator.createLodsAsync(R.RockGenome.create(rockSpecies[i],(this.seed^Math.imul(i+1,92821))>>>0),{},yieldFrame,check));
          treeChunks.push([[],[],[],[]]);if(progress)progress(i+1,'rocks');
        }
        for(const rock of rockLayout(this.seed,this.terrain,this.trees,this.rockSettings,this.plan)){
          check();if(!rock){await yieldFrame();continue;}
          this.rocks.push(rock);treeChunks[species.length+rock.variant][(rock.z>=0?2:0)+(rock.x>=0?1:0)].push(rock);
        }
        const dummy=new THREE.Object3D(),instanceMatrixScratch=new THREE.Matrix4(),boundDelta=new THREE.Vector3();
        for(let index=0;index<this.prototypes.length;index++){
          check();
          const prototypeSet=this.prototypes[index];
          for(let chunk=0;chunk<4;chunk++){
            const trees=treeChunks[index][chunk];
            if(trees.length){
              const center=new THREE.Vector3();
              for(let i=0;i<trees.length;i++){
                check();const tree=trees[i];center.x+=tree.x;center.y+=tree.y;center.z+=tree.z;
                if((i&15)===15)await yieldFrame();
              }
              center.multiplyScalar(1/trees.length);
              const lod=R.LodHysteresis.install(new THREE.LOD());lod.name='Environment LOD '+names[index]+' '+chunk;lod.position.copy(center);
              let canonicalInstanceMatrix=null;
              for(let level=0;level<prototypeSet.length;level++){
                const prototype=prototypeSet[level],levelRoot=new THREE.Group(),levelParts=prototype.root.children;
                for(const part of levelParts){
                  const source=part.geometry;if(!source.boundingSphere)source.computeBoundingSphere();
                  const bound=new THREE.Sphere(new THREE.Vector3(),0);let hasBound=false;
                  const instanceBound=new THREE.Sphere(new THREE.Vector3(),0);
                  const mesh=new THREE.InstancedMesh(chunkGeometry(source,bound),part.material,canonicalInstanceMatrix?0:trees.length);mesh.name='Environment '+names[index]+' '+chunk+' LOD '+level+' '+part.name;mesh.castShadow=!(level===2&&part.name==='foliage');mesh.receiveShadow=true;mesh.frustumCulled=true;
                  if(canonicalInstanceMatrix){
                    // Every material and LOD for this tree chunk uses the same
                    // immutable transforms. Share the attribute itself to avoid
                    // duplicate CPU/GPU instance buffers and matrix copies.
                    mesh.count=trees.length;mesh.instanceMatrix=canonicalInstanceMatrix;
                    for(let i=0;i<trees.length;i++){
                      check();instanceMatrixScratch.fromArray(canonicalInstanceMatrix.array,i*16);
                      instanceBound.copy(source.boundingSphere).applyMatrix4(instanceMatrixScratch);mergeSphere(bound,instanceBound,hasBound,boundDelta);hasBound=true;
                      if((i&15)===15)await yieldFrame();
                    }
                  }else{
                    for(let i=0;i<trees.length;i++){
                      check();const tree=trees[i];
                      dummy.position.set(tree.x-center.x,tree.y-center.y,tree.z-center.z);dummy.rotation.set(tree.kind==='rock'?tree.tiltX||0:0,tree.angle,tree.kind==='rock'?tree.tiltZ||0:0);dummy.scale.set(tree.widthScale,tree.kind==='rock'?tree.scale*(1-tree.burial*.34):tree.scale,tree.widthScale);dummy.updateMatrix();mesh.setMatrixAt(i,dummy.matrix);
                      instanceBound.copy(source.boundingSphere).applyMatrix4(dummy.matrix);mergeSphere(bound,instanceBound,hasBound,boundDelta);hasBound=true;
                      if((i&15)===15)await yieldFrame();
                    }
                    canonicalInstanceMatrix=mesh.instanceMatrix;
                    mesh.instanceMatrix.needsUpdate=true;
                  }
                  levelRoot.add(mesh);
                }
                lod.addLevel(levelRoot,[0,65,130][level]);
              }
              this.root.add(lod);this.instances.push(lod);
            }
            await yieldFrame();
          }
        }
        for(let army=0;army<2;army++)for(let i=0;i<this.unitsPerSide;i++){
          check();
          const sign=army===0?-1:1,rank=Math.floor(i/5),file=i%5,side=this.sides[army===0?'SIDE_A':'SIDE_B'];
          const unit=await R.InfantryUnit.createAsync({id:(army===0?'A_':'B_')+String(i+1).padStart(3,'0'),side,seed:(this.seed+army*100003+i*101)>>>0,state:'WALK',detail:'far',equipmentOptions:{loadout:i===0?'SQUAD_LEADER':'RIFLEMAN'}},yieldFrame,check,4);
          this.units.push(unit);unit.heading=-sign*Math.PI/2;unit.setWorldPosition(sign*(55+rank*3.5),(file-2)*4,this.terrain);unit.marchLocomotionInput={speedMps:1.55};unit.setLocomotion(unit.marchLocomotionInput);unit.marchTarget=sign*(4+rank*3.5);unit.marchSign=-sign;unit.marchArrived=false;this.root.add(unit.root);
          unit.alive=true;unit.aiModelId=this.aiModelIds[unit.id]||unit.aiModelId||this.ai.defaultModelId;this.damageRuntimes.set(unit.id,new R.InfantryDamageRuntime(unit,this.worldDestruction.model,this.worldDestruction.physics));this.ai.add(unit,unit.aiModelId);this.connectUnitWeapon(unit);
          if(progress)progress(this.units.length,'army');
          await yieldFrame();
        }
        return this;
      }catch(error){this.dispose();throw error;}
    }
    step(dt){
      this.elapsed+=dt;this.arrived=0;
      this.ai?.step(dt);
      for(const unit of this.units){
        if(unit.alive===false){unit.step(dt,this.terrain);continue;}
        if(unit.aiHold){if(unit.locomotion.requestedSpeedMps!==0)unit.setLocomotion({speedMps:0});unit.step(dt,this.terrain);continue;}
        // A completed marcher still advances its pose, face and weapon clocks,
        // but its target distance and locomotion request can no longer change.
        // Keep the per-step presentation alive while skipping redundant path
        // math and controller validation for settled units.
        if(unit.marchArrived){this.arrived++;unit.step(dt,this.terrain);continue;}
        const remaining=(unit.marchTarget-unit.position.x)*unit.marchSign;
        const requestedSpeed=remaining>=MARCH_MAX_SPEED_BRAKE_DISTANCE?MARCH_MAX_SPEED:Math.min(MARCH_MAX_SPEED,Math.sqrt(3*Math.max(0,remaining)));
        const missionSpeedChanged=requestedSpeed!==unit.marchLocomotionInput.speedMps;if(missionSpeedChanged)unit.marchLocomotionInput.speedMps=requestedSpeed;
        if(missionSpeedChanged||(unit.locomotion&&unit.locomotion.requestedSpeedMps!==requestedSpeed))unit.setLocomotion(unit.marchLocomotionInput);
        unit.step(dt,this.terrain);
        if(!unit.marchArrived&&(unit.marchTarget-unit.position.x)*unit.marchSign<.04){
          unit.marchArrived=true;unit.speed=0;unit.setState('IDLE');if(unit.marchLocomotionInput.speedMps!==0){unit.marchLocomotionInput.speedMps=0;unit.setLocomotion(unit.marchLocomotionInput);}unit.locomotion.snap();unit.setWorldPosition(unit.marchTarget,unit.position.z,this.terrain);
        }
        if(unit.marchArrived)this.arrived++;
      }
      // Animation/locomotion has now produced the authoritative pose. Update
      // all living hitboxes together and rebuild the moving index once before
      // the ballistic fixed step sees them.
      for(const runtime of this.damageRuntimes?.values?.()||[])runtime.update();
      this.worldDestruction?.model?.reindexMoving?.();
      this.ballisticsWorld?.step(dt);
      this.worldDestruction?.update?.();
    }
    updateBuildingRepresentations(camera){this.buildingRepresentationManager.camera=camera;this.buildingRepresentationManager.update(performance.now());this.unitPresentationScheduler.update(this.units,camera,performance.now());}
    queryBuildingSegment(start,end){return this.buildingRepresentationManager.index.querySegment(start,end);}
    createBallisticsWorld(){return this.ballisticsWorld;}
    setAIModel(unitOrId,modelId,options={}){return this.ai.setModel(unitOrId,modelId,options);}
    connectUnitWeapon(unit){const previous=unit.weapons.onShot;this._weaponCallbacks.set(unit.id,previous);unit.weapons.onShot=event=>{if(typeof previous==='function')previous(event);this.fireUnitWeapon(unit,event);};}
    fireUnitWeapon(unit,event){if(unit.alive===false)return false;const active=unit.weapons.active,target=unit.weapons.aimTargetWorld;if(!active?.muzzle||!target)return false;const muzzle=active.muzzle.clone();active.root.localToWorld(muzzle);const direction=new THREE.Vector3(target.x-muzzle.x,target.y-muzzle.y,target.z-muzzle.z).normalize(),kind=active.item.definitionId,map={carbine:['minimi','556-ball',.55],rifle:['minimi','556-ball',.55],support_gun:['minimi','556-ball',.9],marksman_rifle:['mag','762-ball',.3],heavy_support_gun:['mag','762-ball',1.1],sidearm:['pistol','9mm-ball',1.8]},profile=map[kind]||map.rifle,seed=(this.seed^Math.imul(event.id+1,2654435761)^this.hashUnitId(unit.id))>>>0,random=new R.SeededRandom(seed),spread=profile[2]*Math.PI/180,r=Math.sqrt(random.next())*Math.tan(spread),a=random.next()*Math.PI*2,up=Math.abs(direction.y)<.95?new THREE.Vector3(0,1,0):new THREE.Vector3(1,0,0),right=new THREE.Vector3().crossVectors(direction,up).normalize(),vertical=new THREE.Vector3().crossVectors(right,direction).normalize();direction.addScaledVector(right,Math.cos(a)*r).addScaledVector(vertical,Math.sin(a)*r).normalize();return this.ballisticsWorld.fire({id:unit.id+':'+event.id,weapon:profile[0],ammo:profile[1],position:muzzle.toArray(),direction:direction.toArray(),tick:this.ballisticsWorld.tick,seed});}
    hashUnitId(id){let h=2166136261;for(let i=0;i<id.length;i++)h=Math.imul(h^id.charCodeAt(i),16777619);return h>>>0;}
    damageBuilding(runtime,request){return this.worldDestruction.queueDamage(runtime,request);}
    runtimeStats(){return {buildings:this.buildingRepresentationManager.stats(),destruction:this.worldDestruction.stats(),ai:{models:this.ai?.entries?.size||0,diagnostics:this.ai?.diagnostics?.length||0,contacts:[...(this.ai?.entries?.values?.()||[])].filter(e=>e.memory).length},ballistics:this.ballisticsWorld?.metrics||null,buildingStats:this.buildings.map(b=>b.stats())};}
    dispose(){this.ai?.dispose();for(const unit of this.units){unit.weapons.onShot=this._weaponCallbacks.get(unit.id)||null;}this._weaponCallbacks.clear();for(const runtime of this.damageRuntimes.values())runtime.dispose();this.damageRuntimes.clear();this.ballisticsWorld?.dispose();this.units.forEach(u=>u.dispose());this.units=[];this.worldDestruction.dispose();this.buildingRepresentationManager.dispose();this.buildings=[];if(this.settlementVisuals){const geometries=new Set(),materials=new Set();this.settlementVisuals.traverse(o=>{if(o.isInstancedMesh&&o.dispose)o.dispose();if(o.geometry)geometries.add(o.geometry);if(o.material)(Array.isArray(o.material)?o.material:[o.material]).forEach(material=>materials.add(material));});geometries.forEach(geometry=>geometry.dispose());materials.forEach(material=>material.dispose());this.root.remove(this.settlementVisuals);this.settlementVisuals=null;}this.instances.forEach(lod=>{this.root.remove(lod);lod.traverse(object=>{if(object.isInstancedMesh&&object.dispose)object.dispose();});});this.instances=[];this.prototypes.forEach(variants=>variants.forEach(prototype=>prototype.dispose()));this.prototypes=[];this.trees=[];this.rocks=[];this.terrain.dispose();if(this.root.parent)this.root.parent.remove(this.root);}
  };
  R.Battlefield.vegetationLayout=vegetationLayout;
  R.Battlefield.rockLayout=rockLayout;
  R.Battlefield.settlementLayout=settlementLayout;
  R.Battlefield.rockPlacementDefaults=ROCK_PLACEMENT_DEFAULTS;
  R.Battlefield.normalizeRockSettings=normalizeRockSettings;
  R.Battlefield.vegetationPrototypeCount=VEGETATION_SPECIES.length;
})();
