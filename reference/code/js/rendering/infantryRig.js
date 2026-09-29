(function () {
  'use strict';
  const R = window.RTS;

  // One phenotype drives skeleton, surface and ragdoll dimensions. Coordinates
  // remain normalized to standing height H, then are converted to metres.
  R.InfantryAnatomy = {
    create(H, face = null, body = null) {
      if (!(H > 0) || !Number.isFinite(H)) throw new RangeError('Niepoprawny wzrost.');
      if(!face||!body){
        const neutral=R.InfantryGenome.express(R.InfantryGenome.applyVariation(R.InfantryGenome.create(0),0));
        face=face||neutral.face;body=body||neutral.body;
      }
      const f=face,b=body;
      const faceLayout=new R.FaceAnatomy(f,b);
      const points={},parents={},order=[];
      function joint(name,parent,x,y,z){points[name]=new THREE.Vector3(x*H,y*H,z*H);parents[name]=parent;order.push(name);}

      const hipY=b.hipY||.54;
      const neckY=faceLayout.neck.jointY;
      const headY=faceLayout.headPivotY;
      const chestY=R.Math.mix(hipY,neckY,.76);
      const spineUpperY=R.Math.mix(hipY,neckY,.56);
      const spineLowerY=R.Math.mix(hipY,neckY,.25);
      const shoulderY=R.Math.mix(chestY,neckY,.34);
      const shoulderHalf=.128*(b.shoulderWidthScale||1);
      const clavicleHalf=.050*(.75+.25*(b.shoulderWidthScale||1));
      const hipHalf=.052*(b.hipWidthScale||1);
      const thighY=hipY-.015;
      const ankleY=.045;
      const kneeY=ankleY+(thighY-ankleY)*.50;

      joint('hips',null,0,hipY,0);
      joint('spineLower','hips',0,spineLowerY,0);
      joint('spineUpper','spineLower',0,spineUpperY,0);
      joint('chest','spineUpper',0,chestY,0);
      joint('neck','chest',0,neckY,0);
      joint('head','neck',0,headY,0);

      const a=22*Math.PI/180,upperLen=.18*(b.armLengthScale||1),foreLen=.155*(b.armLengthScale||1);
      for(const [s,sign] of [['L',1],['R',-1]]){
        const ax=Math.sin(a)*sign,ay=-Math.cos(a);
        joint('clavicle.'+s,'chest',clavicleHalf*sign,shoulderY-.010,0);
        joint('upperArm.'+s,'clavicle.'+s,shoulderHalf*sign,shoulderY,0);
        joint('foreArm.'+s,'upperArm.'+s,shoulderHalf*sign+ax*upperLen,shoulderY+ay*upperLen,0);
        joint('hand.'+s,'foreArm.'+s,shoulderHalf*sign+ax*(upperLen+foreLen),shoulderY+ay*(upperLen+foreLen),0);
        joint('thigh.'+s,'hips',hipHalf*sign,thighY,0);
        joint('shin.'+s,'thigh.'+s,hipHalf*sign,kneeY,0);
        joint('foot.'+s,'shin.'+s,hipHalf*sign,ankleY,0);
        joint('toes.'+s,'foot.'+s,hipHalf*sign,.018,.085*(b.footScale||1));
      }

      // Face controls remain on the same skeleton so expression and body
      // animation compose without a second transform hierarchy.
      const F=faceLayout, mouth=F.mouth;
      joint('jaw','head',F.jaw.x,F.jaw.y,F.jaw.z);
      joint('mouthUpper','head',0,mouth.y+.0012,mouth.z+.0009);
      joint('mouthLower','jaw',0,mouth.y-.0012,mouth.z+.0009);
      for(const [s,sign] of [['L',1],['R',-1]]) {
        const e=F.eyes[s],br=F.brows[s];
        joint('eye.'+s,'head',e.x,e.y,e.z);
        // Eyelid controls are in socket space, NOT children of the rotating eyeball.
        joint('lidUpper.'+s,'head',e.x,e.y+e.h,F.globeFront(e,e.x,e.y+e.h));
        joint('lidLower.'+s,'head',e.x,e.y-e.h,F.globeFront(e,e.x,e.y-e.h));
        joint('browInner.'+s,'head',br.inner.x,br.inner.y,br.inner.z);
        joint('browOuter.'+s,'head',br.outer.x,br.outer.y,br.outer.z);
        const x=sign*mouth.w;
        joint('mouthCorner.'+s,'head',x,mouth.y+sign*f.mouthAsymmetry*.5,F.frontZ(x,mouth.y)+.001);
        const cx=e.x+sign*.006,cy=f.eyeY-.016;
        joint('cheek.'+s,'head',cx,cy,F.frontZ(cx,cy));
      }
      const anatomy = {height:H,face:f,body:b,faceLayout,points,parents,order,armAngle:a,hipY,neckY,shoulderY,shoulderHalf,hipHalf,legLength:(thighY-ankleY)*H,ankleHeight:ankleY*H,axes:{up:[0,1,0],forward:[0,0,1],kneeBend:[1,0,0],elbowBend:[-1,0,0]}};
      return R.HandAnatomy.append(anatomy);
    }
  };

  R.InfantryRig = class InfantryRig {
    constructor(anatomy, root) {
      this.anatomy=anatomy;this.root=root;this.bones=[];this.byName={};this.index={};this.rest=[];this.animationPositionIndices=[];this.animationQuaternionIndices=[];this.faceQuaternionMask=[];
      for(const name of anatomy.order){
        const bone=new THREE.Bone();bone.name=name;const parentName=anatomy.parents[name];bone.position.copy(anatomy.points[name]);
        if(parentName)bone.position.sub(anatomy.points[parentName]);(parentName?this.byName[parentName]:root).add(bone);
        const index=this.bones.length;this.index[name]=index;this.bones.push(bone);this.byName[name]=bone;this.rest.push({position:bone.position.clone(),quaternion:bone.quaternion.clone(),scale:bone.scale.clone(),length:bone.position.length()});
        // Body pose writes hips only. Facial translations belong exclusively
        // to FaceAnimator and must persist across independent body cadences.
        if(name==='hips')this.animationPositionIndices.push(index);
        const faceQuaternion=/^(eye|jaw|browInner|browOuter)\./.test(name);
        const fixedQuaternion=/^(cheek|mouthCorner)\.|^(mouthUpper|mouthLower)$/.test(name);
        this.faceQuaternionMask.push(faceQuaternion);
        if(!faceQuaternion&&!fixedQuaternion)this.animationQuaternionIndices.push(index);
      }
      root.updateMatrixWorld(true);this.skeleton=new THREE.Skeleton(this.bones);this.skeleton.calculateInverses();this._p=new THREE.Vector3();this._q=new THREE.Quaternion();this._r=new THREE.Quaternion();
    }
    reset(){this.bones.forEach((b,i)=>{b.position.copy(this.rest[i].position);b.quaternion.copy(this.rest[i].quaternion);b.scale.copy(this.rest[i].scale);});}
    resetPoseTransforms(){this.bones.forEach((b,i)=>{b.position.copy(this.rest[i].position);b.scale.copy(this.rest[i].scale);});}
    resetAnimationPoseTransforms(){for(const i of this.animationPositionIndices)this.bones[i].position.copy(this.rest[i].position);}
    restPosition(name,target=new THREE.Vector3()){const i=this.index[name];if(i===undefined)throw new Error('Nieznana kość '+name);return target.copy(this.rest[i].position);}
    modelPoint(name,target){this.byName[name].getWorldPosition(target);return this.root.worldToLocal(target);}
    setModelQuaternion(name,q,rootWorldQuaternion=null){const b=this.byName[name];if(rootWorldQuaternion)this._r.copy(rootWorldQuaternion);else this.root.getWorldQuaternion(this._r);this._r.multiply(q);b.parent.getWorldQuaternion(this._q);b.quaternion.copy(this._q.invert()).multiply(this._r);if(b.updateWorldMatrix)b.updateWorldMatrix(true,false);else b.updateMatrixWorld(true);}
    maxLengthError(){let error=0;this.bones.forEach((b,i)=>{const facial=/^(lidUpper|lidLower|browInner|browOuter|mouthUpper|mouthLower|mouthCorner|cheek)\./.test(b.name)||b.name==='mouthUpper'||b.name==='mouthLower';if(b.name!=='hips'&&!facial)error=Math.max(error,Math.abs(b.position.length()-this.rest[i].length));});return error;}
    dispose(){if(this.skeleton.dispose)this.skeleton.dispose();}
  };
})();
