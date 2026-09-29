'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const context={RTS:{},Math};context.window=context;
vm.runInNewContext(fs.readFileSync(require.resolve('../js/core/seededRandom.js'),'utf8'),context);
const R=context.RTS;

function populate(seed,numeric){
  const random=new R.SeededRandom(seed^0xa72fb941),spacing=7,spacingSq=spacing*spacing,
    stride=512,offset=256,buckets=new Map(),accepted=[];
  const key=(x,z)=>numeric?(x+offset)*stride+(z+offset):x+','+z;
  for(let attempt=0;attempt<1600&&accepted.length<90;attempt++){
    const x=(random.next()-.5)*174,z=(random.next()-.5)*174;
    if(Math.abs(z)<24)continue;
    const cellX=Math.floor(x/spacing),cellZ=Math.floor(z/spacing);let tooClose=false;
    for(let dz=-1;dz<=1&&!tooClose;dz++)for(let dx=-1;dx<=1&&!tooClose;dx++){
      const nearby=buckets.get(key(cellX+dx,cellZ+dz));
      if(nearby)for(let n=0;n<nearby.length;n++){
        const ox=nearby[n].x-x,oz=nearby[n].z-z;
        if(ox*ox+oz*oz<spacingSq){tooClose=true;break;}
      }
    }
    if(tooClose)continue;
    const tree={x,z,variant:Math.floor(random.next()*6),angle:random.next()*Math.PI*2,scale:.8+random.next()*.35};
    accepted.push(tree);const bucketKey=key(cellX,cellZ),cell=buckets.get(bucketKey)||[];cell.push(tree);buckets.set(bucketKey,cell);
  }
  return accepted;
}

for(let seed=0;seed<128;seed++){
  assert.deepEqual(populate(seed,false),populate(seed,true),'numeric buckets changed seeded tree candidates at seed '+seed);
}
console.log('PASS numeric battlefield bucket keys preserve all accepted seeded tree placements for 128 seeds');
