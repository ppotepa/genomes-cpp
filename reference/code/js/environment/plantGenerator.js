(function(){
  'use strict';
  const R=window.RTS,TAU=Math.PI*2;
  const V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z);
  const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
  function stream(seed,label){let h=seed>>>0;for(let i=0;i<label.length;i++)h=Math.imul(h^label.charCodeAt(i),16777619);return new R.SeededRandom(h>>>0);}
  const GEOMETRY_BATCH=12288,GEOMETRY_TRIANGLE_BATCH=4096,BUCKET_CHUNK_FLOATS=12285;
  const LEAF_POINTS=new Float64Array(39);
  const LEAF_ANCHOR={x:0,y:0,z:0};
  const MAPLE_LEAF_PROFILE=new Float64Array(36);
  for(let i=0;i<12;i++){
    const angle=i*TAU/12,offset=i*3;
    MAPLE_LEAF_PROFILE[offset]=Math.sin(angle);
    MAPLE_LEAF_PROFILE[offset+1]=Math.cos(angle);
    MAPLE_LEAF_PROFILE[offset+2]=i%2?.62:1;
  }
  // tube() is a synchronous primitive. Reuse its temporary vectors so every
  // bark segment avoids allocating its frame and four ring vertices.
  const TUBE_AXIS=V(),TUBE_U=V(),TUBE_V=V(),TUBE_D=V(),TUBE_E=V(),TUBE_P=V(),TUBE_Q=V(),TUBE_S=V(),TUBE_T=V();
  // Segment endpoints are consumed synchronously by tube(); reusing them
  // avoids two Vector3 allocations for every emitted skeleton branch.
  const TIMBER_A=V(),TIMBER_B=V(),BIRCH_A=V(),BIRCH_B=V(),BIRCH_END=V(),BIRCH_OFFSET=V(.01,0,0);
  const TUBE_PROFILES=Object.create(null);
  for(const sides of [3,4,5,6,8]){
    const cosine=new Float64Array(sides+1),sine=new Float64Array(sides+1);
    for(let i=0;i<=sides;i++){const angle=i*TAU/sides;cosine[i]=Math.cos(angle);sine[i]=Math.sin(angle);}
    TUBE_PROFILES[sides]={cosine,sine};
  }
  const FRUIT_RING_POINTS=Array.from({length:40},()=>V());
  const FRUIT_LATITUDE_PROFILE=new Float64Array(15),FRUIT_AZIMUTH_PROFILE=new Float64Array(24);
  for(let k=0;k<5;k++){const angle=k*Math.PI/4;FRUIT_LATITUDE_PROFILE[k*3]=Math.sin(angle);FRUIT_LATITUDE_PROFILE[k*3+1]=Math.cos(angle);FRUIT_LATITUDE_PROFILE[k*3+2]=Math.cos(angle);}
  for(let n=0;n<8;n++){const angle=n*TAU/7;FRUIT_AZIMUTH_PROFILE[n*3]=Math.cos(angle);FRUIT_AZIMUTH_PROFILE[n*3+1]=Math.sin(angle);FRUIT_AZIMUTH_PROFILE[n*3+2]=angle;}
  const FRUIT_CLUSTER_OFFSETS=new Float64Array(10),FLOWER_PETAL_QUATERNIONS=Array.from({length:5},(_,k)=>new THREE.Quaternion().setFromEuler(new THREE.Euler(Math.PI/2,0,k*TAU/5)));
  for(let k=0;k<5;k++){FRUIT_CLUSTER_OFFSETS[k*2]=Math.cos(k*2.4)*.05;FRUIT_CLUSTER_OFFSETS[k*2+1]=Math.sin(k*2.4)*.05;}
  const TIP_POINT=V(),TIP_DIRECTION=V(),LEAF_POSITION=V(),FRUIT_POSITION=V(),FRUIT_CLUSTER_POSITION=V();
  const LEAF_EULER=new THREE.Euler(),LEAF_QUATERNION=new THREE.Quaternion(),LEAF_COLOR=new THREE.Color();
  const BIRCH_BARK=new THREE.Color(0x59564e);
  const ROSE_FLOWER_COLOR=new THREE.Color(0xdf94a7),SPRING_FLOWER_COLOR=new THREE.Color(0xf6e7dc);
  const FRUIT_COLORS={apple:new THREE.Color(0xb94c35),pear:new THREE.Color(0xb8a449),cherry:new THREE.Color(0x992d30),cluster:new THREE.Color(0xd85b2e),haw:new THREE.Color(0xa93427),hip:new THREE.Color(0xcf502e),elder:new THREE.Color(0x343045),cone:new THREE.Color(0x896246),nut:new THREE.Color(0x99734c),acorn:new THREE.Color(0x927243)};
  const FRUIT_CLUSTERS={elder:5,cluster:5,cherry:5};
  const FRUIT_RADII={apple:.075,pear:.065,cherry:.025,cone:.045,nut:.035,acorn:.024};
  // Compact append-only vertex/color data as Float32 while primitives are
  // emitted. This avoids retaining boxed JS numbers alongside the final
  // Float32 geometry buffers during large procedural builds.
  class Float32ChunkBuffer{
    constructor(){this.chunks=[];this.length=0;}
    ensure(count){if(this.length%BUCKET_CHUNK_FLOATS===0)this.chunks.push(new Float32Array(BUCKET_CHUNK_FLOATS));if((this.length%BUCKET_CHUNK_FLOATS)+count>BUCKET_CHUNK_FLOATS)throw new Error('Plant buffer writes must stay aligned to the chunk size');}
    push3(a,b,c){this.ensure(3);const chunk=this.chunks[this.chunks.length-1],offset=this.length%BUCKET_CHUNK_FLOATS;chunk[offset]=a;chunk[offset+1]=b;chunk[offset+2]=c;this.length+=3;}
    push9(a,b,c,d,e,f,g,h,i){this.ensure(9);const chunk=this.chunks[this.chunks.length-1],offset=this.length%BUCKET_CHUNK_FLOATS;chunk[offset]=a;chunk[offset+1]=b;chunk[offset+2]=c;chunk[offset+3]=d;chunk[offset+4]=e;chunk[offset+5]=f;chunk[offset+6]=g;chunk[offset+7]=h;chunk[offset+8]=i;this.length+=9;}
    push(...values){for(const value of values){this.ensure(1);this.chunks[this.chunks.length-1][this.length%BUCKET_CHUNK_FLOATS]=value;this.length++;}
      return this.length;
    }
    copyNormalized(target,start,end,maxValue){
      let targetOffset=start;
      while(start<end){
        const chunkIndex=Math.floor(start/BUCKET_CHUNK_FLOATS),chunkOffset=start%BUCKET_CHUNK_FLOATS;
        const count=Math.min(end-start,BUCKET_CHUNK_FLOATS-chunkOffset),chunk=this.chunks[chunkIndex];
        for(let i=0;i<count;i++)target[targetOffset+i]=Math.max(0,Math.min(maxValue,Math.round(chunk[chunkOffset+i]*maxValue)));
        start+=count;targetOffset+=count;
      }
    }
    copyRange(target,start,end){
      let targetOffset=start;
      while(start<end){
        const chunkIndex=Math.floor(start/BUCKET_CHUNK_FLOATS),chunkOffset=start%BUCKET_CHUNK_FLOATS;
        const count=Math.min(end-start,BUCKET_CHUNK_FLOATS-chunkOffset);
        target.set(this.chunks[chunkIndex].subarray(chunkOffset,chunkOffset+count),targetOffset);
        start+=count;targetOffset+=count;
      }
    }
  }
  function* geometrySteps(positions,colors,onCopied=null){
    const g=new THREE.BufferGeometry(),positionArray=new Float32Array(positions.length),colorArray=new Uint16Array(colors.length);
    for(let start=0;start<positions.length;start+=GEOMETRY_BATCH){
      const end=Math.min(positions.length,start+GEOMETRY_BATCH);
      if(positions instanceof Float32ChunkBuffer){positions.copyRange(positionArray,start,end);colors.copyNormalized(colorArray,start,end,65535);}
      else for(let i=start;i<end;i++){positionArray[i]=positions[i];colorArray[i]=Math.max(0,Math.min(65535,Math.round(colors[i]*65535)));}
      yield;
    }
    if(onCopied)onCopied();
    // The typed arrays now own the source data. Drop these potentially large
    // ordinary-array buffers before normal and bounds passes allocate scratch.
    positions=null;colors=null;
    const pos=new THREE.BufferAttribute(positionArray,3);g.setAttribute('position',pos);g.setAttribute('color',new THREE.BufferAttribute(colorArray,3,true));
    const packedNormals=new Int16Array(positionArray.length),vertexCount=positionArray.length/3;
    let minX=Infinity,minY=Infinity,minZ=Infinity,maxX=-Infinity,maxY=-Infinity,maxZ=-Infinity;
    for(let first=0;first<vertexCount;first+=3){
      const a=first*3,b=a+3,c=b+3;
      const ax=positionArray[a],ay=positionArray[a+1],az=positionArray[a+2],bx=positionArray[b],by=positionArray[b+1],bz=positionArray[b+2],cx=positionArray[c],cy=positionArray[c+1],cz=positionArray[c+2];
      minX=Math.min(minX,ax,bx,cx);minY=Math.min(minY,ay,by,cy);minZ=Math.min(minZ,az,bz,cz);
      maxX=Math.max(maxX,ax,bx,cx);maxY=Math.max(maxY,ay,by,cy);maxZ=Math.max(maxZ,az,bz,cz);
      const cbx=positionArray[c]-positionArray[b],cby=positionArray[c+1]-positionArray[b+1],cbz=positionArray[c+2]-positionArray[b+2];
      const abx=positionArray[a]-positionArray[b],aby=positionArray[a+1]-positionArray[b+1],abz=positionArray[a+2]-positionArray[b+2];
      const nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx,length=Math.sqrt(nx*nx+ny*ny+nz*nz)||1,inverse=1/length;
      const x=nx*inverse,y=ny*inverse,z=nz*inverse;
      packedNormals[a]=Math.round(x*32767);packedNormals[a+1]=Math.round(y*32767);packedNormals[a+2]=Math.round(z*32767);packedNormals[b]=Math.round(x*32767);packedNormals[b+1]=Math.round(y*32767);packedNormals[b+2]=Math.round(z*32767);packedNormals[c]=Math.round(x*32767);packedNormals[c+1]=Math.round(y*32767);packedNormals[c+2]=Math.round(z*32767);
      if((first/3+1)%GEOMETRY_TRIANGLE_BATCH===0)yield;
    }
    const normalAttribute=new THREE.BufferAttribute(packedNormals,3,true);normalAttribute.needsUpdate=true;g.setAttribute('normal',normalAttribute);
    const center=new THREE.Vector3((minX+maxX)*.5,(minY+maxY)*.5,(minZ+maxZ)*.5);let maxRadiusSq=0;
    for(let i=0;i<vertexCount;i++){
      const offset=i*3,dx=center.x-positionArray[offset],dy=center.y-positionArray[offset+1],dz=center.z-positionArray[offset+2];
      maxRadiusSq=Math.max(maxRadiusSq,dx*dx+dy*dy+dz*dz);
      if((i+1)%GEOMETRY_BATCH===0)yield;
    }
    g.boundingSphere=new THREE.Sphere(center,Math.sqrt(maxRadiusSq));
    return g;
  }
  function* bucketGeometrySteps(bucket){
    const ownership=(bucket.ownership||[]).map((r,i,a)=>({...r,count:(i+1<a.length?a[i+1].start:bucket.p.length/3)-r.start}));
    const steps=geometrySteps(bucket.p,bucket.c,()=>{bucket.p=null;bucket.c=null;});let step=steps.next();
    while(!step.done){yield;step=steps.next();}
    step.value.userData=step.value.userData||{};step.value.userData.branchOwnership=ownership;
    return step.value;
  }
  let timberTubeObserver=null;
  function bucket(kind=null){return {p:new Float32ChunkBuffer(),c:new Float32ChunkBuffer(),count:0,kind};}
  function owner(bucket,id){if(!bucket.p)return;bucket.ownership=bucket.ownership||[];bucket.ownership.push({id,start:bucket.p.length/3});}
  function triangle(b,a,c,d,color,factor=1){
    b.p.push9(a.x,a.y,a.z,c.x,c.y,c.z,d.x,d.y,d.z);
    const r=color.r*factor,g=color.g*factor,blue=color.b*factor;
    b.c.push9(r,g,blue,r,g,blue,r,g,blue);
  }
  // Rotate a local vertex and add its anchor without allocating a Vector3.
  function transformedPoint(out,offset,x,y,z,q,p){
    const qx=q.x,qy=q.y,qz=q.z,qw=q.w,tx=2*(qy*z-qz*y),ty=2*(qz*x-qx*z),tz=2*(qx*y-qy*x);
    out[offset]=x+tx*qw+(qy*tz-qz*ty)+p.x;out[offset+1]=y+ty*qw+(qz*tx-qx*tz)+p.y;out[offset+2]=z+tz*qw+(qx*ty-qy*tx)+p.z;
  }
  function emitPoint(b,points,index,color,factor){const offset=index*3;b.p.push3(points[offset],points[offset+1],points[offset+2]);b.c.push3(color.r*factor,color.g*factor,color.b*factor);}
  function tube(b,a,z,r1,r2,color,sides=6,cap=false){
    if(b.kind==='timber'&&timberTubeObserver)timberTubeObserver();
    const axis=TUBE_AXIS.copy(z).sub(a).normalize(),u=TUBE_U.set(0,1,0).cross(axis);if(u.lengthSq()<.001)u.set(1,0,0);u.normalize();const v=TUBE_V.copy(axis).cross(u);
    const profile=TUBE_PROFILES[sides];
    for(let i=0;i<sides;i++){
      const cosAngle=profile?profile.cosine[i]:Math.cos(i*TAU/sides),sinAngle=profile?profile.sine[i]:Math.sin(i*TAU/sides),cosNext=profile?profile.cosine[i+1]:Math.cos((i+1)*TAU/sides),sinNext=profile?profile.sine[i+1]:Math.sin((i+1)*TAU/sides);
      const d=TUBE_D.copy(u).multiplyScalar(cosAngle).addScaledVector(v,sinAngle),e=TUBE_E.copy(u).multiplyScalar(cosNext).addScaledVector(v,sinNext);
      const p=TUBE_P.copy(a).addScaledVector(d,r1),q=TUBE_Q.copy(a).addScaledVector(e,r1),s=TUBE_S.copy(z).addScaledVector(d,r2),t=TUBE_T.copy(z).addScaledVector(e,r2);
      const shade=.88+.12*cosAngle;triangle(b,p,q,s,color,shade);triangle(b,q,t,s,color,shade);
      if(cap)triangle(b,z,s,t,color,1.18);
    }b.count++;
  }
  function leaf(b,p,size,width,q,color,needle,shape){
    let count=0;
    if(needle){transformedPoint(LEAF_POINTS,0,0,0,0,q,p);transformedPoint(LEAF_POINTS,3,-width,size*.38,0,q,p);transformedPoint(LEAF_POINTS,6,0,size,.035*size,q,p);transformedPoint(LEAF_POINTS,9,width,size*.38,0,q,p);count=4;}
    else if(shape==='maple'||shape==='oak'){for(let i=0;i<12;i++){const offset=i*3,sin=MAPLE_LEAF_PROFILE[offset],cos=MAPLE_LEAF_PROFILE[offset+1],r=MAPLE_LEAF_PROFILE[offset+2];transformedPoint(LEAF_POINTS,offset,sin*width*r,(.5+cos*.5*r)*size,0,q,p);}count=12;}
    else{
      transformedPoint(LEAF_POINTS,0,0,0,0,q,p);transformedPoint(LEAF_POINTS,3,-width*.7,size*.2,0,q,p);
      transformedPoint(LEAF_POINTS,6,-width,size*.48,0,q,p);transformedPoint(LEAF_POINTS,9,-width*.55,size*.78,0,q,p);
      transformedPoint(LEAF_POINTS,12,0,size,0,q,p);transformedPoint(LEAF_POINTS,15,width*.55,size*.78,0,q,p);
      transformedPoint(LEAF_POINTS,18,width,size*.48,0,q,p);transformedPoint(LEAF_POINTS,21,width*.7,size*.2,0,q,p);count=8;
    }
    transformedPoint(LEAF_POINTS,36,0,size*.46,.065*size,q,p);
    for(let i=0;i<count;i++){const factor=i%2?.95:1.03;emitPoint(b,LEAF_POINTS,i,color,factor);emitPoint(b,LEAF_POINTS,(i+1)%count,color,factor);emitPoint(b,LEAF_POINTS,12,color,factor);}b.count++;
  }
  function setFruitPoint(out,p,r,type,k,n,tall){
    const offset=k*3,azimuth=n*3;let width=FRUIT_LATITUDE_PROFILE[offset];if(type==='pear')width*=.65+.45*k/4;
    out.set(p.x+FRUIT_AZIMUTH_PROFILE[azimuth]*r*width,p.y+FRUIT_LATITUDE_PROFILE[offset+1]*r*tall,p.z+FRUIT_AZIMUTH_PROFILE[azimuth+1]*r*width);
  }
  function fruit(b,p,r,type,color){
    const tall=type==='pear'?1.6:type==='cone'?2.2:type==='hip'?1.5:1;
    for(let k=0;k<5;k++)for(let n=0;n<8;n++)setFruitPoint(FRUIT_RING_POINTS[k*8+n],p,r,type,k,n,tall);
    for(let j=0;j<4;j++)for(let i=0;i<7;i++){
      const row=j*8,next=(j+1)*8,a=FRUIT_RING_POINTS[row+i],c=FRUIT_RING_POINTS[next+i],d=FRUIT_RING_POINTS[next+i+1],e=FRUIT_RING_POINTS[row+i+1];triangle(b,a,c,d,color);triangle(b,a,d,e,color);
    }b.count++;
  }
  // World prototypes with the same deterministic DNA share immutable GPU geometry.
  // A reference count keeps shared buffers alive until the final instance is disposed.
  const worldGeometryCache=new Map();
  // Bark buffers are season-independent. The LOD is part of the key because
  // branch pruning and radial resolution deliberately differ by tier.
  const timberGeometryCache=new Map();
  // The branch/tip architecture is independent of season and LOD. Retain it
  // while any cached world variant still refers to the same exact inputs.
  const skeletonCache=new Map();
  let retainedPlantGeometryBytes=0,peakRetainedPlantGeometryBytes=0;
  function refreshRetainedPlantGeometryBytes(){
    const buffers=new Set();let bytes=0;
    const addGeometry=geometry=>{
      if(!geometry)return;
      const add=attribute=>{const buffer=attribute&&attribute.array&&attribute.array.buffer;if(buffer&&!buffers.has(buffer)){buffers.add(buffer);bytes+=buffer.byteLength;}};
      add(geometry.index);
      const attributes=geometry.attributes||{};for(const name in attributes)add(attributes[name]);
      const morphs=geometry.morphAttributes||{};for(const name in morphs)for(const attribute of morphs[name])add(attribute);
    };
    for(const entry of worldGeometryCache.values())for(const part of entry.parts)addGeometry(part.geometry);
    for(const entry of timberGeometryCache.values())addGeometry(entry.geometry);
    retainedPlantGeometryBytes=bytes;
    if(bytes>peakRetainedPlantGeometryBytes)peakRetainedPlantGeometryBytes=bytes;
  }
  function worldGeometryKey(genome,state,detail){
    const g=genome.genes;
    return [genome.species,genome.seed,state.age,state.season,detail,
      g.stature,g.spread,g.branching,g.crookedness,g.girth,g.damage,g.foliage,g.leafSize].join('|');
  }
  function timberGeometryKey(genome,age,detail){
    const g=genome.genes;
    return [genome.species,genome.seed,age,detail,g.stature,g.spread,g.branching,g.crookedness,g.girth,g.damage].join('|');
  }
  function skeletonGeometryKey(genome,age){
    const g=genome.genes;
    const version=R.Config&&R.Config.GENERATOR_VERSION||'plant-skeleton-1';
    return [version,genome.species,genome.seed,age,g.stature,g.spread,g.branching,g.crookedness,g.girth,g.damage].join('|');
  }
  function retainSkeletonGeometry(key,skeleton){
    let entry=skeletonCache.get(key);
    if(entry)entry.refs++;
    else skeletonCache.set(key,{skeleton,refs:1});
  }
  function releaseSkeletonGeometry(key){
    if(key===null)return;
    const entry=skeletonCache.get(key);
    if(!entry||--entry.refs>0)return;
    skeletonCache.delete(key);
  }
  function retainWorldGeometry(key){
    const entry=worldGeometryCache.get(key);
    if(!entry)return null;
    entry.refs++;
    return entry;
  }
  function releaseWorldGeometry(key){
    const entry=worldGeometryCache.get(key);
    if(!entry||--entry.refs>0)return;
    for(const part of entry.parts)if(part.name!=='wood'){part.geometry.dispose();part.material.dispose();}
    releaseTimberGeometry(entry.timberKey);
    releaseSkeletonGeometry(entry.skeletonKey);
    worldGeometryCache.delete(key);
    refreshRetainedPlantGeometryBytes();
  }
  function releaseTimberGeometry(key){
    if(key===null)return;
    const entry=timberGeometryCache.get(key);
    if(!entry||--entry.refs>0)return;
    entry.geometry.dispose();entry.material.dispose();timberGeometryCache.delete(key);
  }
  function discardUncommittedBuild(build){
    if(!build)return;
    const owned=build.owned||[];
    for(const part of owned){
      if(build.cacheable&&part.name==='wood')continue;
      part.geometry.dispose();part.material.dispose();
    }
    const timber=build.timberEntry;
    if(timber&&build.cacheable){
      if(build.timberRetained&&timber.refs>0)timber.refs--;
      if(timber.refs===0&&timberGeometryCache.get(build.timberKey)===timber){
        timber.geometry.dispose();timber.material.dispose();timberGeometryCache.delete(build.timberKey);
      }
    }
    if(build.root&&build.root.parent)build.root.parent.remove(build.root);
  }
  R.PlantGenerator={
    _tubeProfile(sides){const profile=TUBE_PROFILES[sides];return profile?{cosine:profile.cosine.slice(),sine:profile.sine.slice()}:null;},
    // Narrow test hook for confirming that cross-season cache hits avoid the
    // timber tube-emission path, with zero work when no observer is installed.
    _setTimberTubeObserver(observer){timberTubeObserver=typeof observer==='function'?observer:null;},
    memoryStats(){return {retainedGeometryBytes:retainedPlantGeometryBytes,peakRetainedGeometryBytes:peakRetainedPlantGeometryBytes,worldEntries:worldGeometryCache.size,timberEntries:timberGeometryCache.size};},
    // Structure is independent of season, display detail and leaf/fruit random streams.
    skeleton(genome,age=1){
      const steps=this.skeletonSteps(genome,age);let step=steps.next();
      while(!step.done)step=steps.next();
      return step.value;
    },
    *skeletonSteps(genome,age=1){
      const s=R.EnvironmentCatalog.plants[genome.species],g=genome.genes,rng=stream(genome.seed,'wood'),nodes=[],tips=[];
      const damage=stream(genome.seed,'branch-damage'),girth=Number.isFinite(g.girth)?g.girth:.5;
      const growth=.12+.88*Math.pow(clamp(age,.05,1),.72),h=s.height*(.78+.44*g.stature)*growth,w=h*s.width*(.75+.5*g.spread),radius=h*(s.form==='shrub'?.015:.019)*(.55+.9*girth);
      // Keep recursion scratch local to this generator: multiple async plants
      // may interleave at yields, but each branch frame remains independent.
      const branchA=Array.from({length:4},()=>V()),branchD=Array.from({length:4},()=>V()),branchEnd=Array.from({length:4},()=>V()),branchChild=Array.from({length:4},()=>V()),branchStart=Array.from({length:4},()=>V()),branchDirection=Array.from({length:4},()=>V());
      function add(a,b,r1,r2,depth){const n={id:nodes.length,a:a.toArray(),b:b.toArray(),r1,r2,depth};nodes.push(n);return b;}
      function* branch(start,dir,len,r,depth){
        const a=branchA[depth].copy(start),d=branchD[depth].copy(dir).normalize(),end=branchEnd[depth],child=branchChild[depth],parts=3;
        // A broken limb ends in a short bare stub; it cannot carry floating leaves.
        if(damage.next()<(g.damage||0)*(depth===1?.32:.18)){
          end.copy(a).addScaledVector(d,len*(.12+damage.next()*.30));
          add(a,end,r,r*.72,depth);nodes[nodes.length-1].broken=true;
          yield;return;
        }
        if(s.form==='weeping'&&depth===3){len*=2.4;d.y=-.65;d.normalize();}
        for(let k=0;k<parts;k++){
          d.x+=(rng.next()-.5)*g.crookedness*.32;d.z+=(rng.next()-.5)*g.crookedness*.32;
          d.y+=(s.form==='weeping'&&depth>1?-.32:.06);d.normalize();
          end.copy(a).addScaledVector(d,len/parts);end.y=Math.max(.08*h,end.y);
          add(a,end,r*(1-k/parts*.65),r*(1-(k+1)/parts*.65),depth);const nodeId=nodes.length-1;
          if(nodes.length%8===0)yield;
          if(depth<3&&k>0){const angle=rng.next()*TAU;child.set(Math.cos(angle)*.75,d.y*.45+s.rise*.5,Math.sin(angle)*.75).addScaledVector(d,.55).normalize();yield* branch(end,child,len*(.40+rng.next()*.16),r*.43,depth+1);}
          if(depth>=2&&k>0)tips.push({p:end.toArray(),direction:d.toArray(),id:tips.length,nodeId});a.copy(end);
        }
      }
      const stems=s.form==='shrub'?5:1,trunkHeight=['broad','orchard','weeping'].includes(s.form)?.72:1;
      for(let stem=0;stem<stems;stem++){
        const stemSlot=stem%branchStart.length;
        let a=branchStart[stemSlot].set(0,0,0),steps=s.needle?12:8,phase=rng.next()*TAU;
        for(let i=0;i<steps;i++){
          const t=(i+1)/steps,lean=s.form==='shrub'?w*.18*t: h*g.crookedness*.055*t*t;
          const end=V(Math.cos(phase)*lean+(rng.next()-.5)*h*.014,t*h*trunkHeight*(stems>1?.7+rng.next()*.12:1),Math.sin(phase)*lean);
          add(a,end,radius*(1-i/steps)* (stems>1?.6:1),Math.max(radius*.06,radius*(1-t))*(stems>1?.6:1),0);
          if(nodes.length%8===0)yield;
          if(t>s.crownBase&&t<.97){
            const count=s.form==='conifer'?4:2+Math.floor(g.branching*2),ct=(t-s.crownBase)/(1-s.crownBase);
            for(let j=0;j<count;j++){
              const angle=phase+i*2.399+j*TAU/count+(rng.next()-.5)*.5;
              const envelope=s.form==='conifer'?1-ct*.85:Math.pow(Math.sin(Math.PI*(.12+.8*ct)),.6);
              const length=w*(s.form==='orchard'?.68:.53)*envelope*(.75+rng.next()*.4)/(stems>1?1.65:1);
              yield* branch(end,branchDirection[stemSlot].set(Math.cos(angle),s.rise+(rng.next()-.5)*.3,Math.sin(angle)),length,radius*(1-t)*.46,1);
            }
          }a=end;
        }
      }
      // Metadata only: do not consume RNG or change the generated coordinates.
      const endpoints=new Map();for(const n of nodes){n.parent=endpoints.get(n.a.join(','))??null;endpoints.set(n.b.join(','),n.id);}
      return {nodes,tips,height:h,width:w};
    },
    createLods(input,state={}){
      const genome=R.PlantGenome.create(input.species,input.seed,input.genes),age=clamp(Number.isFinite(state.age)?state.age:1,.05,1),variants=[];
      try{
        variants.push(this.create(genome,{...state,age,detail:'world'}));
        const skeleton=variants[0].skeleton;
        for(const detail of ['distant','far'])variants.push(this.create(genome,{...state,age,detail,skeleton,_sharedDeterministicSkeleton:true}));
        return variants;
      }catch(error){variants.forEach(variant=>variant.dispose());throw error;}
    },
    async createLodsAsync(input,state={},yieldFrame,check=null,budgetMs=4){
      const genome=R.PlantGenome.create(input.species,input.seed,input.genes),age=clamp(Number.isFinite(state.age)?state.age:1,.05,1),variants=[];
      try{
        variants.push(await this.createAsync(genome,{...state,age,detail:'world'},yieldFrame,check,budgetMs));
        const skeleton=variants[0].skeleton;
        for(const detail of ['distant','far'])variants.push(await this.createAsync(genome,{...state,age,detail,skeleton,_sharedDeterministicSkeleton:true},yieldFrame,check,budgetMs));
        return variants;
      }catch(error){variants.forEach(variant=>variant.dispose());throw error;}
    },
    create(input,state={}){
      const steps=this.createSteps(input,state);let step=steps.next();
      while(!step.done)step=steps.next();
      return step.value;
    },
    async createAsync(input,state={},yieldFrame,check=null,budgetMs=4){
      if(typeof yieldFrame!=='function')throw new TypeError('Async plant generation requires a frame-yield callback.');
      const budget=Number.isFinite(budgetMs)?Math.max(0,budgetMs):4,now=()=>typeof performance!=='undefined'&&performance.now?performance.now():Date.now();
      const build={owned:[],cacheable:false,timberKey:null,timberEntry:null,timberRetained:false,root:null,cacheCommitted:false};
      const steps=this.createSteps(input,{...state,_buildState:build});let step;
      try{
        step=steps.next();let sliceStart=budget>0?now():0,checkpointsSinceClock=0;
        while(!step.done){
          if(check)check();
          // Generator checkpoints already cap each uninterrupted unit of work.
          // Avoid a high-resolution clock read at every checkpoint; check the
          // frame budget every eight checkpoints while retaining cancellation
          // checks at each one.
          const timeDue=budget>0&&++checkpointsSinceClock>=8;
          if(budget===0||(timeDue&&(checkpointsSinceClock=0,now()-sliceStart>=budget))){await yieldFrame();sliceStart=budget>0?now():0;if(check)check();}
          step=steps.next();
        }
        return step.value;
      }catch(error){
        if(steps&&typeof steps.return==='function')steps.return();
        if(!build.cacheCommitted)discardUncommittedBuild(build);
        throw error;
      }
    },
    *createSteps(input,state={}){
      const genome=R.PlantGenome.create(input.species,input.seed,input.genes),s=R.EnvironmentCatalog.plants[genome.species],g=genome.genes;
      const age=clamp(Number.isFinite(state.age)?state.age:1,.05,1),season=['spring','summer','autumn','winter'].includes(state.season)?state.season:'summer',detail=['world','distant','far'].includes(state.detail)?state.detail:'high';
      const world=detail!=='high',sharedSkeleton=state._sharedDeterministicSkeleton===true,cacheable=world&&(!state.skeleton||sharedSkeleton),cacheKey=cacheable?worldGeometryKey(genome,{age,season},detail):null,timberKey=cacheable?timberGeometryKey(genome,age,detail):null,skeletonKey=cacheable?skeletonGeometryKey(genome,age):null;
      const build=state._buildState||null;if(build){build.cacheable=cacheable;build.timberKey=timberKey;}
      // The lab may supply a custom skeleton, so only use the key-only fast path
      // when the generator owns the deterministic skeleton choice.
      const cached=cacheable?retainWorldGeometry(cacheKey):null;
      if(cached){
        const root=new THREE.Group();root.name=s.name;
        for(const part of cached.parts){const mesh=new THREE.Mesh(part.geometry,part.material);mesh.name=part.name;mesh.castShadow=true;mesh.receiveShadow=true;root.add(mesh);}
        const stats={...cached.stats};
        let disposed=false;
        return {root,genome,state:{age,season,detail},skeleton:cached.skeleton,stats,dispose(){if(disposed)return;disposed=true;releaseWorldGeometry(cacheKey);if(root.parent)root.parent.remove(root);}};
      }
      const retainedSkeleton=cacheable&&skeletonCache.get(skeletonKey),skeleton=state.skeleton||(retainedSkeleton&&retainedSkeleton.skeleton)||(yield* this.skeletonSteps(genome,age)),timberCached=cacheable?timberGeometryCache.get(timberKey):null,wood=bucket('timber'),foliage=bucket(),fruits=bucket(),flowers=bucket(),rng=stream(genome.seed,'foliage'),fr=stream(genome.seed,'fruit');
      const visible=s.evergreen||season!=='winter',density=season==='autumn'&&!s.evergreen?.63:season==='spring'?.78:1;
      const bark=timberCached?null:new THREE.Color(s.bark).convertSRGBToLinear(),leafColor=visible?new THREE.Color(season==='autumn'?s.autumn:s.leaf).convertSRGBToLinear():null;
      if(season==='spring')leafColor.lerp(new THREE.Color(0xa4be62).convertSRGBToLinear(),.25);
      const lodLevel=detail==='world'?0:detail==='distant'?1:detail==='far'?2:0;
      if(!timberCached){
        let nodeIndex=0;
        for(const n of skeleton.nodes){
          if((lodLevel===1&&n.depth>=3)||(lodLevel===2&&n.depth>=2))continue;
          const sides=n.depth===0?(lodLevel===0?8:5):(lodLevel===0?5:lodLevel===1?4:3);
          owner(wood,n.id);tube(wood,TIMBER_A.fromArray(n.a),TIMBER_B.fromArray(n.b),n.r1,n.r2,bark,sides,n.broken===true);
          if(++nodeIndex%12===0)yield;
        }
        // Birch lenticels are geometry, so the generator needs no external textures.
        if(s.id==='birch'&&lodLevel<2){const trunks=skeleton.nodes.filter(n=>n.depth===0);for(let i=lodLevel===0?0:1;i<20;i+=lodLevel===0?1:2){const n=trunks[Math.floor(i/20*8)],a=BIRCH_A.fromArray(n.a).lerp(BIRCH_B.fromArray(n.b),(i*.71)%1),end=BIRCH_END.copy(a).add(BIRCH_OFFSET.set(.01,skeleton.height*.007,0));tube(wood,a,end,n.r1*1.01,n.r1,BIRCH_BARK,lodLevel===0?8:4);}}
      }
      const compoundLeaf=['ash','rowan','elder','rose'].includes(s.id),leafCount=s.needle?14:compoundLeaf?(s.form==='shrub'?2:4):(s.form==='shrub'?6:10);
      const crownScale=Math.pow(Math.max(1,s.height/6),.8),leafSize=(s.needle?.42:.25)*crownScale*(.65+g.leafSize*.8)*Math.pow(skeleton.height/s.height,.4)*(detail==='high'?1:lodLevel===0?1.45:lodLevel===1?1.9:2.55),spread=(s.needle?.30:.46)*crownScale;
      let tipIndex=0,foliageWork=0;
      for(const tip of skeleton.tips){
        for(const bucket of [wood,foliage,fruits,flowers])owner(bucket,tip.nodeId??0);
        if(++tipIndex%4===0)yield;
        const p=TIP_POINT.fromArray(tip.p),dir=TIP_DIRECTION.fromArray(tip.direction);
        if(visible)for(let i=0;i<leafCount;i++){
          const keep=rng.next(),angle=rng.next()*TAU,offset=rng.next(),scale=.75+rng.next()*.5;
          const tiltX=(rng.next()-.5)*2.5,tiltZ=(rng.next()-.5)*2.5;
          const lodKeep=detail==='high'||(lodLevel===0?i%2===0:lodLevel===1?i%6===0:(i===0&&tip.id%2===0));
          if(keep>density*(.5+g.foliage*.5)||!lodKeep)continue;
          const q=LEAF_QUATERNION.setFromEuler(LEAF_EULER.set(tiltX,angle,tiltZ));
          // A polygon represents a leaf cluster / needle spray on large trees.
          // Scale the proxy with crown size so distant crowns retain coverage.
          const size=leafSize,along=(offset-.5)*spread,radialX=Math.cos(angle)*spread*.55,radialY=(offset-.5)*spread*.5,radialZ=Math.sin(angle)*spread*.55;
          const pos=LEAF_POSITION.set(p.x+dir.x*along+radialX,p.y+dir.y*along+radialY,p.z+dir.z*along+radialZ),lc=LEAF_COLOR.copy(leafColor).multiplyScalar(.78+keep*.35);
          if(compoundLeaf){for(let k=0;k<5;k++){const lx=(k%2?1:-1)*size*.22,ly=k*size*.16,lz=0,tx=2*(q.y*lz-q.z*ly),ty=2*(q.z*lx-q.x*lz),tz=2*(q.x*ly-q.y*lx);LEAF_ANCHOR.x=pos.x+lx+tx*q.w+(q.y*tz-q.z*ty);LEAF_ANCHOR.y=pos.y+ly+ty*q.w+(q.z*tx-q.x*tz);LEAF_ANCHOR.z=pos.z+lz+tz*q.w+(q.x*ty-q.y*tx);leaf(foliage,LEAF_ANCHOR,size*.70,size*.19,q,lc,false);}}
          else leaf(foliage,pos,size*scale,s.needle?size*.22:size*(s.id==='willow'?.13:.43),q,lc,s.needle,s.id);
          if(++foliageWork%32===0)yield;
        }
        // World LODs never emit fruit, so skip their independent RNG stream.
        // High-detail eligibility keeps its own stream; foliage detail never
        // changes fruit placement in that variant.
        if(world)continue;
        const chance=fr.next(),a=fr.next()*TAU;
        if(age<.45||s.fruit==='none'||chance>g.fruiting*.18)continue;
        const position=FRUIT_POSITION.set(p.x+Math.cos(a)*.05,p.y-.08,p.z+Math.sin(a)*.05);
        const blooming=season==='spring'&&['apple','pear','cherry','haw','hip','elder'].includes(s.fruit);
        if(blooming){const color=s.id==='rose'?ROSE_FLOWER_COLOR:SPRING_FLOWER_COLOR;for(let k=0;k<5;k++){leaf(foliage,position,.075,.033,FLOWER_PETAL_QUATERNIONS[k],color,false);foliage.count--;flowers.count++;}if(++foliageWork%32===0)yield;continue;}
        const ripe=season==='autumn'||(season==='summer'&&['cherry','elder','cone'].includes(s.fruit));
        const persistent=['hip','haw','cluster','cone'].includes(s.fruit);
        if(!ripe&&!(season==='winter'&&persistent))continue;
        const type=s.fruit,color=FRUIT_COLORS[type]||FRUIT_COLORS.apple;
        const cluster=FRUIT_CLUSTERS[type]||1,radius=FRUIT_RADII[type]||.025;
        for(let k=0;k<cluster;k++){const offset=k*2,fp=FRUIT_CLUSTER_POSITION.set(position.x+FRUIT_CLUSTER_OFFSETS[offset],position.y-k*.009,position.z+FRUIT_CLUSTER_OFFSETS[offset+1]);fruit(fruits,fp,radius,type,color);tube(wood,p,fp,.003,.002,bark,3);if(++foliageWork%32===0)yield;}
      }
      const root=new THREE.Group();root.name=s.name;const owned=[];if(build){build.root=root;build.owned=owned;}
      if(cacheable){
        let timber=timberCached;
        if(!timber){
          const geometryBuild=bucketGeometrySteps(wood);let geometryStep=geometryBuild.next();
          while(!geometryStep.done){yield;geometryStep=geometryBuild.next();}
          const geo=geometryStep.value,mat=new THREE.MeshStandardMaterial({vertexColors:true,roughness:.9,side:THREE.FrontSide});
          timber={refs:0,geometry:geo,material:mat,branches:wood.count};timberGeometryCache.set(timberKey,timber);
        }
        if(build)build.timberEntry=timber;
        const timberMesh=new THREE.Mesh(timber.geometry,timber.material);timberMesh.name='wood';timberMesh.castShadow=true;timberMesh.receiveShadow=true;root.add(timberMesh);owned.push(timberMesh);timber.refs++;if(build)build.timberRetained=true;
        for(const [name,b] of [['foliage',foliage],['fruit',fruits],['flowers',flowers]])if(b.p.length){
          const geometryBuild=bucketGeometrySteps(b);let geometryStep=geometryBuild.next();
          while(!geometryStep.done){yield;geometryStep=geometryBuild.next();}
          const geo=geometryStep.value,mat=new THREE.MeshStandardMaterial({vertexColors:true,roughness:.9,side:name==='fruit'?THREE.FrontSide:THREE.DoubleSide});
          if(name==='foliage'&&detail==='far')mat.shadowSide=THREE.FrontSide;
          const mesh=new THREE.Mesh(geo,mat);mesh.name=name;mesh.castShadow=true;mesh.receiveShadow=true;root.add(mesh);owned.push(mesh);
        }
      }else{
        for(const [name,b] of [['wood',wood],['foliage',foliage],['fruit',fruits],['flowers',flowers]])if(b.p.length){
          const geometryBuild=bucketGeometrySteps(b);let geometryStep=geometryBuild.next();
          while(!geometryStep.done){yield;geometryStep=geometryBuild.next();}
          const geo=geometryStep.value,mat=new THREE.MeshStandardMaterial({vertexColors:true,roughness:.9,side:name==='wood'||name==='fruit'?THREE.FrontSide:THREE.DoubleSide});
          if(name==='foliage'&&detail==='far')mat.shadowSide=THREE.FrontSide;
          const mesh=new THREE.Mesh(geo,mat);mesh.name=name;mesh.castShadow=true;mesh.receiveShadow=true;root.add(mesh);owned.push(mesh);
        }
      }
      const timber=cacheable?timberGeometryCache.get(timberKey):null,branchCount=timber?timber.branches:wood.count;
      const stats={branches:branchCount,leaves:foliage.count,fruits:fruits.count,flowers:flowers.count,triangles:owned.reduce((n,m)=>n+m.geometry.attributes.position.count/3,0),drawCalls:owned.length};
      if(cacheable){retainSkeletonGeometry(skeletonKey,skeleton);worldGeometryCache.set(cacheKey,{refs:1,parts:owned.map(mesh=>({name:mesh.name,geometry:mesh.geometry,material:mesh.material})),timberKey,skeletonKey,skeleton,stats});refreshRetainedPlantGeometryBytes();if(build)build.cacheCommitted=true;}
      let disposed=false;
      return {root,genome,state:{age,season,detail},skeleton,stats,dispose(){if(disposed)return;disposed=true;if(cacheable)releaseWorldGeometry(cacheKey);else for(const m of owned){m.geometry.dispose();m.material.dispose();}if(root.parent)root.parent.remove(root);}};
    }
  };
})();
