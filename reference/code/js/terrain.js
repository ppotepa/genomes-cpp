(function () {
  'use strict';
  const R=window.RTS;
  function clampGridCoordinate(value,limit){if(value<0)return 0;if(value>limit)return limit;return value===0?0:value;}
  function TerrainSystem(scene,options={}){this.scene=scene;this.mapSize=options.mapSize||R.Config.MAP_SIZE;this.segments=options.segments||R.Config.TERRAIN_SEGMENTS;this.battlefield=!!options.battlefield;this.chunkSegments=options.chunkSegments||(this.battlefield?40:50);this.mesh=null;this.gridMesh=null;this.chunkGeometries=[];this.gridMaterial=null;this.showSmallGrid=false;this.showBigGrid=false;this.heights=null;this._step=0;this._half=this.mapSize/2;}
  // Split the original indexed plane without rebuilding its vertex attributes. Each chunk
  // keeps the original positions/normals/colors (including shared edge vertices), but has
  // its own cell indices and conservative bounds so Three.js can frustum-cull it.
  function* terrainChunkSteps(source,segments,chunkSegments){
    const position=source.attributes.position,pos=position.array,stride=position.itemSize,srcIndex=source.index.array;
    const chunks=[];
    for(let z0=0,cz=0;z0<segments;z0+=chunkSegments,cz++){
      const z1=Math.min(segments,z0+chunkSegments);
      for(let x0=0,cx=0;x0<segments;x0+=chunkSegments,cx++){
        const x1=Math.min(segments,x0+chunkSegments),cells=(x1-x0)*(z1-z0);
        const ids=new srcIndex.constructor(cells*6);let at=0;
        for(let z=z0;z<z1;z++)for(let x=x0;x<x1;x++){
          const start=(z*segments+x)*6;
          for(let i=0;i<6;i++)ids[at++]=srcIndex[start+i];
        }
        const geometry=new THREE.BufferGeometry();
        for(const name in source.attributes)geometry.setAttribute(name,source.attributes[name]);
        geometry.setIndex(new THREE.BufferAttribute(ids,1));
        geometry.setDrawRange(source.drawRange.start,source.drawRange.count);
        let minX=Infinity,minY=Infinity,minZ=Infinity,maxX=-Infinity,maxY=-Infinity,maxZ=-Infinity;
        for(let z=z0;z<=z1;z++){
          for(let x=x0;x<=x1;x++){
            const offset=(z*(segments+1)+x)*stride,px=pos[offset],py=pos[offset+1],pz=pos[offset+2];
            if(px<minX)minX=px;if(py<minY)minY=py;if(pz<minZ)minZ=pz;
            if(px>maxX)maxX=px;if(py>maxY)maxY=py;if(pz>maxZ)maxZ=pz;
          }
          if((z-z0+1)%16===0&&z<z1)yield;
        }
        const box=new THREE.Box3(new THREE.Vector3(minX,minY,minZ),new THREE.Vector3(maxX,maxY,maxZ));
        geometry.boundingBox=box;
        const center=new THREE.Vector3((minX+maxX)*.5,(minY+maxY)*.5,(minZ+maxZ)*.5);let radiusSq=0;
        for(let z=z0;z<=z1;z++){
          for(let x=x0;x<=x1;x++){
            const offset=(z*(segments+1)+x)*stride,dx=pos[offset]-center.x,dy=pos[offset+1]-center.y,dz=pos[offset+2]-center.z;
            radiusSq=Math.max(radiusSq,dx*dx+dy*dy+dz*dz);
          }
          if((z-z0+1)%16===0&&z<z1)yield;
        }
        geometry.boundingSphere=new THREE.Sphere(center,Math.sqrt(radiusSq));
        chunks.push({geometry,x:cx,z:cz});
        // Let battlefield generation yield between bounded groups of chunk work.
        yield;
      }
    }
    return chunks;
  }
  // Incremental equivalent of PlaneGeometry's row-major vertex/index construction.
  // Transform the plane into its ground orientation while writing instead of doing a
  // second full-geometry rotateX pass after construction.
  function* terrainPlaneSteps(size,segments){
    const geometry=new THREE.BufferGeometry(),rowWidth=segments+1,vertexCount=rowWidth*rowWidth;
    const positions=new Float32Array(vertexCount*3),uvs=new Float32Array(vertexCount*2),IndexArray=vertexCount>65536?Uint32Array:Uint16Array,indices=new IndexArray(segments*segments*6);
    const half=size/2,segment=size/segments,cos=Math.cos(-Math.PI/2),sin=Math.sin(-Math.PI/2);
    for(let iy=0;iy<=segments;iy++){
      const localY=-(iy*segment-half);
      for(let ix=0;ix<=segments;ix++){
        const vertex=iy*rowWidth+ix,po=vertex*3,uo=vertex*2,x=ix*segment-half;
        positions[po]=x;positions[po+1]=localY*cos;positions[po+2]=localY*sin;
        uvs[uo]=ix/segments;uvs[uo+1]=1-(iy/segments);
      }
      if((iy+1)%16===0&&iy<segments)yield;
    }
    let indexOffset=0;
    for(let iy=0;iy<segments;iy++){
      for(let ix=0;ix<segments;ix++){
        const a=ix+rowWidth*iy,b=ix+rowWidth*(iy+1),c=(ix+1)+rowWidth*(iy+1),d=(ix+1)+rowWidth*iy;
        indices[indexOffset++]=a;indices[indexOffset++]=b;indices[indexOffset++]=d;indices[indexOffset++]=b;indices[indexOffset++]=c;indices[indexOffset++]=d;
      }
      if((iy+1)%16===0&&iy+1<segments)yield;
    }
    geometry.setIndex(new THREE.BufferAttribute(indices,1));geometry.setDrawRange(0,Infinity);
    geometry.setAttribute('position',new THREE.BufferAttribute(positions,3));
    geometry.setAttribute('uv',new THREE.BufferAttribute(uvs,2));
    return geometry;
  }
  // Same indexed face-normal accumulation as BufferGeometry.computeVertexNormals,
  // split into bounded triangle batches so async world creation can yield here too.
  function* terrainNormalSteps(geometry,segments,batchRows){
    const position=geometry.attributes.position,pos=position.array,index=geometry.index.array;
    const normal=new Float32Array(position.count*3),trianglesPerRow=segments*2,rowWidth=segments+1;
    const batchTriangles=Math.max(trianglesPerRow,trianglesPerRow*batchRows),limit=index.length;
    let batchEnd=batchTriangles*3;
    for(let i=0;i<limit;i+=3){
      const a=index[i],b=index[i+1],c=index[i+2];
      const ai0=a*3,bi0=b*3,ci0=c*3;
      const ax=pos[ai0],ay=pos[ai0+1],az=pos[ai0+2];
      const bx=pos[bi0],by=pos[bi0+1],bz=pos[bi0+2];
      const cx=pos[ci0],cy=pos[ci0+1],cz=pos[ci0+2];
      const cbx=cx-bx,cby=cy-by,cbz=cz-bz,abx=ax-bx,aby=ay-by,abz=az-bz;
      const nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx;
      const ai=a*3,bi=b*3,ci=c*3;
      normal[ai]+=nx;normal[ai+1]+=ny;normal[ai+2]+=nz;
      normal[bi]+=nx;normal[bi+1]+=ny;normal[bi+2]+=nz;
      normal[ci]+=nx;normal[ci+1]+=ny;normal[ci+2]+=nz;
      if(i+3>=batchEnd&&i+3<limit){yield;batchEnd+=batchTriangles*3;}
    }
    for(let i=0;i<position.count;i++){
      const offset=i*3,x=normal[offset],y=normal[offset+1],z=normal[offset+2],length=Math.sqrt(x*x+y*y+z*z);
      if(length>0){const inverseLength=1/length;normal[offset]=x*inverseLength;normal[offset+1]=y*inverseLength;normal[offset+2]=z*inverseLength;}
      if((i+1)%(rowWidth*16)===0)yield;
    }
    geometry.setAttribute('normal',new THREE.BufferAttribute(normal,3));
  }
  TerrainSystem.prototype.makeHeightFunction = function (seed) {
    var noise = new RTS.SeededPerlin2D(seed);
    var C = RTS.Config;
    if(this.battlefield)return function(x,z){return noise.noise(x/65+11,z/65-7)*7+noise.noise(x/28-19,z/28+31)*2+noise.noise(x/12+5,z/12+8)*.35;};

    function makeFbmPlan(octaves,lacunarity,gain){
      const amplitudes=new Float64Array(octaves),frequencies=new Float64Array(octaves);
      let amplitude=.5,frequency=1,total=0;
      for(let i=0;i<octaves;i++){amplitudes[i]=amplitude;frequencies[i]=frequency;total+=amplitude;amplitude*=gain;frequency*=lacunarity;}
      return {amplitudes,frequencies,total};
    }
    const warpPlan=makeFbmPlan(3,2.02,.50),continentalPlan=makeFbmPlan(5,2.00,.53),
      hillsPlan=makeFbmPlan(5,2.08,.50),detailPlan=makeFbmPlan(4,2.15,.46),ridgePlan=makeFbmPlan(4,2.00,.50);
    function fbm(x, z, plan) {
      var value = 0;
      for (var i = 0; i < plan.amplitudes.length; i++) {
        value += noise.noise(x * plan.frequencies[i], z * plan.frequencies[i]) * plan.amplitudes[i];
      }

      return value / Math.max(plan.total, 0.0001);
    }

    return function (x, z) {
      var warpX = fbm(x / 850 + 18.1, z / 850 - 7.4, warpPlan) * 130;
      var warpZ = fbm(x / 850 - 11.7, z / 850 + 21.8, warpPlan) * 130;

      var wx = x + warpX;
      var wz = z + warpZ;

      var continental = fbm(wx / 1350, wz / 1350, continentalPlan);
      var hills = fbm(wx / 430 + 31.3, wz / 430 - 16.2, hillsPlan);
      var detail = fbm(wx / 120 - 9.0, wz / 120 + 12.0, detailPlan);

      var ridgeNoise = fbm(wx / 620 + 4.2, wz / 620 + 15.6, ridgePlan);
      var ridged = 1 - Math.abs(ridgeNoise);
      var ridgeMask = THREE.MathUtils.clamp((continental + 0.12) * 1.25, 0, 1);
      var plainBase = Math.sign(continental) * Math.pow(Math.abs(continental), 1.35);
      var hillMask = THREE.MathUtils.smoothstep(continental, -0.08, 0.42);

      var h = 0;
      h += plainBase * 92;
      h += hills * (26 + hillMask * 44);
      h += (ridged - 0.56) * 36 * ridgeMask;
      h += detail * 7;

      var edge = Math.max(Math.abs(x), Math.abs(z)) / C.HALF_MAP;
      h -= THREE.MathUtils.smoothstep(edge, 0.83, 1.0) * 8;

      return h;
    };
  };


  TerrainSystem.prototype._buildSteps=function*(seed,preparedTerrain=null){
    const C=R.Config;this.dispose();this._step=this.mapSize/this.segments;this._half=this.mapSize/2;this.heightSampler=preparedTerrain?null:this.makeHeightFunction(seed);
    let g;
    if(preparedTerrain){
      g=new THREE.BufferGeometry();const vertexCount=(this.segments+1)*(this.segments+1),IndexArray=vertexCount>65536?Uint32Array:Uint16Array;
      g.setIndex(new THREE.BufferAttribute(preparedTerrain.indices instanceof IndexArray?preparedTerrain.indices:new IndexArray(preparedTerrain.indices),1));g.setDrawRange(0,Infinity);
      g.setAttribute('position',new THREE.BufferAttribute(preparedTerrain.positions,3));g.setAttribute('uv',new THREE.BufferAttribute(preparedTerrain.uvs,2));
      g.setAttribute('normal',new THREE.BufferAttribute(preparedTerrain.normals,3));g.setAttribute('color',new THREE.BufferAttribute(preparedTerrain.colors,3));
      this.heights=preparedTerrain.heights;
    }else{
      const planeSteps=terrainPlaneSteps(this.mapSize,this.segments);let planeStep=planeSteps.next();
      while(!planeStep.done){yield;planeStep=planeSteps.next();}
      g=planeStep.value;
    }
    const position=g.attributes.position,pos=position.array;
    const rowWidth=this.segments+1;
    if(!preparedTerrain){
      this.heights=new Float32Array(position.count);
      for(let i=0;i<position.count;i++){const offset=i*3,y=this.heightSampler(pos[offset],pos[offset+2]);pos[offset+1]=y;this.heights[i]=pos[offset+1];if((i+1)%(rowWidth*16)===0)yield;}
      pos.needsUpdate=true;
      const normalSteps=terrainNormalSteps(g,this.segments,16);let normalStep=normalSteps.next();
      while(!normalStep.done){yield;normalStep=normalSteps.next();}
    }
    // The source geometry is discarded after chunking; each renderable chunk
    // receives its own exact bounds in terrainChunkSteps, so a full-field scan is redundant.
    const normal=g.attributes.normal.array,col=preparedTerrain?g.attributes.color.array:new Float32Array(position.count*3),c=new THREE.Color();
    const grass=new THREE.Color(0x708454).convertSRGBToLinear(),dry=new THREE.Color(0x8e9261).convertSRGBToLinear(),rock=new THREE.Color(0x827f72).convertSRGBToLinear();
    for(let i=0;!preparedTerrain&&i<position.count;i++){
      const offset=i*3,y=pos[offset+1],slope=1-Math.max(0,normal[offset+1]);
      c.copy(grass).lerp(dry,THREE.MathUtils.smoothstep(y,-10,75));c.lerp(rock,THREE.MathUtils.smoothstep(slope,.05,.3));
      const d=.95+.05*Math.sin(pos[offset]*.019+pos[offset+2]*.027);col[offset]=c.r*d;col[offset+1]=c.g*d;col[offset+2]=c.b*d;
      if((i+1)%(rowWidth*16)===0)yield;
    }
    if(!preparedTerrain)g.setAttribute('color',new THREE.BufferAttribute(col,3));
    // Normals are computed once on the complete field before chunking, so shared borders
    // retain exactly the same smooth shading and contact-height samples as before.
    const chunkSteps=terrainChunkSteps(g,this.segments,this.chunkSegments);let chunkStep=chunkSteps.next();
    while(!chunkStep.done){yield;chunkStep=chunkSteps.next();}
    const chunks=chunkStep.value;
    this.mesh=new THREE.Group();this.mesh.name='Terrain';
    this.terrainMaterial=new THREE.MeshStandardMaterial({vertexColors:true,roughness:1,metalness:0});
    for(const chunk of chunks){const part=new THREE.Mesh(chunk.geometry,this.terrainMaterial);part.name='TerrainChunk-'+chunk.x+'-'+chunk.z;part.receiveShadow=true;part.frustumCulled=true;this.mesh.add(part);}
    this.scene.add(this.mesh);
    this.gridMaterial=new THREE.ShaderMaterial({transparent:true,depthWrite:false,polygonOffset:true,polygonOffsetFactor:-1,polygonOffsetUnits:-2,
      extensions:{derivatives:true},uniforms:{small:{value:0},big:{value:0}},
      vertexShader:`varying vec3 wpos;void main(){vec4 p=modelMatrix*vec4(position,1.);wpos=p.xyz;gl_Position=projectionMatrix*viewMatrix*p;}`,
      fragmentShader:`varying vec3 wpos;uniform float small;uniform float big;
      float grid(float size,float width){vec2 f=max(fwidth(wpos.xz),vec2(.002));vec2 d=abs(fract((wpos.xz+1000.)/size+.5)-.5)*size;
      vec2 line=1.-smoothstep(vec2(width),vec2(width)+f,d);float fade=1.-smoothstep(.12,.38,max(f.x,f.y)/size);return max(line.x,line.y)*fade;}
      void main(){float a=grid(5.,.017)*small*.32;float b=grid(100.,.055)*big*.8;float alpha=max(a,b);if(alpha<.01)discard;
      gl_FragColor=vec4(mix(vec3(.05,.085,.05),vec3(.79,.75,.45),b),alpha);}`
    });
    this.gridMesh=new THREE.Group();this.gridMesh.name='TerrainDiagnosticGrid';this.gridMesh.renderOrder=2;this.gridMesh.visible=false;
    for(const chunk of chunks){const part=new THREE.Mesh(chunk.geometry,this.gridMaterial);part.name='TerrainGridChunk-'+chunk.x+'-'+chunk.z;part.frustumCulled=true;this.gridMesh.add(part);this.chunkGeometries.push(chunk.geometry);}
    this.scene.add(this.gridMesh);this.syncGrid();
  };
  TerrainSystem.prototype.build=function(seed){const steps=this._buildSteps(seed);while(!steps.next().done){};};
  async function terrainDataInWorker(system,seed,check,signal){
    // Browsers assign each file:// document an opaque origin; a module worker
    // then emits a noisy unsafe-load warning and cannot be used reliably.
    if(typeof Worker!=='function'||typeof document==='undefined'||!document.baseURI||location.protocol==='file:')return null;
    let worker;
    try{worker=new Worker(new URL('js/workers/terrainHeightWorker.js',document.baseURI));}catch(_){return null;}
    const perlin=new RTS.SeededPerlin2D(seed),permutation=perlin.perm.slice();
    const palette={grass:new THREE.Color(0x708454).convertSRGBToLinear(),dry:new THREE.Color(0x8e9261).convertSRGBToLinear(),rock:new THREE.Color(0x827f72).convertSRGBToLinear()};
    return new Promise((resolve,reject)=>{
      let settled=false;
      const cleanup=()=>{if(signal)signal.removeEventListener('abort',onAbort);worker.terminate();};
      const finish=(error,value)=>{if(settled)return;settled=true;cleanup();error?reject(error):resolve(value);};
      const onAbort=()=>{const error=new Error('Generowanie swiata zostalo anulowane.');error.name='AbortError';finish(error);};
       worker.onmessage=event=>{try{if(check)check();const data=event.data,vertexCount=(system.segments+1)*(system.segments+1),IndexArray=vertexCount>65536?Uint32Array:Uint16Array;finish(null,{positions:new Float32Array(data.positions),uvs:new Float32Array(data.uvs),heights:new Float32Array(data.heights),normals:new Float32Array(data.normals),colors:new Float32Array(data.colors),indices:new IndexArray(data.indices)});}catch(error){finish(error);}};
      worker.onerror=event=>finish(event.error||new Error(event.message||'Terrain worker failed.'));
      if(signal){signal.addEventListener('abort',onAbort,{once:true});if(signal.aborted){onAbort();return;}}
       worker.postMessage({segments:system.segments,mapSize:system.mapSize,halfMap:system.mapSize*.5,battlefield:system.battlefield,permutation:permutation.buffer,palette},[permutation.buffer]);
    });
  }
  TerrainSystem.prototype.buildAsync=async function(seed,yieldFrame,check,signal=null){
    let preparedTerrain=null;
    try{preparedTerrain=await terrainDataInWorker(this,seed,check,signal);}
    catch(error){if(signal&&signal.aborted)throw error;if(check)check();}
    if(check)check();
    const steps=this._buildSteps(seed,preparedTerrain);let step=steps.next();
    while(!step.done){if(check)check();await yieldFrame();step=steps.next();}
  };
  TerrainSystem.prototype.setSmallGridVisible=function(v){this.showSmallGrid=!!v;this.syncGrid();};
  TerrainSystem.prototype.setBigGridVisible=function(v){this.showBigGrid=!!v;this.syncGrid();};
  TerrainSystem.prototype.syncGrid=function(){if(!this.gridMesh)return;this.gridMesh.visible=this.showSmallGrid||this.showBigGrid;this.gridMaterial.uniforms.small.value=this.showSmallGrid?1:0;this.gridMaterial.uniforms.big.value=this.showBigGrid?1:0;};
  // Construction pads are applied to the authoritative height field, not as
  // an overlay. This keeps rendering, height queries, units and buildings on
  // the same graded ground. A short feather is the procedural equivalent of
  // cut/fill grading rather than an artificial vertical terrace.
  TerrainSystem.prototype.flattenPads=function(pads,margin=2.5,feather=5){
    if(!this.heights?.length||!Array.isArray(pads)||!pads.length)return [];
    const position=this.mesh?.children[0]?.geometry?.attributes.position;if(!position)return [];
    const pos=position.array,N=this.segments,row=N+1,targets=pads.map(p=>({...p,height:this.getHeightAt(p.x,p.z)}));
    for(let z=0;z<=N;z++)for(let x=0;x<=N;x++){
      const index=z*row+x,offset=index*3,px=pos[offset],pz=pos[offset+2];let height=this.heights[index];
      for(const pad of targets){const c=Math.cos(pad.rotation||0),s=Math.sin(pad.rotation||0),dx=px-pad.x,dz=pz-pad.z,localX=dx*c+dz*s,localZ=-dx*s+dz*c,halfW=(pad.width||10)*.5+margin,halfD=(pad.depth||10)*.5+margin,q=Math.max(Math.abs(localX)/halfW,Math.abs(localZ)/halfD);if(q<=1)height=pad.height;else if(q<1+feather/Math.max(halfW,halfD)){const t=(q-1)*Math.max(halfW,halfD)/feather,smooth=t*t*(3-2*t);height+=((pad.height-height)*(1-smooth));}}
      this.heights[index]=height;pos[offset+1]=height;
    }
    const normal=this.mesh.children[0].geometry.attributes.normal.array,step=this._step;
    for(let z=0;z<=N;z++)for(let x=0;x<=N;x++){const left=this.heights[z*row+Math.max(0,x-1)],right=this.heights[z*row+Math.min(N,x+1)],down=this.heights[Math.max(0,z-1)*row+x],up=this.heights[Math.min(N,z+1)*row+x],scaleX=(x===0||x===N)?step:2*step,scaleZ=(z===0||z===N)?step:2*step,offset=(z*row+x)*3,dx=(right-left)/scaleX,dz=(up-down)/scaleZ,length=Math.hypot(dx,1,dz);normal[offset]=-dx/length;normal[offset+1]=1/length;normal[offset+2]=-dz/length;}
    for(const geometry of this.chunkGeometries){geometry.attributes.position.needsUpdate=true;geometry.attributes.normal.needsUpdate=true;geometry.computeBoundingBox();geometry.computeBoundingSphere();}
    return targets;
  };
  // Grade each road corridor directly into the authoritative height field.  The
  // broad shoulder prevents a visually implausible vertical cut beside a road.
  TerrainSystem.prototype.flattenRoads=function(roads,falloff=5){
    if(!this.heights?.length||!Array.isArray(roads)||!roads.length)return;
    const position=this.mesh?.children[0]?.geometry?.attributes.position;if(!position)return;
    const pos=position.array,N=this.segments,row=N+1,targets=[];
    for(const road of roads)for(let i=1;i<road.points.length;i++){const a=road.points[i-1],b=road.points[i];targets.push({a,b,width:road.width*.5+(road.sidewalk||0),height:(this.getHeightAt(a.x,a.z)+this.getHeightAt(b.x,b.z))*.5});}
    for(let z=0;z<=N;z++)for(let x=0;x<=N;x++){
      const index=z*row+x,o=index*3,px=pos[o],pz=pos[o+2];let h=this.heights[index];
      for(const road of targets){const dx=road.b.x-road.a.x,dz=road.b.z-road.a.z,l=dx*dx+dz*dz||1,t=Math.max(0,Math.min(1,((px-road.a.x)*dx+(pz-road.a.z)*dz)/l)),qx=road.a.x+dx*t,qz=road.a.z+dz*t,d=Math.hypot(px-qx,pz-qz),edge=road.width;if(d<=edge)h=road.height;else if(d<edge+falloff){const u=(d-edge)/falloff,s=u*u*(3-2*u);h+=((road.height-h)*(1-s));}}
      this.heights[index]=h;pos[o+1]=h;
    }
    const normal=this.mesh.children[0].geometry.attributes.normal.array,step=this._step;
    for(let z=0;z<=N;z++)for(let x=0;x<=N;x++){const left=this.heights[z*row+Math.max(0,x-1)],right=this.heights[z*row+Math.min(N,x+1)],down=this.heights[Math.max(0,z-1)*row+x],up=this.heights[Math.min(N,z+1)*row+x],sx=(x===0||x===N)?step:2*step,sz=(z===0||z===N)?step:2*step,o=(z*row+x)*3,dx=(right-left)/sx,dz=(up-down)/sz,len=Math.hypot(dx,1,dz);normal[o]=-dx/len;normal[o+1]=1/len;normal[o+2]=-dz/len;}
    for(const geometry of this.chunkGeometries){geometry.attributes.position.needsUpdate=true;geometry.attributes.normal.needsUpdate=true;geometry.computeBoundingBox();geometry.computeBoundingSphere();}
  };
  // Zapytanie O(1) korzysta z Float32Array tej samej geometrii i tej samej przekątnej.
  TerrainSystem.prototype.sample=function(x,z,out){
    out=out||{height:0,normal:new THREE.Vector3(0,1,0)};
    if(!this.heights){out.height=0;out.normal.set(0,1,0);return out;}
    const N=this.segments,step=this._step,half=this._half;
    const fx=clampGridCoordinate((x+half)/step,N),fz=clampGridCoordinate((z+half)/step,N);
    const ix=Math.min(N-1,Math.floor(fx)),iz=Math.min(N-1,Math.floor(fz)),u=fx-ix,v=fz-iz,k=iz*(N+1)+ix;
    const a=this.heights[k],d=this.heights[k+1],b=this.heights[k+N+1],c=this.heights[k+N+2];let dx,dz;
    if(u+v<=1){out.height=a+(d-a)*u+(b-a)*v;dx=(d-a)/step;dz=(b-a)/step;}
    else {out.height=c+(b-c)*(1-u)+(d-c)*(1-v);dx=(c-b)/step;dz=(c-d)/step;}
    out.normal.set(-dx,1,-dz).normalize();return out;
  };
  TerrainSystem.prototype.getHeightAt=function(x,z){
    if(!this.heights)return 0;
    const N=this.segments,step=this._step,half=this._half;
    const fx=clampGridCoordinate((x+half)/step,N),fz=clampGridCoordinate((z+half)/step,N);
    const ix=Math.min(N-1,Math.floor(fx)),iz=Math.min(N-1,Math.floor(fz));
    const u=fx-ix,v=fz-iz,k=iz*(N+1)+ix;
    const a=this.heights[k],d=this.heights[k+1],b=this.heights[k+N+1],c=this.heights[k+N+2];
    return u+v<=1?a+(d-a)*u+(b-a)*v:c+(b-c)*(1-u)+(d-c)*(1-v);
  };
  TerrainSystem.prototype.getHeightAtCached=function(x,z,cache){
    if(!this.heights)return 0;
    const N=this.segments,step=this._step,half=this._half;
    const fx=clampGridCoordinate((x+half)/step,N),fz=clampGridCoordinate((z+half)/step,N);
    let ix,iz;
    const sameGrid=cache&&cache.terrain===this&&cache.heights===this.heights;
    const cachedX=sameGrid&&fx>=cache.ix&&(fx<cache.ix+1||(cache.ix===N-1&&fx===N));
    const cachedZ=sameGrid&&fz>=cache.iz&&(fz<cache.iz+1||(cache.iz===N-1&&fz===N));
    ix=cachedX?cache.ix:Math.min(N-1,Math.floor(fx));
    iz=cachedZ?cache.iz:Math.min(N-1,Math.floor(fz));
    if(cache&&(!sameGrid||ix!==cache.ix||iz!==cache.iz)){
      const k=iz*(N+1)+ix;
      cache.terrain=this;cache.heights=this.heights;cache.ix=ix;cache.iz=iz;
      cache.a=this.heights[k];cache.d=this.heights[k+1];cache.b=this.heights[k+N+1];cache.c=this.heights[k+N+2];
    }
    const u=fx-ix,v=fz-iz;
    if(cache&&cache.terrain===this&&cache.heights===this.heights&&cache.ix===ix&&cache.iz===iz){
      const a=cache.a,d=cache.d,b=cache.b,c=cache.c;
      return u+v<=1?a+(d-a)*u+(b-a)*v:c+(b-c)*(1-u)+(d-c)*(1-v);
    }
    const k=iz*(N+1)+ix,a=this.heights[k],d=this.heights[k+1],b=this.heights[k+N+1],c=this.heights[k+N+2];
    return u+v<=1?a+(d-a)*u+(b-a)*v:c+(b-c)*(1-u)+(d-c)*(1-v);
  };
  TerrainSystem.prototype.sectorAt=function(x,z){
    const half=this.mapSize/2,count=Math.ceil(this.mapSize/5);
    if(x< -half||x>half||z< -half||z>half)return null;
    const ix=Math.min(count-1,Math.floor((x+half)/5)),iz=Math.min(count-1,Math.floor((z+half)/5));
    return {x:ix,z:iz,id:iz*count+ix,bigX:Math.floor(ix/20),bigZ:Math.floor(iz/20)};
  };
  TerrainSystem.prototype.dispose=function(){
    if(this.gridMesh){this.scene.remove(this.gridMesh);this.gridMesh=null;}
    if(this.mesh){this.scene.remove(this.mesh);this.mesh=null;}
    this.chunkGeometries.forEach(geometry=>geometry.dispose());this.chunkGeometries=[];
    if(this.gridMaterial){this.gridMaterial.dispose();this.gridMaterial=null;}
    if(this.terrainMaterial){this.terrainMaterial.dispose();this.terrainMaterial=null;}
    this.heights=null;
  };
  R.TerrainSystem=TerrainSystem;
  R.FlatSurface={getHeightAt(){return 0;},sample(x,z,out){out=out||{height:0,normal:new THREE.Vector3()};out.height=0;out.normal.set(0,1,0);return out;}};
})();
