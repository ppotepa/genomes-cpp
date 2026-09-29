(function(){
  'use strict';
  const R=globalThis.RTS;
  const passive=()=>({movement:'MISSION',targetId:null,lookAt:null,aimAt:null,weapon:'HOLSTER',trigger:false});
  R.AIModelRegistry.register('passive-v1',()=>({update:passive}));
  R.AIModelRegistry.register('simple-combat-v1',()=>({
    update(observation){
      if(!observation.self.alive)return {...passive(),movement:'HOLD'};
      const target=observation.target;
      if(!target)return passive();
      const point=target.aimPoint||target.lastKnownPosition;
      return {movement:'HOLD',targetId:target.id,lookAt:point,aimAt:point,weapon:'DRAW',trigger:!!target.visible&&!!target.lineOfSight&&Math.abs(observation.self.directionError||0)<4*Math.PI/180};
    }
  }));
})();
