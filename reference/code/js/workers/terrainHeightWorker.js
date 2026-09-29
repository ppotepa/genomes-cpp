'use strict';

// Compute the deterministic terrain attribute buffers away from the UI thread.
// Three.js objects and chunk bounds remain owned by the main thread.
self.onmessage=function(event){
  const data=event.data,segments=data.segments,rowWidth=segments+1,step=data.mapSize/segments,half=data.mapSize*.5;
  const perm=new Uint8Array(data.permutation),vertexCount=rowWidth*rowWidth;
  const positions=new Float32Array(vertexCount*3),uvs=new Float32Array(vertexCount*2),heights=new Float32Array(vertexCount),normals=new Float32Array(vertexCount*3),colors=new Float32Array(vertexCount*3);
  const IndexArray=vertexCount>65536?Uint32Array:Uint16Array,indices=new IndexArray(segments*segments*6);
  const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
  const smoothstep=(x,a,b)=>{x=clamp((x-a)/(b-a),0,1);return x*x*(3-2*x);};
  function noise(x,y){
    const floorX=Math.floor(x),floorY=Math.floor(y),xi=floorX&255,yi=floorY&255,xf=x-floorX,yf=y-floorY;
    const u=xf*xf*xf*(xf*(xf*6-15)+10),v=yf*yf*yf*(yf*(yf*6-15)+10);
    const aa=perm[perm[xi]+yi],ab=perm[perm[xi]+yi+1],ba=perm[perm[xi+1]+yi],bb=perm[perm[xi+1]+yi+1];
    const grad=(hash,gx,gy)=>{switch(hash&7){case 0:return gx+gy;case 1:return -gx+gy;case 2:return gx-gy;case 3:return -gx-gy;case 4:return gx;case 5:return -gx;case 6:return gy;default:return -gy;}};
    const a=grad(aa,xf,yf),b=grad(ba,xf-1,yf),c=grad(ab,xf,yf-1),d=grad(bb,xf-1,yf-1),x1=a+u*(b-a),x2=c+u*(d-c);
    return (x1+v*(x2-x1))*.72;
  }
  function fbmPlan(octaves,lacunarity,gain){const amplitudes=new Float64Array(octaves),frequencies=new Float64Array(octaves);let amplitude=.5,frequency=1,total=0;for(let i=0;i<octaves;i++){amplitudes[i]=amplitude;frequencies[i]=frequency;total+=amplitude;amplitude*=gain;frequency*=lacunarity;}return{amplitudes,frequencies,total};}
  const warpPlan=fbmPlan(3,2.02,.5),continentalPlan=fbmPlan(5,2,.53),hillsPlan=fbmPlan(5,2.08,.5),detailPlan=fbmPlan(4,2.15,.46),ridgePlan=fbmPlan(4,2,.5);
  function fbm(x,z,plan){let value=0;for(let i=0;i<plan.amplitudes.length;i++)value+=noise(x*plan.frequencies[i],z*plan.frequencies[i])*plan.amplitudes[i];return value/Math.max(plan.total,.0001);}
  function height(x,z){
    if(data.battlefield)return noise(x/65+11,z/65-7)*7+noise(x/28-19,z/28+31)*2+noise(x/12+5,z/12+8)*.35;
    const warpX=fbm(x/850+18.1,z/850-7.4,warpPlan)*130,warpZ=fbm(x/850-11.7,z/850+21.8,warpPlan)*130,wx=x+warpX,wz=z+warpZ;
    const continental=fbm(wx/1350,wz/1350,continentalPlan),hills=fbm(wx/430+31.3,wz/430-16.2,hillsPlan),detail=fbm(wx/120-9,wz/120+12,detailPlan),ridgeNoise=fbm(wx/620+4.2,wz/620+15.6,ridgePlan);
    const ridged=1-Math.abs(ridgeNoise),ridgeMask=clamp((continental+.12)*1.25,0,1),plainBase=Math.sign(continental)*Math.pow(Math.abs(continental),1.35),hillMask=smoothstep(continental,-.08,.42);
    let h=plainBase*92;h+=hills*(26+hillMask*44);h+=(ridged-.56)*36*ridgeMask;h+=detail*7;const edge=Math.max(Math.abs(x),Math.abs(z))/data.halfMap;h-=smoothstep(edge,.83,1)*8;return h;
  }
  for(let iy=0;iy<=segments;iy++){
    const localY=-(iy*step-half);
    for(let ix=0;ix<=segments;ix++){
      const vertex=iy*rowWidth+ix,po=vertex*3,uo=vertex*2,x=ix*step-half,z=-localY;
      positions[po]=x;positions[po+1]=localY*Math.cos(-Math.PI/2);positions[po+2]=localY*Math.sin(-Math.PI/2);
      uvs[uo]=ix/segments;uvs[uo+1]=1-(iy/segments);heights[vertex]=height(positions[po],positions[po+2]);positions[po+1]=heights[vertex];
    }
  }
  let indexOffset=0;
  for(let iy=0;iy<segments;iy++)for(let ix=0;ix<segments;ix++){
    const a=ix+rowWidth*iy,b=ix+rowWidth*(iy+1),c=(ix+1)+rowWidth*(iy+1),d=(ix+1)+rowWidth*iy;
    indices[indexOffset++]=a;indices[indexOffset++]=b;indices[indexOffset++]=d;indices[indexOffset++]=b;indices[indexOffset++]=c;indices[indexOffset++]=d;
  }
  for(let i=0;i<indices.length;i+=3){
    const a=indices[i],b=indices[i+1],c=indices[i+2],ai=a*3,bi=b*3,ci=c*3;
    const cbx=positions[ci]-positions[bi],cby=positions[ci+1]-positions[bi+1],cbz=positions[ci+2]-positions[bi+2],abx=positions[ai]-positions[bi],aby=positions[ai+1]-positions[bi+1],abz=positions[ai+2]-positions[bi+2];
    const nx=cby*abz-cbz*aby,ny=cbz*abx-cbx*abz,nz=cbx*aby-cby*abx;
    normals[ai]+=nx;normals[ai+1]+=ny;normals[ai+2]+=nz;normals[bi]+=nx;normals[bi+1]+=ny;normals[bi+2]+=nz;normals[ci]+=nx;normals[ci+1]+=ny;normals[ci+2]+=nz;
  }
  for(let i=0;i<vertexCount;i++){
    const offset=i*3,x=normals[offset],y=normals[offset+1],z=normals[offset+2],length=Math.sqrt(x*x+y*y+z*z);
    if(length>0){const inverse=1/length;normals[offset]=x*inverse;normals[offset+1]=y*inverse;normals[offset+2]=z*inverse;}
    const heightValue=positions[offset+1],slope=1-Math.max(0,normals[offset+1]),dryWeight=smoothstep(heightValue,-10,75),rockWeight=smoothstep(slope,.05,.3),grass=data.palette.grass,dry=data.palette.dry,rock=data.palette.rock;
    let r=grass.r+(dry.r-grass.r)*dryWeight,g=grass.g+(dry.g-grass.g)*dryWeight,b=grass.b+(dry.b-grass.b)*dryWeight;
    r+=(rock.r-r)*rockWeight;g+=(rock.g-g)*rockWeight;b+=(rock.b-b)*rockWeight;
    const variation=.95+.05*Math.sin(positions[offset]*.019+positions[offset+2]*.027);colors[offset]=r*variation;colors[offset+1]=g*variation;colors[offset+2]=b*variation;
  }
  const transfers=[positions.buffer,uvs.buffer,heights.buffer,normals.buffer,colors.buffer,indices.buffer];
  self.postMessage({positions:positions.buffer,uvs:uvs.buffer,heights:heights.buffer,normals:normals.buffer,colors:colors.buffer,indices:indices.buffer},transfers);
};
