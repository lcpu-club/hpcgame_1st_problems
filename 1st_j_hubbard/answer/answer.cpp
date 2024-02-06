#include <vector>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <stdint.h>
#include <math.h>
#include <chrono>
#include <thread>

#include <omp.h>

#ifdef _WIN32
#include <windows.h>
#define popcnt __popcnt64
#else
#define popcnt __builtin_popcountll
#endif

#include <iostream>

typedef std::unordered_map<uint64_t, size_t> map_t;

struct restrict_t
{
    int offset, range, minocc, maxocc;
    int occ;
    uint64_t substate;
};

template <typename VT>
struct term_t
{
    VT value;
    uint64_t an, cr, signmask, sign;
};

struct sparse_t
{
    std::vector<size_t> row;
    std::vector<size_t> col;
    std::vector<double> data;
};

int tg_bc;
std::mutex mtx;
std::condition_variable cv;

std::vector<uint64_t> table;
map_t map;
std::vector<term_t<double>> op;
int itn;
std::vector<double> iv;

std::vector<double> result;

int wait()
{
    std::unique_lock<std::mutex> lck(mtx);
    tg_bc--;
    if (tg_bc == 0)
    {
        tg_bc = 2;
        return 1;
    }
    cv.wait(lck);
    return 0;
}

void barrier()
{
    if (wait())
    {
        cv.notify_all();
    }
}

double gx[2];
double reducesum(double x, int tgn)
{
    gx[tgn] = x;
    barrier();
    return gx[0] + gx[1];
}

int itrest(restrict_t &rest)
{
    if (!rest.substate)
    {
        goto next;
    }
    {
        uint64_t x = rest.substate & (-(int64_t)rest.substate);
        uint64_t y = x + rest.substate;
        rest.substate = y + (y ^ rest.substate) / x / 4;
    }
    if (rest.substate >> rest.range)
    {
    next:
        if (rest.occ == rest.maxocc)
        {
            rest.occ = rest.minocc;
            rest.substate = (uint64_t(1) << rest.occ) - 1;
            return 1;
        }
        rest.occ++;
        rest.substate = (uint64_t(1) << rest.occ) - 1;
        return 0;
    }
    return 0;
}

int itrest(std::vector<restrict_t> &rest)
{
    for (restrict_t &re : rest)
    {
        if (!itrest(re))
        {
            return 0;
        }
    }
    return 1;
}

uint64_t getstate(const std::vector<restrict_t> &rest)
{
    uint64_t state = 0;
    for (const restrict_t &re : rest)
    {
        state |= re.substate << re.offset;
    }
    return state;
}

int generatetable(std::vector<uint64_t> &table, map_t &map, std::vector<restrict_t> &rest)
{
    for (restrict_t &re : rest)
    {
        re.occ = re.minocc;
        re.substate = (uint64_t(1) << re.occ) - 1;
    }

    size_t index = 0;
    do
    {
        uint64_t state = getstate(rest);
        table.push_back(state);
        map.insert(std::make_pair(state, index));
        index++;
    } while (!itrest(rest));

    return 0;
}

template <typename VT>
term_t<VT> getterm(VT value, const std::vector<int> &cr, const std::vector<int> &an)
{
    term_t<VT> term;
    term.value = value;
    term.an = 0;
    term.cr = 0;
    term.signmask = 0;
    uint64_t signinit = 0;

    for (int x : an)
    {
        uint64_t mark = uint64_t(1) << x;
        term.signmask ^= (mark - 1) & (~term.an);
        term.an |= mark;
    }
    for (int x : cr)
    {
        uint64_t mark = uint64_t(1) << x;
        signinit ^= (mark - 1) & term.cr;
        term.signmask ^= (mark - 1) & (~term.an) & (~term.cr);
        term.cr |= mark;
    }
    term.sign = popcnt(signinit ^ (term.signmask & term.an));
    term.signmask = term.signmask & (~term.an) & (~term.cr);

    return term;
}

#include <set>
#include <map>

template <typename VT>
int act(std::vector<size_t> &row, std::vector<size_t> &col, std::vector<VT> &data, const std::vector<term_t<VT>> &op, const std::vector<uint64_t> &table, const map_t &map, int tgn)
{
    int64_t n = table.size();
    int64_t cb, ce;
    cb = n * tgn / 2;
    ce = n * (tgn + 1) / 2;
    col.resize(ce - cb + 1);
    col[0] = 0;

#pragma omp parallel for
    for (int64_t i = cb; i < ce; i++)
    {
        uint64_t srcstate = table[i];
        size_t n = 0;

        std::set<uint64_t> ds;

        for (const term_t<VT> &term : op)
        {
            if ((srcstate & term.an) == term.an)
            {
                uint64_t dststate = srcstate ^ term.an;
                if ((dststate & term.cr) == 0)
                {
                    dststate ^= term.cr;
                    ds.insert(dststate);
                }
            }
        }

        for (uint64_t dststate : ds)
        {
            auto it = map.find(dststate);
            if (it != map.end())
            {
                n++;
            }
        }

        col[i + 1 - cb] = n;
    }

    for (int64_t i = 0; i < ce - cb; i++)
    {
        col[i + 1] += col[i];
    }

    row.resize(col[ce - cb]);
    data.resize(col[ce - cb]);

#pragma omp parallel for
    for (int64_t i = cb; i < ce; i++)
    {
        uint64_t srcstate = table[i];

        std::map<uint64_t, VT> dsv;

        size_t n = col[i - cb];
        for (const term_t<VT> &term : op)
        {
            if ((srcstate & term.an) == term.an)
            {
                uint64_t dststate = srcstate ^ term.an;
                if ((dststate & term.cr) == 0)
                {
                    dststate ^= term.cr;

                    uint64_t sign = term.sign + popcnt(srcstate & term.signmask);
                    VT v = term.value;
                    if (sign & 1)
                    {
                        v = -v;
                    }
                    dsv[dststate] += v;
                }
            }
        }

        for (const auto &dsv_ : dsv)
        {
            auto it = map.find(dsv_.first);
            if (it != map.end())
            {
                data[n] = dsv_.second;
                row[n] = it->second;
                n++;
            }
        }
        if (n != col[i + 1 - cb])
        {
            printf("fuck\n");
        }
    }

    return 0;
}

int readss(FILE *fi, std::vector<uint64_t> &table, map_t &map)
{
    int n;
    fread(&n, 1, 4, fi);
    std::vector<restrict_t> restv(n);
    for (auto &rest : restv)
    {
        fread(&rest, 1, 16, fi);
    }
    generatetable(table, map, restv);
    return 0;
}

int readop(FILE *fi, std::vector<term_t<double>> &op)
{
    int n, order;
    fread(&n, 1, 4, fi);
    fread(&order, 1, 4, fi);

    std::vector<double> v(n);
    fread(v.data(), 1, 8 * n, fi);

    std::vector<int> rawterm(order);
    std::vector<int> cr, an;

    for (int i = 0; i < n; i++)
    {
        fread(rawterm.data(), 1, 4 * order, fi);
        int tn = rawterm[0];

        for (int j = 0; j < tn; j++)
        {
            int type = rawterm[tn * 2 - 1 - j * 2];
            if (type)
            {
                cr.push_back(rawterm[tn * 2 - j * 2]);
            }
            else
            {
                an.push_back(rawterm[tn * 2 - j * 2]);
            }
        }

        op.push_back(getterm(v[i], cr, an));
        cr.clear();
        an.clear();
    }

    return 0;
}

void memcpy_(double *__restrict d, const double *__restrict s, int64_t n)
{
#pragma omp parallel for // schedule(dynamic,4096)
    for (int64_t i = 0; i < n; i++)
    {
        d[i] = s[i];
    }
}

double *tempvx[2];

double mmv_(double *__restrict out, const sparse_t &m, double *__restrict v, int tgn, int64_t n, double *__restrict ws)
{

    tempvx[tgn] = v; // ws + table.size() * tgn / 2;//
    barrier();
    if (tgn == 1)
    {
        memcpy_(ws, tempvx[0], table.size() / 2);
    }
    else
    {
        memcpy_(ws + table.size() / 2, tempvx[1], table.size() - table.size() / 2);
    }

    double sdot = 0;
    double *ws_ = ws + table.size() * tgn / 2;

#pragma omp parallel for reduction(+ : sdot) schedule(dynamic, 8192)
    for (int64_t i = 0; i < n; i++)
    {
        size_t b, e;
        b = m.row[i];
        e = m.row[i + 1];
        double s = 0;
        for (size_t j = b; j < e; j++)
        {
            s += m.data[j] * ws[m.col[j]];
        }
        out[i] = s;
        sdot += s * ws_[i];
    }
    sdot = reducesum(sdot, tgn);
    return sdot;
}

// v1+=s*v2;
void avv(double *__restrict v1, double s, const double *__restrict v2, int64_t n)
{

#pragma omp parallel for
    for (int64_t i = 0; i < n; i++)
    {
        v1[i] += s * v2[i];
    }
}

double avv2n(double *__restrict v, double s1, double *__restrict v1, double s2, double *__restrict v2, int tgn, int64_t n)
{
    double s = 0;

#pragma omp parallel for reduction(+ : s)
    for (int64_t i = 0; i < n; i++)
    {
        v[i] += s1 * v1[i] + s2 * v2[i];
        s += v[i] * v[i];
    }
    return reducesum(s, tgn);
}

// v*=s;
void msvc(const double s, double *__restrict v, double *__restrict c, int64_t n)
{

#pragma omp parallel for
    for (int64_t i = 0; i < n; i++)
    {
        v[i] *= s;
        c[i] = v[i];
    }
}

// v'*v;
double norm2(const double *__restrict v, int tgn, int64_t n)
{
    double s = 0;

#pragma omp parallel for reduction(+ : s)
    for (int64_t i = 0; i < n; i++)
    {
        s += v[i] * v[i];
    }

    return reducesum(s, tgn);
}

void getsp(std::vector<double> &out, int itn, const sparse_t &m, double *v, int tgn, int64_t n)
{
    double *ws = (double *)malloc(table.size() * 8);

    uint64_t offset = table.size() * tgn / 2;

    double l = sqrt(norm2(v + offset, tgn, n));
    msvc(1.0 / l, v + offset, ws + offset, n);

    std::vector<double> a(itn), b(itn - 1);

    double *v_ = (double *)malloc(table.size() * 8);
    double *v__ = (double *)malloc(table.size() * 8);

    for (int i = 0; i < itn; i++)
    {
        std::swap(v__, v_);
        std::swap(v_, v);
        auto t1 = std::chrono::steady_clock::now();
        a[i] = mmv_(v + offset, m, v_ + offset, tgn, n, ws);
        auto t2 = std::chrono::steady_clock::now();
        barrier();
        auto t3 = std::chrono::steady_clock::now();

        if (i < itn - 1)
        {
            if (i == 0)
            {
                avv(v + offset, -a[i], ws + offset, n);
                b[i] = sqrt(norm2(v + offset, tgn, n));
            }
            else
            {
                // avv(v, -a[i], v_);
                // avv(v, -b[i - 1], v__);
                b[i] = sqrt(avv2n(v + offset, -a[i], ws + offset, -b[i - 1], v__ + offset, tgn, n));
            }

            auto t4 = std::chrono::steady_clock::now();
            // b[i] = sqrt(norm2(v, tgn));
            msvc(1.0 / b[i], v + offset, ws + offset, n);
            auto t5 = std::chrono::steady_clock::now();
            int d1 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            int d2 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
            int d3 = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
            int d4 = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count();
            printf("%d,%d,%d,%d\n", d1, d2, d3, d4);
        }
    }

    if (tgn == 0)
    {
        out.resize(itn * 2);

        out[0] = l;
        for (int i = 0; i < itn; i++)
        {
            out[1 + i] = a[i];
        }
        for (int i = 0; i < itn - 1; i++)
        {
            out[1 + itn + i] = b[i];
        }
    }
}

#include <pthread.h>

std::chrono::steady_clock::time_point t3;

const int tps = 32;
void calc(int tgn)
{
    omp_set_num_threads(tps);

    printf("tg:%d\n", tgn);
#pragma omp parallel
    {
        int s;
        cpu_set_t cpuset;
        pthread_t thread;
        thread = pthread_self();
        CPU_ZERO(&cpuset);
        for (int i = 0; i < tps; i++)
        {
            CPU_SET(i + tgn * tps, &cpuset);
        }
        s = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    }

    sparse_t opm;
    act(opm.col, opm.row, opm.data, op, table, map, tgn);
    barrier();
    if (tgn == 0)
    {
        t3 = std::chrono::steady_clock::now();
    }

    int64_t n = table.size() * (tgn + 1) / 2 - table.size() * tgn / 2;
    double *v = (double *)malloc(table.size() * 8);
    memcpy_(v + table.size() * tgn / 2, iv.data() + table.size() * tgn / 2, n);
    getsp(result, itn, opm, v, tgn, n);
}

int main()
{
    std::cout << omp_get_max_threads() << "\n";

    FILE *fi;

    fi = fopen("conf.data", "rb");
    auto t1 = std::chrono::steady_clock::now();
    readss(fi, table, map);
    auto t2 = std::chrono::steady_clock::now();
    readop(fi, op);

    fread(&itn, 1, 4, fi);

    iv.resize(table.size());
    fread(iv.data(), 1, table.size() * 8, fi);

    fclose(fi);

    tg_bc = 2;
    std::thread a(calc, 0);
    std::thread b(calc, 1);
    a.join();
    b.join();

    auto t4 = std::chrono::steady_clock::now();
    fi = fopen("out.data", "wb");
    fwrite(result.data(), 1, 16 * itn, fi);
    fclose(fi);

    int d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    int d2 = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
    int d3 = std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();
    printf("%d,%d,%d\n", d1, d2, d3);
    std::cout << "Hello World!\n";
}
