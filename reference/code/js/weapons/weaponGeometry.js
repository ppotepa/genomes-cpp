(function () {
  'use strict';
  const R=window.RTS,V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z),W={root:1};
  // Geometry is immutable after construction. Keep exact variant-specific
  // buffers shared between soldiers while keeping transforms/material state local.
  const geometryCache=new Map();
  function acquireGeometry(key,build) {
    let entry=geometryCache.get(key);
    if(!entry){entry=build();entry.refs=0;geometryCache.set(key,entry);}
    entry.refs++;
    return entry;
  }
  function releaseGeometry(entry) {
    if(!entry||--entry.refs>0)return;
    geometryCache.delete(entry.key);
    entry.geometries.forEach(g=>g.dispose());
  }
  function grip(p,down,normal) {
    const y=V(...down).normalize(),z=V(...normal).normalize(),x=y.clone().cross(z).normalize();
    return {position:V(...p),quaternion:new THREE.Quaternion().setFromRotationMatrix(new THREE.Matrix4().makeBasis(x,y,z))};
  }
  R.WeaponGeometry={
    create(item) {
      const def=R.WeaponCatalog.get(item.definitionId);if(!def)throw new Error('Brak modelu przedmiotu.');
      // Independent, rigid Mesh: no body-height scaling and no skinning to a stow bone.
      const root=new THREE.Group();root.name='Weapon/'+item.slot+'/'+item.definitionId;
      const scale=item.variant.size,L=def.length,shade=item.variant.shade||1;
      // Include every catalog value that affects generated vertices/material colors.
      // The catalog definition id also distinguishes kind-specific construction paths.
      const cacheKey=JSON.stringify([item.definitionId,def.kind,L,scale,shade]);
      let resources=geometryCache.get(cacheKey);
      if(resources)resources.refs++;
      else resources=acquireGeometry(cacheKey,()=>{
      const rig={anatomy:{height:1},index:{root:0}},B=new R.GearGeometry(rig,false);
      // Knife and grenade silhouettes are closed, explicitly hinted primitives.
      // Firearms combine side-profile and attached components with shared edges,
      // so they retain the topology repair path.
      B.trustLocalWindingHints=def.kind==='knife'||def.kind==='grenade';
      const color=hex=>R.SurfaceColor.rgb(hex).map(c=>Math.min(1,c*shade));
      const metal=color(0x586164),dark=color(0x252c30),cloth=color(0x525b43),rubber=color(0x161b1b);
      const box=(p,s,c=dark,group=0)=>B.box(V(...p),V(...s),W,c,group,.18);
      const tube=(a,b,r,c=metal,group=0)=>B.cylinder(V(...a),V(...b),r,W,c,group,12);
      // Convex side profiles extruded in X: proper silhouettes, flat side panels.
      const profile=(points,width,c=cloth,group=1)=>{
        for(const sign of [-1,1]){
          const ids=points.map(([y,z])=>B.vertex(V(sign*width/2,y,z),W,c,V(sign,0,0)));
          for(let i=1;i<ids.length-1;i++)B.triangle(ids[0],ids[i],ids[i+1],group);
        }
        for(let i=0;i<points.length;i++){
          const a=points[i],b=points[(i+1)%points.length],normal=V(0,b[1]-a[1],a[0]-b[0]).normalize();
          const ids=[[-1,a],[1,a],[1,b],[-1,b]].map(([s,p])=>B.vertex(V(s*width/2,...p),W,c,normal));
          B.triangle(ids[0],ids[1],ids[2],group);B.triangle(ids[0],ids[2],ids[3],group);
        }
      };
      const guard=(points)=>B.tube(points.map(p=>V(0,...p)),.003,W,metal);
      const rail=(y,z0,z1)=>{box([0,y,(z0+z1)/2],[.026,.009,z1-z0]);for(let z=z0;z<z1;z+=.019)box([0,y+.006,z],[.033,.005,.008],metal);};
      let slideBuilder=null;
      let primary=grip([-.020,0,0],[0,-1,0],[1,0,0]),secondary=null,muzzle=null,butt=null;
      if(def.kind==='long') {
        const stock=L*.27,front=L-stock,support=def.id.includes('support'),heavy=def.id==='heavy_support_gun',marksman=def.id==='marksman_rifle';
        const handEnd=front*(support?.68:.65),handStart=.155;
        box([0,.069,.070],[heavy?.058:.046,.057,.235]);box([0,.032,.071],[.041,.033,.172],metal);
        tube([0,.080,-stock+.04],[0,.080,-.044],.017,dark);
        profile([[.105,-stock+.014],[.105,-.078],[.063,-.060],[.003,-stock+.04],[-.011,-stock+.014]],.045);
        box([0,.043,-stock+.007],[.053,.120,.016],rubber,2);
        box([0,.107,-stock*.62],[.046,.015,stock*.43],cloth,1);
        profile([[.033,-.029],[.028,.021],[-.070,-.006],[-.068,-.046]],.031,dark,1);
        for(let y=-.054;y<.020;y+=.014)box([0,y,-.015+(y+.020)*.24],[.033,.004,.026],rubber,2);
        guard([[.024,.023],[-.006,.034],[-.012,.071],[.026,.088]]);
        tube([0,.026,.058],[0,.002,.049],.003,dark);
        if(!support){
          profile([[.040,.088],[.040,.148],[-.058,.168],[-.128,.158],[-.128,.103],[-.047,.096]],.033,dark,0);
          for(const x of [-.018,.018])for(const z of [.109,.130,.150])box([x,-.036,z],[.003,.093,.005],metal);
        }
        box([0,.076,(handStart+handEnd)/2],[heavy?.064:.054,heavy?.063:.055,handEnd-handStart],cloth,1);
        rail(.108,-.026,handEnd-.016);
        for(const x of [-1,1])for(let z=handStart+.025;z<handEnd-.012;z+=.029)box([x*(heavy?.033:.028),.074,z],[.002,.012,.018],rubber,2);
        tube([0,.080,handEnd-.018],[0,.080,front-.022],heavy?.017:support?.013:.010,dark);
        tube([0,.080,front-.027],[0,.080,front],heavy?.022:.015,metal);
        tube([0,.080,front+.0002],[0,.080,front+.001],.007,rubber,2);
        box([0,.125,-.016],[.024,.030,.022]);box([0,.132,handEnd-.025],[.016,.043,.016]);
        box([.024,.074,.057],[.003,.020,.053],rubber,2);box([.029,.061,.083],[.013,.010,.019],metal);
        for(const x of [-.025,.025])for(const z of [.015,.135])tube([x,.041,z],[x*1.08,.041,z],.005,metal);
        if(marksman){
          tube([0,.167,.005],[0,.167,.204],.023,dark);
          tube([0,.167,.182],[0,.167,.227],.030,dark);
          tube([0,.167,.227],[0,.167,.228],.023,color(0x243f46));
          for(const z of [.036,.146])box([0,.136,z],[.035,.031,.022],metal);
          tube([0,.178,.096],[0,.200,.096],.012,dark);
        }
        if(support){
          box([.032,-.017,.113],[heavy?.133:.105,heavy?.139:.117,.122],cloth,1);
          box([.032,heavy?.057:.046,.113],[heavy?.138:.110,.012,.126],dark);
          for(const x of [-.032,.032])tube([x,.048,handEnd-.035],[x*1.5,.029,front-.060],.005,dark);
          guard([[.113,.02],[.167,.036],[.167,.137],[.113,.150]]);
        }
        secondary=grip([.022,heavy?.039:.043,Math.min(.24,handEnd-.04)],[-1,0,0],[0,1,0]);
        muzzle=V(0,.080,front+.002);butt=V(0,.075,-stock);
      } else if(def.kind==='pistol') {
        const rear=-.061,front=rear+L;
        profile([[.042,-.047],[.033,.020],[-.077,.001],[-.073,-.054]],.032,dark,1);
        box([0,-.075,-.027],[.039,.012,.059],rubber,2);
        box([0,.030,.066],[.034,.019,.139],dark,1);
        box([0,.037,-.043],[.039,.014,.046],dark,1);
        for(const x of [-.0168,.0168]){
          box([x,-.019,-.024],[.003,.071,.033],rubber,2);
          for(let y=-.044;y<.013;y+=.012)box([x*1.05,y,-.024],[.002,.003,.029],metal);
          box([x,.024,-.017],[.006,.005,.025],metal);
        }
        guard([[.028,.018],[-.015,.030],[-.015,.071],[.027,.083]]);
        B.tube([V(0,.027,.046),V(0,.008,.046),V(0,.001,.038)],.003,W,dark);
        for(const z of [.089,.111,.133])box([0,.018,z],[.028,.005,.009],metal);
        tube([0,.060,front-.035],[0,.060,front+.001],.010,metal);
        tube([0,.060,front+.001],[0,.060,front+.0015],.006,rubber,2);
        // Only the visual slide moves; grip frames stay attached to the frame.
        slideBuilder=new R.GearGeometry(rig,false);
        const sb=(p,s,c=metal,g=0)=>slideBuilder.box(V(...p),V(...s),W,c,g,.22);
        sb([0,.060,(front+rear)/2],[.036,.039,L],metal);
        sb([0,.081,rear+.023],[.029,.006,.016],dark);
        sb([0,.085,rear+.023],[.011,.003,.017],rubber,2);
        sb([0,.083,front-.014],[.007,.007,.012],dark);
        sb([0,.087,front-.014],[.003,.002,.005],color(0xc4c9ab));
        sb([.0185,.065,.026],[.002,.014,.029],rubber,2);
        for(const x of [-.0185,.0185])for(let z=rear+.007;z<rear+.044;z+=.008)sb([x,.058,z],[.002,.026,.003],dark);
        primary=grip([-.018,-.006,-.010],[0,-1,-.16],[1,0,0]);
        secondary=grip([.023,-.009,.015],[0,-1,-.16],[-1,0,0]);muzzle=V(0,.060,front+.002);
      } else if(def.kind==='knife') {
        box([0,0,0],[.027,.095,.027],rubber,2);box([0,.052,0],[.085,.012,.036],metal);
        // Stylised solid blade; no fabrication/functional weapon parameters.
        const a=B.ring(V(0,.058,0),V(1,0,0),V(0,0,1),.018,.0035,8,W,metal);
        const b=B.ring(V(0,L-.06,0),V(1,0,0),V(0,0,1),.012,.003,8,W,metal);
        B.bridge(a,b);const tip=B.vertex(V(0,L-.035,0),W,metal,V(0,1,0));for(let j=0;j<b.length;j++)B.triangle(b[j],tip,b[(j+1)%b.length]);
        primary=grip([-.022,0,0],[0,-1,0],[1,0,0]);
      } else {
        B.ellipsoid(V(),V(.030,.045,.030),W,cloth,1,20,14);
        box([0,.047,0],[.024,.019,.023],metal);box([.025,.012,0],[.009,.074,.017],metal);
        primary=grip([-.022,0,0],[0,-1,0],[1,0,0]);
      }
      const surface=B.finish();
      // Scale every component and contact together, independent of soldier height.
      const scaleGeometry=g=>{const p=g.attributes.position;for(let i=0;i<p.array.length;i++)p.array[i]*=scale;g.computeBoundingBox();g.computeBoundingSphere();};
      scaleGeometry(surface.geometry);
      for(const p of [primary.position,secondary?.position,muzzle,butt])if(p)p.multiplyScalar(scale);
      let slideSurface=null;
      if(slideBuilder){slideSurface=slideBuilder.finish();scaleGeometry(slideSurface.geometry);}
      const bounds={min:surface.geometry.boundingBox.min.clone(),max:surface.geometry.boundingBox.max.clone()};
      if(slideSurface)for(const axis of ['x','y','z']){
        bounds.min[axis]=Math.min(bounds.min[axis],slideSurface.geometry.boundingBox.min[axis]);
        bounds.max[axis]=Math.max(bounds.max[axis],slideSurface.geometry.boundingBox.max[axis]);
      }
      // Clearance probes also cover the rearward travel of the visual slide.
      if(slideSurface)bounds.min.z=Math.min(bounds.min.z,slideSurface.geometry.boundingBox.min.z-.026*scale);
      return {key:cacheKey,geometries:[surface.geometry,...(slideSurface?[slideSurface.geometry]:[])],triangles:surface.triangles+(slideSurface?.triangles||0),bounds,
        primary,secondary,butt,muzzle};
      });
      const bounds={min:resources.bounds.min.clone(),max:resources.bounds.max.clone()},points=[];
      for(const x of [bounds.min.x,bounds.max.x])for(const y of [bounds.min.y,bounds.max.y])for(const z of [bounds.min.z,bounds.max.z])points.push(V(x,y,z));
      const materials=[
        new THREE.MeshStandardMaterial({vertexColors:true,roughness:.52,metalness:.40}),
        new THREE.MeshStandardMaterial({vertexColors:true,roughness:.86,metalness:.02}),
        new THREE.MeshStandardMaterial({vertexColors:true,roughness:.97,metalness:0})
      ];
      const primary={position:resources.primary.position.clone(),quaternion:resources.primary.quaternion.clone()};
      const secondary=resources.secondary&&{position:resources.secondary.position.clone(),quaternion:resources.secondary.quaternion.clone()};
      const butt=resources.butt?.clone(),muzzle=resources.muzzle?.clone();
      const mesh=new THREE.Mesh(resources.geometries[0],materials);mesh.castShadow=true;mesh.receiveShadow=true;root.add(mesh);
      const slide=resources.geometries[1]?new THREE.Mesh(resources.geometries[1],materials):null;
      if(slide){slide.castShadow=true;slide.receiveShadow=true;root.add(slide);}
      let flash=null;
      if(muzzle) {
        const F=new R.GearGeometry({anatomy:{height:1},index:{root:0}},false);
        F.ellipsoid(V(0,0,.032),V(.028,.028,.058),W,[1,.65,.14],0,8,4);
        const fg=F.finish().geometry,fm=new THREE.MeshBasicMaterial({vertexColors:true,transparent:true,opacity:0,depthWrite:false});
        flash=new THREE.Mesh(fg,[fm,fm,fm]);flash.position.copy(muzzle);flash.visible=false;root.add(flash);
      }
      return {item,def,root,mesh,slide,primary,secondary,butt,muzzle,flash,points,triangles:resources.triangles,
        updateVisual(age){if(slide){const z=age>=0&&age<.14?-Math.sin(Math.PI*age/.14)*.026*scale:0;if(slide.position.z!==z)slide.position.z=z;}},
        dispose(){if(root.parent)root.parent.remove(root);releaseGeometry(resources);materials.forEach(m=>m.dispose());if(flash){flash.geometry.dispose();flash.material[0].dispose();}}};
    }
  };
})();
