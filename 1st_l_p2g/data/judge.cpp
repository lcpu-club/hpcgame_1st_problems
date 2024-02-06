#include <array>
#include <fstream>
#include <iostream>
#include <omp.h>
#include <vector>

using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <seq> <para>\n", argv[0]);
        return -1;
    }

    string fileseq(argv[1]);
    string filepara(argv[2]);
    ifstream finseq(fileseq, ios::binary);
    ifstream finpara(filepara, ios::binary);
    if (!finseq)
    {
        printf("Error opening file: %s", argv[1]);
        return -1;
    }
    if (!finpara)
    {
        printf("Error opening file: %s", argv[2]);
        return -1;
    }
    int resolution;
    finseq.read((char *)(&resolution), sizeof(int));
    finpara.read((char *)(&resolution), sizeof(int));
    vector<double> velocityu((resolution + 1) * resolution);
    vector<double> velocityv((resolution + 1) * resolution);
    vector<double> weightu((resolution + 1) * resolution);
    vector<double> weightv((resolution + 1) * resolution);

    vector<double> velocityu_parallel((resolution + 1) * resolution);
    vector<double> velocityv_parallel((resolution + 1) * resolution);
    vector<double> weightu_parallel((resolution + 1) * resolution);
    vector<double> weightv_parallel((resolution + 1) * resolution);

    finseq.read((char *)(velocityu.data()), sizeof(double) * velocityu.size());
    finseq.read((char *)(velocityv.data()), sizeof(double) * velocityv.size());
    finseq.read((char *)(weightu.data()), sizeof(double) * weightu.size());
    finseq.read((char *)(weightv.data()), sizeof(double) * weightv.size());

    finpara.read((char *)(velocityu_parallel.data()),
                 sizeof(double) * velocityu_parallel.size());
    finpara.read((char *)(velocityv_parallel.data()),
                 sizeof(double) * velocityv_parallel.size());
    finpara.read((char *)(weightu_parallel.data()),
                 sizeof(double) * weightu_parallel.size());
    finpara.read((char *)(weightv_parallel.data()),
                 sizeof(double) * weightv_parallel.size());

    double r1[4] = {0.0};
    for (int i = 0; i < resolution * (resolution + 1); i++)
    {
        r1[0] += abs(velocityu[i] - velocityu_parallel[i]);
        r1[1] += abs(velocityv[i] - velocityv_parallel[i]);
        r1[2] += abs(weightu[i] - weightu_parallel[i]);
        r1[3] += abs(weightv[i] - weightv_parallel[i]);
    }
    r1[0] /= resolution * (resolution + 1);
    r1[1] /= resolution * (resolution + 1);
    r1[2] /= resolution * (resolution + 1);
    r1[3] /= resolution * (resolution + 1);

    int retval = 0;

    if (r1[0] < 1e-6 && r1[1] < 1e-6 && r1[2] < 1e-6 && r1[3] < 1e-6)
        retval = 0;
    else
        retval = 1;

    printf("r1: %e %e %e %e\n", r1[0], r1[1], r1[2], r1[3]);
    return retval;
}
