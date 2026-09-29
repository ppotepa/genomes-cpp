(function () {
  'use strict';
  const R=window.RTS;
  R.FixedClock=class FixedClock {
    constructor(){this.accumulator=0;this.last=null;this.droppedSeconds=0;this.steps=0;}
    reset(){this.accumulator=0;this.last=null;}
    advance(now,step){
      if(this.last===null){this.last=now;return 1;}
      const raw=Math.max(0,(now-this.last)/1000);this.last=now;
      const frame=Math.min(raw,R.Config.MAX_FRAME_DT);this.droppedSeconds+=raw-frame;this.accumulator+=frame;
      let count=0;
      while(this.accumulator+1e-12>=R.Config.FIXED_DT&&count<R.Config.MAX_STEPS){step(R.Config.FIXED_DT);this.accumulator-=R.Config.FIXED_DT;count++;this.steps++;}
      if(this.accumulator>=R.Config.FIXED_DT){const drop=Math.floor(this.accumulator/R.Config.FIXED_DT)*R.Config.FIXED_DT;this.accumulator-=drop;this.droppedSeconds+=drop;}
      return R.Math.clamp(this.accumulator/R.Config.FIXED_DT,0,1);
    }
  };
})();
