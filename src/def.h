/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DEF_H_
#define _AFK_DEF_H_

template<class F>
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

    Mat4<F> operator*(Mat4<F> p)
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

