(function(){
  'use strict';
  const R=globalThis.RTS,V=R.DestructionSolid.V;
  const normalize=q=>{const l=Math.hypot(...q);return l>1e-12?q.map(x=>x/l):[0,0,0,1];};
  const multiply=(a,b)=>[a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]];
  function rotate(q,v){const t=V.mul(V.cross(q.slice(0,3),v),2);return V.add(v,V.add(V.mul(t,q[3]),V.cross(q.slice(0,3),t)));}
  function between(a,b){a=V.unit(a);b=V.unit(b);const d=V.dot(a,b);if(d<-.999999){const axis=V.unit(V.cross(a,Math.abs(a[0])<.8?[1,0,0]:[0,1,0]));return [...axis,0];}return normalize([...V.cross(a,b),1+d]);}
  function integrate(q,w,dt){const dq=multiply([...w,0],q);return normalize(q.map((x,i)=>x+dq[i]*dt*.5));}
  R.ProjectileMath={normalize,multiply,rotate,between,integrate,inverseRotate:(q,v)=>rotate([-q[0],-q[1],-q[2],q[3]],v)};
})();
