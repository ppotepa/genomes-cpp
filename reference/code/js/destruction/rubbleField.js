(function(){
  'use strict';
  const R=globalThis.RTS=globalThis.RTS||{},S=R.DestructionSolid,V=S.V;
  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  const PACKING={brick:.56,concrete:.58,rock:.62,wood:.42,steel:.66,armor:.68,glass:.50,foliage:.35,tissue:.45};
  const COLORS={brick:0x8b6651,concrete:0x8b8c86,rock:0x73736d,wood:0x66533b,steel:0x555b5d,armor:0x484e52,glass:0xaab9b8,foliage:0x68755d,tissue:0x806b63};
  function key(x,z){return x+':'+z;}
  function hash(text){let h=2166136261;for(const c of String(text))h=Math.imul(h^c.charCodeAt(0),16777619)>>>0;h^=h>>>16;return h>>>0;}
  function rand01(n){n^=n>>>16;n=Math.imul(n,0x7feb352d);n^=n>>>15;n=Math.imul(n,0x846ca68b);n^=n>>>16;return (n>>>0)/4294967296;}
  class RubbleField{
    constructor(options={}){
      this.cellSize=options.cellSize||.5;this.tileSize=options.tileSize||4;this.n=Math.max(2,Math.round(this.tileSize/this.cellSize));this.tileSize=this.n*this.cellSize;
      this.maxTiles=options.maxTiles||256;this.maxChunkInstances=options.maxChunkInstances||1024;this.maxSlope=options.maxSlope||.78;this.tiles=new Map();this.dirty=new Set();
      this.totalDeposited=0;this.totalEjected=0;this.totalRemoved=0;this.baseHeightAt=()=>0;this.metrics={deposits:0,microDeposits:0,bakes:0,cheapSettles:0,tilesBuilt:0,tileMerges:0,colliderBuilds:0,colliderFallbacks:0,relaxTransfers:0,ballisticHits:0,blastEvents:0,ejectedPackets:0};
      this.world=null;this.P=null;this.model=null;this.onEject=null;this.root=null;this.chunkMesh=null;this.chunkGeometry=null;this.chunkMaterial=null;this.visualMaterials=new Map();this.visualDirty=true;this.disposed=false;
    }
    setBaseHeightProvider(provider){this.baseHeightAt=typeof provider==='function'?provider:()=>0;return this;}
    terrainHeightAt(x,z){const h=Number(this.baseHeightAt(x,z));return Number.isFinite(h)?h:0;}
    tileCoords(cx,cz){return [Math.floor(cx/this.n),Math.floor(cz/this.n)];}
    cellCoords(x,z){return [Math.floor(x/this.cellSize),Math.floor(z/this.cellSize)];}
    mergeOverflowTile(requestTx,requestTz){
      if(this.tiles.size<2)return null;
      let source=null;for(const q of this.tiles.values())if(!source||q.total<source.total)source=q;if(!source)return null;
      let target=null,best=Infinity;for(const q of this.tiles.values()){if(q===source)continue;const dx=q.tx-source.tx,dz=q.tz-source.tz,d=dx*dx+dz*dz;if(d<best){best=d;target=q;}}
      if(!target)return null;
      const worldX=(source.tx+.5)*this.tileSize,worldZ=(source.tz+.5)*this.tileSize,cx=Math.max(target.tx*this.n,Math.min((target.tx+1)*this.n-1,Math.floor(worldX/this.cellSize))),cz=Math.max(target.tz*this.n,Math.min((target.tz+1)*this.n-1,Math.floor(worldZ/this.cellSize)));
      for(let i=0;i<source.volume.length;i++){const total=source.volume[i];if(total<=1e-12)continue;for(const [m,a] of Object.entries(source.mix)){const q=a[i]||0;if(q>0)this.addCell(cx,cz,q,m);}}
      this.removeTile(source);this.metrics.tileMerges++;return target;
    }
    ensureTile(tx,tz){
      const k=key(tx,tz);let t=this.tiles.get(k);if(t)return t;
      if(this.tiles.size>=this.maxTiles){let candidate=null;for(const q of this.tiles.values())if(!candidate||q.total<candidate.total)candidate=q;
        if(candidate&&candidate.total<.003)this.removeTile(candidate);
        else if(this.maxTiles>1)this.mergeOverflowTile(tx,tz);
        else{let nearest=null,d2=Infinity;for(const q of this.tiles.values()){const dx=q.tx-tx,dz=q.tz-tz,v=dx*dx+dz*dz;if(v<d2){d2=v;nearest=q;}}if(nearest){this.metrics.tileMerges++;return nearest;}}
      }
      t={key:k,tx,tz,total:0,maxHeight:0,volume:new Float64Array(this.n*this.n),mix:Object.create(null),collider:null,visual:null,part:{id:'rubble:'+k,rubble:true,rubbleTile:k,material:'brick',rubbleMixture:{brick:1},packing:.56,damage:0,strengthScale:1,velocity:[0,0,0],indestructible:false,dynamic:false,bounds:{min:[tx*this.tileSize,0,tz*this.tileSize],max:[(tx+1)*this.tileSize,.01,(tz+1)*this.tileSize]}}};this.tiles.set(k,t);return t;
    }
    removeTile(tile){
      if(tile.collider&&this.world){try{this.world.removeCollider(tile.collider,true);}catch(_){}}tile.collider=null;
      if(tile.visual){tile.visual.parent?.remove(tile.visual);tile.visual.geometry?.dispose();tile.visual=null;}this.tiles.delete(tile.key);this.dirty.delete(tile.key);this.visualDirty=true;
    }
    ensureMix(tile,material){return tile.mix[material]||(tile.mix[material]=new Float64Array(this.n*this.n));}
    getCell(cx,cz,create=false){
      const [tx,tz]=this.tileCoords(cx,cz),tile=create?this.ensureTile(tx,tz):this.tiles.get(key(tx,tz));if(!tile)return null;
      const lx=Math.max(0,Math.min(this.n-1,cx-tile.tx*this.n)),lz=Math.max(0,Math.min(this.n-1,cz-tile.tz*this.n)),index=lz*this.n+lx;return {tile,index,cx:tile.tx*this.n+lx,cz:tile.tz*this.n+lz};
    }
    cellPacking(cell){
      const {tile,index}=cell,total=tile.volume[index];if(total<=1e-12)return .56;let sum=0;for(const [m,a] of Object.entries(tile.mix))sum+=(a[index]||0)*(PACKING[m]||.56);return clamp(sum/total,.28,.78);
    }
    cellHeightByCell(cell){if(!cell)return 0;return cell.tile.volume[cell.index]/(this.cellSize*this.cellSize*this.cellPacking(cell));}
    cellHeight(cx,cz){return this.cellHeightByCell(this.getCell(cx,cz,false));}
    heightAt(x,z){
      const fx=x/this.cellSize-.5,fz=z/this.cellSize-.5,cx=Math.floor(fx),cz=Math.floor(fz),u=fx-cx,v=fz-cz;
      const h00=this.cellHeight(cx,cz),h10=this.cellHeight(cx+1,cz),h01=this.cellHeight(cx,cz+1),h11=this.cellHeight(cx+1,cz+1);
      return (h00*(1-u)+h10*u)*(1-v)+(h01*(1-u)+h11*u)*v;
    }
    surfaceHeightAt(x,z){return this.terrainHeightAt(x,z)+this.heightAt(x,z);}
    pileSlopeAt(x,z){const s=this.cellSize,hx=(this.heightAt(x+s,z)-this.heightAt(x-s,z))/(2*s),hz=(this.heightAt(x,z+s)-this.heightAt(x,z-s))/(2*s);return Math.hypot(hx,hz);}
    slopeAt(x,z){const s=this.cellSize,hx=(this.surfaceHeightAt(x+s,z)-this.surfaceHeightAt(x-s,z))/(2*s),hz=(this.surfaceHeightAt(x,z+s)-this.surfaceHeightAt(x,z-s))/(2*s);return Math.hypot(hx,hz);}
    movementCostAt(x,z){const h=this.heightAt(x,z),s=this.pileSlopeAt(x,z);return 1+Math.min(3,h*.9+s*1.8);}
    coverAt(x,z){return clamp(this.heightAt(x,z)/1.35,0,1);}
    sampleNavigation(x,z){return {height:this.heightAt(x,z),surfaceHeight:this.surfaceHeightAt(x,z),rubbleSlope:this.pileSlopeAt(x,z),slope:this.slopeAt(x,z),movementCost:this.movementCostAt(x,z),cover:this.coverAt(x,z)};}
    mixtureAt(x,z){
      const [cx,cz]=this.cellCoords(x,z),cell=this.getCell(cx,cz,false);if(!cell||cell.tile.volume[cell.index]<=1e-12)return {};
      const out={},total=cell.tile.volume[cell.index];for(const [m,a] of Object.entries(cell.tile.mix))if(a[cell.index]>1e-12)out[m]=a[cell.index]/total;return out;
    }
    dominantAt(x,z){const mix=this.mixtureAt(x,z);let best='brick',weight=0;for(const [m,w] of Object.entries(mix))if(w>weight){best=m;weight=w;}return best;}
    mark(tile){if(!tile)return;this.dirty.add(tile.key);this.visualDirty=true;}
    addCell(cx,cz,volume,material){
      if(volume<=0)return 0;const cell=this.getCell(cx,cz,true);if(!cell)return 0;const a=this.ensureMix(cell.tile,material);a[cell.index]+=volume;cell.tile.volume[cell.index]+=volume;cell.tile.total+=volume;this.mark(cell.tile);return volume;
    }
    removeCellVolume(cell,volume){
      if(!cell||volume<=0)return null;const tile=cell.tile,index=cell.index,total=tile.volume[index],take=Math.min(total,volume);if(take<=0)return null;const mix={};for(const [m,a] of Object.entries(tile.mix)){const share=total>0?a[index]/total:0,q=take*share;if(q>0){a[index]-=q;mix[m]=q;}}
      tile.volume[index]-=take;tile.total=Math.max(0,tile.total-take);this.mark(tile);return {volume:take,mix};
    }
    transfer(from,to,volume){
      const removed=this.removeCellVolume(from,volume);if(!removed)return 0;let moved=0;for(const [m,q] of Object.entries(removed.mix))moved+=this.addCell(to.cx,to.cz,q,m);this.metrics.relaxTransfers++;return moved;
    }
    relax(cx,cz,radius=3,iterations=4){
      const maxDelta=this.maxSlope*this.cellSize;
      for(let it=0;it<iterations;it++)for(let z=cz-radius;z<=cz+radius;z++)for(let x=cx-radius;x<=cx+radius;x++){
        const a=this.getCell(x,z,false);if(!a||a.tile.volume[a.index]<=1e-12)continue;const ha=this.cellHeightByCell(a);
        for(const [dx,dz] of [[1,0],[0,1],[-1,0],[0,-1]]){const b=this.getCell(x+dx,z+dz,true);if(!b)continue;const hb=this.cellHeightByCell(b),diff=ha-hb;if(diff<=maxDelta)continue;const packing=this.cellPacking(a),move=Math.min(a.tile.volume[a.index]*.22,(diff-maxDelta)*this.cellSize*this.cellSize*packing*.24);if(move>1e-7)this.transfer(a,b,move);}
      }
    }
    deposit(position,solidVolume,material='brick',options={}){
      if(!Number.isFinite(solidVolume)||solidVolume<=0||this.disposed)return 0;const x=position[0],z=position[2],impact=Math.max(0,options.impactSpeed||0),base=Math.cbrt(solidVolume),spread=clamp(options.spreadRadius??base*(1.15+Math.min(1.8,impact*.035)),this.cellSize*.55,2.4),[cx,cz]=this.cellCoords(x,z),r=Math.max(1,Math.ceil(spread/this.cellSize)),weights=[];let sum=0;
      for(let dz=-r;dz<=r;dz++)for(let dx=-r;dx<=r;dx++){const px=(cx+dx+.5)*this.cellSize,pz=(cz+dz+.5)*this.cellSize,d=Math.hypot(px-x,pz-z);if(d>spread*1.15)continue;const w=Math.exp(-2.35*(d/Math.max(this.cellSize*.4,spread))**2);weights.push([cx+dx,cz+dz,w]);sum+=w;}
      if(!weights.length){weights.push([cx,cz,1]);sum=1;}let deposited=0;for(const [gx,gz,w] of weights)deposited+=this.addCell(gx,gz,solidVolume*w/sum,material);this.totalDeposited+=deposited;this.metrics.deposits++;this.relax(cx,cz,r+2,options.relaxIterations??4);return deposited;
    }
    depositMicro(position,solidVolume,material='brick'){const q=this.deposit(position,solidVolume,material,{spreadRadius:Math.max(this.cellSize*.8,Math.cbrt(Math.max(0,solidVolume))*2.1),relaxIterations:3});if(q>0)this.metrics.microDeposits++;return q;}
    depositPart(part,options={}){
      const center=part.bounds?V.mul(V.add(part.bounds.min,part.bounds.max),.5):S.centroid(part.faces),volume=Math.max(0,part.volume||S.volume(part.faces));const q=this.deposit(center,volume,part.material||'brick',options);if(q>0)this.metrics.bakes++;return q;
    }
    totalVolume(){let total=0;for(const t of this.tiles.values())total+=t.total;return total;}
    materialVolumes(){const out={};for(const tile of this.tiles.values())for(const [m,a] of Object.entries(tile.mix)){let total=0;for(let i=0;i<a.length;i++)total+=a[i];if(total>1e-12)out[m]=(out[m]||0)+total;}return out;}
    accounting(){return {bakedVolume:this.totalVolume(),materials:this.materialVolumes(),tiles:this.tiles.size,totalDeposited:this.totalDeposited,totalEjected:this.totalEjected,totalRemoved:this.totalRemoved};}
    rebuildTileStats(tile){
      let max=0,total=0,mixes={};for(let i=0;i<tile.volume.length;i++){total+=tile.volume[i];max=Math.max(max,this.cellHeightByCell({tile,index:i}));for(const [m,a] of Object.entries(tile.mix))mixes[m]=(mixes[m]||0)+a[i];}
      let baseMin=Infinity,baseMax=-Infinity,surfaceMin=Infinity,surfaceMax=-Infinity;for(let z=0;z<=this.n;z++)for(let x=0;x<=this.n;x++){const wx=tile.tx*this.tileSize+x*this.cellSize,wz=tile.tz*this.tileSize+z*this.cellSize,base=this.terrainHeightAt(wx,wz),surface=this.vertexHeight(tile,x,z);baseMin=Math.min(baseMin,base);baseMax=Math.max(baseMax,base);surfaceMin=Math.min(surfaceMin,surface);surfaceMax=Math.max(surfaceMax,surface);}
      tile.total=total;tile.maxHeight=max;tile.baseMin=Number.isFinite(baseMin)?baseMin:0;tile.baseMax=Number.isFinite(baseMax)?baseMax:0;tile.surfaceMin=Number.isFinite(surfaceMin)?surfaceMin:tile.baseMin;tile.surfaceMax=Number.isFinite(surfaceMax)?surfaceMax:tile.baseMax;let material='brick',best=0;for(const [m,q] of Object.entries(mixes))if(q>best){best=q;material=m;}const mixture={};if(total>0)for(const [m,q] of Object.entries(mixes))if(q>1e-10)mixture[m]=q/total;
      tile.part.material=material;tile.part.rubbleMixture=mixture;tile.part.packing=total>0?Object.entries(mixture).reduce((s,[m,w])=>s+w*(PACKING[m]||.56),0):.56;tile.part.bounds.min[1]=tile.baseMin;tile.part.bounds.max[1]=Math.max(tile.baseMin+.01,tile.surfaceMax);
    }
    vertexHeight(tile,vx,vz){
      let sum=0,n=0;for(let dz=-1;dz<=0;dz++)for(let dx=-1;dx<=0;dx++){const lx=vx+dx,lz=vz+dz;if(lx<0||lz<0||lx>=this.n||lz>=this.n)continue;sum+=this.cellHeightByCell({tile,index:lz*this.n+lx});n++;}
      const x=tile.tx*this.tileSize+vx*this.cellSize,z=tile.tz*this.tileSize+vz*this.cellSize;return this.terrainHeightAt(x,z)+(n?sum/n:0);
    }
    meshData(tile){
      const vertices=[],indices=[],ox=tile.tx*this.tileSize,oz=tile.tz*this.tileSize;for(let z=0;z<=this.n;z++)for(let x=0;x<=this.n;x++)vertices.push(ox+x*this.cellSize,this.vertexHeight(tile,x,z),oz+z*this.cellSize);
      for(let z=0;z<this.n;z++)for(let x=0;x<this.n;x++){const a=z*(this.n+1)+x,b=a+1,c=a+(this.n+1),d=c+1;indices.push(a,c,b,b,c,d);}return {vertices,indices};
    }
    attachPhysics(world,P){this.world=world;this.P=P;return this;}
    attachModel(model){this.model=model;model.rubbleField=this;return this;}
    attachVisual(parent){
      if(!parent||typeof THREE==='undefined')return this;this.root=new THREE.Group();this.root.name='RubbleField';this.root.userData.rubbleField=true;parent.updateMatrixWorld?.(true);parent.add(this.root);this.root.matrixAutoUpdate=false;this.root.matrix.copy(parent.matrixWorld).invert();this.root.updateMatrixWorld(true);
      this.chunkGeometry=new THREE.BoxGeometry(.22,.13,.18);this.chunkMaterial=new THREE.MeshStandardMaterial({color:0xffffff,roughness:1,vertexColors:true});this.chunkMesh=new THREE.InstancedMesh(this.chunkGeometry,this.chunkMaterial,this.maxChunkInstances);this.chunkMesh.count=0;this.chunkMesh.castShadow=true;this.chunkMesh.receiveShadow=true;this.chunkMesh.frustumCulled=false;this.root.add(this.chunkMesh);return this;
    }
    materialFor(material){
      if(!this.visualMaterials.has(material)&&typeof THREE!=='undefined')this.visualMaterials.set(material,new THREE.MeshStandardMaterial({color:COLORS[material]||0x7d756b,roughness:1,metalness:material==='steel'||material==='armor'?.18:0}));
      return this.visualMaterials.get(material);
    }
    rebuildVisualTile(tile,data){
      if(!this.root||typeof THREE==='undefined')return;if(tile.visual){tile.visual.parent?.remove(tile.visual);tile.visual.geometry.dispose();}
      if(tile.total<1e-6){tile.visual=null;return;}const g=new THREE.BufferGeometry();g.setAttribute('position',new THREE.Float32BufferAttribute(data.vertices,3));g.setIndex(data.indices);g.computeVertexNormals();g.computeBoundingSphere();const mesh=new THREE.Mesh(g,this.materialFor(tile.part.material));mesh.receiveShadow=true;mesh.castShadow=false;mesh.userData.rubbleTile=tile.key;this.root.add(mesh);tile.visual=mesh;
    }
    sampleCellMaterial(tile,index,r=.5){
      const total=tile.volume[index];if(total<=1e-12)return tile.part.material||'brick';let acc=0;for(const [m,a] of Object.entries(tile.mix)){acc+=(a[index]||0)/total;if(r<=acc)return m;}return tile.part.material||'brick';
    }
    rebuildChunks(camera=null){
      if(!this.chunkMesh||typeof THREE==='undefined')return;const m=new THREE.Matrix4(),q=new THREE.Quaternion(),scale=new THREE.Vector3(),pos=new THREE.Vector3();let count=0;
      const near=tile=>!camera||Math.hypot((tile.tx+.5)*this.tileSize-camera.position.x,(tile.tz+.5)*this.tileSize-camera.position.z)<34;
      for(const tile of this.tiles.values()){if(!near(tile))continue;for(let i=0;i<tile.volume.length&&count<this.maxChunkInstances;i++){const vol=tile.volume[i];if(vol<.0012)continue;const x=i%this.n,z=Math.floor(i/this.n),seed=hash(tile.key+'/'+i),pieces=Math.min(3,Math.max(1,Math.floor(vol/.018)+1));for(let p=0;p<pieces&&count<this.maxChunkInstances;p++){const r1=rand01(seed+p*17),r2=rand01(seed+p*31),r3=rand01(seed+p*47),size=clamp(Math.cbrt(vol/pieces)*.65,.05,.28);const wx=tile.tx*this.tileSize+(x+.2+.6*r1)*this.cellSize,wz=tile.tz*this.tileSize+(z+.2+.6*r2)*this.cellSize;pos.set(wx,this.surfaceHeightAt(wx,wz)+size*.28,wz);q.setFromEuler(new THREE.Euler(r3*2.5,r1*6.28,r2*2.5));scale.set(size*(.75+r1*.6),size*(.45+r2*.45),size*(.7+r3*.7));m.compose(pos,q,scale);this.chunkMesh.setMatrixAt(count,m);if(this.chunkMesh.setColorAt){const material=this.sampleCellMaterial(tile,i,rand01(seed+p*73));this.chunkMesh.setColorAt(count,new THREE.Color(COLORS[material]||0x7d756b));}count++;}}}
      this.chunkMesh.count=count;this.chunkMesh.instanceMatrix.needsUpdate=true;if(this.chunkMesh.instanceColor)this.chunkMesh.instanceColor.needsUpdate=true;
    }
    heightfieldData(tile){
      const size=this.n+1,heights=new Float32Array(size*size);for(let x=0;x<size;x++)for(let z=0;z<size;z++)heights[x*size+z]=this.vertexHeight(tile,x,z);return {nrows:this.n,ncols:this.n,heights};
    }
    rebuildCollider(tile,data){
      if(!this.world||!this.P)return;if(tile.collider){try{this.world.removeCollider(tile.collider,true);}catch(_){ }tile.collider=null;}tile.colliderMode=null;if(tile.total<1e-6)return;
      const create=(desc,mode)=>{if(!desc)return false;try{tile.collider=this.world.createCollider(desc);tile.colliderMode=mode;this.metrics.colliderBuilds++;return true;}catch(_){tile.collider=null;return false;}};
      if(typeof this.P.ColliderDesc.heightfield==='function')try{const h=this.heightfieldData(tile),cx=(tile.tx+.5)*this.tileSize,cz=(tile.tz+.5)*this.tileSize,desc=this.P.ColliderDesc.heightfield(h.nrows,h.ncols,h.heights,{x:this.tileSize,y:1,z:this.tileSize}).setTranslation(cx,0,cz);if(create(desc,'heightfield'))return;}catch(_){}
      if(this.P.ColliderDesc.trimesh)try{const desc=this.P.ColliderDesc.trimesh(new Float32Array(data.vertices),new Uint32Array(data.indices));if(create(desc,'trimesh')){this.metrics.colliderFallbacks++;return;}}catch(_){}
      this.metrics.colliderFallbacks++;const cx=(tile.tx+.5)*this.tileSize,cz=(tile.tz+.5)*this.tileSize,h=Math.max(.02,tile.surfaceMax-(tile.baseMin??0)),base=tile.baseMin??0,desc=this.P.ColliderDesc.cuboid(this.tileSize/2,h/2,this.tileSize/2).setTranslation(cx,base+h/2,cz);create(desc,'cuboid');
    }
    flush(){
      if(!this.dirty.size)return 0;const keys=[...this.dirty];this.dirty.clear();for(const k of keys){const tile=this.tiles.get(k);if(!tile)continue;this.rebuildTileStats(tile);if(tile.total<1e-12){this.removeTile(tile);continue;}const data=this.meshData(tile);this.rebuildCollider(tile,data);this.rebuildVisualTile(tile,data);this.metrics.tilesBuilt++;}this.rebuildChunks();this.visualDirty=false;return keys.length;
    }
    updateVisual(camera){
      if(!this.root)return;for(const tile of this.tiles.values())if(tile.visual){const d=Math.hypot((tile.tx+.5)*this.tileSize-camera.position.x,(tile.tz+.5)*this.tileSize-camera.position.z);tile.visual.visible=d<170;}if(this.chunkMesh){const was=this.chunkMesh.visible,visible=[...this.tiles.values()].some(t=>Math.hypot((t.tx+.5)*this.tileSize-camera.position.x,(t.tz+.5)*this.tileSize-camera.position.z)<34);this.chunkMesh.visible=visible;if(visible&&(!was||this.visualDirty))this.rebuildChunks(camera);}
    }
    tileForPoint(x,z){const [cx,cz]=this.cellCoords(x,z),[tx,tz]=this.tileCoords(cx,cz);return this.tiles.get(key(tx,tz));}
    normalAt(x,z){const s=this.cellSize*.5,hx=(this.surfaceHeightAt(x+s,z)-this.surfaceHeightAt(x-s,z))/(2*s),hz=(this.surfaceHeightAt(x,z+s)-this.surfaceHeightAt(x,z-s))/(2*s);return V.unit([-hx,1,-hz]);}
    thicknessFrom(point,dir,radius=0,maxDistance=20){
      const d=V.unit(dir),step=Math.min(.12,this.cellSize*.24);let inside=true,last=0;for(let dist=step;dist<=maxDistance;dist+=step){const p=V.add(point,V.mul(d,dist)),pile=this.heightAt(p[0],p[2]),surface=this.surfaceHeightAt(p[0],p[2]),base=this.terrainHeightAt(p[0],p[2]),now=pile>1e-4&&p[1]-radius<=surface&&p[1]+radius>=base-.02;if(!now&&inside){let lo=last,hi=dist;for(let i=0;i<8;i++){const mid=(lo+hi)/2,q=V.add(point,V.mul(d,mid)),pile=this.heightAt(q[0],q[2]),surface=this.surfaceHeightAt(q[0],q[2]),base=this.terrainHeightAt(q[0],q[2]);if(pile>1e-4&&q[1]-radius<=surface&&q[1]+radius>=base-.02)lo=mid;else hi=mid;}return Math.max(.001,lo);}inside=now;last=dist;}return maxDistance;
    }
    trace(a,b,radius=0){
      const delta=V.sub(b,a),length=V.length(delta);if(length<1e-9||!this.tiles.size)return null;
      const minTx=Math.floor((Math.min(a[0],b[0])-radius)/this.tileSize),maxTx=Math.floor((Math.max(a[0],b[0])+radius)/this.tileSize),minTz=Math.floor((Math.min(a[2],b[2])-radius)/this.tileSize),maxTz=Math.floor((Math.max(a[2],b[2])+radius)/this.tileSize),minY=Math.min(a[1],b[1])-radius,maxY=Math.max(a[1],b[1])+radius;let near=false;
      outer:for(let tz=minTz;tz<=maxTz;tz++)for(let tx=minTx;tx<=maxTx;tx++){const tile=this.tiles.get(key(tx,tz));if(!tile||tile.total<=1e-8)continue;if(this.dirty.has(tile.key))this.rebuildTileStats(tile);if((tile.surfaceMax??tile.maxHeight)+radius>=minY&&maxY>=(tile.baseMin??0)-.02){near=true;break outer;}}if(!near)return null;const steps=Math.min(128,Math.max(1,Math.ceil(length/(this.cellSize*.28)))),inside=t=>{const p=V.mix(a,b,t),pile=this.heightAt(p[0],p[2]),surface=this.surfaceHeightAt(p[0],p[2]),base=this.terrainHeightAt(p[0],p[2]);return pile>1e-4&&p[1]-radius<=surface&&p[1]+radius>=base-.02;};let prev=inside(0),prevT=0;if(prev){const p=a.slice(),tile=this.tileForPoint(p[0],p[2]);if(!tile)return null;if(this.dirty.has(tile.key))this.rebuildTileStats(tile);const thickness=this.thicknessFrom(p,delta,radius);this.metrics.ballisticHits++;return {enter:0,exit:Math.min(1,thickness/length),normal:this.normalAt(p[0],p[2]),part:tile.part,point:p,rubbleThickness:thickness};}
      for(let i=1;i<=steps;i++){const t=i/steps,cur=inside(t);if(cur&&!prev){let lo=prevT,hi=t;for(let j=0;j<8;j++){const mid=(lo+hi)/2;if(inside(mid))hi=mid;else lo=mid;}const enter=hi,p=V.mix(a,b,enter),tile=this.tileForPoint(p[0],p[2]);if(!tile)return null;if(this.dirty.has(tile.key))this.rebuildTileStats(tile);const thickness=this.thicknessFrom(p,delta,radius);this.metrics.ballisticHits++;return {enter,exit:Math.min(1,enter+thickness/length),normal:this.normalAt(p[0],p[2]),part:tile.part,point:p,rubbleThickness:thickness};}prev=cur;prevT=t;}return null;
    }
    removeDistributed(point,radius,targetVolume){
      const [cx,cz]=this.cellCoords(point[0],point[2]),r=Math.max(1,Math.ceil(radius/this.cellSize)),cells=[];let totalWeight=0,available=0;for(let dz=-r;dz<=r;dz++)for(let dx=-r;dx<=r;dx++){const c=this.getCell(cx+dx,cz+dz,false);if(!c)continue;const px=(cx+dx+.5)*this.cellSize,pz=(cz+dz+.5)*this.cellSize,d=Math.hypot(px-point[0],pz-point[2]);if(d>radius)continue;const w=(1-d/Math.max(radius,1e-6))**2+.04;cells.push([c,w]);totalWeight+=w;available+=c.tile.volume[c.index];}
      const take=Math.min(available,targetVolume),mix={};let removed=0;if(take<=0)return {volume:0,mix};for(const [c,w] of cells){const q=take*w/totalWeight,res=this.removeCellVolume(c,q);if(!res)continue;removed+=res.volume;for(const [m,v] of Object.entries(res.mix))mix[m]=(mix[m]||0)+v;}return {volume:removed,mix};
    }
    emitPackets(point,direction,removed,energy,count=2){
      if(!removed.volume||!this.onEject)return {volume:0,mix:{}};const totalEject=removed.volume*.32,materials=Object.entries(removed.mix).filter(([,v])=>v>1e-12).sort((a,b)=>b[1]-a[1]);if(!materials.length)return {volume:0,mix:{}};
      const targetCount=Math.max(1,Math.min(count,materials.length*2)),selected=materials.slice(0,Math.min(materials.length,targetCount)),packets=[],ejectedMix={};let emitted=0,selectedVolume=selected.reduce((s,[,v])=>s+v,0);
      for(let i=0;i<targetCount;i++){const [material,available]=selected[i%selected.length],share=available/Math.max(1e-12,selectedVolume),targetMaterial=totalEject*share,already=ejectedMix[material]||0,slotsLeft=Math.ceil((targetCount-i)/selected.length),volume=Math.max(0,(targetMaterial-already)/Math.max(1,slotsLeft));if(volume<=1e-12)continue;packets.push({material,volume});ejectedMix[material]=(ejectedMix[material]||0)+volume;}
      for(let i=0;i<packets.length;i++){const packet=packets[i],side=(i-(packets.length-1)/2)*.18,dir=V.unit(V.add(V.unit(direction),[side,.28+Math.abs(side),-side*.4])),speed=clamp(Math.sqrt(Math.max(0,energy))*0.025,1.5,14);this.onEject({position:point.slice(),volume:packet.volume,material:packet.material,velocity:V.mul(dir,speed),source:'rubble'});emitted+=packet.volume;this.metrics.ejectedPackets++;}
      this.totalEjected+=emitted;return {volume:emitted,mix:ejectedMix};
    }
    damageAt(part,energy,point,direction,diameter,context={}){
      const radius=clamp(Math.max(this.cellSize*.75,diameter*7,Math.cbrt(Math.max(0,energy)/1e5)*.22),.3,1.6),target=Math.min(.18,Math.max(.002,energy/5e6)),removed=this.removeDistributed(point,radius,target);if(!removed.volume)return {material:part.material,materialMode:'rubble-scatter',response:'mark',geometryChanged:false,damageEnergy:0,radius};
      const ejected=this.emitPackets(point,direction,removed,energy,Math.min(3,Math.max(1,Math.round(removed.volume/.025)))),p=V.add(point,[direction[0]*radius*.7,0,direction[2]*radius*.7]);
      for(const [m,v] of Object.entries(removed.mix)){const q=Math.max(0,v-(ejected.mix[m]||0));if(q>0)this.deposit(p,q,m,{spreadRadius:radius*.85,relaxIterations:2});}
      this.totalRemoved+=removed.volume;return {material:part.material,damageMaterial:part.material,materialMode:'rubble-scatter',response:'rubble-scatter',geometryChanged:true,damageEnergy:energy,radius,frontRadius:radius*.55,stressRadius:radius,exitRadius:radius*.65,removedVolume:removed.volume,ejectedVolume:ejected.volume};
    }
    blast(origin,radius,energy){
      if(radius<=0||energy<=0)return {removed:0,ejected:0};let removed=0,ejected=0;const tiles=[...this.tiles.values()];for(const tile of tiles){const cx=(tile.tx+.5)*this.tileSize,cz=(tile.tz+.5)*this.tileSize,d=Math.hypot(cx-origin[0],cz-origin[2]);if(d>radius+this.tileSize*.8)continue;const fall=clamp(1-d/Math.max(radius,1e-6),0,1),target=Math.min(tile.total*.28,energy*fall/4e7);if(target<=1e-5)continue;const r=this.removeDistributed([cx,0,cz],Math.min(this.tileSize*.7,radius),target);removed+=r.volume;const localEjected=this.emitPackets([cx,this.surfaceHeightAt(cx,cz),cz],V.unit([cx-origin[0],.45,cz-origin[2]]),r,energy*fall,Math.min(3,Math.max(1,Math.round(r.volume/.03))));ejected+=localEjected.volume;if(r.volume>0)for(const [m,v] of Object.entries(r.mix)){const kept=Math.max(0,v-(localEjected.mix[m]||0));if(kept>0)this.deposit([cx,0,cz],kept,m,{spreadRadius:Math.min(2,radius*.18),relaxIterations:2});}}
      this.metrics.blastEvents++;return {removed,ejected};
    }
    dispose(){
      if(this.disposed)return;this.disposed=true;for(const tile of [...this.tiles.values()])this.removeTile(tile);this.tiles.clear();this.dirty.clear();if(this.chunkMesh)this.chunkMesh.parent?.remove(this.chunkMesh);this.chunkGeometry?.dispose();this.chunkMaterial?.dispose();for(const m of this.visualMaterials.values())m.dispose();this.visualMaterials.clear();if(this.root)this.root.parent?.remove(this.root);if(this.model?.rubbleField===this)this.model.rubbleField=null;this.model=null;this.world=null;this.P=null;
    }
  }
  R.RubbleField=RubbleField;
})();
