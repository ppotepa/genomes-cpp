(function () {
  'use strict';
  const R=window.RTS;
  R.UnitDiagnostics={
    validate(model){
      const g=model.mesh.geometry,pos=g.attributes.position,w=g.attributes.skinWeight,idx=g.attributes.skinIndex,n=model.rig.bones.length;
      let maxWeightError=0,invalid=0;
      for(let i=0;i<pos.count;i++){
        let sum=0;for(let k=0;k<4;k++){
          const weight=w.array[i*4+k],index=idx.array[i*4+k];sum+=weight;
          if(!Number.isFinite(weight)||weight<0||index<0||index>=n)invalid++;
        }
        for(let k=0;k<3;k++)if(!Number.isFinite(pos.array[i*3+k]))invalid++;
        maxWeightError=Math.max(maxWeightError,Math.abs(1-sum));
      }
      const F=model.anatomy.faceLayout,meta=model.surface.faceMetadata;
      const anatomyOK=!!meta&&meta.neckConnected&&meta.mouthOpening&&!!meta.eyes.L&&!!meta.eyes.R&&F.levels.every(p=>p.every(Number.isFinite));
      return {anatomyOK,adjustments:F.adjustments.length,vertices:pos.count,triangles:g.index.count/3,bones:n,meshes:1,materialGroups:g.groups.length,
        maxWeightError,invalid,ok:invalid===0&&maxWeightError<1e-5&&anatomyOK,
        referenceMinY:model.referenceMinY,measuredStandingHeight:model.standingHeight,
        heightError:Math.abs(model.standingHeight-model.anatomy.height)};
    }
  };
  R.ColliderPreview=class ColliderPreview {
    constructor(rig,schema){
      this.rig=rig;this.schema=schema;this.root=new THREE.Group();this.root.name='PhysicsDescriptorsOnly';this.items=[];
      this.material=new THREE.MeshBasicMaterial({color:0xd9a261,wireframe:true,transparent:true,opacity:.35,depthTest:false,depthWrite:false});
      for(const d of schema.bodies){
        let g;
        if(d.shape==='box')g=new THREE.BoxGeometry(...d.halfExtents.map(v=>v*2));
        else {
          const points=[],r=d.radius,h=d.halfCylinder;
          for(let i=0;i<=6;i++){const t=-Math.PI/2+(Math.PI/2)*i/6;points.push(new THREE.Vector2(Math.max(0,r*Math.cos(t)),-h+r*Math.sin(t)));}
          for(let i=0;i<=6;i++){const t=Math.PI/2*i/6;points.push(new THREE.Vector2(Math.max(0,r*Math.cos(t)),h+r*Math.sin(t)));}
          g=new THREE.LatheGeometry(points,10);
        }
        const mesh=new THREE.Mesh(g,this.material);mesh.matrixAutoUpdate=false;mesh.renderOrder=20;this.root.add(mesh);this.items.push({mesh,definition:d});
      }
    }
    update(){this.rig.root.updateMatrixWorld(true);for(const it of this.items)R.RagdollSchema.bodyWorld(this.rig.byName[it.definition.bone],it.definition,it.mesh.matrix);this.root.updateMatrixWorld(true);}
    dispose(){this.items.forEach(i=>i.mesh.geometry.dispose());this.material.dispose();if(this.root.parent)this.root.parent.remove(this.root);}
  };
})();
