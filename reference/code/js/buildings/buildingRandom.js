(function () {
  'use strict';
  const R = globalThis.RTS = globalThis.RTS || {};
  const B = R.Buildings = R.Buildings || {};
  const mix=(a,b,t)=>a+(b-a)*t;
  function deriveSeed(seed,path){
    let hash=(2166136261^(seed>>>0))>>>0;
    for(const char of String(path))hash=Math.imul(hash^char.charCodeAt(0),16777619)>>>0;
    return hash||1;
  }

  class RNG {
    constructor(seed){
      this.seed=seed>>>0;
      this.state=this.seed;
    }
    next(){
      // Same Mulberry32 stream with or without the application's optional RNG script.
      this.state=(this.state+0x6D2B79F5)>>>0;
      let t=this.state;t=Math.imul(t^(t>>>15),t|1);t^=t+Math.imul(t^(t>>>7),t|61);
      return ((t^(t>>>14))>>>0)/4294967296;
    }
    range(a,b){return mix(a,b,this.next());}
    int(a,b){return Math.floor(this.range(a,b+1));}
    pick(a){return a[Math.min(a.length-1,Math.floor(this.next()*a.length))];}
    chance(p){return this.next()<p;}
    fork(path){return new RNG(deriveSeed(this.seed,path));}
  }


  Object.assign(B, {deriveSeed, RNG});
})();
