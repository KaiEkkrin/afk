/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DEF_H_
#define _AFK_DEF_H_

#include <math.h>
#include <sstream>

#include <boost/static_assert.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_constructor.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#define SQUARE(x) ((x) * (x))
#define CUBE(x) ((x) * (x) * (x))

template<typename F>
class Vec3
{
public:
    F v[3];

    /* TODO remove these, replace with directly embedded Vec3 and
     * memcpy() */
    Vec3<F>& fromArray(const F* a)
    {
        v[0] = *a;
        v[1] = *(a+1);
        v[2] = *(a+2);
        return *this;
    }

    void toArray(F* a) const
    {
        *a = v[0];
        *(a+1) = v[1];
        *(a+2) = v[2];
    }

    Vec3<F> operator+(F f) const
    {
        Vec3<F> r;
        r.v[0] = v[0] + f; r.v[1] = v[1] + f; r.v[2] = v[2] + f;
        return r;
    }

    Vec3<F> operator+(const Vec3<F>& p) const
    {
        Vec3<F> r;
        r.v[0] = v[0] + p.v[0]; r.v[1] = v[1] + p.v[1]; r.v[2] = v[2] + p.v[2];
        return r;
    }

    Vec3<F>& operator+=(const Vec3<F>& p)
    {
        v[0] += p.v[0]; v[1] += p.v[1]; v[2] += p.v[2];
        return *this;
    }

    Vec3<F> operator-(const Vec3<F>& p) const
    {
        Vec3<F> r;
        r.v[0] = v[0] - p.v[0]; r.v[1] = v[1] - p.v[1]; r.v[2] = v[2] - p.v[2];
        return r;
    }

    Vec3<F> operator*(F f) const
    {
        Vec3<F> r;
        r.v[0] = v[0] * f; r.v[1] = v[1] * f; r.v[2] = v[2] * f;
        return r;
    }

    Vec3<F> operator/(F f) const
    {
        Vec3<F> r;
        r.v[0] = v[0] / f; r.v[1] = v[1] / f; r.v[2] = v[2] / f;
        return r;
    }

    Vec3<F>& normalise()
    {
        F mag = magnitude();
        v[0] = v[0] / mag;
        v[1] = v[1] / mag;
        v[2] = v[2] / mag;
        return *this;
    }

    F magnitudeSquared(void) const
    {
        return SQUARE(v[0]) + SQUARE(v[1]) + SQUARE(v[2]);
    }

    F magnitude(void) const
    {
        return sqrt(magnitudeSquared());
    }

    F dot(const Vec3<F>& p) const
    {
        return v[0] * p.v[0] + v[1] * p.v[1] + v[2] * p.v[2];
    }

    Vec3<F> cross(const Vec3<F>& p) const
    {
        Vec3<F> r;
        r.v[0] = v[1] * p.v[2] - v[2] * p.v[1];
        r.v[1] = v[2] * p.v[0] - v[0] * p.v[2];
        r.v[2] = v[0] * p.v[1] - v[1] * p.v[0];
        return r;
    }
};

template<typename F>
Vec3<F> afk_vec3(const Vec3<F>& o)
{
    Vec3<F> v;
    v.v[0] = o.v[0]; v.v[1] = o.v[1]; v.v[2] = o.v[2];
    return v;
}

template<typename F>
Vec3<F> afk_vec3(F e0, F e1, F e2)
{
    Vec3<F> v;
    v.v[0] = e0; v.v[1] = e1; v.v[2] = e2;
    return v;
}

template<typename F>
std::ostream& operator<<(std::ostream& os, const Vec3<F>& v)
{
    return os << "(" << v.v[0] << ", " << v.v[1] << ", " << v.v[2] << ")";
}

BOOST_STATIC_ASSERT((boost::has_trivial_assign<Vec3<float> >::value));
BOOST_STATIC_ASSERT((boost::has_trivial_constructor<Vec3<float> >::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<Vec3<float> >::value));

template<typename F>
class Vec4
{
public:
    F v[4];

    bool operator==(const Vec4<F>& p) const
    {
        return v[0] == p.v[0] && v[1] == p.v[1] && v[2] == p.v[2] && v[3] == p.v[3];
    }

    bool operator!=(const Vec4<F>& p) const
    {
        return v[0] != p.v[0] || v[1] != p.v[1] || v[2] != p.v[2] || v[3] != p.v[3];
    }
};

template<typename F>
Vec4<F> afk_vec4(const Vec4<F>& o)
{
    Vec4<F> v;
    v.v[0] = o.v[0]; v.v[1] = o.v[1]; v.v[2] = o.v[2]; v.v[3] = o.v[3];
    return v;
}

template<typename F>
Vec4<F> afk_vec4(F e0, F e1, F e2, F e3)
{
    Vec4<F> v;
    v.v[0] = e0; v.v[1] = e1; v.v[2] = e2; v.v[3] = e3;
    return v;
}

BOOST_STATIC_ASSERT((boost::has_trivial_assign<Vec4<float> >::value));
BOOST_STATIC_ASSERT((boost::has_trivial_constructor<Vec4<float> >::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<Vec4<float> >::value));

template<typename F>
std::ostream& operator<<(std::ostream& os, const Vec4<F>& v)
{
    return os << "(" << v.v[0] << ", " << v.v[1] << ", " << v.v[2] << ", " << v.v[3] << ")";
}

/* This one isn't trivially copyable and assignable.  Make it so? */
template<typename F>
class Mat4
{
public:
    F m[4][4];

    Mat4() {}
    Mat4(F v00, F v01, F v02, F v03,
         F v10, F v11, F v12, F v13,
         F v20, F v21, F v22, F v23,
         F v30, F v31, F v32, F v33)
    {
        m[0][0] = v00; m[0][1] = v01; m[0][2] = v02; m[0][3] = v03;
        m[1][0] = v10; m[1][1] = v11; m[1][2] = v12; m[1][3] = v13;
        m[2][0] = v20; m[2][1] = v21; m[2][2] = v22; m[2][3] = v23;
        m[3][0] = v30; m[3][1] = v31; m[3][2] = v32; m[3][3] = v33;
    }

    Mat4<F>& operator=(const Mat4<F>& p)
    {
        m[0][0] = p.m[0][0]; m[0][1] = p.m[0][1]; m[0][2] = p.m[0][2]; m[0][3] = p.m[0][3];   
        m[1][0] = p.m[1][0]; m[1][1] = p.m[1][1]; m[1][2] = p.m[1][2]; m[1][3] = p.m[1][3];   
        m[2][0] = p.m[2][0]; m[2][1] = p.m[2][1]; m[2][2] = p.m[2][2]; m[2][3] = p.m[2][3];   
        m[3][0] = p.m[3][0]; m[3][1] = p.m[3][1]; m[3][2] = p.m[3][2]; m[3][3] = p.m[3][3];   
        return *this;
    }

    /* Assume we intend to convert that vector to homogeneous co-ordinates :) */
    Vec4<F> operator*(const Vec3<F>& p) const
    {
        return *this * afk_vec4<F>(p.v[0], p.v[1], p.v[2], 1.0f);
    }

    Vec4<F> operator*(const Vec4<F>& p) const
    {
        return afk_vec4<F>(
            m[0][0] * p.v[0] + m[0][1] * p.v[1] + m[0][2] * p.v[2] + m[0][3] * p.v[3],
            m[1][0] * p.v[0] + m[1][1] * p.v[1] + m[1][2] * p.v[2] + m[1][3] * p.v[3],
            m[2][0] * p.v[0] + m[2][1] * p.v[1] + m[2][2] * p.v[2] + m[2][3] * p.v[3],
            m[3][0] * p.v[0] + m[3][1] * p.v[1] + m[3][2] * p.v[2] + m[3][3] * p.v[3]
        );
    }

    Mat4<F> operator*(const Mat4<F>& p) const
    {
        return Mat4<F>(
            m[0][0] * p.m[0][0] + m[0][1] * p.m[1][0] + m[0][2] * p.m[2][0] + m[0][3] * p.m[3][0],
            m[0][0] * p.m[0][1] + m[0][1] * p.m[1][1] + m[0][2] * p.m[2][1] + m[0][3] * p.m[3][1],
            m[0][0] * p.m[0][2] + m[0][1] * p.m[1][2] + m[0][2] * p.m[2][2] + m[0][3] * p.m[3][2],
            m[0][0] * p.m[0][3] + m[0][1] * p.m[1][3] + m[0][2] * p.m[2][3] + m[0][3] * p.m[3][3],

            m[1][0] * p.m[0][0] + m[1][1] * p.m[1][0] + m[1][2] * p.m[2][0] + m[1][3] * p.m[3][0],
            m[1][0] * p.m[0][1] + m[1][1] * p.m[1][1] + m[1][2] * p.m[2][1] + m[1][3] * p.m[3][1],
            m[1][0] * p.m[0][2] + m[1][1] * p.m[1][2] + m[1][2] * p.m[2][2] + m[1][3] * p.m[3][2],
            m[1][0] * p.m[0][3] + m[1][1] * p.m[1][3] + m[1][2] * p.m[2][3] + m[1][3] * p.m[3][3],

            m[2][0] * p.m[0][0] + m[2][1] * p.m[1][0] + m[2][2] * p.m[2][0] + m[2][3] * p.m[3][0],
            m[2][0] * p.m[0][1] + m[2][1] * p.m[1][1] + m[2][2] * p.m[2][1] + m[2][3] * p.m[3][1],
            m[2][0] * p.m[0][2] + m[2][1] * p.m[1][2] + m[2][2] * p.m[2][2] + m[2][3] * p.m[3][2],
            m[2][0] * p.m[0][3] + m[2][1] * p.m[1][3] + m[2][2] * p.m[2][3] + m[2][3] * p.m[3][3],

            m[3][0] * p.m[0][0] + m[3][1] * p.m[1][0] + m[3][2] * p.m[2][0] + m[3][3] * p.m[3][0],
            m[3][0] * p.m[0][1] + m[3][1] * p.m[1][1] + m[3][2] * p.m[2][1] + m[3][3] * p.m[3][1],
            m[3][0] * p.m[0][2] + m[3][1] * p.m[1][2] + m[3][2] * p.m[2][2] + m[3][3] * p.m[3][2],
            m[3][0] * p.m[0][3] + m[3][1] * p.m[1][3] + m[3][2] * p.m[2][3] + m[3][3] * p.m[3][3]
        );
    }
};

#endif /* _AFK_DEF_H_ */

