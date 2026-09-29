(function () {
  'use strict';
  const R=window.RTS;
  const surfaceOwners=new Map();
  function retainSurface(geometry){surfaceOwners.set(geometry,(surfaceOwners.get(geometry)||0)+1);}
  function releaseSurface(geometry){const refs=surfaceOwners.get(geometry)||0;if(refs<=1){surfaceOwners.delete(geometry);return 0;}surfaceOwners.set(geometry,refs-1);return refs-1;}
  function retireAppearance(key,resources,surfaceGeometry,detail){
    const last=R.AppearanceCache.release(key,resources),remaining=releaseSurface(surfaceGeometry);
    if(!last)return;
    if(remaining>0){if(resources.gear)resources.gear.geometry.dispose();return;}
    R.AppearanceCache.put(key,resources,detail);
  }
  function skinWithPalette(mesh,index,out,palette,base,point,includeMorphs,activeMorphIndices=null,activeMorphWeights=null){
    const g=mesh.geometry,positions=g.attributes.position.array,ids=g.attributes.skinIndex.array,weights=g.attributes.skinWeight.array;
    base.fromArray(positions,index*3);
    if(includeMorphs){
      const targets=g.morphAttributes.position||[];
      if(activeMorphIndices){for(let i=0;i<activeMorphIndices.length;i++){const m=activeMorphIndices[i];base.addScaledVector(point.fromArray(targets[m].array,index*3),activeMorphWeights[i]);}}
      else {const values=mesh.morphTargetInfluences||[];for(let m=0;m<targets.length;m++)if(values[m])base.addScaledVector(point.fromArray(targets[m].array,index*3),values[m]);}
    }
    base.applyMatrix4(mesh.bindMatrix);out.set(0,0,0);
    for(let k=0;k<4;k++){
      const weight=weights[index*4+k];if(weight<=0)continue;
      const offset=ids[index*4+k]*12,x=base.x,y=base.y,z=base.z;
      point.set(x*palette[offset]+y*palette[offset+3]+z*palette[offset+6]+palette[offset+9],
        x*palette[offset+1]+y*palette[offset+4]+z*palette[offset+7]+palette[offset+10],
        x*palette[offset+2]+y*palette[offset+5]+z*palette[offset+8]+palette[offset+11]);
      out.addScaledVector(point,weight);
    }
    return out.applyMatrix4(mesh.bindMatrixInverse);
  }
  function animatedBounds(mesh,height){
    const geometry=mesh.geometry;if(!geometry.boundingBox)geometry.computeBoundingBox();
    const box=geometry.boundingBox,center=new THREE.Vector3(0,height*.5,0);let radiusSq=0;
    for(let mask=0;mask<8;mask++){
      const x=mask&1?box.max.x:box.min.x,y=mask&2?box.max.y:box.min.y,z=mask&4?box.max.z:box.min.z;
      radiusSq=Math.max(radiusSq,(x-center.x)*(x-center.x)+(y-center.y)*(y-center.y)+(z-center.z)*(z-center.z));
    }
    // Covers articulated limbs, prone reach and the fitted gear envelope around bind-space bounds.
    geometry.boundingSphere=new THREE.Sphere(center,Math.sqrt(radiusSq)+height*.75);
    mesh.frustumCulled=true;
  }
  function installMaterialRegions(geometry){
    if(geometry.attributes.materialRegion)return true;
    if(!geometry.index||!geometry.attributes.position)return false;
    const indices=geometry.index.array,values=new Uint8Array(geometry.attributes.position.count);values.fill(255);
    for(const group of geometry.groups){
      const region=group.materialIndex;
      if(region<0||region>254)return false;
      const end=group.start+group.count;
      for(let i=group.start;i<end;i++){
        const vertex=indices[i],previous=values[vertex];
        if(previous!==255&&previous!==region)return false;
        values[vertex]=region;
      }
    }
    for(let i=0;i<values.length;i++)if(values[i]===255)values[i]=0;
    geometry.setAttribute('materialRegion',new THREE.BufferAttribute(values,1));
    return true;
  }
  function appearance(rig,phenotype,side,equipment,detail,key,previous=null){
    const fit=new R.EquipmentFit(rig.anatomy,equipment),surfaceKey=R.AppearanceCache.surfaceKey(phenotype,side,detail,equipment,fit.surfaceSignature());
    const cached=R.AppearanceCache.take(key);
    if(cached){
      // The immutable geometry may belong to another live unit. Keep all
      // per-rig metadata on shallow instance wrappers, never on shared assets.
      const surface={...cached.surface,clothingFit:fit};
      const gear=cached.gear?{...cached.gear,fit}:null;
      return {surface,gear,surfaceKey};
    }
    const start=performance.now();let surface=null,gear=null,surfaceOwned=false;
    try{
      if(previous&&previous.surfaceKey===surfaceKey)surface={...previous.surface,clothingFit:fit};
      else {surface=R.InfantrySurface.create(rig,side,detail,equipment,fit);surfaceOwned=true;}
      gear=equipment?R.InfantryGear.build(rig,side,equipment,detail,fit):null;
    }
    catch(error){if(surfaceOwned&&surface)surface.geometry.dispose();if(gear)gear.geometry.dispose();throw error;}
    R.AppearanceCache.recordBuild(performance.now()-start);return {surface,gear,surfaceKey};
  }
  async function appearanceAsync(rig,phenotype,side,equipment,detail,key,previous,yieldFrame,check,budgetMs){
    const fit=new R.EquipmentFit(rig.anatomy,equipment),surfaceKey=R.AppearanceCache.surfaceKey(phenotype,side,detail,equipment,fit.surfaceSignature()),cached=R.AppearanceCache.take(key);
    if(cached)return {surface:{...cached.surface,clothingFit:fit},gear:cached.gear?{...cached.gear,fit}:null,surfaceKey,fromCache:true};
    const start=performance.now();let surface=null,gear=null,surfaceOwned=false;
    try{
      if(previous&&previous.surfaceKey===surfaceKey)surface={...previous.surface,clothingFit:fit};
      else {surface=await R.InfantrySurface.createAsync(rig,side,detail,equipment,fit,yieldFrame,check,budgetMs);surfaceOwned=true;}
      if(check)check();
      gear=equipment?await R.InfantryGear.buildAsync(rig,side,equipment,detail,fit,yieldFrame,check,budgetMs):null;
    }catch(error){if(surfaceOwned&&surface)surface.geometry.dispose();if(gear)gear.geometry.dispose();throw error;}
    R.AppearanceCache.recordBuild(performance.now()-start);return {surface,gear,surfaceKey,fromCache:false};
  }
  function commitAppearance(model,detail,equipment,key,built){
    const {surface,gear}=built,oldKey=model.appearanceKey,oldResources=model.appearanceResources,oldDetail=model.detail,oldSurfaceGeometry=model.surface.geometry;
    const oldInfluences=model.mesh.morphTargetInfluences||[];
    model.mesh.geometry=surface.geometry;model.mesh.material=installMaterialRegions(surface.geometry)?R.InfantryMaterials.getRegionAware():R.InfantryMaterials.get();animatedBounds(model.mesh,model.anatomy.height);model.mesh.updateMorphTargets();
    for(let i=0;i<oldInfluences.length&&i<model.mesh.morphTargetInfluences.length;i++)model.mesh.morphTargetInfluences[i]=oldInfluences[i];
    if(gear){
      if(model.gearMesh){model.gearMesh.geometry=gear.geometry;model.gearMesh.material=installMaterialRegions(gear.geometry)?R.InfantryGear.materials(true):R.InfantryGear.materials();animatedBounds(model.gearMesh,model.anatomy.height);}
      else {
        model.gearMesh=new THREE.SkinnedMesh(gear.geometry,installMaterialRegions(gear.geometry)?R.InfantryGear.materials(true):R.InfantryGear.materials());
        Object.assign(model.gearMesh,{name:'InfantryEquipment',castShadow:true,receiveShadow:true});animatedBounds(model.gearMesh,model.anatomy.height);
        model.root.add(model.gearMesh);model.gearMesh.bind(model.rig.skeleton,model.mesh.bindMatrix);
      }
    } else if(model.gearMesh){model.root.remove(model.gearMesh);model.gearMesh=null;}
    model.surface=surface;model.detail=detail;model.gear=gear;model.equipment=equipment;model.surfaceKey=built.surfaceKey;
    model.referenceMinY=surface.geometry.boundingBox.min.y;
    model.standingHeight=surface.geometry.boundingBox.max.y-model.referenceMinY;
    model.appearanceKey=key;model.appearanceResources=built;delete model.appearanceResources.fromCache;
    R.AppearanceCache.acquire(key,built);retainSurface(surface.geometry);
    retireAppearance(oldKey,oldResources,oldSurfaceGeometry,oldDetail);return true;
  }
  R.InfantryFactory={
    async createModelAsync(side,phenotype,detail='high',equipment=null,yieldFrame,check=null,budgetMs=4){
      if(typeof yieldFrame!=='function')throw new TypeError('Async infantry model generation requires a frame-yield callback.');
      const root=new THREE.Group();root.name='unitWorldRoot';
      const anatomy=R.InfantryAnatomy.create(phenotype.height,phenotype.face,phenotype.body),rig=new R.InfantryRig(anatomy,root);
      const appearanceKey=R.AppearanceCache.key(phenotype,side,equipment,detail);let built=null;
      try{
        const guard=()=>{if(check)check();};guard();
        built=await appearanceAsync(rig,phenotype,side,equipment,detail,appearanceKey,null,yieldFrame,guard,budgetMs);guard();
        return this.createModel(side,phenotype,detail,equipment,{root,anatomy,rig,appearanceKey,built});
      }catch(error){
        if(built){
          if(built.fromCache)R.AppearanceCache.put(appearanceKey,built,detail);
          else {if(built.surface&&built.surface.geometry)built.surface.geometry.dispose();if(built.gear&&built.gear.geometry)built.gear.geometry.dispose();}
        }
        rig.dispose();if(root.parent)root.parent.remove(root);throw error;
      }
    },
    createModel(side,phenotype,detail='high',equipment=null,prepared=null) {
      const root=prepared?prepared.root:new THREE.Group();root.name='unitWorldRoot';
      const anatomy=prepared?prepared.anatomy:R.InfantryAnatomy.create(phenotype.height,phenotype.face,phenotype.body),rig=prepared?prepared.rig:new R.InfantryRig(anatomy,root);
      const appearanceKey=prepared?prepared.appearanceKey:R.AppearanceCache.key(phenotype,side,equipment,detail),built=prepared?prepared.built:appearance(rig,phenotype,side,equipment,detail,appearanceKey),surface=built.surface;
      delete built.fromCache;
      const mesh=new THREE.SkinnedMesh(surface.geometry,installMaterialRegions(surface.geometry)?R.InfantryMaterials.getRegionAware():R.InfantryMaterials.get());
      mesh.updateMorphTargets();
      mesh.name='InfantrySurface';mesh.castShadow=true;mesh.receiveShadow=true;animatedBounds(mesh,anatomy.height);
      root.add(mesh);root.updateMatrixWorld(true);mesh.bind(rig.skeleton,new THREE.Matrix4());
      let gear=built.gear,gearMesh=null;
      if(equipment){
        gearMesh=new THREE.SkinnedMesh(gear.geometry,installMaterialRegions(gear.geometry)?R.InfantryGear.materials(true):R.InfantryGear.materials());
        gearMesh.name='InfantryEquipment';gearMesh.castShadow=true;gearMesh.receiveShadow=true;animatedBounds(gearMesh,anatomy.height);
        root.add(gearMesh);gearMesh.bind(rig.skeleton,new THREE.Matrix4());
      }
      const base=new THREE.Vector3(),point=new THREE.Vector3(),matrix=new THREE.Matrix4(),skinPalette=new Float64Array(rig.bones.length*12);
      const supportSkinCache={body:new Map(),gear:new Map()};
      const activeMorphIndices=[],activeMorphWeights=[];
      let paletteNodes=rig.bones.slice(),paletteAncestors=[],paletteModes=null,paletteState=null,skipUnchangedTransformScan=false;
      const paletteDirtyNodes=[],paletteDirtyRoots=[];let paletteNodeStamp=new WeakMap(),paletteChangeRevision=0,paletteNeedsFullUpdate=true;
      let skinSampleRevision=0,sampledSurfaceGeometry=mesh.geometry,sampledGearGeometry=gearMesh&&gearMesh.geometry;
      let morphSnapshot=new Float64Array((mesh.morphTargetInfluences||[]).length),morphSnapshotReady=false;
      function paletteTransformsChanged(){
        // The pose writer opts into this fast path only after it has compared
        // every rig transform with the palette snapshot and found no changes.
        // Keep the exact scan for all external/direct transform mutations.
        if(skipUnchangedTransformScan){skipUnchangedTransformScan=false;return false;}
        let offset=0,changed=paletteState===null;paletteDirtyNodes.length=0;paletteDirtyRoots.length=0;paletteNeedsFullUpdate=paletteState===null;
        let ancestorIndex=0,chainChanged=false;
        for(let node=root;node;node=node.parent){if(paletteAncestors[ancestorIndex]!==node)chainChanged=true;paletteAncestors[ancestorIndex++]=node;}
        if(paletteAncestors.length!==ancestorIndex)chainChanged=true;
        paletteAncestors.length=ancestorIndex;
        if(chainChanged){paletteNodes=rig.bones.slice();for(let i=0;i<paletteAncestors.length;i++)paletteNodes.push(paletteAncestors[i]);paletteState=null;paletteModes=null;changed=true;paletteNeedsFullUpdate=true;}
        if(!paletteModes||paletteModes.length!==paletteNodes.length){paletteModes=new Uint8Array(paletteNodes.length);paletteModes.fill(255);}
        let modeChanged=false;
        for(let i=0;i<paletteNodes.length;i++){
          const mode=paletteNodes[i].matrixAutoUpdate===false?1:0;
          if(paletteModes[i]!==mode){paletteModes[i]=mode;modeChanged=true;}
        }
        if(modeChanged){paletteState=null;changed=true;paletteNeedsFullUpdate=true;}
        if(!paletteState){let size=0;for(const node of paletteNodes)size+=node.matrixAutoUpdate===false?16:10;paletteState=new Float64Array(size);}
        for(const node of paletteNodes){
          let nodeChanged=false;
          if(node.matrixAutoUpdate===false){
            const elements=node.matrix.elements;
            for(let i=0;i<16;i++,offset++){const value=elements[i];if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset]=value;}
          }else{
            const p=node.position,q=node.quaternion,s=node.scale;
            let value=p.x;if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset++]=value;
            value=p.y;if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset++]=value;
            value=p.z;if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset++]=value;
            value=q.x;if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset++]=value;
            value=q.y;if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset++]=value;
            value=q.z;if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset++]=value;
            value=q.w;if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset++]=value;
            value=s.x;if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset++]=value;
            value=s.y;if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset++]=value;
            value=s.z;if(paletteState[offset]!==value){changed=true;nodeChanged=true;}paletteState[offset++]=value;
          }
          if(nodeChanged)paletteDirtyNodes.push(node);
        }
        if(!paletteNeedsFullUpdate&&paletteDirtyNodes.length){
          paletteChangeRevision=paletteChangeRevision>=Number.MAX_SAFE_INTEGER?1:paletteChangeRevision+1;
          if(paletteChangeRevision===1)paletteNodeStamp=new WeakMap();
          const revision=paletteChangeRevision;
          for(const node of paletteDirtyNodes)paletteNodeStamp.set(node,revision);
          for(const node of paletteDirtyNodes){let parent=node.parent,covered=false;while(parent){if(paletteNodeStamp.get(parent)===revision){covered=true;break;}parent=parent.parent;}if(!covered)paletteDirtyRoots.push(node);}
        }
        return changed;
      }
      function unchangedTransformsMatchSnapshot(){
        if(!paletteState)return false;
        let offset=0,ancestorIndex=0,chainChanged=false;
        for(let node=root;node;node=node.parent){if(paletteAncestors[ancestorIndex]!==node)chainChanged=true;ancestorIndex++;}
        if(ancestorIndex!==paletteAncestors.length)chainChanged=true;
        if(chainChanged)return false;
        for(const node of paletteNodes){
          if(node.matrixAutoUpdate===false){
            const elements=node.matrix.elements;
            for(let i=0;i<16;i++)if(paletteState[offset++]!==elements[i])return false;
          }else{
            const p=node.position,q=node.quaternion,s=node.scale;
            if(paletteState[offset++]!==p.x||paletteState[offset++]!==p.y||paletteState[offset++]!==p.z||
               paletteState[offset++]!==q.x||paletteState[offset++]!==q.y||paletteState[offset++]!==q.z||paletteState[offset++]!==q.w||
               paletteState[offset++]!==s.x||paletteState[offset++]!==s.y||paletteState[offset++]!==s.z)return false;
          }
        }
        return true;
      }
      function confirmUnchangedTransforms(){skipUnchangedTransformScan=unchangedTransformsMatchSnapshot();}
      function refreshSkinSampleRevision(transformsChanged){
        let changed=transformsChanged,morphsChanged=false;
        const influences=mesh.morphTargetInfluences||[],currentGearGeometry=model.gearMesh&&model.gearMesh.geometry;
        if(mesh.geometry!==sampledSurfaceGeometry||currentGearGeometry!==sampledGearGeometry){
          supportSkinCache.body.clear();supportSkinCache.gear.clear();
          sampledSurfaceGeometry=mesh.geometry;sampledGearGeometry=currentGearGeometry;
          morphSnapshot=new Float64Array(influences.length);morphSnapshotReady=false;changed=true;morphsChanged=true;
        }
        if(morphSnapshot.length!==influences.length){morphSnapshot=new Float64Array(influences.length);morphSnapshotReady=false;changed=true;morphsChanged=true;}
        if(!morphSnapshotReady){for(let i=0;i<influences.length;i++)morphSnapshot[i]=influences[i];morphSnapshotReady=true;changed=true;morphsChanged=true;}
        else for(let i=0;i<influences.length;i++)if(morphSnapshot[i]!==influences[i]){morphSnapshot[i]=influences[i];changed=true;morphsChanged=true;}
        if(morphsChanged){
          activeMorphIndices.length=0;activeMorphWeights.length=0;
          for(let i=0;i<influences.length;i++)if(influences[i]!==0){activeMorphIndices.push(i);activeMorphWeights.push(influences[i]);}
        }
        if(changed)skinSampleRevision++;
        return changed;
      }
      function cachedSupportSkin(index,out,includeMorphs,cache){
        let entry=cache.get(index);
        if(entry&&entry.revision===skinSampleRevision)return out.set(entry.x,entry.y,entry.z);
        if(includeMorphs)model.skinVertex(index,out,true);else model.skinGearVertex(index,out);
        if(!entry){entry={revision:-1,x:0,y:0,z:0};cache.set(index,entry);}
        entry.revision=skinSampleRevision;entry.x=out.x;entry.y=out.y;entry.z=out.z;
        return out;
      }
      const model={root,rig,mesh,anatomy,surface,side,equipment,gear,gearMesh,phenotype,appearanceKey,appearanceResources:built,surfaceKey:built.surfaceKey,disposed:false,_appearanceBuildToken:0,
        standingHeight:surface.geometry.boundingBox.max.y-surface.geometry.boundingBox.min.y,
        referenceMinY:surface.geometry.boundingBox.min.y,detail,
        get skinSampleRevision(){return skinSampleRevision;},
        // Contact and bounds scans share a palette while bones/ancestors are
        // unchanged. Exact morph snapshots invalidate support samples without
        // rebuilding bone matrices when the skeleton itself has not moved.
        prepareSkinPalette(){
          skipUnchangedTransformScan=false;
          const transformsChanged=paletteTransformsChanged();refreshSkinSampleRevision(transformsChanged);
          if(!transformsChanged)return skinPalette;
          if(paletteNeedsFullUpdate||!root.updateWorldMatrix)root.updateMatrixWorld(true);
          else for(const node of paletteDirtyRoots)node.updateWorldMatrix(true,true);
          paletteNeedsFullUpdate=false;
          for(let i=0;i<rig.bones.length;i++){
            matrix.multiplyMatrices(rig.bones[i].matrixWorld,rig.skeleton.boneInverses[i]);
            // All bone transforms are affine. Store the 3×4 rows used by
            // skinWithPalette, omitting the constant final row without
            // reducing Float64 precision used by CPU contact checks.
            const e=matrix.elements,offset=i*12;
            skinPalette[offset]=e[0];skinPalette[offset+1]=e[1];skinPalette[offset+2]=e[2];
            skinPalette[offset+3]=e[4];skinPalette[offset+4]=e[5];skinPalette[offset+5]=e[6];
            skinPalette[offset+6]=e[8];skinPalette[offset+7]=e[9];skinPalette[offset+8]=e[10];
            skinPalette[offset+9]=e[12];skinPalette[offset+10]=e[13];skinPalette[offset+11]=e[14];
          }
          return skinPalette;
        },
        confirmUnchangedTransforms,
        // Direct skin reads are uncached. Contact scans use the revisioned
        // methods below after prepareSkinPalette has checked bones and morphs.
        skinVertex(index,out,prepared=false){if(!prepared)refreshSkinSampleRevision(false);return skinWithPalette(mesh,index,out,skinPalette,base,point,true,activeMorphIndices,activeMorphWeights);},
        skinGearVertex(index,out){return model.gearMesh?skinWithPalette(model.gearMesh,index,out,skinPalette,base,point,false):out.set(0,0,0);},
        skinSupportVertex(index,out){return cachedSupportSkin(index,out,true,supportSkinCache.body);},
        skinSupportGearVertex(index,out){return model.gearMesh?cachedSupportSkin(index,out,false,supportSkinCache.gear):out.set(0,0,0);},
        dispose(){if(model.disposed)return;model.disposed=true;model._appearanceBuildToken++;if(root.parent)root.parent.remove(root);retireAppearance(model.appearanceKey,model.appearanceResources,model.surface.geometry,model.detail);rig.dispose();}
      };
      R.AppearanceCache.acquire(appearanceKey,built);retainSurface(surface.geometry);
      return model;
    },
    replaceAppearance(model,detail,equipment) {
      // Build first, commit second. A failed selection cannot destroy the
      // previously displayed model. Keep the rig and all animation state.
      model._appearanceBuildToken++;
      const key=R.AppearanceCache.key(model.phenotype,model.side,equipment,detail);
      if(key===model.appearanceKey){model.equipment=equipment;return false;}
      const built=appearance(model.rig,model.phenotype,model.side,equipment,detail,key,model);
      return commitAppearance(model,detail,equipment,key,built);
    },
    async setDetailAsync(model,detail,yieldFrame,check=null,budgetMs=4){
      if(model.detail===detail)return false;
      const token=++model._appearanceBuildToken,key=R.AppearanceCache.key(model.phenotype,model.side,model.equipment,detail);
      if(key===model.appearanceKey)return false;
      const guard=()=>{if(model.disposed||model._appearanceBuildToken!==token){const error=new Error('Budowa wariantu wyglądu została anulowana.');error.name='AbortError';throw error;}if(check)check();};
      guard();
      const built=await appearanceAsync(model.rig,model.phenotype,model.side,model.equipment,detail,key,model,yieldFrame,guard,budgetMs);
      try{guard();}catch(error){
        if(built.fromCache)R.AppearanceCache.put(key,built,detail);
        else {if(built.surface.geometry!==model.surface.geometry)built.surface.geometry.dispose();if(built.gear&&built.gear.geometry!==(model.gear&&model.gear.geometry))built.gear.geometry.dispose();}
        throw error;
      }
      return commitAppearance(model,detail,model.equipment,key,built);
    },
    setDetail(model,detail) {
      if(model.detail===detail)return false;
      return this.replaceAppearance(model,detail,model.equipment);
    },
    setEquipment(model,equipment){return this.replaceAppearance(model,model.detail,equipment);}
  };
})();
