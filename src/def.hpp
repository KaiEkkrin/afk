/* AFK
 * Copyright (C) 2013, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#ifndef _AFK_DEF_H_
#define _AFK_DEF_H_

#include <cmath>
#include <sstream>

#include <boost/static_assert.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_constructor.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#define SQUARE(x) ((x) * (x))
#define CUBE(x) ((x) * (x) * (x))

template<typename F>
class Vec2
{
public:
    F v[2];

    bool operator==(const Vec2<F>& p) const
    {
        return v[0] == p.v[0] && v[1] == p.v[1];
    }

    bool operator!=(const Vec2<F>& p) const
    {
        return v[0] != p.v[0] || v[1] != p.v[1];
    }

    Vec2<F> operator+(const Vec2<F>& p) const
    {
        Vec2<F> r;
        r.v[0] = v[0] + p.v[0]; r.v[1] = v[1] + p.v[1];
        return r;
    }

    Vec2<F> operator+=(const Vec2<F>& p)
    {
        v[0] += p.v[0]; v[1] += p.v[1];
        return *this;
    }

    Vec2<F> operator*(F f) const
    {
        Vec2<F> r;
        r.v[0] = v[0] * f; r.v[1] = v[1] * f;
        return r;
    }
};

template<typename F>
Vec2<F> afk_vec2(const Vec2<F>& o)
{
    Vec2<F> v;
    v.v[0] = o.v[0]; v.v[1] = o.v[1];
    return v;
}

template<typename F>
Vec2<F> afk_vec2(F e0, F e1)
{
    Vec2<F> v;
    v.v[0] = e0; v.v[1] = e1;
    return v;
}

template<typename F>
std::ostream& operator<<(std::ostream& os, const Vec2<F>& v)
{
    return os << "(" << v.v[0] << ", " << v.v[1] << ")";
}

BOOST_STATIC_ASSERT((boost::has_trivial_assign<Vec2<float> >::value));
BOOST_STATIC_ASSERT((boost::has_trivial_constructor<Vec2<float> >::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<Vec2<float> >::value));

template<typename F>
class Vec3
{
public:
    /* use a storage space of 4, to match OpenCL's
     * 3-vectors.
     */
    F v[4];

    bool operator==(const Vec3<F>& p) const
    {
        return v[0] == p.v[0] && v[1] == p.v[1] && v[2] == p.v[2];
    }

    bool operator!=(const Vec3<F>& p) const
    {
        return v[0] != p.v[0] || v[1] != p.v[1] || v[2] != p.v[2];
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

    Vec3<F> operator-(void) const
    {
        Vec3<F> r;
        r.v[0] = -v[0]; r.v[1] = -v[1]; r.v[2] = -v[2];
        return r;
    }

    Vec3<F> operator-(F f) const
    {
        Vec3<F> r;
        r.v[0] = v[0] - f; r.v[1] = v[1] - f; r.v[2] = v[2] - f;
        return r;
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

    /* Convenient const squishage of the above. */
    void magnitudeAndDirection(float& o_magnitude, Vec3<F>& o_direction) const
    {
        o_magnitude = magnitude();
        o_direction.v[0] = v[0] / o_magnitude;
        o_direction.v[1] = v[1] / o_magnitude;
        o_direction.v[2] = v[2] / o_magnitude;
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

    Vec4<F> operator*(F f) const
    {
        Vec4<F> r;
        r.v[0] = v[0] * f; r.v[1] = v[1] * f; r.v[2] = v[2] * f; r.v[3] = v[3] * f;
        return r;
    }

    Vec4<F> operator/(F f) const
    {
        Vec4<F> r;
        r.v[0] = v[0] / f; r.v[1] = v[1] / f; r.v[2] = v[2] / f; r.v[3] = v[3] / f;
        return r;
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

template<typename F>
Vec4<F> afk_vec4(const Vec3<F>& o, F e3)
{
    Vec4<F> v;
    v.v[0] = o.v[0]; v.v[1] = o.v[1]; v.v[2] = o.v[2]; v.v[3] = e3;
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

template<typename F>
class Mat4
{
public:
    F m[4][4];

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
        Mat4<F> r;
        r.m[0][0] = m[0][0] * p.m[0][0] + m[0][1] * p.m[1][0] + m[0][2] * p.m[2][0] + m[0][3] * p.m[3][0];
        r.m[0][1] = m[0][0] * p.m[0][1] + m[0][1] * p.m[1][1] + m[0][2] * p.m[2][1] + m[0][3] * p.m[3][1];
        r.m[0][2] = m[0][0] * p.m[0][2] + m[0][1] * p.m[1][2] + m[0][2] * p.m[2][2] + m[0][3] * p.m[3][2];
        r.m[0][3] = m[0][0] * p.m[0][3] + m[0][1] * p.m[1][3] + m[0][2] * p.m[2][3] + m[0][3] * p.m[3][3];

        r.m[1][0] = m[1][0] * p.m[0][0] + m[1][1] * p.m[1][0] + m[1][2] * p.m[2][0] + m[1][3] * p.m[3][0];
        r.m[1][1] = m[1][0] * p.m[0][1] + m[1][1] * p.m[1][1] + m[1][2] * p.m[2][1] + m[1][3] * p.m[3][1];
        r.m[1][2] = m[1][0] * p.m[0][2] + m[1][1] * p.m[1][2] + m[1][2] * p.m[2][2] + m[1][3] * p.m[3][2];
        r.m[1][3] = m[1][0] * p.m[0][3] + m[1][1] * p.m[1][3] + m[1][2] * p.m[2][3] + m[1][3] * p.m[3][3];

        r.m[2][0] = m[2][0] * p.m[0][0] + m[2][1] * p.m[1][0] + m[2][2] * p.m[2][0] + m[2][3] * p.m[3][0];
        r.m[2][1] = m[2][0] * p.m[0][1] + m[2][1] * p.m[1][1] + m[2][2] * p.m[2][1] + m[2][3] * p.m[3][1];
        r.m[2][2] = m[2][0] * p.m[0][2] + m[2][1] * p.m[1][2] + m[2][2] * p.m[2][2] + m[2][3] * p.m[3][2];
        r.m[2][3] = m[2][0] * p.m[0][3] + m[2][1] * p.m[1][3] + m[2][2] * p.m[2][3] + m[2][3] * p.m[3][3];

        r.m[3][0] = m[3][0] * p.m[0][0] + m[3][1] * p.m[1][0] + m[3][2] * p.m[2][0] + m[3][3] * p.m[3][0];
        r.m[3][1] = m[3][0] * p.m[0][1] + m[3][1] * p.m[1][1] + m[3][2] * p.m[2][1] + m[3][3] * p.m[3][1];
        r.m[3][2] = m[3][0] * p.m[0][2] + m[3][1] * p.m[1][2] + m[3][2] * p.m[2][2] + m[3][3] * p.m[3][2];
        r.m[3][3] = m[3][0] * p.m[0][3] + m[3][1] * p.m[1][3] + m[3][2] * p.m[2][3] + m[3][3] * p.m[3][3];

        return r;
    }
};

template<typename F>
Mat4<F> afk_mat4(F e00, F e01, F e02, F e03,
                 F e10, F e11, F e12, F e13,
                 F e20, F e21, F e22, F e23,
                 F e30, F e31, F e32, F e33)
{
    Mat4<F> m;
    m.m[0][0] = e00; m.m[0][1] = e01; m.m[0][2] = e02; m.m[0][3] = e03;
    m.m[1][0] = e10; m.m[1][1] = e11; m.m[1][2] = e12; m.m[1][3] = e13;
    m.m[2][0] = e20; m.m[2][1] = e21; m.m[2][2] = e22; m.m[2][3] = e23;
    m.m[3][0] = e30; m.m[3][1] = e31; m.m[3][2] = e32; m.m[3][3] = e33;
    return m;
}

template<typename F>
std::ostream& operator<<(std::ostream& os, const Mat4<F>& m)
{
    os << std::dec;

    for (unsigned int row = 0; row < 4; ++row)
    {
        for (unsigned int col = 0; col < 4; ++col)
        {
            os << m.m[row][col] << "\t";
        }

        os << std::endl;
    }

    return os;
}

BOOST_STATIC_ASSERT((boost::has_trivial_assign<Mat4<float> >::value));
BOOST_STATIC_ASSERT((boost::has_trivial_constructor<Mat4<float> >::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<Mat4<float> >::value));

/* Quaternion: see
 * http://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation
 * etc.
 */

template<typename F>
class Quaternion
{
public:
    F q[4]; /* real, i, j, k */

    Quaternion<F> operator*(const F f) const
    {
        Quaternion<F> p;
        p.q[0] = q[0] * f;
        p.q[1] = q[1] * f;
        p.q[2] = q[2] * f;
        p.q[3] = q[3] * f;
        return p;
    }

    /* Hamilton product. http://en.wikipedia.org/wiki/Hamilton_product#Hamilton_product */
    Quaternion<F> operator*(const Quaternion<F>& o) const
    {
        Quaternion<F> h;
        h.q[0] = q[0] * o.q[0] - q[1] * o.q[1] - q[2] * o.q[2] - q[3] * o.q[3];
        h.q[1] = q[0] * o.q[1] + q[1] * o.q[0] + q[2] * o.q[3] - q[3] * o.q[2];
        h.q[2] = q[0] * o.q[2] - q[1] * o.q[3] + q[2] * o.q[0] + q[3] * o.q[1];
        h.q[3] = q[0] * o.q[3] + q[1] * o.q[2] - q[2] * o.q[1] + q[3] * o.q[0];
        return h;
    }

    Quaternion<F> operator/(const F f) const
    {
        Quaternion<F> p;
        p.q[0] = q[0] / f;
        p.q[1] = q[1] / f;
        p.q[2] = q[2] / f;
        p.q[3] = q[3] / f;
        return p;
    }

    Quaternion<F> conjugate(void) const
    {
        Quaternion<F> c;
        c.q[0] = q[0];
        c.q[1] = -q[1];
        c.q[2] = -q[2];
        c.q[3] = -q[3];
        return c;
    }

    Quaternion<F> inverse(void) const
    {
        Quaternion<F> i;
        i.q[0] = -q[0];
        i.q[1] = q[1];
        i.q[2] = q[2];
        i.q[3] = q[3];
        return i;
    }

    F normSquared(void) const
    {
        return SQUARE(q[0]) + SQUARE(q[1]) + SQUARE(q[2]) + SQUARE(q[3]);
    }

    F norm(void) const
    {
        return sqrt(normSquared());
    }
    
    Quaternion<F> reciprocal(void) const
    {
        return conjugate() / normSquared();
    }

    Vec3<F> rotate(const Vec3<F>& v) const
    {
        Quaternion<F> vq;
        vq.q[0] = 0.0f;
        vq.q[1] = v.v[0];
        vq.q[2] = v.v[1];
        vq.q[3] = v.v[2];

        Quaternion<F> rq = *this * vq * conjugate();
        Vec3<F> r;
        r.v[0] = rq.q[1];
        r.v[1] = rq.q[2];
        r.v[2] = rq.q[3];
        return r;
    }

    /* From http://en.wikipedia.org/wiki/Rotation_matrix#Quaternion . */
    Mat4<F> rotationMatrix(void) const
    {
        Mat4<F> r;
        r.m[0][0] = 1.0f - 2.0f * SQUARE(q[2]) - 2.0f * SQUARE(q[3]);
        r.m[0][1] = 2.0f * q[1] * q[2] - 2.0f * q[3] * q[0];
        r.m[0][2] = 2.0f * q[1] * q[3] + 2.0f * q[2] * q[0];
        r.m[0][3] = 0.0f;

        r.m[1][0] = 2.0f * q[1] * q[2] + 2.0f * q[3] * q[0];
        r.m[1][1] = 1.0f - 2.0f * SQUARE(q[1]) - 2.0f * SQUARE(q[3]);
        r.m[1][2] = 2.0f * q[2] * q[3] - 2.0f * q[1] * q[0];
        r.m[1][3] = 0.0f;

        r.m[2][0] = 2.0f * q[1] * q[3] - 2.0f * q[2] * q[0];
        r.m[2][1] = 2.0f * q[2] * q[3] + 2.0f * q[1] * q[0];
        r.m[2][2] = 1.0f - 2.0f * SQUARE(q[1]) - 2.0f * SQUARE(q[2]);
        r.m[2][3] = 0.0f;

        r.m[3][0] = 0.0f;
        r.m[3][1] = 0.0f;
        r.m[3][2] = 0.0f;
        r.m[3][3] = 1.0f;
        return r;
    }
};

/* Creates a rotation-describing quaternion */
template<typename F>
Quaternion<F> afk_quaternion(F theta, Vec3<F> uvec)
{
    uvec.normalise();

    Quaternion<F> r;
    r.q[0] = std::cos(theta / 2.0f);

    F sinHalfTheta = std::sin(theta / 2.0f);
    r.q[1] = uvec.v[0] * sinHalfTheta;
    r.q[2] = uvec.v[1] * sinHalfTheta;
    r.q[3] = uvec.v[2] * sinHalfTheta;

    return r;
}

BOOST_STATIC_ASSERT((boost::has_trivial_assign<Quaternion<float> >::value));
BOOST_STATIC_ASSERT((boost::has_trivial_constructor<Quaternion<float> >::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<Quaternion<float> >::value));

#endif /* _AFK_DEF_H_ */

