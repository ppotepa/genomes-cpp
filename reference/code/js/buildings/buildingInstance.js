(function(){
  'use strict';
  const B=globalThis.RTS.Buildings;
  class BuildingInstance{
    constructor(root,manifest,materials,components,navOverlay){this.root=root;this.manifest=manifest;this.components=components;this.materials=materials;this.navOverlay=navOverlay;this.plan=null;this.cutaway=false;this.roofVisible=true;this.navVisible=false;this.visibleFloor=null;this.disposed=false;this._initialState=components.map(c=>({id:c.id,health:c.health,destroyed:!!c.destroyed}));}
    _refresh(){
      const bounds=this.plan?B.Polygon.bounds(this.plan.footprint.polygon):[0,0,0,0],midZ=(bounds[2]+bounds[3])/2;
      for(const c of this.components){
        const floorHidden=this.visibleFloor!==null&&Number.isFinite(c.floor)&&c.floor>this.visibleFloor;
        const roofPart=['roof','roof-detail','gable'].includes(c.category),roofHidden=(!this.roofVisible||this.cutaway||this.visibleFloor!==null&&this.visibleFloor<this.plan.storeys.length-1)&&roofPart;
        const wall=this.cutaway&&c.wallId?this.plan.walls.find(w=>w.id===c.wallId):null;
        const cutHidden=!!wall&&(wall.edge.a[1]+wall.edge.b[1])/2>=midZ;
        c.mesh.visible=!c.destroyed&&!floorHidden&&!roofHidden&&!cutHidden;
      }
      if(this.navOverlay)this.navOverlay.visible=this.navVisible;
    }
    setCutaway(v){this.cutaway=!!v;this._refresh();return this;} setVisibleFloor(v){this.visibleFloor=Number.isInteger(v)?Math.max(0,v):null;this._refresh();return this;}
    setRoofVisible(v){this.roofVisible=!!v;this._refresh();return this;} setNavVisible(v){this.navVisible=!!v;this._refresh();return this;}
    _metrics(){const lost=this.components.reduce((n,c)=>n+(c.surfaceArea||1)*(1-Math.max(0,c.health)/Math.max(1,c.maxHealth)),0),area=this.components.reduce((n,c)=>n+(c.surfaceArea||1),0);this.manifest.damage.destroyed=this.components.filter(c=>c.destroyed).length;this.manifest.damage.total=this.components.length;this.manifest.damage.achievedPercent=Math.round(lost/Math.max(1,area)*100);this.manifest.damage.damagePercent=this.manifest.damage.achievedPercent;}
    damageSphere(point,radius=1.4,power=95){
      if(!(radius>0)||!Number.isFinite(power))return 0;
      const p=point?.isVector3?point:new THREE.Vector3(point?.x??point?.[0]??0,point?.y??point?.[1]??0,point?.z??point?.[2]??0);
      let destroyed=0;this.root.updateMatrixWorld(true);
      for(const c of this.components){
        if(c.destroyed||c.category==='furniture'||c.category==='nav')continue;
        // Geometry may use absolute plan coordinates (roof/slab prisms), so a
        // mesh origin is not the location of its physical surface.
        const bounds=new THREE.Box3().setFromObject(c.mesh),d=bounds.distanceToPoint(p);
        if(d>=radius)continue;c.health-=power*(1-d/radius)*(c.structural?.8:1.1);
        if(c.health<=0){c.destroyed=true;destroyed++;}
      }
      this._metrics();this._refresh();return destroyed;
    }
    breachFront(){
      const wall=this.plan.walls.filter(w=>!w.internal&&w.floor===0).sort((a,b)=>b.length-a.length)[0];
      if(!wall)return 0;
      const p=new THREE.Vector3((wall.edge.a[0]+wall.edge.b[0])/2,1.1,(wall.edge.a[1]+wall.edge.b[1])/2);
      this.root.updateMatrixWorld(true);return this.damageSphere(this.root.localToWorld(p),1.8,155);
    }
    resetDamage(){for(const c of this.components){const s=this._initialState.find(x=>x.id===c.id);c.health=s?.health??c.maxHealth;c.destroyed=!!s?.destroyed;}this._metrics();this._refresh();return this;}
    exportManifest(){return JSON.parse(JSON.stringify(this.manifest));} stats(){return {components:this.components.length,visible:this.components.filter(c=>c.mesh.visible).length,damage:this.manifest.damage};}
    dispose(){if(this.disposed)return;this.disposed=true;const gs=new Set(),ms=new Set(this.materials);this.root.traverse(o=>{if(o.geometry)gs.add(o.geometry);if(o.material)(Array.isArray(o.material)?o.material:[o.material]).forEach(m=>ms.add(m));});gs.forEach(g=>g.dispose());ms.forEach(m=>m.dispose?.());this.root.parent?.remove(this.root);this.root.clear();}
  }
  B.BuildingInstance=BuildingInstance;
})();
