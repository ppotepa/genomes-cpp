(function(){
'use strict';
const B=globalThis.RTS.Buildings;
const p=(outer,holes=[])=>B.Polygon.canonical({outer,holes});
// Bounded attachment grammar: a central spine owns side wings; irregular
// footprints also grow a smaller terminal wing from each side wing. Every
// attachment overlaps its parent, stays inside the requested envelope, and
// records its own geometry so the union can be independently reconstructed.
function hierarchy(f,w,d,rng){
  const rect=b=>B.Polygon.rectangle(...b),x=w/2,z=d/2,spineWidth=w*(.42+rng.next()*.12),spine=[-spineWidth/2,spineWidth/2,-z,z];
  const tree={id:'spine',rule:'SPINE',bounds:spine,polygon:rect(spine),children:[]},parts=[tree.polygon];
  for(const side of [-1,1]){
    const height=d*(.38+rng.next()*.18),start=-z+rng.next()*(d-height),overlap=Math.min(.5,spineWidth*.15),end=side<0?-x:x,join=side*spineWidth/2;
    const box=side<0?[end,join+overlap,start,start+height]:[join-overlap,end,start,start+height],node={id:`wing-${side}`,rule:'ATTACH_SIDE',side,overlap,bounds:box,polygon:rect(box),children:[]};
    tree.children.push(node);parts.push(node.polygon);
    if(f==='IRREGULAR_ORTHO'){
      const upward=start+height/2<0,extension=Math.min(d*.22,upward?z-start-height:start+z),wingWidth=(x-spineWidth/2)*(.5+rng.next()*.2);
      if(extension>=.5){const terminal=side<0?[-x,-x+wingWidth,upward?start+height-overlap:start-extension,upward?start+height+extension:start+overlap]:[x-wingWidth,x,upward?start+height-overlap:start-extension,upward?start+height+extension:start+overlap];const child={id:node.id+'/terminal',rule:'ATTACH_TERMINAL',overlap,bounds:terminal,polygon:rect(terminal),children:[]};node.children.push(child);parts.push(child.polygon);}
    }
  }
  const union=B.Polygon.fromBackend(B.PolygonBackend.union(...parts.map(B.Polygon.backendFormat)));
  return {polygon:union.length===1?union[0]:null,tree};
}
function grammar(f,w,d,rng,minWing=0){const x=w/2,z=d/2,a=+Math.min(w*.45,Math.max(minWing,w*(.28+rng.next()*.12))).toFixed(4),b=+Math.min(d*(f==='COURTYARD'?.45:.75),Math.max(minWing,d*(.28+rng.next()*.12))).toFixed(4);switch(f){
case 'RECT':return p([[-x,-z],[x,-z],[x,z],[-x,z]]);
case 'L':return p([[-x,-z],[x,-z],[x,-z+b],[-x+a,-z+b],[-x+a,z],[-x,z]]);
case 'T':return p([[-x,-z],[x,-z],[x,-z+b], [a/2,-z+b],[a/2,z],[-a/2,z],[-a/2,-z+b],[-x,-z+b]]);
case 'U':return p([[-x,-z],[x,-z],[x,z],[x-a,z],[x-a,-z+b],[-x+a,-z+b],[-x+a,z],[-x,z]]);
case 'H':return p([[-x,-z],[-x+a,-z],[-x+a,-b/2],[x-a,-b/2],[x-a,-z],[x,-z],[x,z],[x-a,z],[x-a,b/2],[-x+a,b/2],[-x+a,z],[-x,z]]);
case 'Z':return p([[-x,-z],[x-a,-z],[x-a,-b/2],[x,-b/2],[x,z],[-x+a,z],[-x+a,b/2],[-x,b/2]]);
case 'COURTYARD':return p([[-x,-z],[x,-z],[x,z],[-x,z]],[[[-x+a,-z+b],[-x+a,z-b],[x-a,z-b],[x-a,-z+b]]]);
default:throw new TypeError('Unknown footprint family '+f);
}}
function insetUsable(poly,margin){const [x0,x1,z0,z1]=B.Polygon.bounds(poly);if(x1-x0<=margin*2||z1-z0<=margin*2)return null;const box=B.Polygon.rectangle(x0+margin,x1-margin,z0+margin,z1-margin);const result=B.Polygon.fromBackend(B.PolygonBackend.intersection(B.Polygon.backendFormat(poly),B.Polygon.backendFormat(box)));return result.length===1?result[0]:null;}
B.generateFootprint=function(spec){
  // Multi-storey grammar bars must accommodate the actual stair sweep plus
  // the 0.3 m usable-envelope setback on each side; dimensions never grow.
  const stairBox=spec.storeys.count>1?B.stairCoreBounds(spec):null,minWing=stairBox?Math.min(stairBox[1]-stairBox[0],stairBox[3]-stairBox[2])+.6:0;
  if(spec.explicitFootprint){const check=B.Polygon.validate(spec.explicitFootprint,{minArea:4,minEdge:.25}),usable=check.valid&&insetUsable(check.polygon,.3);if(!check.valid||!usable)throw new B.BuildingGenerationError('FOOTPRINT_INVALID','footprint',spec,null,check.failures.concat(usable?[]:['NO_CONNECTED_USABLE_INSET']),{spec});return {family:'EXPLICIT',polygon:check.polygon,candidates:[check.polygon],usable,diagnostics:{grammarTree:{rule:'EXPLICIT',polygon:check.polygon,children:[]}}};}
  const candidates=[],trees=[],failures=[];for(let i=0;i<8;i++){const rng=new B.RNG(B.deriveSeed(spec.seeds.footprint,'candidate-'+i)),generated=['COMPOSITE','IRREGULAR_ORTHO'].includes(spec.footprintFamily)?hierarchy(spec.footprintFamily,spec.width,spec.depth,rng):null,candidate=generated?generated.polygon:grammar(spec.footprintFamily,spec.width,spec.depth,rng,minWing);if(!candidate){failures.push({candidateId:i,failures:['DISCONNECTED_GRAMMAR']});continue;}const check=B.Polygon.validate(candidate,{minArea:Math.max(2,spec.width*spec.depth*.22),minEdge:.25}),usable=check.valid&&insetUsable(candidate,.3);if(check.valid&&usable&&B.Polygon.area(usable)>2){candidates.push(candidate);trees.push(generated?generated.tree:{rule:spec.footprintFamily,polygon:candidate,children:[]});}else failures.push({candidateId:i,failures:check.failures.concat(usable?[]:['NO_USABLE_INSET'])});}
  if(!candidates.length)throw new B.BuildingGenerationError('NO_FOOTPRINT','footprint',spec,null,failures,{spec});return {family:spec.footprintFamily,polygon:candidates[0],candidates,usable:insetUsable(candidates[0],.3),grammarTree:trees[0],diagnostics:{attempted:8,failures,grammarTree:trees[0],grammarLimits:{maxDepth:2,maxAttachments:4},minimumStairWing:minWing}};
};
})();
