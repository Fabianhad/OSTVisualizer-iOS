#ifndef MATHUTILS_HPP
#define MATHUTILS_HPP

#include <cmath>

struct Vec3
{
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3 &b) const { return {x + b.x, y + b.y, z + b.z}; }
    Vec3 operator-(const Vec3 &b) const { return {x - b.x, y - b.y, z - b.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float dot(const Vec3 &b) const { return x * b.x + y * b.y + z * b.z; }
    float length() const { return sqrtf(x * x + y * y + z * z); }

    Vec3 cross(const Vec3 &b) const
    {
        return {y * b.z - z * b.y,
                z * b.x - x * b.z,
                x * b.y - y * b.x};
    }

    Vec3 normalized() const
    {
        float len = length();
        if (len < 1e-8f)
            return {0, 0, 0};
        float inv = 1.0f / len;
        return {x * inv, y * inv, z * inv};
    }
};

struct Vec4
{
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(Vec3 v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

    Vec3 perspDiv() const
    {
        if (fabsf(w) < 1e-8f)
            return {x, y, z};
        float inv = 1.0f / w;
        return {x * inv, y * inv, z * inv};
    }
};

struct Mat4
{
    float m[16];

    Mat4()
    {
        for (int i = 0; i < 16; i++)
            m[i] = 0;
    }

    static Mat4 identity()
    {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
    float &at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }

    Mat4 operator*(const Mat4 &b) const
    {
        Mat4 r;
        for (int c = 0; c < 4; c++)
            for (int row = 0; row < 4; row++)
            {
                float sum = 0;
                for (int k = 0; k < 4; k++)
                    sum += at(row, k) * b.at(k, c);
                r.at(row, c) = sum;
            }
        return r;
    }

    Vec4 operator*(const Vec4 &v) const
    {
        return {
            at(0, 0) * v.x + at(0, 1) * v.y + at(0, 2) * v.z + at(0, 3) * v.w,
            at(1, 0) * v.x + at(1, 1) * v.y + at(1, 2) * v.z + at(1, 3) * v.w,
            at(2, 0) * v.x + at(2, 1) * v.y + at(2, 2) * v.z + at(2, 3) * v.w,
            at(3, 0) * v.x + at(3, 1) * v.y + at(3, 2) * v.z + at(3, 3) * v.w};
    }
    Vec3 transformDir(const Vec3 &v) const
    {
        return {
            at(0, 0) * v.x + at(0, 1) * v.y + at(0, 2) * v.z,
            at(1, 0) * v.x + at(1, 1) * v.y + at(1, 2) * v.z,
            at(2, 0) * v.x + at(2, 1) * v.y + at(2, 2) * v.z};
    }

    static Mat4 translation(float tx, float ty, float tz)
    {
        Mat4 r = identity();
        r.at(0, 3) = tx;
        r.at(1, 3) = ty;
        r.at(2, 3) = tz;
        return r;
    }

    static Mat4 scale(float sx, float sy, float sz)
    {
        Mat4 r;
        r.at(0, 0) = sx;
        r.at(1, 1) = sy;
        r.at(2, 2) = sz;
        r.at(3, 3) = 1;
        return r;
    }

    static Mat4 rotationX(float rad)
    {
        Mat4 r = identity();
        float c = cosf(rad), s = sinf(rad);
        r.at(1, 1) = c;
        r.at(1, 2) = -s;
        r.at(2, 1) = s;
        r.at(2, 2) = c;
        return r;
    }

    static Mat4 rotationY(float rad)
    {
        Mat4 r = identity();
        float c = cosf(rad), s = sinf(rad);
        r.at(0, 0) = c;
        r.at(0, 2) = s;
        r.at(2, 0) = -s;
        r.at(2, 2) = c;
        return r;
    }

    static Mat4 perspective(float fovY, float aspect, float near, float far)
    {
        Mat4 r;
        float tanHalf = tanf(fovY * 0.5f);
        r.at(0, 0) = 1.0f / (aspect * tanHalf);
        r.at(1, 1) = 1.0f / tanHalf;
        r.at(2, 2) = -(far + near) / (far - near);
        r.at(2, 3) = -(2.0f * far * near) / (far - near);
        r.at(3, 2) = -1.0f;
        return r;
    }
    Mat4 transposed() const
    {
        Mat4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r.m[i * 4 + j] = m[j * 4 + i];
        return r;
    }

    static Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up)
    {
        Vec3 f = (target - eye).normalized();
        Vec3 r = f.cross(up).normalized();
        Vec3 u = r.cross(f);

        Mat4 m = identity();
        m.at(0, 0) = r.x;
        m.at(0, 1) = r.y;
        m.at(0, 2) = r.z;
        m.at(1, 0) = u.x;
        m.at(1, 1) = u.y;
        m.at(1, 2) = u.z;
        m.at(2, 0) = -f.x;
        m.at(2, 1) = -f.y;
        m.at(2, 2) = -f.z;
        m.at(0, 3) = -r.dot(eye);
        m.at(1, 3) = -u.dot(eye);
        m.at(2, 3) = f.dot(eye);
        return m;
    }
};

struct Color
{
    float r, g, b, a;
    Color() : r(0), g(0), b(0), a(1) {}
    Color(float r, float g, float b) : r(r), g(g), b(b), a(1) {}
    Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

    bool is_transparent() const { return a < 0.99f; }
};

#endif
