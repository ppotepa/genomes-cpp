(function(){
  'use strict';
  const R=window.RTS;
  const defaults=Object.freeze({version:'world-generation-1',seed:1001,size:600,preset:'village',vegetation:.62,buildings:.55,fencedParcels:.48});
  const types=['field','field','fallow','meadow','pasture','orchard','grove','forest'];
  const clamp=(v,f)=>Number.isFinite(+v)?Math.max(0,Math.min(1,+v)):f;
  function normalize(c={}){return {version:defaults.version,seed:(Number.isFinite(+c.seed)?+c.seed:defaults.seed)>>>0,size:[400,600,800,1200].includes(+c.size)?+c.size:600,preset:'village',vegetation:clamp(c.vegetation,defaults.vegetation),buildings:clamp(c.buildings,defaults.buildings),fencedParcels:clamp(c.fencedParcels,defaults.fencedParcels),skipEnvironment:!!c.skipEnvironment};}
  function distanceToSegment(x,z,a,b){const dx=b.x-a.x,dz=b.z-a.z,t=Math.max(0,Math.min(1,((x-a.x)*dx+(z-a.z)*dz)/(dx*dx+dz*dz||1)));return Math.hypot(x-a.x-dx*t,z-a.z-dz*t);}
  function inside(x,z,p){let hit=false;for(let i=0,j=p.length-1;i<p.length;j=i++){const a=p[i],b=p[j];if((a.z>z)!==(b.z>z)&&x<(b.x-a.x)*(z-a.z)/(b.z-a.z)+a.x)hit=!hit;}return hit;}
  function polygonDistance(x,z,p){if(inside(x,z,p))return 0;let d=Infinity;for(let i=0;i<p.length;i++)d=Math.min(d,distanceToSegment(x,z,p[i],p[(i+1)%p.length]));return d;}
  function intersects(a,b,c,d){const cross=(p,q,r)=>(q.x-p.x)*(r.z-p.z)-(q.z-p.z)*(r.x-p.x);return cross(a,b,c)*cross(a,b,d)<0&&cross(c,d,a)*cross(c,d,b)<0;}
  function segmentsDistance(a,b,c,d){if(intersects(a,b,c,d))return 0;return Math.min(distanceToSegment(a.x,a.z,c,d),distanceToSegment(b.x,b.z,c,d),distanceToSegment(c.x,c.z,a,b),distanceToSegment(d.x,d.z,a,b));}
  function polygonOverlap(a,b,gap=0){for(const p of a)if(polygonDistance(p.x,p.z,b)<gap+1e-5)return true;for(const p of b)if(polygonDistance(p.x,p.z,a)<gap+1e-5)return true;for(let i=0;i<a.length;i++)for(let j=0;j<b.length;j++)if(segmentsDistance(a[i],a[(i+1)%a.length],b[j],b[(j+1)%b.length])<gap+1e-5)return true;return false;}
  function rectangle(x,z,w,d,angle=0){const c=Math.cos(angle),s=Math.sin(angle);return [[-1,-1],[1,-1],[1,1],[-1,1]].map(([u,v])=>({x:x+(u*w*c-v*d*s)*.5,z:z+(u*w*s+v*d*c)*.5}));}
  function chamfer(polygon,index,fraction){const p=polygon[index],before=polygon[(index+polygon.length-1)%polygon.length],after=polygon[(index+1)%polygon.length],mix=q=>({x:p.x+(q.x-p.x)*fraction,z:p.z+(q.z-p.z)*fraction});return [...polygon.slice(0,index),mix(before),mix(after),...polygon.slice(index+1)];}
  function create(input){
    const config=normalize(input),random=new R.SeededRandom(config.seed^0x51e77e),half=config.size/2,roads=[],parcels=[],buildings=[],zones=[];
    const variant=['winding','crossroads','hamlets'][config.seed%3],mainZ=(random.next()-.5)*config.size*.1;
    const road=(id,kind,width,points,sidewalk=0)=>roads.push({id,kind,width,sidewalk,points});
    const curve=(start,end,z,phase)=>Array.from({length:9},(_,i)=>({x:start+(end-start)*i/8,z:z+Math.sin(i*.85+phase)*7+Math.sin(i*1.9+phase)*2}));
    road('main',variant==='hamlets'?'dirt':'asphalt',variant==='hamlets'?4.7:6.5,curve(-half+15,half-15,mainZ,random.next()*3),variant==='hamlets'?0:1.2);
    if(variant==='crossroads'){const p=roads[0].points[4];road('cross','dirt',4.4,[{x:p.x,z:-half+18},{x:p.x,z:p.z},{x:p.x+3,z:half-18}]);}
    if(variant==='hamlets')for(const [i,x] of [-config.size*.24,config.size*.24].entries()){const p=roads[0].points.reduce((a,b)=>Math.abs(a.x-x)<Math.abs(b.x-x)?a:b);road('hamlet-'+i,'dirt',4.5,[{x:p.x,z:p.z},{x:p.x+(i?8:-8),z:p.z+(i?65:-65)}]);}
    const arterial=roads.slice(),target=Math.max(5,Math.min(34,Math.round(config.size/100*(2.4+config.buildings*3.4)))),candidates=[];
    for(const route of arterial)for(let j=1;j<route.points.length;j++){const a=route.points[j-1],b=route.points[j],count=Math.floor(Math.hypot(b.x-a.x,b.z-a.z)/20);for(let k=0;k<count;k++)for(const side of [-1,1])candidates.push({a,b,t:(k+.35+random.next()*.3)/count,side,priority:random.next()});}
    candidates.sort((a,b)=>a.priority-b.priority);
    for(const {a,b,t,side} of candidates){
      if(buildings.length>=target)break;
      const dx=b.x-a.x,dz=b.z-a.z,len=Math.hypot(dx,dz),nx=-dz/len*side,nz=dx/len*side,roadPoint={x:a.x+dx*t,z:a.z+dz*t},angle=Math.atan2(dz,dx),width=18+random.next()*7,depth=22+random.next()*8;
      const center={x:roadPoint.x+nx*(13+depth*.5+random.next()*3),z:roadPoint.z+nz*(13+depth*.5+random.next()*3)};
      const bent=random.next()<.45,polygon=bent?chamfer(rectangle(center.x,center.z,width,depth,angle),side>0?2:0,.12+random.next()*.1):rectangle(center.x,center.z,width,depth,angle);
      if(polygon.some(p=>Math.abs(p.x)>half-12||Math.abs(p.z)>half-12)||parcels.some(p=>polygonOverlap(p.polygon,polygon,2)))continue;
      if(arterial.some(r=>r.points.some((end,i)=>i&&polygon.some(p=>distanceToSegment(p.x,p.z,r.points[i-1],end)<r.width*.5+2))))continue;
      const front={x:center.x-nx*depth*.5,z:center.z-nz*depth*.5},id=buildings.length;
      road('drive-'+id,'track',2.5,[roadPoint,{x:front.x-nx*1.5,z:front.z-nz*1.5}]);
      const parcel={id:'parcel-'+id,x:center.x,z:center.z,width,depth,rotation:angle,polygon,frontEdge:side>0?0:bent?3:2,gate:{x:front.x,z:front.z,width:4.2},driveway:'drive-'+id,fenced:random.next()<config.fencedParcels,fenceType:['wood','wire','wall'][Math.floor(random.next()*3)],buildingFootprint:rectangle(center.x,center.z,14,12,angle)};
      const presetId=id%7===3?'MULTI_FAMILY':'FAMILY_HOUSE';
      buildings.push({id:'settlement-'+id,x:center.x,z:center.z,rotation:angle+(side<0?Math.PI:0),seed:(config.seed+Math.imul(id+1,0x9e3779b1))>>>0,presetId,footprintFamily:id%2?'L':'RECT',roofFamily:presetId==='FAMILY_HOUSE'?(id%2?'GABLE':'HIP'):'FLAT',width:14,depth:12,parcel});
      parcels.push(parcel);
    }
    const cols=Math.ceil(config.size/Math.max(45,config.size/12)),step=config.size/cols;
    for(let iz=0;iz<cols;iz++)for(let ix=0;ix<cols;ix++){
      const x=-half+(ix+.5)*step,z=-half+(iz+.5)*step,w=step*(.77+random.next()*.12),h=step*(.77+random.next()*.12),j=step*.06;
      const polygon=[{x:x-w*.5+j*random.next(),z:z-h*.5},{x:x+w*.5,z:z-h*.5+j*random.next()},{x:x+w*.5-j*random.next(),z:z+h*.5},{x:x-w*.5,z:z+h*.5-j*random.next()}];
      const blocked=parcels.some(p=>polygonOverlap(p.polygon,polygon,4))||roads.some(r=>r.points.some((end,i)=>i&&(inside(r.points[i-1].x,r.points[i-1].z,polygon)||inside(end.x,end.z,polygon)||polygon.some((p,j)=>segmentsDistance(p,polygon[(j+1)%polygon.length],r.points[i-1],end)<r.width*.5+4))));
      if(!blocked)zones.push({id:'zone-'+zones.length,type:types[Math.floor(random.next()*types.length)],polygon});
    }
    const plan={config,variant,roads,parcels,buildings,zones};
    plan.distanceToReserved=(x,z)=>Math.min(...roads.flatMap(r=>r.points.slice(1).map((p,i)=>distanceToSegment(x,z,r.points[i],p)-r.width*.5-(r.sidewalk||0))),...parcels.map(p=>polygonDistance(x,z,p.polygon)));
    plan.isReserved=(x,z,padding=0)=>plan.distanceToReserved(x,z)<padding;
    plan.zoneAt=(x,z)=>zones.find(zone=>inside(x,z,zone.polygon))?.type||'open';
    return plan;
  }
  R.WorldGeneration={defaults,normalize,create,distanceToSegment,segmentsDistance,inside,polygonDistance,polygonOverlap,rectangle};
})();
