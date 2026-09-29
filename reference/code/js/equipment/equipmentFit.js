(function () {
  'use strict';
  const R=window.RTS,M=R.Math,V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z);
  const JACKET=[
    [.504,.112,.071],[.519,.113,.071],[.552,.110,.067],[.595,.094,.056],
    [.640,.100,.059],[.698,.113,.066],[.735,.122,.066],[.759,.129,.064],
    [.785,.134,.062],[.809,.127,.057],[.825,.112,.051],[.841,.070,.040],
    [.852,.045,.036],[.859,.042,.034]
  ];
  function visual(eq,slot){const d=eq&&eq.definition(slot);return d?d.visual:{};}
  R.EquipmentFit=class EquipmentFit {
    constructor(anatomy,equipment=null){
      this.A=anatomy;this.H=anatomy.height;this.body=anatomy.body;this.face=anatomy.faceLayout;this.equipment=equipment;
      this.shirt=visual(equipment,'torsoBase');this.pants=visual(equipment,'legs');this.boots=visual(equipment,'feet');this.gloves=visual(equipment,'hands');
      this.armor=visual(equipment,'torsoArmor');this.pack=visual(equipment,'back');
      this.headgear=equipment&&equipment.definition('head');
      const b=this.body,A=this.A,ease=this.shirt.ease||1;
      this.jacket=JACKET.map(([y,rx,rz])=>{
        const chestT=M.smooth((y-.60)/.14),shoulder=Math.exp(-Math.pow((y-.790)/.060,2));
        let wx=M.mix(b.waistWidthScale,b.chestWidthScale,chestT);wx*=1+(b.shoulderWidthScale-1)*shoulder*.78;
        const dz=M.mix(b.waistDepthScale,b.chestDepthScale,chestT),mappedY=this.mapTorsoY(y);
        if(y>=.841){const neck=A.faceLayout.section(mappedY),t=M.smooth((y-.825)/.034),gap=.0025;
          return[mappedY,M.mix(rx*wx*ease,neck[1]+gap,t),M.mix(rz*dz*ease,neck[2]+gap,t)];}
        return[mappedY,rx*wx*ease,rz*dz*ease];
      });
      this.packDimensions=this.resolvePackDimensions();
      this.sockets=this.makeSockets();
    }
    static surfaceSignatureFor(equipment){
      const visual=slot=>{const item=equipment&&equipment.definition(slot);return item?item.visual:{};};
      const shirt=visual('torsoBase'),pants=visual('legs'),boots=visual('feet'),gloves=visual('hands');
      const head=equipment&&equipment.definition('head');
      return JSON.stringify([
        shirt.ease||1,pants.ease||1,boots.width||1,boots.shaft||1,
        !!gloves.style,gloves.style==='fingerless',
        !!(equipment&&equipment.definition('face')?.id==='balaclava'),
        head?[head.kind,head.visual.style]:null
      ]);
    }
    surfaceSignature(){return EquipmentFit.surfaceSignatureFor(this.equipment);}
    mapTorsoY(y){return M.mix(this.A.hipY-.036,.859,M.clamp((y-.504)/.355,0,1));}
    profile(y){for(let i=0;i<this.jacket.length-1;i++)if(y<=this.jacket[i+1][0]){
      const a=this.jacket[i],b=this.jacket[i+1],t=M.clamp((y-a[0])/(b[0]-a[0]),0,1);return[M.mix(a[1],b[1],t),M.mix(a[2],b[2],t)];}
      return this.jacket[this.jacket.length-1].slice(1);
    }
    front(x,y,gap=0){const [rx,rz]=this.profile(y);return V(x,y,rz*Math.sqrt(Math.max(.04,1-x*x/(rx*rx)))+gap);}
    torsoWeights(p){
      const A=this.A,sl=A.points.spineLower.y/this.H,su=A.points.spineUpper.y/this.H,ch=A.points.chest.y/this.H,y=p.y;
      if(y<sl){const t=M.smooth((y-A.hipY)/Math.max(.001,sl-A.hipY));return {hips:1-t,spineLower:t};}
      if(y<su){const t=M.smooth((y-sl)/Math.max(.001,su-sl));return {spineLower:1-t,spineUpper:t};}
      if(y<ch){const t=M.smooth((y-su)/Math.max(.001,ch-su));return {spineUpper:1-t,chest:t};}
      return y>.832?A.faceLayout.neckWeights(y):{chest:1};
    }
    armorGap(){return this.armor.thickness?this.armor.thickness/this.H:0;}
    headBottom(theta){
      if(!this.headgear)return Infinity;
      const f=this.face,front=Math.max(0,Math.cos(theta)),side=Math.abs(Math.sin(theta));
      if(this.headgear.kind==='helmet')return Math.max(.926+front*.031+side*(this.headgear.visual.style==='light'?.014:.006),f.hairFloor*front+(.929)*(1-front));
      return Math.max(.949-Math.max(0,-Math.cos(theta))*.009,f.hairFloor+.001);
    }
    hairVisible(p){
      if(this.equipment&&this.equipment.definition('face')?.id==='balaclava')return false;
      if(!this.headgear)return true;
      return p.y<this.headBottom(Math.atan2(p.x,p.z))-.001;
    }
    resolvePackDimensions(){
      if(!this.pack.size)return null;
      const item=this.equipment.slots.back,scale=M.clamp(.97+.10*(this.body.shoulderWidthScale-1),.90,1.08)*item.variant.size;
      return this.pack.size.map((v,i)=>v/this.H*(i===2?1:scale));
    }
    makeSockets(){
      const A=this.A,H=this.H,torsoY=this.mapTorsoY(.725),hipY=A.hipY,profile=this.profile(torsoY),wprof=this.profile(hipY+.012);
      const sockets={},add=(name,bone,position,normal=V(0,0,1))=>sockets[name]={name,bone,position,normal};
      add('HEAD','head',A.points.head.clone().multiplyScalar(1/H));
      add('HEAD_FRONT','head',V(0,this.face.face.eyeY,this.face.frontZ(0,this.face.face.eyeY)));
      add('NECK','neck',V(0,.866,0));
      add('CHEST_CENTER','chest',this.front(0,torsoY,.003+this.armorGap()));
      add('CHEST_LEFT','chest',this.front(profile[0]*.48,this.mapTorsoY(.78),.004+this.armorGap()));
      add('BACK_CENTER','spineUpper',V(0,torsoY,-profile[1]-.008-this.armorGap()),V(0,0,-1));
      add('WAIST','hips',V(0,hipY+.008,0));
      add('WAIST_FRONT','hips',V(0,hipY-.007,wprof[1]+.012));
      add('WAIST_BACK','hips',V(0,hipY-.006,-wprof[1]-.012),V(0,0,-1));
      for(const [s,sign]of [['L',1],['R',-1]]){
        add('HIP_'+s,'hips',V(sign*(wprof[0]+.014),hipY-.018,0),V(sign,0,0));
        const thigh=A.points['thigh.'+s].clone().multiplyScalar(1/H);
        add('THIGH_'+s,'thigh.'+s,thigh.add(V(sign*(.054*this.body.legThicknessScale+.013),-.062,0)),V(sign,0,0));
        add('HAND_'+s,'hand.'+s,A.points['hand.'+s].clone().multiplyScalar(1/H));
        add('FOOT_'+s,'foot.'+s,A.points['foot.'+s].clone().multiplyScalar(1/H));
      }
      const packDepth=this.packDimensions?this.packDimensions[2]:0;
      add('WEAPON_BACK','spineUpper',V(-profile[0]*.30,torsoY,-profile[1]-.025-packDepth-this.armorGap()),V(0,0,-1));
      add('WEAPON_HIP','thigh.R',V(-A.hipHalf-.048*this.body.legThicknessScale,hipY-.092,.040),V(-.7,0,.7));
      return sockets;
    }
    socket(name){const s=this.sockets[name];if(!s)throw new Error('Brak punktu wyposażenia '+name);return s;}
  };
})();
