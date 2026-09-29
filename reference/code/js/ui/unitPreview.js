(function () {
  'use strict';
  const R=window.RTS;
  const materialList=value=>Array.isArray(value)?value:[value];
  const cloneMaterials=value=>Array.isArray(value)?value.map(material=>material.clone()):value.clone();
  const disposeMaterials=value=>materialList(value).forEach(material=>material.dispose());
  const validationScans=new WeakMap();
  function validateModel(model){
    const g=model.mesh.geometry,pos=g.attributes.position,w=g.attributes.skinWeight,idx=g.attributes.skinIndex,n=model.rig.bones.length;
    const index=g.index;
    let cache=validationScans.get(g);
    if(cache&&(cache.posArray!==pos.array||cache.weightArray!==w.array||cache.indexArray!==idx.array||cache.triangleArray!==index.array||
      cache.posVersion!==pos.version||cache.weightVersion!==w.version||cache.indexVersion!==idx.version||cache.triangleVersion!==index.version))cache=null;
    let byBoneCount=cache&&cache.byBoneCount,scan=byBoneCount&&byBoneCount.get(n);
    if(!scan){
      let maxWeightError=0,invalid=0;
      for(let i=0;i<pos.count;i++){
        let sum=0;for(let k=0;k<4;k++){
          const weight=w.array[i*4+k],index=idx.array[i*4+k];sum+=weight;
          if(!Number.isFinite(weight)||weight<0||index<0||index>=n)invalid++;
        }
        for(let k=0;k<3;k++)if(!Number.isFinite(pos.array[i*3+k]))invalid++;
        maxWeightError=Math.max(maxWeightError,Math.abs(1-sum));
      }
      scan={vertices:pos.count,triangles:index.count/3,bones:n,meshes:1,materialGroups:g.groups.length,maxWeightError,invalid};
      if(!cache){cache={posArray:pos.array,weightArray:w.array,indexArray:idx.array,triangleArray:index.array,
        posVersion:pos.version,weightVersion:w.version,indexVersion:idx.version,triangleVersion:index.version,byBoneCount:new Map()};validationScans.set(g,cache);byBoneCount=cache.byBoneCount;}
      byBoneCount.set(n,scan);
    }
    const F=model.anatomy.faceLayout,meta=model.surface.faceMetadata;
    const anatomyOK=!!meta&&meta.neckConnected&&meta.mouthOpening&&!!meta.eyes.L&&!!meta.eyes.R&&F.levels.every(p=>p.every(Number.isFinite));
    return {...scan,anatomyOK,adjustments:F.adjustments.length,ok:scan.invalid===0&&scan.maxWeightError<1e-5&&anatomyOK,
      referenceMinY:model.referenceMinY,measuredStandingHeight:model.standingHeight,heightError:Math.abs(model.standingHeight-model.anatomy.height)};
  }
  R.UnitPreview=class UnitPreview {
    constructor(container,sides){
      this.container=container;this.sides=sides;this.unit=null;this._generateToken=0;this.visible=false;this.paused=false;this.clock=new R.FixedClock();this.lastReadout=0;
      this.scene=new THREE.Scene();this.scene.background=new THREE.Color(0x253137);
      this.camera=new THREE.PerspectiveCamera(34,1,.02,70);
      this.renderer=new THREE.WebGLRenderer({antialias:true,powerPreference:'high-performance'});
      this._pixelRatio=0;this.renderer.outputEncoding=THREE.sRGBEncoding;
      this.renderer.toneMapping=THREE.ACESFilmicToneMapping;this.renderer.toneMappingExposure=1;
      this.renderer.shadowMap.enabled=true;this.renderer.shadowMap.type=THREE.PCFSoftShadowMap;
      this.container.appendChild(this.renderer.domElement);
      this.controls=new THREE.OrbitControls(this.camera,this.renderer.domElement);this.controls.enableDamping=true;this.controls.dampingFactor=.08;
      this.controls.minDistance=.32;this.controls.maxDistance=9;this.controls.maxPolarAngle=Math.PI*.495;
      this._removeQualityListener=R.RenderQuality.onChange(()=>this.resize());
      this.scene.add(new THREE.HemisphereLight(0xd9e9ef,0x6d6357,.65));
      const key=new THREE.DirectionalLight(0xfff2da,1.85);key.position.set(-3,6,5);key.castShadow=true;
      key.shadow.mapSize.set(2048,2048);Object.assign(key.shadow.camera,{left:-2.3,right:2.3,top:2.6,bottom:-2.3,near:.1,far:15});
      key.shadow.normalBias=.007;key.shadow.bias=-.00015;this.scene.add(key);
      const fill=new THREE.DirectionalLight(0xb9d7e9,.35);fill.position.set(4,3,-3);this.scene.add(fill);
      this.floor=new THREE.Mesh(new THREE.PlaneGeometry(80,80),new THREE.MeshStandardMaterial({color:0x566366,roughness:.95}));
      this.floor.rotation.x=-Math.PI/2;this.floor.receiveShadow=true;this.scene.add(this.floor);
      this.bounds=new THREE.Box3();this.size=new THREE.Vector3();this.centre=new THREE.Vector3();
      this.boundsHelper=new THREE.Box3Helper(this.bounds,0xb5bfc6);this.boundsHelper.visible=false;this.scene.add(this.boundsHelper);
      this.settings={surface:true,wire:false,bones:false,colliders:false,bounds:false};
      this.skeletonHelper=null;this.colliders=null;this.dirty=true;
      this.onControlsChange=()=>{this.dirty=true;};this.controls.addEventListener('change',this.onControlsChange);
      this.resizeObserver=new ResizeObserver(()=>this.resize());this.resizeObserver.observe(container);
      this.destructionButton=R.DestructionDemo.button(container.parentElement,()=>this.unit?{renderer:this.renderer,controls:this.controls,target:R.DestructionAdapters.infantry(this.unit),resume:()=>{this.clock.reset();this.dirty=true;this.resize();}}:null);
    }
    generate(sideId,seed,state,expression='NEUTRAL',expressionIntensity=1,genomeOptions={},equipmentOptions={}){
      this._generateToken++;
      const previous=this.unit;
      const saved=previous?{weapons:previous.weapons.exportState(),state:previous.animator.state,phase:previous.animator.phase,time:previous.animator.time,
        crouch:previous.locomotion.requestedCrouch,speedMps:previous.locomotion.requestedSpeedMps,custom:previous.locomotion.custom}:null;
      this.disposeUnit();
      this.baseGenome=R.InfantryGenome.create(seed);
      const varied=R.InfantryGenome.applyVariation(this.baseGenome,genomeOptions.variation===undefined?1:genomeOptions.variation);
      const genome=R.InfantryGenome.withOverrides(varied,genomeOptions.overrides||{});
      this.unit=new R.InfantryUnit({id:'PREVIEW',side:this.sides[sideId],seed,detail:'high',state,expression,expressionIntensity,genome,equipmentOptions});
      // Podgląd nie wchodzi do list jednostek którejkolwiek strony ani świata.
      this.unit.position.set(0,0,0);this.scene.add(this.unit.root);
      this.previewMaterials=cloneMaterials(this.unit.model.mesh.material);this.unit.model.mesh.material=this.previewMaterials;
      this.gearMaterials=cloneMaterials(this.unit.model.gearMesh.material);this.unit.model.gearMesh.material=this.gearMaterials;
      if(saved && saved.state===state){
        if(R.PostureProfile.isBiped(state)){this.unit.setLocomotion({crouch:saved.crouch,speedMps:saved.speedMps});this.unit.locomotion.custom=saved.custom;}
        this.unit.animator.time=saved.time;this.unit.seek(saved.phase);
      }
      if(saved){this.unit.weapons.restoreState(saved.weapons);this.unit.seek(this.unit.animator.phase);}
      this.validation=validateModel(this.unit.model);this.unit.render(1);this.clock.reset();this.updateSettings();this.dirty=true;
    }
    async generateAsync(sideId,seed,state,expression='NEUTRAL',expressionIntensity=1,genomeOptions={},equipmentOptions={}){
      const token=++this._generateToken,previous=this.unit;
      const saved=previous?{weapons:previous.weapons.exportState(),state:previous.animator.state,phase:previous.animator.phase,time:previous.animator.time,
        crouch:previous.locomotion.requestedCrouch,speedMps:previous.locomotion.requestedSpeedMps,custom:previous.locomotion.custom}:null;
      const baseGenome=R.InfantryGenome.create(seed),varied=R.InfantryGenome.applyVariation(baseGenome,genomeOptions.variation===undefined?1:genomeOptions.variation),genome=R.InfantryGenome.withOverrides(varied,genomeOptions.overrides||{});
      const check=()=>{if(token!==this._generateToken){const error=new Error('Budowa podglądu została zastąpiona nowszym żądaniem.');error.name='AbortError';throw error;}};
      const yieldFrame=()=>new Promise((resolve,reject)=>requestAnimationFrame(()=>{try{check();resolve();}catch(error){reject(error);}}));
      let candidate=null,previewMaterials=null,gearMaterials=null;
      try{
        candidate=await R.InfantryUnit.createAsync({id:'PREVIEW',side:this.sides[sideId],seed,detail:'high',state,expression,expressionIntensity,genome,equipmentOptions},yieldFrame,check,4);
        check();candidate.position.set(0,0,0);
        if(saved&&saved.state===state){
          if(R.PostureProfile.isBiped(state)){candidate.setLocomotion({crouch:saved.crouch,speedMps:saved.speedMps});candidate.locomotion.custom=saved.custom;}
          candidate.animator.time=saved.time;candidate.seek(saved.phase);
        }
        if(saved){candidate.weapons.restoreState(saved.weapons);candidate.seek(candidate.animator.phase);}
        previewMaterials=cloneMaterials(candidate.model.mesh.material);candidate.model.mesh.material=previewMaterials;
        gearMaterials=candidate.model.gearMesh?cloneMaterials(candidate.model.gearMesh.material):null;
        if(candidate.model.gearMesh)candidate.model.gearMesh.material=gearMaterials;
        const validation=validateModel(candidate.model);candidate.render(1);check();

        // Commit only after the replacement has compiled and passed validation.
        if(this.skeletonHelper){this.scene.remove(this.skeletonHelper);this.skeletonHelper.geometry.dispose();this.skeletonHelper.material.dispose();this.skeletonHelper=null;}
        if(this.colliders){this.scene.remove(this.colliders.root);this.colliders.dispose();this.colliders=null;}
        const oldPreviewMaterials=this.previewMaterials,oldGearMaterials=this.gearMaterials;
        this.unit=candidate;this.previewMaterials=previewMaterials;this.gearMaterials=gearMaterials;this.validation=validation;
        this.scene.add(candidate.root);
        if(oldPreviewMaterials)disposeMaterials(oldPreviewMaterials);
        if(oldGearMaterials)disposeMaterials(oldGearMaterials);
        if(previous)previous.dispose();
        this.clock.reset();this.updateSettings();this.dirty=true;
        return candidate;
      }catch(error){if(candidate&&candidate!==this.unit){if(previewMaterials)disposeMaterials(previewMaterials);if(gearMaterials)disposeMaterials(gearMaterials);candidate.dispose();}throw error;}
    }
    disposeUnit(){
      this._generateToken++;
      if(this.skeletonHelper){this.scene.remove(this.skeletonHelper);this.skeletonHelper.geometry.dispose();this.skeletonHelper.material.dispose();this.skeletonHelper=null;}
      if(this.colliders){this.scene.remove(this.colliders.root);this.colliders.dispose();this.colliders=null;}
      if(this.gearMaterials){disposeMaterials(this.gearMaterials);this.gearMaterials=null;}
      if(this.previewMaterials){disposeMaterials(this.previewMaterials);this.previewMaterials=null;}
      if(this.unit){this.unit.dispose();this.unit=null;}
    }
    setView(name='threequarter'){
      if(!this.unit)return;const h=this.unit.phenotype.height,prone=this.unit.animator.state.startsWith('PRONE');
      const y=prone?.23*h:.54*h,z=prone?.025*h:0;
      this.controls.target.set(0,y,z);
      if(name==='face'){
        const p=this.unit.rig.byName.head.getWorldPosition(this.centre);this.controls.target.set(p.x,p.y+.023*h,p.z+.016*h);
        this.camera.position.set(p.x+.10*h,p.y+.04*h,p.z+.65*h);
      }else {
        const v={front:[0,.68,2.65],side:[2.65,.66,0],back:[0,.68,-2.65],threequarter:[1.65,.92,2.22]}[name]||[1.65,.92,2.22];
        this.camera.position.set(v[0]*h,v[1]*h,v[2]*h);if(prone)this.camera.position.y=.85*h;
      }
      this.controls.update();this.dirty=true;
    }
    setEquipment(equipment){
      if(!this.unit)return;
      this.unit.setEquipment(equipment);
      this.validation=validateModel(this.unit.model);
      this.unit.weapons.apply(this.unit._animContext,0);this.unit.syncSnapshots();
      this.unit.render(1);this.updateSettings();this.dirty=true;
    }
    setState(s){if(this.unit){this.unit.setState(s);this.clock.reset();if(this.paused)this.unit.seek(this.unit.animator.phase);this.dirty=true;}}
    setLocomotion(values){
      if(!this.unit)return;
      this.unit.setLocomotion(values);
      if(this.paused){this.unit.seek(this.unit.animator.phase);this.unit.render(1);}
      this.dirty=true;
    }
    setExpression(name,intensity=1){if(this.unit){this.unit.setExpression(name,intensity);this.clock.reset();if(this.paused){this.unit.faceAnimator.update(.22);this.unit.capture();this.unit.render(1);}this.dirty=true;}}
    setPaused(v){if(v&&this.unit)this.unit.weapons.setTrigger(false);this.paused=!!v;this.clock.reset();if(this.unit)this.unit.render(1);this.dirty=true;}
    setVisible(v){if(!v&&this.unit)this.unit.weapons.setTrigger(false);this.visible=v;this.clock.reset();if(v){this.resize();this.dirty=true;}}
    seek(v){if(this.unit){this.setPaused(true);this.unit.seek(v);this.unit.render(1);this.dirty=true;}}
    updateSettings(){
      if(!this.unit)return;const s=this.settings;this.unit.model.mesh.visible=s.surface;this.unit.model.gearMesh.visible=s.surface;
      materialList(this.previewMaterials).forEach(m=>{m.wireframe=s.wire;});materialList(this.gearMaterials).forEach(m=>{m.wireframe=s.wire;});
      for(const item of Object.values(this.unit.weapons.instances)){item.root.visible=s.surface;item.mesh.material.forEach(m=>{m.wireframe=s.wire;});}
      if(s.bones&&!this.skeletonHelper){this.skeletonHelper=new THREE.SkeletonHelper(this.unit.root);this.skeletonHelper.material.depthTest=false;this.skeletonHelper.material.depthWrite=false;this.skeletonHelper.renderOrder=25;this.scene.add(this.skeletonHelper);}
      if(this.skeletonHelper)this.skeletonHelper.visible=s.bones;
      if(s.colliders&&!this.colliders){this.colliders=new R.ColliderPreview(this.unit.rig,this.unit.physicsSchema);this.scene.add(this.colliders.root);}
      if(this.colliders)this.colliders.root.visible=s.colliders;
      this.boundsHelper.visible=s.bounds;this.dirty=true;
    }
    resize(){const r=this.container.getBoundingClientRect();if(r.width<1||r.height<1)return;const ratio=R.RenderQuality.ratio(r.width,r.height,window.devicePixelRatio||1,'preview');if(Math.abs(ratio-this._pixelRatio)>1e-4){this._pixelRatio=ratio;this.renderer.setPixelRatio(ratio);}this.camera.aspect=r.width/r.height;this.camera.updateProjectionMatrix();this.renderer.setSize(r.width,r.height,false);this.dirty=true;}
    frame(now){
      if(R.DestructionDemo.frame(this.renderer,now))return;
      if(!this.visible||!this.unit)return;
      const alpha=this.paused?1:this.clock.advance(now,dt=>this.unit.step(dt,R.FlatSurface,true));
      this.unit.render(alpha);
      if(this.settings.colliders&&this.colliders)this.colliders.update();
      if(now-this.lastReadout>180){
        this.lastReadout=now;
        if(this.onReadout)this.onReadout(this);
      }
      this.controls.update();
      if(!this.paused||this.dirty){this.renderer.render(this.scene,this.camera);this.dirty=false;}
    }
    dispose(){if(R.DestructionDemo.owns(this.renderer))R.DestructionDemo.close();this.destructionButton?.remove();this.disposeUnit();if(this._removeQualityListener)this._removeQualityListener();this.resizeObserver.disconnect();this.controls.removeEventListener('change',this.onControlsChange);this.controls.dispose();this.floor.geometry.dispose();this.floor.material.dispose();this.boundsHelper.geometry.dispose();this.boundsHelper.material.dispose();this.renderer.dispose();this.renderer.domElement.remove();}
  };
})();
