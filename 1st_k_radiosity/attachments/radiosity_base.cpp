#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

class Vector {
public:
    double x, y, z;
    Vector(const double x = 0, const double y = 0, const double z = 0) : x(x), y(y), z(z) {}
    Vector(const Vector&) = default;
    Vector& operator=(const Vector&) = default;
    Vector operator+(const Vector& b) const { return Vector(x + b.x, y + b.y, z + b.z); }
    Vector operator-(const Vector& b) const { return Vector(x - b.x, y - b.y, z - b.z); }
    Vector operator-() const { return Vector(-x, -y, -z); }
    Vector operator*(const double s) const { return Vector(x * s, y * s, z * s); }
    Vector operator/(const double s) const { return Vector(x / s, y / s, z / s); }
    bool isZero() const { return (x == 0.) && (y == 0.) && (z == 0.); }
};
inline double LengthSquared(const Vector& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
inline double Length(const Vector& v) { return std::sqrt(LengthSquared(v)); }
inline Vector operator*(const double s, const Vector& v) { return v * s; }
inline Vector Normalize(const Vector& v) { return v / Length(v); }
inline Vector Multiply(const Vector& v1, const Vector& v2) { return Vector(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z); }
inline double Dot(const Vector& v1, const Vector& v2) { return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z; }
inline const Vector Cross(const Vector& v1, const Vector& v2) {
    return Vector((v1.y * v2.z) - (v1.z * v2.y), (v1.z * v2.x) - (v1.x * v2.z), (v1.x * v2.y) - (v1.y * v2.x));
}
using Color = Vector;
constexpr double pi_2 = 1.5707963267948966192313216916398;
constexpr int hemicube_res = 256;

double multiplier_front[hemicube_res][hemicube_res];
double multiplier_down[hemicube_res / 2][hemicube_res];

// prospective camera
class Camera {
public:
    Vector pos, dir, up;
    double fov, aspect_ratio;
    double width, height;
    Camera(Vector pos, Vector dir, Vector up, double fov, double aspect_ratio = 1.) :
        pos(pos), dir(dir), up(up), fov(fov), aspect_ratio(aspect_ratio) {
        height = 2 * std::tan(fov / 2);
        width = height * aspect_ratio;
    }
    Vector project(const Vector& v) const {
        Vector a = v - pos;
        double z = Dot(a, dir);
        double y = Dot(a, up);
        double x = Dot(a, Cross(up, dir));
        return Vector(-x / z, y / z, z);
    }
};

// Patches are rectangle
class Patch {
public:
    Vector pos;
    Vector a, b; // regard Cross(b, a) as normal
    Color emission;
    Color reflectance;
    Color incident;
    Color excident;
    Patch(Vector pos, Vector a, Vector b, Color emission, Color reflectance) :
        pos(pos), a(a), b(b), emission(emission), reflectance(reflectance), incident(), excident(emission) {}
};

bool inside_quadrilateral(double cx, double cy, Vector a, Vector b, Vector c, Vector d, double& depth) {
    Vector ab = b - a, ac = c - a;
    double tmp = ab.x * ac.y - ab.y * ac.x;
    if (std::fabs(tmp) > 1e-6) {
        double alpha = (-ac.x * (cy - a.y) + ac.y * (cx - a.x)) / tmp;
        double beta = (ab.x * (cy - a.y) - ab.y * (cx - a.x)) / tmp;
        if (alpha >= 0 && beta >= 0 && alpha + beta <= 1) {
            depth = alpha * b.z + beta * c.z + (1 - alpha - beta) * a.z;
            return true;
        }
    }
    Vector db = b - d, dc = c - d;
    tmp = db.x * dc.y - db.y * dc.x;
    if (std::fabs(tmp) > 1e-6) {
        double alpha = (-dc.x * (cy - d.y) + dc.y * (cx - d.x)) / tmp;
        double beta = (db.x * (cy - d.y) - db.y * (cx - d.x)) / tmp;
        if (alpha >= 0 && beta >= 0 && alpha + beta <= 1) {
            depth = alpha * b.z + beta * c.z + (1 - alpha - beta) * d.z;
            return true;
        }
    }
    return false;
}

// rasterization
Color* render_view(const Camera& camera, const std::vector<Patch>& scene, const int width, const int height) {
    Color* image = new Color[width * height];
    double* zbuffer = new double[width * height];
    for (int i = 0; i < width * height; ++i) {
        image[i] = Color();
        zbuffer[i] = 1000000;
    }
    for (const auto& p : scene) {
        if (Length(p.pos + 0.5 * (p.a + p.b) - camera.pos) < 1e-6) continue;
        Vector a = camera.project(p.pos);
        Vector b = camera.project(p.pos + p.a);
        Vector c = camera.project(p.pos + p.b);
        Vector d = camera.project(p.pos + p.a + p.b);
        if (a.z < 1e-6 || b.z < 1e-6 || c.z < 1e-6 || d.z < 1e-6)continue;
        double px = camera.width / width;
        double py = camera.height / height;
        for (int i = 0; i < height; ++i) {
            for (int j = 0; j < width; ++j) {
                double cx = (j + 0.5) * px - camera.width * 0.5;
                double cy = (i + 0.5) * py - camera.height * 0.5;
                double depth = 1000000;
                if (inside_quadrilateral(cx, cy, a, b, c, d, depth)) {
                    if (depth < zbuffer[i * width + j] && depth > 1e-6) {
                        image[i * width + j] = p.excident;
                        zbuffer[i * width + j] = depth;
                    }
                }
            }
        }
    }
    delete[] zbuffer;
    return image;
}

// https://stackoverflow.com/a/2654860
void save_bmp_file(const std::string& filename, const Color* image, const int width, const int height) {
    FILE* f = fopen(filename.c_str(), "wb");
    int filesize = 54 + 3 * width * height;
    unsigned char* img = new unsigned char[3 * width * height];
    memset(img, 0, 3 * width * height);

    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < height; j++)
        {
            double r = sqrt(image[i + j * width].x) * 255;
            double g = sqrt(image[i + j * width].y) * 255;
            double b = sqrt(image[i + j * width].z) * 255;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            img[(i + j * width) * 3 + 2] = (unsigned char)(r);
            img[(i + j * width) * 3 + 1] = (unsigned char)(g);
            img[(i + j * width) * 3 + 0] = (unsigned char)(b);
        }
    }
    unsigned char bmpfileheader[14] = { 'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0 };
    unsigned char bmpinfoheader[40] = { 40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0 };
    unsigned char bmppad[3] = { 0,0,0 };

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
    for (int i = 0; i < height; ++i) {
        fwrite(img + (i * width * 3), 3, width, f);
        fwrite(bmppad, 1, (4 - (width * 3) % 4) % 4, f);
    }
    fclose(f);
    delete[] img;
}

// Cornell Box
void load_scene(std::vector<Patch>& scene) {
    FILE* fi;
    int64_t n;
    fi = fopen("conf.data", "rb");
    fread(&n, 1, 8, fi);
    for (int i = 0; i < n; i++) {
        Vector a[5];
        fread(a, 1, 120, fi);
        scene.emplace_back(Patch(a[0], a[1], a[2], a[3], a[4]));
    }
    fclose(fi);
}

// divide patches into subpatches whose width and length are no less than given threshold
void divide_patches(std::vector<Patch>& scene, double threshold) {
    std::vector<Patch> tmp = std::move(scene);
    for (const auto& p : tmp) {
        double len_a = Length(p.a);
        double len_b = Length(p.b);
        int a = static_cast<int>(len_a / threshold);
        int b = static_cast<int>(len_b / threshold);
        for (int i = 0; i <= a; ++i) {
            for (int j = 0; j <= b; ++j) {
                Vector pos = p.pos + i * threshold * Normalize(p.a) + j * threshold * Normalize(p.b);
                Vector pa = (i + 1) * threshold > len_a ? p.a - i * threshold * Normalize(p.a) : threshold * Normalize(p.a);
                Vector pb = (j + 1) * threshold > len_b ? p.b - j * threshold * Normalize(p.b) : threshold * Normalize(p.b);
                if (pa.isZero() || pb.isZero()) continue;
                scene.emplace_back(Patch(pos, pa, pb, p.emission, p.reflectance));
            }
        }
    }
}

void cal_multiplier_map() {
    constexpr double pw = 2. / hemicube_res;

    double s=0;
    for (int i = 0; i < hemicube_res; ++i) {
        for (int j = 0; j < hemicube_res; ++j) {
            double cx = (j + 0.5) * pw - 1;
            double cy = (i + 0.5) * pw - 1;
            multiplier_front[i][j] = 1 / (cx * cx + cy * cy + 1)/ (cx * cx + cy * cy + 1);
            s = s + multiplier_front[i][j];
        }
    }
    for (int i = 0; i < hemicube_res / 2; ++i) {
        for (int j = 0; j < hemicube_res; ++j) {
            double cx = (j + 0.5) * pw - 1;
            double cz = (i + 0.5) * pw;
            multiplier_down[i][j] = cz / (cx * cx + cz * cz + 1)/(cx * cx + cz * cz + 1);
            //multiplier_down[i][j] *= multiplier_front[i + hemicube_res / 2][j];
            s = s + 4 * multiplier_down[i][j];
        }
    }
    for (int i = 0; i < hemicube_res; ++i) {
        for (int j = 0; j < hemicube_res; ++j) {
            //multiplier_front[i][j] *= multiplier_front[i][j];
        }
    }
    s = s / (hemicube_res * hemicube_res / 4) / 3.1416;
    printf("%f\n", s);
}

void cal_incident_light(std::vector<Patch>& scene) {

    for (int i = 0; i < scene.size(); i++) {
        auto& p = scene[i];

        Vector cpos = p.pos + 0.5 * (p.a + p.b);
        Color* front = render_view(Camera(cpos, Normalize(Cross(p.b, p.a)), Normalize(p.b), pi_2), scene, hemicube_res, hemicube_res);
        Color* up = render_view(Camera(cpos, Normalize(p.b), -Normalize(Cross(p.b, p.a)), pi_2), scene, hemicube_res, hemicube_res);
        Color* left = render_view(Camera(cpos, -Normalize(p.a), Normalize(p.b), pi_2), scene, hemicube_res, hemicube_res);
        Color* right = render_view(Camera(cpos, Normalize(p.a), Normalize(p.b), pi_2), scene, hemicube_res, hemicube_res);
        Color* down = render_view(Camera(cpos, -Normalize(p.b), Normalize(Cross(p.b, p.a)), pi_2), scene, hemicube_res, hemicube_res);
        Color total_light{};
        for (int i = 0; i < hemicube_res; ++i) {
            for (int j = 0; j < hemicube_res; ++j) {
                total_light = total_light + front[i * hemicube_res + j] * multiplier_front[i][j];
                if (i < hemicube_res / 2) total_light = total_light + up[i * hemicube_res + j] * multiplier_down[hemicube_res / 2 - 1 - i][j];
                if (i >= hemicube_res / 2) total_light = total_light + down[i * hemicube_res + j] * multiplier_down[i - hemicube_res / 2][j];
                if (j < hemicube_res / 2) total_light = total_light + right[i * hemicube_res + j] * multiplier_down[hemicube_res / 2 - 1 - j][i];
                if (j >= hemicube_res / 2) total_light = total_light + left[i * hemicube_res + j] * multiplier_down[j - hemicube_res / 2][i];
            }
        }
        p.incident = total_light / (hemicube_res * hemicube_res / 4) / 3.1416;
        delete[] front;
        delete[] up;
        delete[] left;
        delete[] right;
        delete[] down;
    }
}

void cal_excident_light(std::vector<Patch>& scene) {
    for (auto& p : scene) {
        p.excident = Multiply(p.incident, p.reflectance) + p.emission;
    }
}

int main() {
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
    Color* image = render_view(camera, scene, width, height);

    std::cout << "save image " << iter << std::endl;
    save_bmp_file("cornellbox" + std::to_string(iter) + ".bmp", image, width, height);
    delete[] image;

    cal_multiplier_map();

    const int max_iteration = 5;
    for (iter = 1; iter <= max_iteration; ++iter) {
        cal_incident_light(scene);
        cal_excident_light(scene);
        std::cout << "render view " << iter << std::endl;
        Color* image = render_view(camera, scene, width, height);

        std::cout << "save image " << iter << std::endl;
        save_bmp_file("cornellbox" + std::to_string(iter) + ".bmp", image, width, height);
        delete[] image;
    }

    return 0;
}