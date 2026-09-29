(function(){
  'use strict';
  const R=globalThis.RTS;
  class UnitPresentationScheduler{
    constructor(){this.last=new Map();}
    cadence(distance,visible=true,combat=false){if(combat||distance<20)return {movement:1/60,animation:1/60};if(!visible)return {movement:1/15,animation:1/5};if(distance<220)return {movement:1/60,animation:1/30};return {movement:1/15,animation:1/15};}
    update(units,camera,now=0){for(const unit of units||[]){const p=unit.position,d=camera?.position?camera.position.distanceTo(p):0,c=this.cadence(d,unit.visible!==false,!!unit.inCombat);if(unit.setVisualCadence)unit.setVisualCadence(c.animation,c.animation);this.last.set(unit.id,{at:now,distance:d,cadence:c});}return this;}
    stats(){return {units:this.last.size};}
  }
  R.UnitPresentationScheduler=UnitPresentationScheduler;
})();
