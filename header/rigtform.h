#ifndef RIGTFORM_H
#define RIGTFORM_H

#include <iostream>
#include <cassert>

#include "matrix4.h"
#include "quat.h"

class RigTForm {
  Cvec3 t_; // translation component
  Quat r_;  // rotation component represented as a quaternion

public:
  RigTForm() : t_(0) {
    assert(norm2(Quat(1,0,0,0) - r_) < CS175_EPS2);
  }

  RigTForm(const Cvec3& t, const Quat& r) {
    t_ = t;
    r_ = r;
  }

  explicit RigTForm(const Cvec3& t) {
    t_ = t;
  }

  explicit RigTForm(const Quat& r) {
    r_ = r;
  }

  Cvec3 getTranslation() const {
    return t_;
  }

  Quat getRotation() const {
    return r_;
  }

  RigTForm& setTranslation(const Cvec3& t) {
    t_ = t;
    return *this;
  }

  RigTForm& setRotation(const Quat& r) {
    r_ = r;
    return *this;
  }

  Cvec4 operator * (const Cvec4& a) const {
    return Cvec4( (r_*Cvec3(a)+t_)*a[3], a[3] );
  }

  RigTForm operator * (const RigTForm& a) const {
    return RigTForm(t_+r_*a.t_, r_*a.r_);
  }
};

inline RigTForm inv(const RigTForm& tform) {
  return RigTForm(inv(tform.getRotation())*tform.getTranslation()*(-1), inv(tform.getRotation()));
}

inline RigTForm transFact(const RigTForm& tform) {
  return RigTForm(tform.getTranslation());
}

inline RigTForm linFact(const RigTForm& tform) {
  return RigTForm(tform.getRotation());
}

inline Matrix4 rigTFormToMatrix(const RigTForm& tform) {
  return Matrix4::makeTranslation(tform.getTranslation())*quatToMatrix(tform.getRotation());
  // return m;
}

inline RigTForm slerp(double alpha, RigTForm f1, RigTForm f2, RigTForm f_1, RigTForm f_2) {
    Cvec3 t1 = f1.getTranslation();
    Cvec3 t2 = f2.getTranslation();
    Cvec3 t_1 = f_1.getTranslation();
    Cvec3 t_2 = f_2.getTranslation();
    Cvec3 t_d = (t2 - t_1) / 6 + t1;
    Cvec3 t_e = (t_2 - t1) / (-6) + t2;
    Cvec3 t_a = t1 * (1 - alpha) * (1 - alpha) * (1 - alpha)
                + t_d * 3 * alpha * (1 - alpha) * (1 - alpha)
                + t_e * 3 * alpha * alpha * (1 - alpha)
                + t2 * alpha * alpha * alpha;

    Quat r1 = f1.getRotation();
    Quat r2 = f2.getRotation();
    Quat r_1 = f_1.getRotation();
    Quat r_2 = f_2.getRotation();
    Quat r_d = powerq(r2 * inv(r_1), double(1) / 6) * r1;
    Quat r_e = powerq(r_2 * inv(r1), -double(1) / 6) * r2;
    Cvec4 sk = quat2sk(r1) * (1 - alpha) * (1 - alpha) * (1 - alpha)
               + quat2sk(r_d) * 3 * alpha * (1 - alpha) * (1 - alpha)
               + quat2sk(r_e) * 3 * alpha * alpha * (1 - alpha)
               + quat2sk(r2) * alpha * alpha * alpha;
    Quat r_a = sk2quat(sk);
    return RigTForm(t_a, r_a);
}

#endif