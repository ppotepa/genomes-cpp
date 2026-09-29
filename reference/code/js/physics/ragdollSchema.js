(function () {
  'use strict';
  const R=window.RTS;
  // To jest kontrakt danych i konwersji, NIE świat fizyki ani aktywny ragdoll.
  // Szkielet wizualny ma 22 kości ciała + 17 kontrolek twarzy; uproszczona reprezentacja fizyczna pozostaje 14-członowa.
  R.RagdollSchema={
    VERSION: 'ragdoll-contract-1',
    create(rig){
      const H=rig.anatomy.height,P=rig.anatomy.points,A=rig.anatomy,B=A.body||{},bodies=[],joints=[];
      const axisY=new THREE.Vector3(0,1,0);
      const totalMass=75*Math.pow(H/1.775,3)*(B.mass||1)*(.92+.08*(B.fat||1)); // Parametr szablonu gry, nie gen i nie norma medyczna.
      function add(id,bone,parent,start,end,radius,massFraction,kind='capsule',half=null){
        const p=start.clone(),q=end.clone(),center=p.clone().add(q).multiplyScalar(.5),direction=q.clone().sub(p),length=direction.length();
        const rotation=kind==='capsule'?new THREE.Quaternion().setFromUnitVectors(axisY,direction.clone().normalize()):new THREE.Quaternion();
        const bodyRest=new THREE.Matrix4().compose(center,rotation,new THREE.Vector3(1,1,1));
        const boneBind=new THREE.Matrix4().makeTranslation(P[bone].x,P[bone].y,P[bone].z);
        const boneToBody=new THREE.Matrix4().copy(boneBind).invert().multiply(bodyRest);
        bodies.push({id,bone,parent,shape:kind,radius:radius*H,halfCylinder:Math.max(0,(length-2*radius*H)/2),
          halfExtents:half?half.map(v=>v*H):null,massFraction,massKg:totalMass*massFraction,
          boneToBody,bodyToBone:boneToBody.clone().invert(),bindWorld:bodyRest,
          localBendAxis:[1,0,0],localTwistAxis:[0,1,0]});
      }
      const v=(x,y,z)=>new THREE.Vector3(x*H,y*H,z*H);
      add('pelvis','hips',null,v(0,A.hipY-.025,0),v(0,A.hipY+.025,0),.075*(B.hipWidthScale||1),.16,'box',[.092*(B.hipWidthScale||1),.050,.058*(B.waistDepthScale||1)]);
      add('abdomen','spineLower','pelvis',P.spineLower,P.spineUpper,.067*(B.waistWidthScale||1),.14,'box',[.085*(B.waistWidthScale||1),.061,.050*(B.waistDepthScale||1)]);
      add('chest','chest','abdomen',P.spineUpper,P.neck,.075*(B.chestWidthScale||1),.22,'box',[.108*(B.chestWidthScale||1),.065,.057*(B.chestDepthScale||1)]);
      const skull=A.faceLayout,headRadius=Math.max(...skull.levels.filter(p=>p[0]>.89).map(p=>Math.max(p[1],p[2])))*.87;
      add('head','head','chest',v(0,skull.chinY+.009,0),v(0,skull.topY-.010,0),headRadius,.08);
      for(const [s,sign]of [['L',1],['R',-1]]){
        add('upperArm.'+s,'upperArm.'+s,'chest',P['upperArm.'+s],P['foreArm.'+s],.032*(B.armThicknessScale||1),.03);
        add('foreArm.'+s,'foreArm.'+s,'upperArm.'+s,P['foreArm.'+s],P['hand.'+s],.025*(B.armThicknessScale||1),.02);
        add('thigh.'+s,'thigh.'+s,'pelvis',P['thigh.'+s],P['shin.'+s],.043*(B.legThicknessScale||1),.105);
        add('shin.'+s,'shin.'+s,'thigh.'+s,P['shin.'+s],P['foot.'+s],.030*(B.legThicknessScale||1),.035);
        const fx=P['foot.'+s].x/H,fs=B.footScale||1;add('foot.'+s,'foot.'+s,'shin.'+s,v(fx,.020,.041*fs),v(fx,.046,.041*fs),.02*fs,.01,'box',[.032*fs,.023,.084*fs]);
      }
      const byId=Object.fromEntries(bodies.map(b=>[b.id,b]));
      for(const b of bodies){
        if(!b.parent)continue;const p=byId[b.parent];const point=P[b.bone].clone();
        const hinge=/^(shin|foreArm)/.test(b.id);
        const limits=hinge?{flexion: b.id.startsWith('foreArm')?[-2.55,0]:[0,2.65],twist:[-.08,.08]}:
          {swing: b.id.startsWith('thigh')?[1.55,.8]:[1.15,.85],twist:[-.5,.5]};
        joints.push({parent:p.id,child:b.id,bone:b.bone,type:hinge?'limited-hinge':'limited-swing-twist',
          anchorParent:point.clone().applyMatrix4(p.bindWorld.clone().invert()).toArray(),
          anchorChild:point.clone().applyMatrix4(b.bindWorld.clone().invert()).toArray(),
          bendAxisParent:new THREE.Vector3(1,0,0).transformDirection(p.bindWorld.clone().invert()).toArray(),
          limitsRadians:limits,disableAdjacentCollision:true});
      }
      return {schemaVersion:this.VERSION,totalMassKg:totalMass,bodies,joints,byId,
        poseOwner:'ANIMATION',activePhysics:false,units:'metres-kilograms-seconds',
        collisionPolicy:'ignore-direct-neighbours; allow-nonadjacent-self-contact'};
    },
    bodyWorld(bone,definition,out=new THREE.Matrix4()){return out.multiplyMatrices(bone.matrixWorld,definition.boneToBody);},
    boneWorld(bodyWorld,definition,out=new THREE.Matrix4()){return out.multiplyMatrices(bodyWorld,definition.bodyToBone);},
    boneLocal(boneWorld,parentWorld,out=new THREE.Matrix4()){return out.copy(parentWorld).invert().multiply(boneWorld);},
    roundtripError(rig,schema){
      rig.root.updateMatrixWorld(true);let max=0;const p=new THREE.Matrix4(),b=new THREE.Matrix4(),l=new THREE.Matrix4();
      for(const d of schema.bodies){
        const bone=rig.byName[d.bone];this.bodyWorld(bone,d,p);this.boneWorld(p,d,b);this.boneLocal(b,bone.parent.matrixWorld,l);
        for(let i=0;i<16;i++)max=Math.max(max,Math.abs(l.elements[i]-bone.matrix.elements[i]));
      }return max;
    },
    serializable(schema){
      return {...schema,byId:undefined,bodies:schema.bodies.map(b=>({...b,boneToBody:b.boneToBody.toArray(),bodyToBone:b.bodyToBone.toArray(),bindWorld:b.bindWorld.toArray()}))};
    }
  };
})();
