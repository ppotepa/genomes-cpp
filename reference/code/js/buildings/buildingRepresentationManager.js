(function(){
  'use strict';
  const R=globalThis.RTS;
  class BuildingRepresentationManager{
    constructor(camera,options={}){this.camera=camera;this.options={...R.Config.WORLD_BUILDING_RUNTIME,...options};this.runtimes=[];this.lastUpdate=-Infinity;this.index=options.index||new R.BuildingSpatialIndex(this.options.buildingSectorSize);}
    add(runtime){if(this.runtimes.includes(runtime))return runtime;this.runtimes.push(runtime);this.index.add(runtime);return runtime;}
    remove(runtime){this.runtimes=this.runtimes.filter(x=>x!==runtime);this.index.remove(runtime);return runtime;}
    update(now=0){if(now-this.lastUpdate<100)return this;this.lastUpdate=now;const p=this.camera?.getWorldPosition?this.camera.getWorldPosition(new THREE.Vector3()):this.camera?.position||new THREE.Vector3(),distances=this.runtimes.filter(r=>r.active!==false).map(runtime=>{const b=runtime.representation._bounds,h=runtime.plan.spec.storeys.count*runtime.plan.spec.storeys.floorHeight,center=runtime.localToWorld([(b[0]+b[1])/2,h/2,(b[2]+b[3])/2]);return {runtime,d:Math.hypot(p.x-center[0],p.y-center[1],p.z-center[2])};}).sort((a,b)=>a.d-b.d);let detailed=0;for(const {runtime,d} of distances){const threshold=this.options.worldDistance+(runtime.representation.detail==='WORLD'?10:-10),keep=d<threshold&&detailed<this.options.maxDetailedBuildings;runtime.setDetail(keep?'WORLD':'FAR');if(keep)detailed++;}return this;}
    stats(){return {proxyCount:this.runtimes.length,worldCount:this.runtimes.filter(r=>r.representation.detail==='WORLD').length,farCount:this.runtimes.filter(r=>r.representation.detail==='FAR').length,index:this.index.stats()};}
    dispose(){for(const r of this.runtimes)r.dispose();this.runtimes=[];this.index.clear();}
  }
  R.BuildingRepresentationManager=BuildingRepresentationManager;
})();
