(function () {
  'use strict';
  const R = window.RTS;
  const V = (x=0,y=0,z=0) => new THREE.Vector3(x,y,z);
  const STATIC_WEIGHT_MAP=Symbol('staticBoneWeights');
  const WEIGHT_CHANNEL_MAP=Symbol('weightChannels');
  function* attributeSteps(values,Type,itemSize){
    const data=new Type(values.length);
    for(let i=0;i<values.length;i++){data[i]=values[i];if((i&12287)===12287)yield;}
    return new THREE.BufferAttribute(data,itemSize);
  }
  R.SurfaceBuilder = class SurfaceBuilder {
    constructor(rig) {
      this.rig = rig; this.H = rig.anatomy.height;
      this.positions=[]; this.colors=[]; this.uvs=[]; this.skinIndices=[]; this.skinWeights=[];
      this.hints=[]; this.triangles=[[],[],[]]; this.tags={}; this.currentTag='';
      this.morphs={eyelidsClose:new Map(),eyelidsArc:new Map(),neckFlex:new Map(),handsRelax:new Map()};
      this.omitFacialMorphs=false;
      this.faceMetadata=null;
      this.trustLocalWindingHints=false;
      this.materialRegions=false;
      this._frozenWeightCache=new WeakMap();
      this._a=V(); this._b=V(); this._n=V(); this._c=V();
      this._faceA=V();this._faceB=V();this._faceC=V();this._faceHint=V();
      this._ringPoint=V();this._ringNormal=V();this._capCenter=V();this._capPoint=V();this._ellipsoidPoint=V();this._ellipsoidNormal=V();
      this._ringUV=[0,0];this._shapePoint=[0,0];
      this._weightNames=['','','',''];this._weightValues=[0,0,0,0];
    }
    vertex(p, weights, color, normal, uv=null) {
      const i=this.positions.length/3,names=this._weightNames,values=this._weightValues;let count=0,total=0;
      const cacheable=weights[STATIC_WEIGHT_MAP]===true,cached=cacheable?this._frozenWeightCache.get(weights):null;
      if(cached){
        this.positions.push(p.x*this.H,p.y*this.H,p.z*this.H);
        this.colors.push(color[0],color[1],color[2]);this.uvs.push(uv?uv[0]:0,uv?uv[1]:0);
        this.hints.push(normal.x,normal.y,normal.z);
        for(let k=0;k<4;k++){this.skinIndices.push(cached.indices[k]);this.skinWeights.push(cached.weights[k]);}
        if(this.currentTag){if(!this.tags[this.currentTag])this.tags[this.currentTag]=[];this.tags[this.currentTag].push(i);}
        return i;
      }
      const channels=weights[WEIGHT_CHANNEL_MAP]===true;
      if(channels){
        const sourceNames=weights.names,sourceIndices=weights.indices,sourceValues=weights.values;
        for(let source=0;source<weights.count;source++){
          const weight=sourceValues[source];if(!(weight>0))continue;
          let at=count;if(count===4){if(weight<=values[3])continue;at=3;}else count++;
          while(at>0&&values[at-1]<weight){if(at<4){values[at]=values[at-1];names[at]=names[at-1];}at--;}
          values[at]=weight;names[at]=sourceNames[sourceIndices[source]];
        }
      }else{
        for(const name in weights){
          if(!Object.prototype.hasOwnProperty.call(weights,name))continue;
          const weight=weights[name];if(!(weight>0))continue;
          let at=count;
          if(count===4){if(weight<=values[3])continue;at=3;}else count++;
          while(at>0&&values[at-1]<weight){if(at<4){values[at]=values[at-1];names[at]=names[at-1];}at--;}
          values[at]=weight;names[at]=name;
        }
      }
      for(let k=0;k<count;k++)total+=values[k];
      if (!(total>0)) throw new Error('Brak wag dla wierzchołka.');
      this.positions.push(p.x*this.H,p.y*this.H,p.z*this.H);
      this.colors.push(color[0],color[1],color[2]); this.uvs.push(uv?uv[0]:0,uv?uv[1]:0);
      this.hints.push(normal.x,normal.y,normal.z);
      const cachedIndices=cacheable?[0,0,0,0]:null,cachedWeights=cacheable?[0,0,0,0]:null;
      for (let k=0;k<4;k++) {
        if (k<count) {
          const index=this.rig.index[names[k]];
          if (index===undefined) throw new Error('Nieznana kość '+names[k]);
          const normalized=values[k]/total;this.skinIndices.push(index);this.skinWeights.push(normalized);
          if(cacheable){cachedIndices[k]=index;cachedWeights[k]=normalized;}
        } else { this.skinIndices.push(0); this.skinWeights.push(0); }
      }
      if(cacheable)this._frozenWeightCache.set(weights,{indices:cachedIndices,weights:cachedWeights});
      if(this.currentTag) { if(!this.tags[this.currentTag])this.tags[this.currentTag]=[]; this.tags[this.currentTag].push(i); }
      return i;
    }
    point(i,target=V()) { return target.fromArray(this.positions,3*i).multiplyScalar(1/this.H); }
    triangle(a,b,c,material=0) {
      const pa=this.point(a,this._a), pb=this.point(b,this._b), pc=this.point(c,this._c);
      if(this.triangleFilter&&!this.triangleFilter(a,b,c,pa,pb,pc))return;
      this._b.sub(pa); this._c.sub(pa); this._n.crossVectors(this._b,this._c);
      const hints=this.hints;
      const dot=this._n.x*(hints[3*a]+hints[3*b]+hints[3*c])+this._n.y*(hints[3*a+1]+hints[3*b+1]+hints[3*c+1])+this._n.z*(hints[3*a+2]+hints[3*b+2]+hints[3*c+2]);
      if(this._n.lengthSq()<1e-18)return;
      this.triangles[material].push(a, dot<0?c:b, dot<0?b:c);
    }
    bridge(a,b,material=0) {
      if(a.length!==b.length)throw new Error('Niepasujące pętle topologii.');
      for(let j=0;j<a.length;j++){
        const k=(j+1)%a.length;
        this.triangle(a[j],b[j],a[k],material); this.triangle(a[k],b[j],b[k],material);
      }
    }
    cap(loop,weights,color,normal,material=0) {
      const center=this._capCenter,point=this._capPoint;center.set(0,0,0);
      for(let i=0;i<loop.length;i++)center.add(this.point(loop[i],point));
      center.multiplyScalar(1/loop.length);
      const k=this.vertex(center,weights,color,normal);
      for(let i=0;i<loop.length;i++)this.triangle(k,loop[i],loop[(i+1)%loop.length],material);
    }
    ring(center,u,v,rx,rz,n,weights,color,uvY=0,angles=null,shape=null) {
      const ring=[],p=this._ringPoint,normal=this._ringNormal;
      for(let j=0;j<n;j++){
        const a=angles?angles[j]:2*Math.PI*j/n;
        let xx=Math.cos(a), zz=Math.sin(a);
        if(shape){const out=this._shapePoint;out[0]=xx;out[1]=zz;shape(xx,zz,a,out);xx=out[0];zz=out[1];}
        p.copy(center).addScaledVector(u,rx*xx).addScaledVector(v,rz*zz);
        normal.copy(u).multiplyScalar(xx/Math.max(rx,1e-5)).addScaledVector(v,zz/Math.max(rz,1e-5)).normalize();
        const w=typeof weights==='function'?weights(p,j):weights;
        const c=typeof color==='function'?color(p,j):color;
        const uv=this._ringUV;uv[0]=j/n*3;uv[1]=uvY;
        ring.push(this.vertex(p,w,c,normal,uv));
      }
      return ring;
    }
    ellipsoid(center,radii,weights,color,material=1,segments=20,rows=12,rotation=null) {
      const rings=[],p=this._ellipsoidPoint,normal=this._ellipsoidNormal;
      for(let y=0;y<=rows;y++){
        const phi=-Math.PI/2+Math.PI*y/rows, ring=[];
        for(let j=0;j<segments;j++){
          const a=j/segments*Math.PI*2;
          p.set(Math.cos(phi)*Math.cos(a)*radii.x,Math.sin(phi)*radii.y,Math.cos(phi)*Math.sin(a)*radii.z);
          normal.set(Math.cos(phi)*Math.cos(a)/radii.x,Math.sin(phi)/radii.y,Math.cos(phi)*Math.sin(a)/radii.z).normalize();
          if(rotation){p.applyQuaternion(rotation);normal.applyQuaternion(rotation);}
          p.add(center);
          const uv=this._ringUV;uv[0]=j/segments;uv[1]=y/rows;
          ring.push(this.vertex(p,weights,color,normal,uv));
        }
        if(y)this.bridge(rings[y-1],ring,material); rings.push(ring);
      }
      return rings;
    }
    tubePath(points,radius,weights,color,material=0,segments=8) {
      let prev=null;
      for(let i=0;i<points.length;i++){
        const dir=(i<points.length-1?points[i+1].clone().sub(points[i]):points[i].clone().sub(points[i-1])).normalize();
        const guide=Math.abs(dir.z)<.9?V(0,0,1):V(0,1,0);
        const u=dir.clone().cross(guide).normalize(),v=u.clone().cross(dir).normalize();
        const ring=this.ring(points[i],u,v,radius,radius,segments,typeof weights==='function'?weights(points[i]):weights,color,i/points.length);
        if(prev)this.bridge(prev,ring,material);prev=ring;
      }
    }
    orientConnectedFaces(indices) {
      // Kontrolowane profile zawierają miejsca wklęsłe (pacha, krok).
      // Sam radialny normalHint nie określa tam poprawnie strony trójkąta.
      // Spójność orientacji ustala graf wspólnych krawędzi, bez rozcinania siatki.
      const count=indices.length/3,vertexCount=this.positions.length/3,numericKeys=vertexCount*vertexCount<=Number.MAX_SAFE_INTEGER,edges=new Map(),neighbours=Array.from({length:count},()=>[]);
      for(let t=0;t<count;t++)for(let j=0;j<3;j++){
        const a=indices[t*3+j],b=indices[t*3+(j+1)%3],lo=Math.min(a,b),hi=Math.max(a,b),key=numericKeys?lo*vertexCount+hi:lo+','+hi,entry={t,dir:a<b};
        if(edges.has(key)){const other=edges.get(key);const different=other.dir===entry.dir?1:0;neighbours[t].push([other.t,different]);neighbours[other.t].push([t,different]);}
        else edges.set(key,entry);
      }
      const flips=new Int8Array(count);flips.fill(-1);
      for(let start=0;start<count;start++){
        if(flips[start]!==-1)continue;flips[start]=0;const queue=[start];let score=0;
        for(let at=0;at<queue.length;at++){
          const t=queue[at];
          for(const [next,delta]of neighbours[t])if(flips[next]===-1){flips[next]=flips[t]^delta;queue.push(next);}
          const a=indices[t*3],b=indices[t*3+1],c=indices[t*3+2],pa=this.point(a,this._faceA),pb=this.point(b,this._faceB).sub(pa),pc=this.point(c,this._faceC).sub(pa),normal=pb.cross(pc);
          const hint=this._faceHint.set(this.hints[a*3]+this.hints[b*3]+this.hints[c*3],this.hints[a*3+1]+this.hints[b*3+1]+this.hints[c*3+1],this.hints[a*3+2]+this.hints[b*3+2]+this.hints[c*3+2]);
          score+=normal.dot(hint)*(flips[t]?-1:1);
        }
        const invert=score<0?1:0;
        for(const t of queue)if(flips[t]^invert){const tmp=indices[t*3+1];indices[t*3+1]=indices[t*3+2];indices[t*3+2]=tmp;}
      }
    }
    *orientConnectedFacesSteps(indices) {
      // Async counterpart of orientConnectedFaces. Keep edge insertion,
      // neighbour traversal and score accumulation in exactly the same order.
      const count=indices.length/3,vertexCount=this.positions.length/3,numericKeys=vertexCount*vertexCount<=Number.MAX_SAFE_INTEGER,edges=new Map(),neighbours=Array.from({length:count},()=>[]);
      for(let t=0;t<count;t++){
        for(let j=0;j<3;j++){
          const a=indices[t*3+j],b=indices[t*3+(j+1)%3],lo=Math.min(a,b),hi=Math.max(a,b),key=numericKeys?lo*vertexCount+hi:lo+','+hi,entry={t,dir:a<b};
          if(edges.has(key)){const other=edges.get(key),different=other.dir===entry.dir?1:0;neighbours[t].push([other.t,different]);neighbours[other.t].push([t,different]);}
          else edges.set(key,entry);
        }
        if((t&4095)===4095)yield;
      }
      const flips=new Int8Array(count);flips.fill(-1);
      for(let start=0;start<count;start++){
        if(flips[start]!==-1)continue;flips[start]=0;const queue=[start];let score=0;
        for(let at=0;at<queue.length;at++){
          const t=queue[at];
          for(const [next,delta]of neighbours[t])if(flips[next]===-1){flips[next]=flips[t]^delta;queue.push(next);}
          const a=indices[t*3],b=indices[t*3+1],c=indices[t*3+2],pa=this.point(a,this._faceA),pb=this.point(b,this._faceB).sub(pa),pc=this.point(c,this._faceC).sub(pa),normal=pb.cross(pc);
          const hint=this._faceHint.set(this.hints[a*3]+this.hints[b*3]+this.hints[c*3],this.hints[a*3+1]+this.hints[b*3+1]+this.hints[c*3+1],this.hints[a*3+2]+this.hints[b*3+2]+this.hints[c*3+2]);
          score+=normal.dot(hint)*(flips[t]?-1:1);
          if((at&4095)===4095)yield;
        }
        const invert=score<0?1:0;
        for(let q=0;q<queue.length;q++){const t=queue[q];if(flips[t]^invert){const tmp=indices[t*3+1];indices[t*3+1]=indices[t*3+2];indices[t*3+2]=tmp;}if((q&4095)===4095)yield;}
      }
    }
    morph(name,index,delta) {
      if(!this.morphs[name])throw new Error('Nieznany morph '+name);
      if(this.omitFacialMorphs&&name!=='handsRelax')return;
      this.morphs[name].set(index,[delta.x*this.H,delta.y*this.H,delta.z*this.H]);
    }
    finish() { const steps=this.finishSteps();let step;do{step=steps.next();}while(!step.done);return step.value; }
    async finishAsync(yieldFrame,check=null,budgetMs=4) {
      if(typeof yieldFrame!=='function')throw new TypeError('Async surface finalization requires a frame-yield callback.');
      const steps=this.finishSteps(),now=()=>typeof performance!=='undefined'&&performance.now?performance.now():Date.now(),budget=Number.isFinite(budgetMs)?Math.max(0,budgetMs):4;let step=steps.next(),sliceStart=budget>0?now():0,checkpointsSinceClock=0;
      while(!step.done){
        if(check)check();
        // The finalizer already yields from bounded typed-array/topology
        // chunks. Read the clock every eight checkpoints; cancellation stays
        // responsive at every chunk and after every frame yield.
        const timeDue=budget>0&&++checkpointsSinceClock>=8;
        if(budget===0||(timeDue&&(checkpointsSinceClock=0,now()-sliceStart>=budget))){await yieldFrame();sliceStart=budget>0?now():0;if(check)check();}
        step=steps.next();
      }
      return step.value;
    }
    *finishSteps() {
      const vertexCount=this.positions.length/3;
      const g=new THREE.BufferGeometry();
      g.setAttribute('position',yield* attributeSteps(this.positions,Float32Array,3));
      g.setAttribute('color',yield* attributeSteps(this.colors,Float32Array,3));
      this.colors=null;
      g.setAttribute('uv',yield* attributeSteps(this.uvs,Float32Array,2));
      this.uvs=null;
      g.setAttribute('skinIndex',yield* attributeSteps(this.skinIndices,Uint16Array,4));
      this.skinIndices=null;
      g.setAttribute('skinWeight',yield* attributeSteps(this.skinWeights,Float32Array,4));
      this.skinWeights=null;
      // Merge material streams directly into the final-width typed index.
      // Winding repair can swap typed-array elements in place, so retaining a
      // large JS number array and then copying it in setIndex is unnecessary.
      let indexCount=0,maximumIndex=0;
      for(const triangles of this.triangles){indexCount+=triangles.length;for(let i=0;i<triangles.length;i++)if(triangles[i]>maximumIndex)maximumIndex=triangles[i];}
      const IndexArray=maximumIndex>65535?Uint32Array:Uint16Array,idx=new IndexArray(indexCount),materialRegion=this.materialRegions?new Uint8Array(vertexCount):null;
      if(materialRegion)materialRegion.fill(255);
      let indexOffset=0,materialRegionsCompatible=true;
      for(let material=0;material<this.triangles.length;material++){
        const triangles=this.triangles[material];
        if(triangles.length)g.addGroup(indexOffset,triangles.length,material);
        for(let i=0;i<triangles.length;i++){
          const vertex=triangles[i];idx[indexOffset++]=vertex;
          if(materialRegion){const previous=materialRegion[vertex];if(previous===255)materialRegion[vertex]=material;else if(previous!==material)materialRegionsCompatible=false;}
          if((i&12287)===12287)yield;
        }
      }
      if(materialRegion&&materialRegionsCompatible){for(let i=0;i<materialRegion.length;i++)if(materialRegion[i]===255)materialRegion[i]=0;g.setAttribute('materialRegion',new THREE.BufferAttribute(materialRegion,1));}
      this.triangles=null;
      // Unknown/profile-built surfaces still need global adjacency repair;
      // audited primitive builders may prove every triangle locally against
      // explicit outward normal hints and skip the edge graph/BFS.
      if(!this.trustLocalWindingHints){const orientation=this.orientConnectedFacesSteps(idx);let oriented;do{oriented=orientation.next();if(!oriented.done)yield;}while(!oriented.done);}
      this.hints=null;this.positions=null;
      g.setIndex(new THREE.BufferAttribute(idx,1));
      // Match Three.js r128's indexed computeVertexNormals operation order.
      // Float32 accumulation happens per face, as in BufferAttribute.setXYZ.
      const pos=g.attributes.position.array,normal=new Float32Array(pos.length),faceCount=idx.length/3;
      for(let t=0;t<faceCount;t++){
        const a=idx[t*3],b=idx[t*3+1],c=idx[t*3+2],ai=3*a,bi=3*b,ci=3*c;
        // Operation order mirrors BufferGeometry.computeVertexNormals in Three.js r128.
        const cbx=pos[ci]-pos[bi],cby=pos[ci+1]-pos[bi+1],cbz=pos[ci+2]-pos[bi+2],abx=pos[ai]-pos[bi],aby=pos[ai+1]-pos[bi+1],abz=pos[ai+2]-pos[bi+2];
        const nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx;
        normal[ai]+=nx;normal[ai+1]+=ny;normal[ai+2]+=nz;normal[bi]+=nx;normal[bi+1]+=ny;normal[bi+2]+=nz;normal[ci]+=nx;normal[ci+1]+=ny;normal[ci+2]+=nz;
        if((t&4095)===4095)yield;
      }
      for(let i=0;i<normal.length;i+=3){const x=normal[i],y=normal[i+1],z=normal[i+2],length=Math.sqrt(x*x+y*y+z*z)||1;normal[i]=x/length;normal[i+1]=y/length;normal[i+2]=z/length;if((i&12287)===12285)yield;}
      g.setAttribute('normal',new THREE.BufferAttribute(normal,3));
      const morphPositions=[],morphNormals=[],baseNormal=g.attributes.normal.array,basePosition=g.attributes.position.array,triangleCount=idx.length/3;
      const morphEntries=Object.entries(this.morphs).filter(([,changes])=>changes.size>0);
      if(!morphEntries.length){
        // Equipment surfaces often have no deformers. Avoid allocating and
        // filling vertex-to-triangle adjacency tables for geometry that has no
        // morph normals to compile.
        g.morphTargetsRelative=true;g.morphAttributes={position:morphPositions,normal:morphNormals};
        const bounds=yield* this.computeBoundsSteps(g);g.boundingBox=bounds.box;g.boundingSphere=bounds.sphere;
        return {geometry:g,tags:this.tags,faceMetadata:this.faceMetadata,triangles:idx.length/3,vertices:vertexCount};
      }
      // Keep a vertex-to-triangle adjacency table. A sparse morph only changes
      // normals in the one-ring around its moved vertices; all others have zero delta.
      const incidenceCounts=new Uint32Array(vertexCount);
      for(let i=0;i<idx.length;i++){incidenceCounts[idx[i]]++;if((i&12287)===12287)yield;}
      const incidenceOffsets=new Uint32Array(vertexCount+1);
      for(let i=0;i<vertexCount;i++){incidenceOffsets[i+1]=incidenceOffsets[i]+incidenceCounts[i];if((i&8191)===8191)yield;}
      const incidentTriangles=new Uint32Array(idx.length);
      // Reuse the count buffer as the fill cursor; restore prefix offsets
      // afterwards so morph scans keep their original contiguous ranges.
      for(let i=0;i<vertexCount;i++)incidenceCounts[i]=incidenceOffsets[i];
      for(let t=0;t<triangleCount;t++){for(let c=0;c<3;c++){const vertex=idx[t*3+c];incidentTriangles[incidenceCounts[vertex]++]=t;}if((t&4095)===4095)yield;}
      for(let i=0;i<vertexCount;i++)incidenceCounts[i]=incidenceOffsets[i];
      const changedTriangleMarks=new Uint32Array(triangleCount),affectedVertexMarks=new Uint32Array(vertexCount),candidateTriangleMarks=new Uint32Array(triangleCount),normalAccum=new Float32Array(baseNormal.length);
      // Reuse the worklists across morph channels. Their contents are only
      // needed until the current channel's normals have been accumulated.
      // Keeping one growing allocation per list avoids repeated GC churn
      // without changing the sorted candidate order or emitted attributes.
      const affectedVertices=[],candidates=[];
      let stamp=0;
      for(const [name,changes] of morphEntries) {
        if(changes.size===0)continue;
        const delta=new Float32Array(basePosition.length),savedPositions=new Float32Array(changes.size*3);let changeWork=0;
        for(const [i,d] of changes){const at=3*i,savedAt=changeWork*3;savedPositions[savedAt]=basePosition[at];savedPositions[savedAt+1]=basePosition[at+1];savedPositions[savedAt+2]=basePosition[at+2];for(let k=0;k<3;k++){delta[at+k]=d[k];basePosition[at+k]=Math.fround(basePosition[at+k]+d[k]);}if((++changeWork&4095)===0)yield;}
        const attribute=new THREE.Float32BufferAttribute(delta,3);attribute.name=name;morphPositions.push(attribute);
        const dn=new Float32Array(delta.length);stamp++;
        // Find vertices belonging to triangles touched by this morph.
        affectedVertices.length=0;
        let work=0;for(const i of changes.keys())for(let at=incidenceCounts[i];at<incidenceCounts[i+1];at++){
          const t=incidentTriangles[at];if(changedTriangleMarks[t]===stamp)continue;changedTriangleMarks[t]=stamp;
          for(let c=0;c<3;c++){const vertex=idx[t*3+c];if(affectedVertexMarks[vertex]!==stamp){affectedVertexMarks[vertex]=stamp;affectedVertices.push(vertex);}}
          if((++work&4095)===0)yield;
        }
        // Recalculate complete area-weighted sums for those vertices only.
        // Triangle order matches computeVertexNormals for identical Float32 results.
        candidates.length=0;
        for(const i of affectedVertices){
          normalAccum[3*i]=normalAccum[3*i+1]=normalAccum[3*i+2]=0;
          for(let at=incidenceCounts[i];at<incidenceCounts[i+1];at++){
            const t=incidentTriangles[at];if(candidateTriangleMarks[t]!==stamp){candidateTriangleMarks[t]=stamp;candidates.push(t);}
          }
          if((i&4095)===4095)yield;
        }
        candidates.sort((a,b)=>a-b);
        for(const t of candidates){
          const a=idx[t*3],b=idx[t*3+1],c=idx[t*3+2],ai=3*a,bi=3*b,ci=3*c;
          const abx=basePosition[ai]-basePosition[bi],aby=basePosition[ai+1]-basePosition[bi+1],abz=basePosition[ai+2]-basePosition[bi+2];
          const cbx=basePosition[ci]-basePosition[bi],cby=basePosition[ci+1]-basePosition[bi+1],cbz=basePosition[ci+2]-basePosition[bi+2];
          const nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx;
          if(affectedVertexMarks[a]===stamp){normalAccum[ai]+=nx;normalAccum[ai+1]+=ny;normalAccum[ai+2]+=nz;}
          if(affectedVertexMarks[b]===stamp){normalAccum[bi]+=nx;normalAccum[bi+1]+=ny;normalAccum[bi+2]+=nz;}
          if(affectedVertexMarks[c]===stamp){normalAccum[ci]+=nx;normalAccum[ci+1]+=ny;normalAccum[ci+2]+=nz;}
          if((t&4095)===4095)yield;
        }
        for(const i of affectedVertices){
          const at=3*i,x=normalAccum[at],y=normalAccum[at+1],z=normalAccum[at+2],length=Math.sqrt(x*x+y*y+z*z),scale=length===0?1:1/length;
          dn[at]=x*scale-baseNormal[at];dn[at+1]=y*scale-baseNormal[at+1];dn[at+2]=z*scale-baseNormal[at+2];
          if((i&4095)===4095)yield;
        }
        // Restore only sparse changed coordinates before the next morph;
        // this avoids a second full-position scratch buffer per surface.
        let restoreWork=0;for(const i of changes.keys()){const at=3*i,savedAt=restoreWork*3;basePosition[at]=savedPositions[savedAt];basePosition[at+1]=savedPositions[savedAt+1];basePosition[at+2]=savedPositions[savedAt+2];if((++restoreWork&4095)===0)yield;}
        const normal=new THREE.Float32BufferAttribute(dn,3);normal.name=name;morphNormals.push(normal);
      }
      g.morphTargetsRelative=true;g.morphAttributes={position:morphPositions,normal:morphNormals};
      const bounds=yield* this.computeBoundsSteps(g);g.boundingBox=bounds.box;g.boundingSphere=bounds.sphere;
      return {geometry:g,tags:this.tags,faceMetadata:this.faceMetadata,triangles:idx.length/3,vertices:vertexCount};
    }
    *computeBoundsSteps(g) {
      const position=g.attributes.position.array,count=position.length/3,min=V(Infinity,Infinity,Infinity),max=V(-Infinity,-Infinity,-Infinity),point=V();
      for(let i=0;i<count;i++){
        const at=i*3;point.set(position[at],position[at+1],position[at+2]);
        min.x=Math.min(min.x,point.x);min.y=Math.min(min.y,point.y);min.z=Math.min(min.z,point.z);
        max.x=Math.max(max.x,point.x);max.y=Math.max(max.y,point.y);max.z=Math.max(max.z,point.z);
        if((i&4095)===4095)yield;
      }
      const morphs=g.morphAttributes.position||[];
      for(let m=0;m<morphs.length;m++){
        const values=morphs[m].array,morphMin=V(Infinity,Infinity,Infinity),morphMax=V(-Infinity,-Infinity,-Infinity);
        for(let i=0;i<values.length/3;i++){
          const at=i*3,x=values[at],y=values[at+1],z=values[at+2];morphMin.x=Math.min(morphMin.x,x);morphMin.y=Math.min(morphMin.y,y);morphMin.z=Math.min(morphMin.z,z);morphMax.x=Math.max(morphMax.x,x);morphMax.y=Math.max(morphMax.y,y);morphMax.z=Math.max(morphMax.z,z);
          if((i&4095)===4095)yield;
        }
        if(g.morphTargetsRelative){min.x+=Math.min(0,morphMin.x);min.y+=Math.min(0,morphMin.y);min.z+=Math.min(0,morphMin.z);max.x+=Math.max(0,morphMax.x);max.y+=Math.max(0,morphMax.y);max.z+=Math.max(0,morphMax.z);}
        else {min.x=Math.min(min.x,morphMin.x);min.y=Math.min(min.y,morphMin.y);min.z=Math.min(min.z,morphMin.z);max.x=Math.max(max.x,morphMax.x);max.y=Math.max(max.y,morphMax.y);max.z=Math.max(max.z,morphMax.z);}
      }
      const box=new THREE.Box3(min,max),center=count?V((min.x+max.x)*.5,(min.y+max.y)*.5,(min.z+max.z)*.5):V();let radiusSq=0;
      for(let i=0;i<count;i++){
        const at=i*3,dx=position[at]-center.x,dy=position[at+1]-center.y,dz=position[at+2]-center.z,distanceSq=dx*dx+dy*dy+dz*dz;
        if(distanceSq>radiusSq)radiusSq=distanceSq;
        if((i&4095)===4095)yield;
      }
      for(let m=0;m<morphs.length;m++){
        const values=morphs[m].array;
        for(let i=0;i<values.length/3;i++){
          const at=i*3,dx=(g.morphTargetsRelative?position[at]:0)+values[at]-center.x,dy=(g.morphTargetsRelative?position[at+1]:0)+values[at+1]-center.y,dz=(g.morphTargetsRelative?position[at+2]:0)+values[at+2]-center.z,distanceSq=dx*dx+dy*dy+dz*dz;
          if(distanceSq>radiusSq)radiusSq=distanceSq;
          if((i&4095)===4095)yield;
        }
      }
      return {box,sphere:new THREE.Sphere(center,Math.sqrt(radiusSq))};
    }
  };
  R.SurfaceColor = {
    rgb(hex) { const c=new THREE.Color(hex).convertSRGBToLinear(); return [c.r,c.g,c.b]; },
    shade(c,t) { return c.map(v=>Math.min(1,v*t)); }
  };
  R.SurfaceBuilder.staticWeights=function(weights){Object.defineProperty(weights,STATIC_WEIGHT_MAP,{value:true});return Object.freeze(weights);};
  R.SurfaceBuilder.createWeightChannels=function(names){
    const channels={names,indices:new Uint8Array(names.length),values:new Float64Array(names.length),count:0};
    Object.defineProperty(channels,WEIGHT_CHANNEL_MAP,{value:true});return channels;
  };
})();
