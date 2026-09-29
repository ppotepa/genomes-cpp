(function(){
  'use strict';
  const R=window.RTS;
  R.LodHysteresis={
    install(lod,fraction=.08){
      if(!lod||!Array.isArray(lod.levels))throw new TypeError('Oczekiwano obiektu THREE.LOD.');
      if(!Number.isFinite(fraction)||fraction<0||fraction>=1)throw new RangeError('Niepoprawna histereza LOD.');
      const cameraPosition=new THREE.Vector3(),worldPosition=new THREE.Vector3();
      lod._currentLevel=0;
      lod.update=function(camera){
        const levels=this.levels;if(levels.length<2)return;
        camera.getWorldPosition(cameraPosition);this.getWorldPosition(worldPosition);
        const distance=cameraPosition.distanceTo(worldPosition)/(camera.zoom||1);
        let level=Math.max(0,Math.min(levels.length-1,this._currentLevel||0));
        while(level<levels.length-1&&distance>=levels[level+1].distance*(1+fraction))level++;
        while(level>0&&distance<levels[level].distance*(1-fraction))level--;
        for(let i=0;i<levels.length;i++)levels[i].object.visible=i===level;
        this._currentLevel=level;
      };
      return lod;
    }
  };
})();
