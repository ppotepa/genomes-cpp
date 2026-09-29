(function(){
  'use strict';
  const R=globalThis.RTS;
  class BuildingRuntime{
    constructor(plan,options={}){this.plan=plan;this.id=options.id||`building-runtime-${plan.spec.seed}`;this.position=options.position||[0,0,0];this.rotation=options.rotation||0;this.representation=options.representation||new R.BuildingWorldRepresentation(plan,options);this.collision=new R.BuildingCollisionProxy(plan);this.damage=null;this.active=true;this.damageState={damagePercent:0,regions:new Set()};this.root=this.representation.root;this.root.position.set(...this.position);this.root.rotation.y=this.rotation;this.root.userData.buildingRuntime=this;}
    setWorldTransform(x,y,z,rotation=0){this.root.position.set(x,y,z);this.root.rotation.y=rotation;this.position=[x,y,z];this.rotation=rotation;return this;}
    worldToLocal(point){this.root.updateWorldMatrix(true,false);return new THREE.Vector3(...point).applyMatrix4(new THREE.Matrix4().copy(this.root.matrixWorld).invert()).toArray();}
    localToWorld(point){this.root.updateWorldMatrix(true,false);return new THREE.Vector3(...point).applyMatrix4(this.root.matrixWorld).toArray();}
    setDetail(detail){return this.representation.setDetail(detail);}
    queryLogicalHit(start,end){this.root.updateWorldMatrix(true,false);const inverse=new THREE.Matrix4().copy(this.root.matrixWorld).invert(),a=new THREE.Vector3(...start).applyMatrix4(inverse).toArray(),b=new THREE.Vector3(...end).applyMatrix4(inverse).toArray();return this.collision.querySegment(a,b).map(hit=>({...hit,point:new THREE.Vector3(...hit.point).applyMatrix4(this.root.matrixWorld).toArray(),normal:hit.normal?new THREE.Vector3(...hit.normal).transformDirection(this.root.matrixWorld).normalize().toArray():undefined,worldStart:start.slice(),worldEnd:end.slice()}));}
    hideDamageRegion(id){this.damageState.regions.add(id);return this.representation.hideDamageRegion(id);}
    showDamageRegion(id){this.damageState.regions.delete(id);return this.representation.showDamageRegion(id);}
    activateDamage(hit,context={}){if(!this.damage)this.damage=new R.BuildingDamageChunkManager(this,context);this.damage.activateForAP(hit);return this.damage.flush();}
    activateBlast(sphere,context={}){if(!this.damage)this.damage=new R.BuildingDamageChunkManager(this,context);this.damage.activateForHE(sphere);return this.damage.flush();}
    stats(){return {...this.representation.stats(),id:this.id,logical:this.collision.stats(),damageChunks:this.damage?.stats?.().activeChunks||0};}
    dispose(){if(!this.active)return;this.active=false;this.damage?.dispose?.();this.collision=null;this.representation.dispose();}
  }
  R.BuildingRuntime=BuildingRuntime;
})();
