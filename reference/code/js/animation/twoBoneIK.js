(function () {
  'use strict';
  const R = window.RTS;
  R.TwoBoneIK = class TwoBoneIK {
    constructor(rig) {
      this.rig = rig;
      for (const name of ['start','axis','bend','joint','end','dir','x','y','z','restY','restZ','restX']) this[name] = new THREE.Vector3();
      this.q = new THREE.Quaternion();
      this.rootWorldQuaternion=new THREE.Quaternion();
      this.frame = new THREE.Matrix4(); this.restFrame = new THREE.Matrix4();
      this.maxTargetError = 0; this.lastTargetError = 0; this.lengths=Object.create(null);this.restOrientations=Object.create(null);
      this.lastForward = Object.create(null);
    }
    orient(name, restDirection, direction, fullFrame, rootWorldQuaternion=null) {
      let rest=this.restOrientations[name];
      if(!rest) {
        // The normalized rest direction lives in the per-chain length cache and
        // is immutable for this rig. Keep the reference instead of cloning it.
        rest={direction:restDirection,inverse:null};
        this.restOrientations[name]=rest;
      }
      if(fullFrame&&!rest.inverse) {
        const y=new THREE.Vector3().copy(rest.direction).multiplyScalar(-1),z=new THREE.Vector3().set(0,0,1).addScaledVector(y,-y.z).normalize(),x=new THREE.Vector3().crossVectors(y,z).normalize();
        z.crossVectors(x,y).normalize();rest.inverse=new THREE.Matrix4().makeBasis(x,y,z).invert();
      }
      if (!fullFrame) {
        this.q.setFromUnitVectors(rest.direction, direction);
      } else {
        // Resolve twist as well as segment direction. Local +Z follows the
        // bend plane, so in prone the calf AND boot turn toward the same side.
        this.y.copy(direction).multiplyScalar(-1);
        const forward=this.orientationHint||this.bend;
        this.z.copy(forward).addScaledVector(this.y, -forward.dot(this.y));
        if (this.z.lengthSq() < 1e-8) {
          if(this.lastForward[name])this.z.copy(this.lastForward[name]);else this.z.set(0,0,1);
          this.z.addScaledVector(this.y, -this.z.dot(this.y));
        }
        if (this.z.lengthSq() < 1e-8) {
          this.z.set(1,0,0).addScaledVector(this.y, -this.y.x);
        }
        this.z.normalize(); this.x.crossVectors(this.y, this.z).normalize();
        this.z.crossVectors(this.x, this.y).normalize();
        this.frame.makeBasis(this.x, this.y, this.z);
        this.frame.multiply(rest.inverse);
        this.q.setFromRotationMatrix(this.frame).normalize();
        if (!this.lastForward[name]) this.lastForward[name] = new THREE.Vector3();
        this.lastForward[name].copy(this.z);
      }
      this.rig.setModelQuaternion(name, this.q, rootWorldQuaternion);
    }
    solve(upper, lower, tip, target, pole, fullFrame = false, orientationHint = null) {
      this.orientationHint=orientationHint;
      const rig = this.rig, rest = rig.anatomy.points;
      let lengths=this.lengths[upper];
      if(!lengths||lengths.lower!==lower||lengths.tip!==tip) {
        const upperDirection=rest[lower].clone().sub(rest[upper]).normalize(),lowerDirection=rest[tip].clone().sub(rest[lower]).normalize();
        lengths=this.lengths[upper]={lower,tip,l1:rest[upper].distanceTo(rest[lower]),l2:rest[lower].distanceTo(rest[tip]),upperDirection,lowerDirection};
      }
      const l1=lengths.l1,l2=lengths.l2;
      rig.modelPoint(upper, this.start);
      this.axis.copy(target).sub(this.start);
      const requested = this.axis.length();
      if (!Number.isFinite(requested)) throw new RangeError('Niepoprawny cel IK: '+tip);
      if (requested < 1e-9) this.axis.set(0,-1,0); else this.axis.multiplyScalar(1/requested);
      const d = R.Math.clamp(requested, Math.abs(l1-l2)+1e-5, l1+l2-1e-6);
      this.lastTargetError = Math.abs(requested-d);
      this.maxTargetError = Math.max(this.maxTargetError, this.lastTargetError);
      this.end.copy(this.start).addScaledVector(this.axis, d);
      this.bend.copy(pole).sub(this.start).addScaledVector(this.axis, -this.bend.dot(this.axis));
      if (this.bend.lengthSq()<1e-10) {
        this.bend.set(0,0,1).addScaledVector(this.axis, -this.axis.z);
      }
      if (this.bend.lengthSq()<1e-10) this.bend.set(1,0,0).addScaledVector(this.axis, -this.axis.x);
      this.bend.normalize();
      const along = (l1*l1+d*d-l2*l2)/(2*d), height = Math.sqrt(Math.max(0,l1*l1-along*along));
      this.joint.copy(this.start).addScaledVector(this.axis,along).addScaledVector(this.bend,height);
      const rootWorldQuaternion=rig.root&&rig.root.getWorldQuaternion?rig.root.getWorldQuaternion(this.rootWorldQuaternion):null;
      this.dir.copy(this.joint).sub(this.start).normalize();
      this.orient(upper, lengths.upperDirection, this.dir, fullFrame,rootWorldQuaternion);
      this.dir.copy(this.end).sub(this.joint).normalize();
      this.orient(lower, lengths.lowerDirection, this.dir, fullFrame,rootWorldQuaternion);
      return this.lastTargetError;
    }
  };
})();
