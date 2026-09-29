(function () {
  'use strict';
  const R=window.RTS,M=R.Math,V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z),shade=R.SurfaceColor.shade;
  const styles=[
    {name:'bald',side:0,top:0,front:0},
    {name:'buzz',side:.12,top:.10,front:.08},
    {name:'crew',side:.36,top:.72,front:.40},
    {name:'crop',side:.30,top:.82,front:.65},
    {name:'side',side:.32,top:.93,front:.50,part:.35},
    {name:'fade',side:.12,top:.94,front:.52},
    {name:'messy',side:.42,top:1.05,front:.67}
  ];
  R.InfantryHair={
    names:styles.map(s=>s.name),
    build(B,high,f,F,n,far=false) {
      const style=styles[f.hairStyle]||styles[2];
      if(style.name==='bald'||B.equipmentFit?.equipment?.definition('face')?.id==='balaclava')return;
      B.currentTag='hair';
      const color=R.SurfaceColor.rgb(f.hairColor),w={head:1},rows=high?17:far?6:12,segments=high?Math.max(n,56):far?Math.max(n,20):Math.max(n,40);
      const fit=B.equipmentFit,visible=p=>!fit||fit.hairVisible(p);
      const bridgeHair=(a,b)=>{
        for(let j=0;j<a.length;j++){
          const k=(j+1)%a.length;
          if(a[j]>=0&&b[j]>=0&&a[k]>=0)B.triangle(a[j],b[j],a[k],2);
          if(a[k]>=0&&b[j]>=0&&b[k]>=0)B.triangle(a[k],b[j],b[k],2);
        }
      };
      let last=null,first=null;const sectionScratch=[0,0,0,0];
      for(let r=0;r<rows;r++) {
        const t=r/rows,loop=[];
        for(let j=0;j<segments;j++) {
          const a=-Math.PI+j/segments*Math.PI*2,front=Math.max(0,Math.cos(a)),side=Math.abs(Math.sin(a));
          let bottom=F.hairBottom(a);
          if(style.name==='fade')bottom+=.0025*side;
          const y=M.mix(bottom,F.topY-.0003,t),section=sectionScratch,p=F.point(y,a,V(),section);
          const wave=Math.sin(a*15+t*13)*.08+Math.sin(a*9-t*7)*.04;
          let radial=(.0008+f.hairThickness*.22+f.hairVolume*(style.side*side+style.front*front)*.30)*Math.sin((1-t)*Math.PI/2);
          if(B.equipmentFit&&B.equipmentFit.headgear)radial=Math.min(radial,.0018);
          p.x+=Math.sin(a)*radial*(1+wave);
          p.z+=Math.cos(a)*radial*(1+wave);
          const top=f.hairVolume*style.top*Math.pow(t,1.4)*.58;
          p.y+=top*(1+wave*(style.name==='messy'?1:.3));
          if(style.part)p.x+=style.part*top*Math.sin(t*Math.PI);
          if(!visible(p)){loop.push(-1);continue;}
          const normal=V(Math.sin(a)*(1-t*.75),t,Math.cos(a)*(1-t*.75)).normalize();
          loop.push(B.vertex(p,w,shade(color,.92+wave*.32),normal,[j/segments,t]));
        }
        if(last)bridgeHair(last,loop);else first=loop;last=loop;
      }
      // A single shared crown vertex avoids the old open/flattened cap.
      const top=F.point(F.topY,0);top.y+=f.hairVolume*style.top*.58;
      const tip=visible(top)?B.vertex(top,w,color,V(0,1,0)):-1;
      if(tip>=0)for(let j=0;j<segments;j++)if(last[j]>=0&&last[(j+1)%segments]>=0)B.triangle(last[j],tip,last[(j+1)%segments],2);
      // Thin skirt terminates on the final scalp, not an unrelated ellipsoid.
      const inset=[];
      for(let j=0;j<segments;j++){
        const a=-Math.PI+j/segments*Math.PI*2,y=F.hairBottom(a)+(style.name==='fade'?.0025*Math.abs(Math.sin(a)):0),p=F.point(y,a);
        p.y+=.0002;inset.push(visible(p)?B.vertex(p,w,shade(color,.87),V(Math.sin(a),0,Math.cos(a))):-1);
      }
      bridgeHair(inset,first);
    }
  };
})();
