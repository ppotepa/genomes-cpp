(function(){
'use strict';
const B=globalThis.RTS.Buildings,EPS=1e-7,round=n=>Math.abs(n)<EPS?0:+n.toFixed(7);
const same=(a,b)=>Math.abs(a[0]-b[0])<=EPS&&Math.abs(a[1]-b[1])<=EPS;
function openRing(ring){const r=(ring||[]).map(p=>[round(+p[0]),round(+p[1])]);if(r.length>1&&same(r[0],r[r.length-1]))r.pop();return r;}
function signedArea(ring){let sum=0;for(let i=0;i<ring.length;i++){const p=ring[i],q=ring[(i+1)%ring.length];sum+=p[0]*q[1]-q[0]*p[1];}return sum/2;}
function rotateStable(ring){let best=0;for(let i=1;i<ring.length;i++)if(ring[i][0]<ring[best][0]-EPS||(Math.abs(ring[i][0]-ring[best][0])<=EPS&&ring[i][1]<ring[best][1]-EPS))best=i;return ring.slice(best).concat(ring.slice(0,best));}
function canonicalRing(input,ccw){let ring=openRing(input);if(ring.length<3)throw new TypeError('Polygon ring needs at least three vertices');if((signedArea(ring)>0)!==ccw)ring.reverse();return rotateStable(ring);}
function canonical(value){if(!value)throw new TypeError('Polygon is required');const outer=canonicalRing(value.outer||value[0],true),holes=(value.holes||value.slice?.(1)||[]).map(r=>canonicalRing(r,false));holes.sort((a,b)=>JSON.stringify(a).localeCompare(JSON.stringify(b)));return {outer,holes};}
function backendFormat(value){const p=canonical(value);return [[p.outer.concat([p.outer[0]]),...p.holes.map(r=>r.concat([r[0]]))]];}
function fromBackend(multi){return (multi||[]).map(poly=>canonical({outer:poly[0],holes:poly.slice(1)})).sort((a,b)=>JSON.stringify(a).localeCompare(JSON.stringify(b)));}
function pointInRing(point,ring){let inside=false;for(let i=0,j=ring.length-1;i<ring.length;j=i++){const a=ring[i],b=ring[j];if(((a[1]>point[1])!==(b[1]>point[1]))&&point[0]<(b[0]-a[0])*(point[1]-a[1])/(b[1]-a[1])+a[0])inside=!inside;}return inside;}
function containsPoint(poly,p){const c=canonical(poly);return pointInRing(p,c.outer)&&!c.holes.some(h=>pointInRing(p,h));}
function bounds(poly){const p=canonical(poly),xs=p.outer.map(v=>v[0]),ys=p.outer.map(v=>v[1]);return [Math.min(...xs),Math.max(...xs),Math.min(...ys),Math.max(...ys)];}
function area(poly){const p=canonical(poly);return Math.abs(signedArea(p.outer))-p.holes.reduce((n,h)=>n+Math.abs(signedArea(h)),0);}
function validate(poly,options={}){const p=canonical(poly),minEdge=options.minEdge??.2,failures=[];for(const [name,ring] of [['outer',p.outer],...p.holes.map((h,i)=>['hole-'+i,h])])for(let i=0;i<ring.length;i++){const a=ring[i],b=ring[(i+1)%ring.length];if(Math.hypot(a[0]-b[0],a[1]-b[1])<minEdge)failures.push(name+':MIN_EDGE');}if(area(p)<(options.minArea??1))failures.push('MIN_AREA');for(const h of p.holes)if(!h.every(v=>pointInRing(v,p.outer)))failures.push('HOLE_OUTSIDE');return {valid:!failures.length,failures,polygon:p};}
function triangulate(poly){const p=canonical(poly),vertices=[],holes=[];for(const [index,ring] of [p.outer,...p.holes].entries()){if(index)holes.push(vertices.length/2);for(const v of ring)vertices.push(v[0],v[1]);}return {vertices,holes,indices:B.Triangulation.triangulate(vertices,holes,2)};}
function rectangle(x0,x1,y0,y1){return canonical({outer:[[x0,y0],[x1,y0],[x1,y1],[x0,y1]],holes:[]});}
function stableStringify(poly){return JSON.stringify(canonical(poly));}
B.EPS=EPS;B.Polygon=Object.freeze({canonical,validate,area,bounds,containsPoint,backendFormat,fromBackend,triangulate,rectangle,stableStringify,signedArea});
})();
