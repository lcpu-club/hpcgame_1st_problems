#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <omp.h>
#include <string>
#include <atomic>
#include <chrono>

// #define double float

class Vector
{
public:
    double x, y, z;
    Vector(const double x = 0, const double y = 0, const double z = 0) : x(x), y(y), z(z) {}
    Vector(const Vector &) = default;
    Vector &operator=(const Vector &) = default;
    Vector operator+(const Vector &b) const { return Vector(x + b.x, y + b.y, z + b.z); }
    Vector operator-(const Vector &b) const { return Vector(x - b.x, y - b.y, z - b.z); }
    Vector operator-() const { return Vector(-x, -y, -z); }
    Vector operator*(const double s) const { return Vector(x * s, y * s, z * s); }
    Vector operator/(const double s) const { return Vector(x / s, y / s, z / s); }
    bool isZero() const { return (x == 0.) && (y == 0.) && (z == 0.); }
};
inline double LengthSquared(const Vector &v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
inline double Length(const Vector &v) { return std::sqrt(LengthSquared(v)); }
inline Vector operator*(const double s, const Vector &v) { return v * s; }
inline Vector Normalize(const Vector &v) { return v / Length(v); }
inline Vector Multiply(const Vector &v1, const Vector &v2) { return Vector(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z); }
inline double Dot(const Vector &v1, const Vector &v2) { return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z; }
inline const Vector Cross(const Vector &v1, const Vector &v2)
{
    return Vector((v1.y * v2.z) - (v1.z * v2.y), (v1.z * v2.x) - (v1.x * v2.z), (v1.x * v2.y) - (v1.y * v2.x));
}
using Color = Vector;
constexpr double pi_2 = 1.5707963267948966192313216916398;
constexpr int hemicube_res = 256;

double multiplier_front[hemicube_res][hemicube_res];
double multiplier_down[hemicube_res / 2][hemicube_res];

// prospective camera
class Camera
{
public:
    Vector pos, dir, up;
    Vector temp;
    double invw, invh;
    double fov, aspect_ratio;
    double width, height;
    Camera(Vector pos, Vector dir, Vector up, double fov, double aspect_ratio = 1.) : pos(pos), dir(dir), up(up), fov(fov), aspect_ratio(aspect_ratio)
    {
        height = 2 * std::tan(fov / 2);
        width = height * aspect_ratio;
        temp = Cross(up, dir);
        invw = 1.0 / width;
        invh = 1.0 / height;
    }
    Vector project(const Vector &v) const
    {
        Vector a = v - pos;
        double z = Dot(a, dir);
        double invz = 1.0 / z;
        double y = Dot(a, up);
        double x = Dot(a, temp);
        return Vector(-x * invz, y * invz, z);
    }
};

// Patches are rectangle
class Patch
{
public:
    Vector pos;
    Vector a, b; // regard Cross(b, a) as normal
    Color emission;
    Color reflectance;
    Color incident;
    Color excident;
    // std::vector<double> m;
    double *m;

    Patch(Vector pos, Vector a, Vector b, Color emission, Color reflectance) : pos(pos), a(a), b(b), emission(emission), reflectance(reflectance), incident(), excident(emission) {}
};

bool inside_quadrilateral(double cx, double cy, Vector a, Vector b, Vector c, Vector d, double &depth)
{
    Vector ab = b - a, ac = c - a;
    double tmp = ab.x * ac.y - ab.y * ac.x;
    if (std::fabs(tmp) > 1e-6)
    {
        double alpha = (-ac.x * (cy - a.y) + ac.y * (cx - a.x)) / tmp;
        double beta = (ab.x * (cy - a.y) - ab.y * (cx - a.x)) / tmp;
        if (alpha >= 0 && beta >= 0 && alpha + beta <= 1)
        {
            depth = alpha * b.z + beta * c.z + (1 - alpha - beta) * a.z;
            return true;
        }
    }
    Vector db = b - d, dc = c - d;
    tmp = db.x * dc.y - db.y * dc.x;
    if (std::fabs(tmp) > 1e-6)
    {
        double alpha = (-dc.x * (cy - d.y) + dc.y * (cx - d.x)) / tmp;
        double beta = (db.x * (cy - d.y) - db.y * (cx - d.x)) / tmp;
        if (alpha >= 0 && beta >= 0 && alpha + beta <= 1)
        {
            depth = alpha * b.z + beta * c.z + (1 - alpha - beta) * d.z;
            return true;
        }
    }
    return false;
}

double vtpx(const Camera &camera, const Vector &a, const int width, const int height)
{
    return (a.x * camera.invw + 0.5) * width;
}

double vtpy(const Camera &camera, const Vector &a, const int width, const int height)
{
    return (a.y * camera.invh + 0.5) * height;
}

void fragrender(const Camera &camera, const Patch &p, const int width, const int height, double *zbuffer, Vector *image, double xa, double xb, double ya, double yb)
{
    if (Length(p.pos + 0.5 * (p.a + p.b) - camera.pos) < 1e-6)
    {
        return;
    }

    Vector a = camera.project(p.pos);
    Vector b = camera.project(p.pos + p.a);
    Vector c = camera.project(p.pos + p.b);
    Vector d = camera.project(p.pos + p.a + p.b);

    if (a.z < 1e-6 || b.z < 1e-6 || c.z < 1e-6 || d.z < 1e-6)
    {
        return;
    }

    int xs = fmin(xb, fmax(xa, fmin(fmin(vtpx(camera, a, width, height), vtpx(camera, b, width, height)), fmin(vtpx(camera, c, width, height), vtpx(camera, d, width, height))) + 0.4375));
    int xe = fmin(xb, fmax(xa, fmax(fmax(vtpx(camera, a, width, height), vtpx(camera, b, width, height)), fmax(vtpx(camera, c, width, height), vtpx(camera, d, width, height))) + 0.5625));

    int ys = fmin(yb, fmax(ya, fmin(fmin(vtpy(camera, a, width, height), vtpy(camera, b, width, height)), fmin(vtpy(camera, c, width, height), vtpy(camera, d, width, height))) + 0.4375));
    int ye = fmin(yb, fmax(ya, fmax(fmax(vtpy(camera, a, width, height), vtpy(camera, b, width, height)), fmax(vtpy(camera, c, width, height), vtpy(camera, d, width, height))) + 0.5625));

    if (xs == xe || ys == ye)
    {
        return;
    }

    double px = camera.width / width;
    double py = camera.height / height;

    Vector ab = b - a, ac = c - a;
    double tmp1 = ab.x * ac.y - ab.y * ac.x;
    double invtmp1 = 1.0 / tmp1;
    double tmp1ax = ac.y * invtmp1;
    double tmp1ay = -ac.x * invtmp1;
    double tmp1ac = (ac.x * a.y - ac.y * a.x) * invtmp1;
    double tmp1bx = -ab.y * invtmp1;
    double tmp1by = ab.x * invtmp1;
    double tmp1bc = (-ab.x * a.y + ab.y * a.x) * invtmp1;

    Vector db = b - d, dc = c - d;
    double tmp2 = db.x * dc.y - db.y * dc.x;
    double invtmp2 = 1.0 / tmp2;
    double tmp2ax = dc.y * invtmp2;
    double tmp2ay = -dc.x * invtmp2;
    double tmp2ac = (dc.x * d.y - dc.y * d.x) * invtmp2;
    double tmp2bx = -db.y * invtmp2;
    double tmp2by = db.x * invtmp2;
    double tmp2bc = (-db.x * d.y + db.y * d.x) * invtmp2;

    for (int i = ys; i < ye; i++)
    {
        for (int j = xs; j < xe; j++)
        {
            double cx = (j + 0.5) * px - camera.width * 0.5;
            double cy = (i + 0.5) * py - camera.height * 0.5;

            if (fabs(tmp1) > 1e-6)
            {
                double alpha = tmp1ax * cx + tmp1ay * cy + tmp1ac;
                double beta = tmp1bx * cx + tmp1by * cy + tmp1bc;
                double omab = 1 - alpha - beta;
                if (alpha >= 0 && beta >= 0 && omab >= 0)
                {
                    double depth = alpha * b.z + beta * c.z + omab * a.z;
                    if (depth < zbuffer[i * width + j] && depth > 1e-6)
                    {
                        zbuffer[i * width + j] = depth;
                        image[i * width + j] = p.excident;
                    }
                    continue;
                }
            }

            if (fabs(tmp2) > 1e-6)
            {
                double alpha = tmp2ax * cx + tmp2ay * cy + tmp2ac;
                double beta = tmp2bx * cx + tmp2by * cy + tmp2bc;
                double omab = 1 - alpha - beta;
                if (alpha >= 0 && beta >= 0 && omab >= 0)
                {
                    double depth = alpha * b.z + beta * c.z + omab * d.z;
                    if (depth < zbuffer[i * width + j] && depth > 1e-6)
                    {
                        zbuffer[i * width + j] = depth;
                        image[i * width + j] = p.excident;
                    }
                }
            }
        }
    }
}

void fragrender(const Camera &camera, Vector a, Vector b, Vector c, const int width, const int height, double xa, double xb, double ya, double yb, double *zbuffer, int *image, int pi)
{
    if (a.y > b.y)
    {
        std::swap(a, b);
    }
    if (a.y > c.y)
    {
        std::swap(a, c);
    }
    if (b.y > c.y)
    {
        std::swap(b, c);
    }
    if (a.y == c.y)
    {
        return;
    }

    double li = (b.y - a.y) / (c.y - a.y);
    Vector d = li * c + (1.0 - li) * a;
    if (b.x > d.x)
    {
        std::swap(b, d);
    }

    int ys = fmin(yb, fmax(ya, vtpy(camera, a, width, height) + 0.5));
    int ym = fmin(yb, fmax(ya, vtpy(camera, b, width, height) + 0.5));
    int ye = fmin(yb, fmax(ya, vtpy(camera, c, width, height) + 0.5));

    double px = camera.width / width;
    double py = camera.height / height;

    for (int i = ys; i < ym; i++)
    {
        double cy = (i + 0.5) * py - camera.height * 0.5;
        li = (cy - a.y) / (b.y - a.y);
        Vector l = li * b + (1.0 - li) * a;
        Vector r = li * d + (1.0 - li) * a;

        int xs = fmin(xb, fmax(xa, vtpx(camera, l, width, height) + 0.5));
        int xe = fmin(xb, fmax(xa, vtpx(camera, r, width, height) + 0.5));

        for (int j = xs; j < xe; j++)
        {
            double cx = (j + 0.5) * px - camera.width * 0.5;
            li = (cx - l.x) / (r.x - l.x);
            double depth = li * r.z + (1.0 - li) * l.z;
            if (depth < zbuffer[i * width + j] && depth > 1e-6)
            {
                zbuffer[i * width + j] = depth;
                image[i * width + j] = pi;
            }
        }
    }

    for (int i = ym; i < ye; i++)
    {
        double cy = (i + 0.5) * py - camera.height * 0.5;
        li = (cy - b.y) / (c.y - b.y);
        Vector l = li * c + (1.0 - li) * b;
        Vector r = li * c + (1.0 - li) * d;

        int xs = fmin(xb, fmax(xa, vtpx(camera, l, width, height) + 0.5));
        int xe = fmin(xb, fmax(xa, vtpx(camera, r, width, height) + 0.5));

        for (int j = xs; j < xe; j++)
        {
            double cx = (j + 0.5) * px - camera.width * 0.5;
            li = (cx - l.x) / (r.x - l.x);
            double depth = li * r.z + (1.0 - li) * l.z;
            if (depth < zbuffer[i * width + j] && depth > 1e-6)
            {
                zbuffer[i * width + j] = depth;
                image[i * width + j] = pi;
            }
        }
    }
}

double fmin_(double a, double b)
{
    return a < b ? a : b;
}
double fmax_(double a, double b)
{
    return a > b ? a : b;
}

void fragrender(const Camera &camera, const Patch &p, const int width, const int height, double *zbuffer, int *image, double xa, double xb, double ya, double yb, int pi)
{
    if (Length(p.pos + 0.5 * (p.a + p.b) - camera.pos) < 1e-6)
    {
        return;
    }

    Vector a = camera.project(p.pos);
    Vector b = camera.project(p.pos + p.a);
    Vector c = camera.project(p.pos + p.b);
    Vector d = camera.project(p.pos + p.a + p.b);

    if (a.z < 1e-6 || b.z < 1e-6 || c.z < 1e-6 || d.z < 1e-6)
    {
        return;
    }

    double x[4] = {vtpx(camera, a, width, height), vtpx(camera, b, width, height), vtpx(camera, c, width, height), vtpx(camera, d, width, height)};
    double y[4] = {vtpy(camera, a, width, height), vtpy(camera, b, width, height), vtpy(camera, c, width, height), vtpy(camera, d, width, height)};

    double xmin = fmin(fmin(x[0], x[1]), fmin(x[2], x[3]));
    double xmax = fmax(fmax(x[0], x[1]), fmax(x[2], x[3]));
    double ymin = fmin(fmin(y[0], y[1]), fmin(y[2], y[3]));
    double ymax = fmax(fmax(y[0], y[1]), fmax(y[2], y[3]));

    int xs = fmin(xb, fmax(xa, xmin + 0.5));
    int xe = fmin(xb, fmax(xa, xmax + 0.5));

    int ys = fmin(yb, fmax(ya, ymin + 0.5));
    int ye = fmin(yb, fmax(ya, ymax + 0.5));

    // fragrender(camera, a, b, c, width, height, xa, xb, ya, yb, zbuffer, image, pi);
    // fragrender(camera, d, b, c, width, height, xa, xb, ya, yb, zbuffer, image, pi);
    // return;

    if (xs == xe || ys == ye)
    {
        return;
    }

    double px = camera.width / width;
    double py = camera.height / height;

    Vector ab = b - a, ac = c - a;
    double tmp1 = ab.x * ac.y - ab.y * ac.x;
    double invtmp1 = 1.0 / tmp1;
    tmp1 = fabs(tmp1);
    double tmp1ax = ac.y * invtmp1;
    double tmp1ay = -ac.x * invtmp1;
    double tmp1ac = (ac.x * a.y - ac.y * a.x) * invtmp1;
    double tmp1bx = -ab.y * invtmp1;
    double tmp1by = ab.x * invtmp1;
    double tmp1bc = (-ab.x * a.y + ab.y * a.x) * invtmp1;

    Vector db = b - d, dc = c - d;
    double tmp2 = db.x * dc.y - db.y * dc.x;
    double invtmp2 = 1.0 / tmp2;
    tmp2 = fabs(tmp2);
    double tmp2ax = dc.y * invtmp2;
    double tmp2ay = -dc.x * invtmp2;
    double tmp2ac = (dc.x * d.y - dc.y * d.x) * invtmp2;
    double tmp2bx = -db.y * invtmp2;
    double tmp2by = db.x * invtmp2;
    double tmp2bc = (-db.x * d.y + db.y * d.x) * invtmp2;

    for (int i = ys; i < ye; i++)
    {
        for (int j = xs; j < xe; j++)
        {
            double cx = (j + 0.5) * px - camera.width * 0.5;
            double cy = (i + 0.5) * py - camera.height * 0.5;

            if (tmp1 > 1e-6)
            {
                double alpha = tmp1ac + tmp1ax * cx + tmp1ay * cy;
                if (alpha >= 0)
                {
                    double beta = tmp1bc + tmp1bx * cx + tmp1by * cy;
                    if (beta >= 0)
                    {
                        double omab = 1 - alpha - beta;
                        if (omab >= 0)
                        {
                            double depth = alpha * b.z + beta * c.z + omab * a.z;
                            if (depth < zbuffer[i * width + j] && depth > 1e-6)
                            {
                                zbuffer[i * width + j] = depth;
                                image[i * width + j] = pi;
                            }
                            continue;
                        }
                    }
                }
            }

            if (tmp2 > 1e-6)
            {
                double alpha = tmp2ac + tmp2ax * cx + tmp2ay * cy;
                if (alpha >= 0)
                {
                    double beta = tmp2bc + tmp2bx * cx + tmp2by * cy;
                    if (beta >= 0)
                    {
                        double omab = 1 - alpha - beta;
                        if (omab >= 0)
                        {
                            double depth = alpha * b.z + beta * c.z + omab * d.z;
                            if (depth < zbuffer[i * width + j] && depth > 1e-6)
                            {
                                zbuffer[i * width + j] = depth;
                                image[i * width + j] = pi;
                            }
                        }
                    }
                }
            }
        }
    }
}

// rasterization
Color *render_view(const Camera &camera, const std::vector<Patch> &scene, const int width, const int height, double *__restrict zbuffer, Color *__restrict image)
{
    for (int i = 0; i < width * height; i++)
    {
        image[i] = {0.0, 0.0, 0.0};
        zbuffer[i] = 1000000;
    }
    for (const auto &p : scene)
    {
        // fragrender(camera, p, width, height, zbuffer, image, 0, width, 0, height);
        // continue;

        if (Length(p.pos + 0.5 * (p.a + p.b) - camera.pos) < 1e-6)
            continue;
        Vector a = camera.project(p.pos);
        Vector b = camera.project(p.pos + p.a);
        Vector c = camera.project(p.pos + p.b);
        Vector d = camera.project(p.pos + p.a + p.b);
        if (a.z < 1e-6 || b.z < 1e-6 || c.z < 1e-6 || d.z < 1e-6)
            continue;
        double px = camera.width / width;
        double py = camera.height / height;

        int xa = 0;
        int ya = 0;
        int xb = width;
        int yb = height;
        int xs = fmin(xb, fmax(xa, fmin(fmin(vtpx(camera, a, width, height), vtpx(camera, b, width, height)), fmin(vtpx(camera, c, width, height), vtpx(camera, d, width, height))) + 0.4375));
        int xe = fmin(xb, fmax(xa, fmax(fmax(vtpx(camera, a, width, height), vtpx(camera, b, width, height)), fmax(vtpx(camera, c, width, height), vtpx(camera, d, width, height))) + 0.5625));

        int ys = fmin(yb, fmax(ya, fmin(fmin(vtpy(camera, a, width, height), vtpy(camera, b, width, height)), fmin(vtpy(camera, c, width, height), vtpy(camera, d, width, height))) + 0.4375));
        int ye = fmin(yb, fmax(ya, fmax(fmax(vtpy(camera, a, width, height), vtpy(camera, b, width, height)), fmax(vtpy(camera, c, width, height), vtpy(camera, d, width, height))) + 0.5625));

        // xs = 0; xe = width; ys = 0; ye = height;

        for (int i = ys; i < ye; i++)
        {
            for (int j = xs; j < xe; j++)
            {
                double cx = (j + 0.5) * px - camera.width * 0.5;
                double cy = (i + 0.5) * py - camera.height * 0.5;
                double depth = 1000000;
                if (inside_quadrilateral(cx, cy, a, b, c, d, depth))
                {
                    if (depth < zbuffer[i * width + j] && depth > 1e-6)
                    {
                        image[i * width + j] = p.excident;
                        zbuffer[i * width + j] = depth;
                    }
                }
            }
        }
    }
    return image;
}

void render_view(const Camera &camera, const std::vector<Patch> &scene, const int width, const int height, double *__restrict zbuffer, int *__restrict image, double xa, double xb, double ya, double yb)
{
    for (int i = 0; i < width * height; i++)
    {
        image[i] = -1;
        zbuffer[i] = 1000000;
    }
    for (int pi = 0; pi < scene.size(); pi++)
    {
        const Patch &p = scene[pi];
        fragrender(camera, p, width, height, zbuffer, image, xa, xb, ya, yb, pi);
        continue;

        if (Length(p.pos + 0.5 * (p.a + p.b) - camera.pos) < 1e-6)
            continue;
        Vector a = camera.project(p.pos);
        Vector b = camera.project(p.pos + p.a);
        Vector c = camera.project(p.pos + p.b);
        Vector d = camera.project(p.pos + p.a + p.b);
        if (a.z < 1e-6 || b.z < 1e-6 || c.z < 1e-6 || d.z < 1e-6)
            continue;
        double px = camera.width / width;
        double py = camera.height / height;

        int xa = 0;
        int ya = 0;
        int xb = width;
        int yb = height;
        int xs = fmin(xb, fmax(xa, fmin(fmin(vtpx(camera, a, width, height), vtpx(camera, b, width, height)), fmin(vtpx(camera, c, width, height), vtpx(camera, d, width, height))) + 0.4375));
        int xe = fmin(xb, fmax(xa, fmax(fmax(vtpx(camera, a, width, height), vtpx(camera, b, width, height)), fmax(vtpx(camera, c, width, height), vtpx(camera, d, width, height))) + 0.5625));

        int ys = fmin(yb, fmax(ya, fmin(fmin(vtpy(camera, a, width, height), vtpy(camera, b, width, height)), fmin(vtpy(camera, c, width, height), vtpy(camera, d, width, height))) + 0.4375));
        int ye = fmin(yb, fmax(ya, fmax(fmax(vtpy(camera, a, width, height), vtpy(camera, b, width, height)), fmax(vtpy(camera, c, width, height), vtpy(camera, d, width, height))) + 0.5625));

        // xs = 0; xe = width; ys = 0; ye = height;

        for (int i = ys; i < ye; i++)
        {
            for (int j = xs; j < xe; j++)
            {
                double cx = (j + 0.5) * px - camera.width * 0.5;
                double cy = (i + 0.5) * py - camera.height * 0.5;
                double depth = 1000000;
                if (inside_quadrilateral(cx, cy, a, b, c, d, depth))
                {
                    if (depth < zbuffer[i * width + j] && depth > 1e-6)
                    {
                        image[i * width + j] = pi;
                        zbuffer[i * width + j] = depth;
                    }
                }
            }
        }
    }
}

// https://stackoverflow.com/a/2654860
void save_bmp_file(const std::string &filename, const Color *image, const int width, const int height)
{
    FILE *f = fopen(filename.c_str(), "wb");
    int filesize = 54 + 3 * width * height;
    unsigned char *img = new unsigned char[3 * width * height];
    memset(img, 0, 3 * width * height);

    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < height; j++)
        {
            double r = sqrt(image[i + j * width].x) * 255;
            double g = sqrt(image[i + j * width].y) * 255;
            double b = sqrt(image[i + j * width].z) * 255;
            if (r > 255)
                r = 255;
            if (g > 255)
                g = 255;
            if (b > 255)
                b = 255;
            img[(i + j * width) * 3 + 2] = (unsigned char)(r);
            img[(i + j * width) * 3 + 1] = (unsigned char)(g);
            img[(i + j * width) * 3 + 0] = (unsigned char)(b);
        }
    }
    unsigned char bmpfileheader[14] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0};
    unsigned char bmpinfoheader[40] = {40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0};
    unsigned char bmppad[3] = {0, 0, 0};

    bmpfileheader[2] = (unsigned char)(filesize);
    bmpfileheader[3] = (unsigned char)(filesize >> 8);
    bmpfileheader[4] = (unsigned char)(filesize >> 16);
    bmpfileheader[5] = (unsigned char)(filesize >> 24);

    bmpinfoheader[4] = (unsigned char)(width);
    bmpinfoheader[5] = (unsigned char)(width >> 8);
    bmpinfoheader[6] = (unsigned char)(width >> 16);
    bmpinfoheader[7] = (unsigned char)(width >> 24);
    bmpinfoheader[8] = (unsigned char)(height);
    bmpinfoheader[9] = (unsigned char)(height >> 8);
    bmpinfoheader[10] = (unsigned char)(height >> 16);
    bmpinfoheader[11] = (unsigned char)(height >> 24);

    fwrite(bmpfileheader, 1, 14, f);
    fwrite(bmpinfoheader, 1, 40, f);
    for (int i = 0; i < height; ++i)
    {
        fwrite(img + (i * width * 3), 3, width, f);
        fwrite(bmppad, 1, (4 - (width * 3) % 4) % 4, f);
    }
    fclose(f);
    delete[] img;
}

// Cornell Box
void load_scene(std::vector<Patch> &scene)
{

    if (1)
    {
        FILE *fi;
        int64_t n;
        fi = fopen("conf.data", "rb");
        fread(&n, 1, 8, fi);
        for (int i = 0; i < n; i++)
        {
            Vector a[5];
            fread(a, 1, 120, fi);
            scene.emplace_back(Patch(a[0], a[1], a[2], a[3], a[4]));
        }
        fclose(fi);
    }
    else
    {
        scene.emplace_back(Patch(Vector(0, 0, 0), Vector(550, 0, 0), Vector(0, 0, 560), Color(), Color(0.5, 0.5, 0.5)));                 // floor
        scene.emplace_back(Patch(Vector(200, 550, 200), Vector(0, 0, 150), Vector(150, 0, 0), Color(40, 40, 40), Color(0.1, 0.1, 0.1))); // light source
        scene.emplace_back(Patch(Vector(0, 550, 0), Vector(0, 0, 200), Vector(550, 0, 0), Color(), Color(0.5, 0.5, 0.5)));               // ceiling
        scene.emplace_back(Patch(Vector(350, 550, 200), Vector(0, 0, 360), Vector(200, 0, 0), Color(), Color(0.5, 0.5, 0.5)));           // ceiling
        scene.emplace_back(Patch(Vector(0, 550, 200), Vector(0, 0, 360), Vector(200, 0, 0), Color(), Color(0.5, 0.5, 0.5)));             // ceiling
        scene.emplace_back(Patch(Vector(200, 550, 350), Vector(0, 0, 210), Vector(150, 0, 0), Color(), Color(0.5, 0.5, 0.5)));           // ceiling
        scene.emplace_back(Patch(Vector(0, 0, 560), Vector(550, 0, 0), Vector(0, 550, 0), Color(), Color(0.5, 0.5, 0.5)));               // back
        scene.emplace_back(Patch(Vector(0, 0, 0), Vector(0, 0, 560), Vector(0, 550, 0), Color(), Color(0, 0.7, 0)));                     // right
        scene.emplace_back(Patch(Vector(550, 0, 0), Vector(0, 550, 0), Vector(0, 0, 560), Color(), Color(0.7, 0, 0)));                   // left
        // short block
        scene.emplace_back(Patch(Vector(130, 165, 65), Vector(160, 0, 50), Vector(-50, 0, 160), Color(), Color(0.5, 0.5, 0.5)));
        scene.emplace_back(Patch(Vector(290, 0, 115), Vector(-50, 0, 160), Vector(0, 165, 0), Color(), Color(0.5, 0.5, 0.5)));
        scene.emplace_back(Patch(Vector(130, 0, 65), Vector(160, 0, 50), Vector(0, 165, 0), Color(), Color(0.5, 0.5, 0.5)));
        scene.emplace_back(Patch(Vector(80, 0, 225), Vector(50, 0, -160), Vector(0, 165, 0), Color(), Color(0.5, 0.5, 0.5)));
        scene.emplace_back(Patch(Vector(240, 0, 275), Vector(-160, 0, -50), Vector(0, 165, 0), Color(), Color(0.5, 0.5, 0.5)));
        // tall block
        scene.emplace_back(Patch(Vector(420, 330, 250), Vector(50, 0, 160), Vector(-160, 0, 50), Color(), Color(0.5, 0.5, 0.5)));
        scene.emplace_back(Patch(Vector(420, 0, 250), Vector(50, 0, 160), Vector(0, 330, 0), Color(), Color(0.5, 0.5, 0.5)));
        scene.emplace_back(Patch(Vector(470, 0, 410), Vector(-160, 0, 50), Vector(0, 330, 0), Color(), Color(0.5, 0.5, 0.5)));
        scene.emplace_back(Patch(Vector(310, 0, 460), Vector(-50, 0, -160), Vector(0, 330, 0), Color(), Color(0.5, 0.5, 0.5)));
        scene.emplace_back(Patch(Vector(260, 0, 300), Vector(160, 0, -50), Vector(0, 330, 0), Color(), Color(0.5, 0.5, 0.5)));
    }

    for (auto &p : scene)
    {
        auto v = p.pos;
        printf("%f,%f,%f,", v.x, v.y, v.z);
        v = p.a;
        printf("  %f,%f,%f,", v.x, v.y, v.z);
        v = p.b;
        printf("  %f,%f,%f,", v.x, v.y, v.z);
        v = p.emission;
        printf("  %f,%f,%f,", v.x, v.y, v.z);
        v = p.reflectance;
        printf("  %f,%f,%f;\n", v.x, v.y, v.z);
    }
}

// divide patches into subpatches whose width and length are no less than given threshold
void divide_patches(std::vector<Patch> &scene, double threshold)
{
    std::vector<Patch> tmp = std::move(scene);
    for (const auto &p : tmp)
    {
        double len_a = Length(p.a);
        double len_b = Length(p.b);
        int a = static_cast<int>(len_a / threshold);
        int b = static_cast<int>(len_b / threshold);
        for (int i = 0; i <= a; ++i)
        {
            for (int j = 0; j <= b; ++j)
            {
                Vector pos = p.pos + i * threshold * Normalize(p.a) + j * threshold * Normalize(p.b);
                Vector pa = (i + 1) * threshold > len_a ? p.a - i * threshold * Normalize(p.a) : threshold * Normalize(p.a);
                Vector pb = (j + 1) * threshold > len_b ? p.b - j * threshold * Normalize(p.b) : threshold * Normalize(p.b);
                if (pa.isZero() || pb.isZero())
                    continue;
                scene.emplace_back(Patch(pos, pa, pb, p.emission, p.reflectance));
            }
        }
    }
}

void cal_multiplier_map()
{
    constexpr double pw = 2. / hemicube_res;

    double s = 0;
    for (int i = 0; i < hemicube_res; ++i)
    {
        for (int j = 0; j < hemicube_res; ++j)
        {
            double cx = (j + 0.5) * pw - 1;
            double cy = (i + 0.5) * pw - 1;
            multiplier_front[i][j] = 1 / (cx * cx + cy * cy + 1) / (cx * cx + cy * cy + 1);
            s = s + multiplier_front[i][j];
        }
    }
    for (int i = 0; i < hemicube_res / 2; ++i)
    {
        for (int j = 0; j < hemicube_res; ++j)
        {
            double cx = (j + 0.5) * pw - 1;
            double cz = (i + 0.5) * pw;
            multiplier_down[i][j] = cz / (cx * cx + cz * cz + 1) / (cx * cx + cz * cz + 1);
            // multiplier_down[i][j] *= multiplier_front[i + hemicube_res / 2][j];
            s = s + 4 * multiplier_down[i][j];
        }
    }
    for (int i = 0; i < hemicube_res; ++i)
    {
        for (int j = 0; j < hemicube_res; ++j)
        {
            // multiplier_front[i][j] *= multiplier_front[i][j];
        }
    }
    s = s / (hemicube_res * hemicube_res / 4) / 3.1416;
    printf("%f\n", s);
}

void cal_incident_light(std::vector<Patch> &scene)
{
    std::atomic<int> gi = 0;

#pragma omp parallel
    {
        Color *front = new Color[hemicube_res * hemicube_res];
        Color *up = new Color[hemicube_res * hemicube_res];
        Color *left = new Color[hemicube_res * hemicube_res];
        Color *right = new Color[hemicube_res * hemicube_res];
        Color *down = new Color[hemicube_res * hemicube_res];
        double *zbuffer = new double[hemicube_res * hemicube_res];

        for (;;)
        {
            int i = gi++;
            if (i >= scene.size())
            {
                break;
            }
            auto &p = scene[i];

            Vector cpos = p.pos + 0.5 * (p.a + p.b);
            render_view(Camera(cpos, Normalize(Cross(p.b, p.a)), Normalize(p.b), pi_2), scene, hemicube_res, hemicube_res, zbuffer, front);
            render_view(Camera(cpos, Normalize(p.b), -Normalize(Cross(p.b, p.a)), pi_2), scene, hemicube_res, hemicube_res, zbuffer, up);
            render_view(Camera(cpos, -Normalize(p.a), Normalize(p.b), pi_2), scene, hemicube_res, hemicube_res, zbuffer, left);
            render_view(Camera(cpos, Normalize(p.a), Normalize(p.b), pi_2), scene, hemicube_res, hemicube_res, zbuffer, right);
            render_view(Camera(cpos, -Normalize(p.b), Normalize(Cross(p.b, p.a)), pi_2), scene, hemicube_res, hemicube_res, zbuffer, down);

            Color total_light{};
            for (int i = 0; i < hemicube_res; ++i)
            {
                for (int j = 0; j < hemicube_res; ++j)
                {
                    total_light = total_light + front[i * hemicube_res + j] * multiplier_front[i][j];
                    if (i < hemicube_res / 2)
                        total_light = total_light + up[i * hemicube_res + j] * multiplier_down[hemicube_res / 2 - 1 - i][j];
                    if (i >= hemicube_res / 2)
                        total_light = total_light + down[i * hemicube_res + j] * multiplier_down[i - hemicube_res / 2][j];
                    if (j < hemicube_res / 2)
                        total_light = total_light + right[i * hemicube_res + j] * multiplier_down[hemicube_res / 2 - 1 - j][i];
                    if (j >= hemicube_res / 2)
                        total_light = total_light + left[i * hemicube_res + j] * multiplier_down[j - hemicube_res / 2][i];
                }
            }
            p.incident = total_light / (hemicube_res * hemicube_res / 4) / 3.1416;
        }

        delete[] front;
        delete[] up;
        delete[] down;
        delete[] left;
        delete[] right;
        delete[] zbuffer;
    }
}

void cal_incident_light_v(std::vector<Patch> &scene)
{
    std::atomic<int> gi = 0;

#pragma omp parallel
    {
        int *front = new int[hemicube_res * hemicube_res];
        int *up = new int[hemicube_res * hemicube_res];
        int *left = new int[hemicube_res * hemicube_res];
        int *right = new int[hemicube_res * hemicube_res];
        int *down = new int[hemicube_res * hemicube_res];
        double *zbuffer = new double[hemicube_res * hemicube_res];

        for (;;)
        {
            int i = gi++;
            if (i >= scene.size())
            {
                break;
            }
            auto &p = scene[i];
            memset(p.m, 0, 8 * scene.size());
            // p.m.resize(scene.size());

            Vector cpos = p.pos + 0.5 * (p.a + p.b);
            render_view(Camera(cpos, Normalize(Cross(p.b, p.a)), Normalize(p.b), pi_2), scene, hemicube_res, hemicube_res, zbuffer, front, 0, hemicube_res, 0, hemicube_res);
            render_view(Camera(cpos, Normalize(p.b), -Normalize(Cross(p.b, p.a)), pi_2), scene, hemicube_res, hemicube_res, zbuffer, up, 0, hemicube_res, 0, hemicube_res / 2);
            render_view(Camera(cpos, -Normalize(p.a), Normalize(p.b), pi_2), scene, hemicube_res, hemicube_res, zbuffer, left, hemicube_res / 2, hemicube_res, 0, hemicube_res);
            render_view(Camera(cpos, Normalize(p.a), Normalize(p.b), pi_2), scene, hemicube_res, hemicube_res, zbuffer, right, 0, hemicube_res / 2, 0, hemicube_res);
            render_view(Camera(cpos, -Normalize(p.b), Normalize(Cross(p.b, p.a)), pi_2), scene, hemicube_res, hemicube_res, zbuffer, down, 0, hemicube_res, hemicube_res / 2, hemicube_res);

            for (int i = 0; i < hemicube_res; ++i)
            {
                for (int j = 0; j < hemicube_res; ++j)
                {
                    if (front[i * hemicube_res + j] >= 0)
                    {
                        p.m[front[i * hemicube_res + j]] += multiplier_front[i][j];
                    }

                    if (i < hemicube_res / 2)
                    {
                        if (up[i * hemicube_res + j] >= 0)
                        {
                            p.m[up[i * hemicube_res + j]] += multiplier_down[hemicube_res / 2 - 1 - i][j];
                        }
                    }

                    if (i >= hemicube_res / 2)
                    {
                        if (down[i * hemicube_res + j] >= 0)
                        {
                            p.m[down[i * hemicube_res + j]] += multiplier_down[i - hemicube_res / 2][j];
                        }
                    }
                    if (j < hemicube_res / 2)
                    {
                        if (right[i * hemicube_res + j] >= 0)
                        {
                            p.m[right[i * hemicube_res + j]] += multiplier_down[hemicube_res / 2 - 1 - j][i];
                        }
                    }
                    if (j >= hemicube_res / 2)
                    {
                        if (left[i * hemicube_res + j] >= 0)
                        {
                            p.m[left[i * hemicube_res + j]] += multiplier_down[j - hemicube_res / 2][i];
                        }
                    }
                }
            }
            // p.incident = total_light / (hemicube_res * hemicube_res / 4) / 3.1416;
        }

        delete[] front;
        delete[] up;
        delete[] down;
        delete[] left;
        delete[] right;
        delete[] zbuffer;
    }
}

void cal_incident_light_fv(std::vector<Patch> &scene)
{
#pragma omp parallel for
    for (int i = 0; i < scene.size(); i++)
    {
        auto &p = scene[i];
        p.incident = {0.0, 0.0, 0.0};
        for (int i = 0; i < scene.size(); i++)
        {
            p.incident = p.incident + p.m[i] * scene[i].excident;
        }
        p.incident = p.incident / (hemicube_res * hemicube_res / 4) / 3.1416;
    }
}

void cal_excident_light(std::vector<Patch> &scene)
{
    for (auto &p : scene)
    {
        p.excident = Multiply(p.incident, p.reflectance) + p.emission;
    }
}

int main()
{
    const int width = 512, height = 512;
    Camera camera(Vector(278, 273, -800), Vector(0, 0, 1), Vector(0, 1, 0), 2 * std::atan2(0.0125, 0.035));
    std::vector<Patch> scene;

    std::cout << "init scene" << std::endl;
    load_scene(scene);

    std::cout << "divide patches" << std::endl;
    divide_patches(scene, 15);
    std::cout << "total patch number: " << scene.size() << std::endl;

    int iter = 0;
    std::cout << "render view " << iter << std::endl;
    Color *image = new Color[width * height];
    double *zbuffer = new double[width * height];

    render_view(camera, scene, width, height, zbuffer, image);

    std::cout << "save image " << iter << std::endl;
    save_bmp_file("cornellbox" + std::to_string(iter) + ".bmp", image, width, height);

    cal_multiplier_map();

    double *ws = (double *)malloc(scene.size() * scene.size() * 8);
    for (int i = 0; i < scene.size(); i++)
    {
        scene[i].m = ws + i * scene.size();
    }

    auto t1 = std::chrono::steady_clock::now();
    cal_incident_light_v(scene);
    auto t2 = std::chrono::steady_clock::now();
    int d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    printf("%d\n", d1);

    const int max_iteration = 5;
    for (iter = 1; iter <= max_iteration; ++iter)
    {
        cal_incident_light_fv(scene);
        cal_excident_light(scene);
        std::cout << "render view " << iter << std::endl;
        render_view(camera, scene, width, height, zbuffer, image);

        std::cout << "save image " << iter << std::endl;
        save_bmp_file("cornellbox" + std::to_string(iter) + ".bmp", image, width, height);
    }

    return 0;
}
