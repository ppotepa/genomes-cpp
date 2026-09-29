(function(){
  'use strict';
  const R=globalThis.RTS;
  class BuildingSpatialIndex{
    constructor(cellSize=R.Config.WORLD_BUILDING_RUNTIME.buildingSectorSize){this.cellSize=cellSize;this.cells=new Map();this.items=new Set();}
    _cells(runtime){const b=runtime.representation?runtime.representation._bounds:[-1,1,-1,1],a=runtime.root?.position||{x:0,z:0},angle=runtime.root?.rotation?.y||0,c=Math.cos(angle),s=Math.sin(angle),corners=[[b[0],b[2]],[b[1],b[2]],[b[1],b[3]],[b[0],b[3]]].map(([x,z])=>[a.x+x*c-z*s,a.z+x*s+z*c]),xs=corners.map(p=>p[0]),zs=corners.map(p=>p[1]),out=[];for(let x=Math.floor(Math.min(...xs)/this.cellSize);x<=Math.floor(Math.max(...xs)/this.cellSize);x++)for(let z=Math.floor(Math.min(...zs)/this.cellSize);z<=Math.floor(Math.max(...zs)/this.cellSize);z++)out.push(`${x},${z}`);return out;}
    add(runtime){this.remove(runtime);for(const key of this._cells(runtime)){const c=this.cells.get(key)||new Set();c.add(runtime);this.cells.set(key,c);}this.items.add(runtime);return runtime;}
    remove(runtime){for(const c of this.cells.values())c.delete(runtime);this.items.delete(runtime);return runtime;}
    queryAabb(bounds){const found=new Set();for(let x=Math.floor(bounds[0]/this.cellSize);x<=Math.floor(bounds[1]/this.cellSize);x++)for(let z=Math.floor(bounds[2]/this.cellSize);z<=Math.floor(bounds[3]/this.cellSize);z++)for(const r of this.cells.get(`${x},${z}`)||[])found.add(r);return [...found];}
    querySegment(start,end){const b=[Math.min(start[0],end[0]),Math.max(start[0],end[0]),Math.min(start[2],end[2]),Math.max(start[2],end[2])],hits=[];for(const r of this.queryAabb(b))for(const hit of r.queryLogicalHit(start,end))hits.push({...hit,runtime:r});return hits.sort((a,b)=>a.t-b.t);}
    stats(){return {buildings:this.items.size,cells:[...this.cells.values()].filter(c=>c.size).length};}
    clear(){this.cells.clear();this.items.clear();}
  }
  R.BuildingSpatialIndex=BuildingSpatialIndex;
})();
