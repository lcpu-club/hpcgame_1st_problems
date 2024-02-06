#include <array>
#include <fstream>
#include <iostream>
#include <omp.h>
#include <vector>
#include <cmath>
#include <tuple>

using namespace std;

void particle2grid_parallel(int resolution, int numparticle,
                            const vector<double> &particle_position,
                            const vector<double> &particle_velocity,
                            vector<double> &velocityu, vector<double> &velocityv, vector<double> &weightu,
                            vector<double> &weightv)
{
    double grid_spacing = 1.0 / resolution;
    double inv_grid_spacing = 1.0 / grid_spacing;
    auto get_frac = [&inv_grid_spacing](double x, double y)
    {
        int xidx = floor(x * inv_grid_spacing);
        int yidx = floor(y * inv_grid_spacing);
        double fracx = x * inv_grid_spacing - xidx;
        double fracy = y * inv_grid_spacing - yidx;
        return tuple(array<int, 2>{xidx, yidx},
                     array<double, 4>{fracx * fracy, (1 - fracx) * fracy,
                                      fracx * (1 - fracy),
                                      (1 - fracx) * (1 - fracy)});
    };
#pragma omp parallel for
    for (int i = 0; i < numparticle; i++)
    {
        array<int, 4> offsetx = {0, 1, 0, 1};
        array<int, 4> offsety = {0, 0, 1, 1};

        auto [idxu, fracu] =
            get_frac(particle_position[i * 2 + 0],
                     particle_position[i * 2 + 1] - 0.5 * grid_spacing);
        auto [idxv, fracv] =
            get_frac(particle_position[i * 2 + 0] - 0.5 * grid_spacing,
                     particle_position[i * 2 + 1]);

        for (int j = 0; j < 4; j++)
        {
            int tmpidx = 0;
            double tmp[2];
            tmpidx =
                (idxu[0] + offsetx[j]) * resolution + (idxu[1] + offsety[j]);
            tmp[0] = particle_velocity[i * 2 + 0] * fracu[j];
            tmp[1] = fracu[j];
#pragma omp atomic
            velocityu[tmpidx] += tmp[0];
#pragma omp atomic
            weightu[tmpidx] += tmp[1];

            tmpidx = (idxv[0] + offsetx[j]) * (resolution + 1) +
                     (idxv[1] + offsety[j]);

            tmp[0] = particle_velocity[i * 2 + 1] * fracv[j];
            tmp[1] = fracv[j];
#pragma omp atomic
            velocityv[tmpidx] += tmp[0];
#pragma omp atomic
            weightv[tmpidx] += tmp[1];
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s inputfile [-p]\n", argv[0]);
        return -1;
    }

    string inputfile(argv[1]);
    ifstream fin(inputfile, ios::binary);
    if (!fin)
    {
        printf("Error opening file");
        return -1;
    }

    int resolution;
    int numparticle;
    vector<double> particle_position;
    vector<double> particle_velocity;
    fin.read((char *)(&resolution), sizeof(int));
    fin.read((char *)(&numparticle), sizeof(int));
    particle_position.resize(numparticle * 2);
    particle_velocity.resize(numparticle * 2);
    printf("resolution: %d\n", resolution);
    printf("numparticle: %d\n", numparticle);
    fin.read((char *)(particle_position.data()),
             sizeof(double) * particle_position.size());
    fin.read((char *)(particle_velocity.data()),
             sizeof(double) * particle_velocity.size());

    vector<double> velocityu((resolution + 1) * resolution, 0.0);
    vector<double> velocityv((resolution + 1) * resolution, 0.0);
    vector<double> weightu((resolution + 1) * resolution, 0.0);
    vector<double> weightv((resolution + 1) * resolution, 0.0);

    double st = omp_get_wtime();
    string outputfile;
    particle2grid_parallel(resolution, numparticle, particle_position,
                           particle_velocity, velocityu, velocityv, weightu,
                           weightv);
    outputfile = "output.dat";
    double et = omp_get_wtime();
    printf("time cost: %.3e s\n", et - st);

    ofstream fout(outputfile, ios::binary);
    if (!fout)
    {
        printf("Error output file");
        return -1;
    }
    fout.write((char *)(&resolution), sizeof(int));
    fout.write(reinterpret_cast<char *>(velocityu.data()),
               sizeof(double) * velocityu.size());
    fout.write(reinterpret_cast<char *>(velocityv.data()),
               sizeof(double) * velocityv.size());
    fout.write(reinterpret_cast<char *>(weightu.data()),
               sizeof(double) * weightu.size());
    fout.write(reinterpret_cast<char *>(weightv.data()),
               sizeof(double) * weightv.size());

    return 0;
}
