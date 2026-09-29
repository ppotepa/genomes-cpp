(function(){
  'use strict';
  const R=globalThis.RTS;
  class WorldBallisticsBridge{
    constructor(battlefield,options={}){this.battlefield=battlefield;this.host=battlefield.worldDestruction;this.index=battlefield.buildingRepresentationManager.index;this.maxHits=options.maxHits||32;}
    querySegment(start,end){return this.index.querySegment(start,end).slice(0,this.maxHits);}
    traceSegment(start,end,options={}){const material=this.host.traceSegment(start,end,options.radius||0);if(material)return material;const hits=this.querySegment(start,end);if(!hits.length)return null;const hit=hits[0];if(options.materialize!==false)this.host.queueDamage(hit.runtime,{type:options.type==='HE'?'HE':'AP',hit,sphere:options.sphere});return hit;}
    materializeForSegment(start,end,projectile){const hit=this.querySegment(start,end)[0];if(!hit)return null;const type=projectile?.ammo?.kind==='he'?'HE':'AP';if(type==='HE'){const center=hit.point||start,dir=R.DestructionSolid.V.unit(R.DestructionSolid.V.sub(end,start));this.host.activateBlast(hit.runtime,{center,radius:projectile.ammo?.explosive?Math.cbrt(projectile.ammo.explosive)*10:2},{type:'HE'});}else this.host.activateHit(hit.runtime,hit,{type:'AP'});return hit;}
    flush(){this.host.update();return this;}
    fire(projectile){const hit=this.traceSegment(projectile.position,projectile.position.map((v,i)=>v+projectile.direction[i]*(projectile.length||10000)),{type:projectile.ammoKind==='he'?'HE':'AP',materialize:true});return hit;}
    stats(){return {indexedBuildings:this.index.stats().buildings,materializedParts:this.host.model.parts.size,queued:this.host.queue.length};}
  }
  R.WorldBallisticsBridge=WorldBallisticsBridge;
})();
