(function () {
  'use strict';
  const R=window.RTS,V=(x=0,y=0,z=0)=>new THREE.Vector3(x,y,z),Z=V(0,0,1);
  const DEFAULT_POSE=[[.18,.32,.28],[.35,.65,.42]],PISTOL_POSE=[[.27,.48,.30],[.93,1.06,.65]],RELAXED_POSE=[[.10,.25,.20],[.38,.58,.40]];
  const POSES={
    KNIFE:[[.28,.46,.25],[.92,1.02,.58]],CYLINDER:[[.22,.50,.34],[.80,.92,.52]],
    PISTOL_SUPPORT:[[.12,.25,.17],[.63,.82,.51]],SPHERE:[[.30,.47,.30],[.72,.80,.42]],
    HANDLE:[[.30,.50,.30],[.98,1.05,.64]],SUPPORT:[[.28,.40,.24],[.78,.85,.50]]
  };
  // Append only: the original 39 body/face bone indices are never renumbered.
  R.HandAnatomy={
    append(A) {
      A.fingers={}; A.handFrames={};
      const H=A.height,hs=A.body.handScale||1;
      for(const [side,sign] of [['L',1],['R',-1]]) {
        const wrist=A.points['hand.'+side],dir=wrist.clone().sub(A.points['foreArm.'+side]).normalize();
        const u=dir.clone().cross(Z).normalize(),frame=new THREE.Matrix4().makeBasis(u,dir,Z);
        A.handFrames[side]={quaternion:new THREE.Quaternion().setFromRotationMatrix(frame),
          offset:dir.clone().multiplyScalar(.040*hs*H).addScaledVector(Z,.008*hs*H)};
        const lengths=side==='L'?[.029,.039,.042,.033]:[.033,.042,.039,.029];
        const names=side==='L'?['little','ring','middle','index']:['index','middle','ring','little'];
        const digits=[];
        for(let f=0;f<5;f++) {
          const thumb=f===4,axis=dir.clone().addScaledVector(Z,thumb?.22:.08);
          if(thumb)axis.addScaledVector(u,sign*.75);axis.normalize();
          const start=wrist.clone().addScaledVector(dir,(thumb?.024:.052)*hs*H)
            .addScaledVector(u,(thumb?sign*.017:(f-1.5)*.0101)*hs*H);
          const len=(thumb?.034:lengths[f])*hs*H;
          const name=thumb?'thumb':names[f],curlAxis=axis.clone().cross(Z).normalize();
          const bones=[0,1,2].map(i=>'finger.'+side+'.'+name+'.'+i);
          for(let j=0;j<3;j++) {
            A.points[bones[j]]=start.clone().addScaledVector(axis,[0,.32,.65][j]*len);
            A.parents[bones[j]]=j?bones[j-1]:'hand.'+side;A.order.push(bones[j]);
          }
          digits.push({name,bones,start,axis,curlAxis,length:len});
        }
        A.fingers[side]=digits;
      }
      return A;
    },
    weights(d,t) {
      const weights={};
      if(t<.26)weights[d.bones[0]]=1;
      else if(t<.38){const a=(t-.26)/.12;weights[d.bones[0]]=1-a;weights[d.bones[1]]=a;}
      else if(t<.59)weights[d.bones[1]]=1;
      else if(t<.71){const a=(t-.59)/.12;weights[d.bones[1]]=1-a;weights[d.bones[2]]=a;}
      else weights[d.bones[2]]=1;
      return weights;
    }
  };
  R.HandPose={
    apply(rig,side,profile,weight,freeCurl=0,trigger=0) {
      const digits=rig.anatomy.fingers?.[side];if(!digits)return;
      const q=rig._handPoseQuaternion||(rig._handPoseQuaternion=new THREE.Quaternion()),t=R.Math.clamp(weight,0,1),pose=POSES[profile];
      for(const d of digits) {
        const thumb=d.name==='thumb',index=d.name==='index';
        const angles=pose?pose[thumb?0:1]:profile==='PISTOL'?PISTOL_POSE[thumb?0:1]:DEFAULT_POSE[thumb?0:1];
        for(let j=0;j<3;j++) {
          const closed=profile==='PISTOL'&&index?j===0?.10+.62*trigger:j===1?.12+.75*trigger:.10+.40*trigger:angles[j];
          const relaxed=RELAXED_POSE[thumb?0:1][j]*freeCurl;
          q.setFromAxisAngle(d.curlAxis,R.Math.mix(relaxed,closed,t));
          rig.byName[d.bones[j]].quaternion.copy(q);
        }
      }
    }
  };
})();
