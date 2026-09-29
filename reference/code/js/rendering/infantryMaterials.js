(function () {
  'use strict';
  const R=window.RTS;
  let weave=null,shared=null,regionBody=null,regionGear=null;
  function createRegionMaterial(kind,options){
    const material=new THREE.MeshStandardMaterial(options),regions=kind==='body'?[.95,.72,.9]:[.95,.72,.56],metals=kind==='body'?[0,0,0]:[0,0,.35];
    const floatLiteral=value=>value===0?'0.0':String(value);
    material.onBeforeCompile=shader=>{
      shader.vertexShader=shader.vertexShader.replace('#include <common>','#include <common>\nattribute float materialRegion;\nvarying float vMaterialRegion;')
        .replace('#include <begin_vertex>','#include <begin_vertex>\nvMaterialRegion = materialRegion;');
      shader.fragmentShader=shader.fragmentShader.replace('#include <common>','#include <common>\nvarying float vMaterialRegion;');
      if(kind==='body'){
        shader.fragmentShader=shader.fragmentShader.replace('#include <map_fragment>','if (vMaterialRegion < 0.5) {\n#include <map_fragment>\n}')
          .replace('#include <normal_fragment_maps>','if (vMaterialRegion < 0.5) {\n#include <normal_fragment_maps>\n}');
      }else shader.fragmentShader=shader.fragmentShader.replace('void main() {','void main() {\nif (vMaterialRegion > 1.5 && !gl_FrontFacing) discard;');
      shader.fragmentShader=shader.fragmentShader.replace('#include <roughnessmap_fragment>','#include <roughnessmap_fragment>\nroughnessFactor = vMaterialRegion < 0.5 ? '+regions.map(floatLiteral)[0]+' : (vMaterialRegion < 1.5 ? '+regions.map(floatLiteral)[1]+' : '+regions.map(floatLiteral)[2]+');')
        .replace('#include <metalnessmap_fragment>','#include <metalnessmap_fragment>\nmetalnessFactor = vMaterialRegion < 1.5 ? '+floatLiteral(metals[0])+' : '+floatLiteral(metals[2])+';');
    };
    material.customProgramCacheKey=()=>`infantry-material-region-v1-${kind}`;
    return material;
  }
  R.InfantryMaterials={
    get(){
      if(shared)return shared;
      const canvas=document.createElement('canvas');canvas.width=128;canvas.height=128;
      const ctx=canvas.getContext('2d'),img=ctx.createImageData(128,128),rng=new R.SeededRandom(71819);
      for(let y=0;y<128;y++)for(let x=0;x<128;x++){
        const k=(y*128+x)*4,v=232+(x%4===0?8:0)+(y%4===0?7:0)+Math.floor(rng.next()*8);
        img.data[k]=v;img.data[k+1]=v;img.data[k+2]=v;img.data[k+3]=255;
      }
      ctx.putImageData(img,0,0);weave=new THREE.CanvasTexture(canvas);
      weave.wrapS=weave.wrapT=THREE.RepeatWrapping;weave.encoding=THREE.sRGBEncoding;
      shared=[
        new THREE.MeshStandardMaterial({color:0xffffff,vertexColors:true,skinning:true,morphTargets:true,morphNormals:true,roughness:.95,metalness:0,map:weave,bumpMap:weave,bumpScale:.00035}),
        new THREE.MeshStandardMaterial({color:0xffffff,vertexColors:true,skinning:true,morphTargets:true,morphNormals:true,roughness:.72,metalness:0}),
        new THREE.MeshStandardMaterial({color:0xffffff,vertexColors:true,skinning:true,morphTargets:true,morphNormals:true,roughness:.9,metalness:0})
      ];
      return shared;
    },
    getRegionAware(){
      if(!regionBody)regionBody=createRegionMaterial('body',{color:0xffffff,vertexColors:true,skinning:true,morphTargets:true,morphNormals:true,roughness:.95,metalness:0,map:this.get()[0].map,bumpMap:this.get()[0].bumpMap,bumpScale:.00035});
      return regionBody;
    },
    createRegionMaterial,
    dispose(){if(shared)shared.forEach(m=>m.dispose());if(regionBody)regionBody.dispose();if(regionGear)regionGear.dispose();if(weave)weave.dispose();shared=weave=regionBody=regionGear=null;}
  };
  Object.defineProperty(R.InfantryMaterials,'_regionGear',{get(){if(!regionGear)regionGear=createRegionMaterial('gear',{color:0xffffff,vertexColors:true,skinning:true,roughness:.95,metalness:0,side:THREE.DoubleSide});return regionGear;}});
})();
