// md5zero
//
// Benchmark execution speed of ways to test if an MD5 hash is equal to zero.

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

#define NUM_TESTS 102000
static char *sChecksums[NUM_TESTS];

void init()
{
    int i;
    for (i = 0; i < NUM_TESTS; ++i) {
        sChecksums[i] = (char *) malloc(17);
        memset(sChecksums[i], 0, 17);
        sChecksums[i][i % 17] = (char) 1;
    }
}

static inline int IsZeroByOneLoop(char *checksum)
{
    int i;
    for (i = 0; i < 16; ++i) {
        if (checksum[i]) {
            return 0;
        }
    }
    return 1;
}

static inline int IsZeroByOneUnrolled(char *checksum)
{
    return     (checksum[ 0] == 0) && (checksum[ 1] == 0)
            && (checksum[ 2] == 0) && (checksum[ 3] == 0)
            && (checksum[ 4] == 0) && (checksum[ 5] == 0)
            && (checksum[ 6] == 0) && (checksum[ 7] == 0)
            && (checksum[ 8] == 0) && (checksum[ 9] == 0)
            && (checksum[10] == 0) && (checksum[11] == 0)
            && (checksum[12] == 0) && (checksum[13] == 0)
            && (checksum[14] == 0) && (checksum[15] == 0);
}

static inline int IsZeroByOneOr(char *checksum)
{
    return   (checksum[ 0] | checksum[ 1] | checksum[ 2] | checksum[ 3]
            | checksum[ 4] | checksum[ 5] | checksum[ 6] | checksum[ 7]
            | checksum[ 8] | checksum[ 9] | checksum[10] | checksum[11]
            | checksum[12] | checksum[13] | checksum[14] | checksum[15]) == 0;
}

static inline int IsZeroByFour(char *raw)
{
    unsigned int *checksum = (unsigned int *) raw;
    return     (checksum[0] == 0) && (checksum[1] == 0)
            && (checksum[2] == 0) && (checksum[3] == 0);
}

static inline int IsZeroByEight(char *raw)
{
    unsigned long long *checksum = (unsigned long long *) raw;
    return (checksum[0] == 0) && (checksum[1] == 0);
}

static inline int IsZeroFFS(char *raw)
{
    unsigned int *checksum = (unsigned int *) raw;
    return (__builtin_ffs(checksum[0]) == 0)
            && (__builtin_ffs(checksum[1]) == 0)
            && (__builtin_ffs(checksum[2]) == 0)
            && (__builtin_ffs(checksum[3]) == 0);
}

#define BENCHMARK(fxn, off, lbl)                                \
{                                                               \
    int count, i, rate, nsPer;                                  \
    struct timeval start, end;                                  \
                                                                \
    gettimeofday(&start, 0);                                    \
                                                                \
    for (count = 0, i = 0; i < NUM_TESTS; ++i) {                \
        count += fxn(&sChecksums[i][off]);                      \
    }                                                           \
    gettimeofday(&end, 0);                                      \
    end.tv_usec -= start.tv_usec;                               \
    end.tv_sec  -= start.tv_sec;                                \
    if (end.tv_usec < 0) {                                      \
        end.tv_usec += 1000000;                                 \
        end.tv_sec  -= 1;                                       \
    }                                                           \
                                                                \
    nsPer = (int)((end.tv_usec * 1000) / NUM_TESTS);            \
    rate  = (int)(1000000000 / nsPer);                          \
                                                                \
    printf("%-30s  %d  %6dns each, %10d checks per second\n",   \
            lbl,                                                \
            count,                                              \
            nsPer,                                              \
            rate);                                              \
}

int main(int argc, char *argv[])
{
    init();
    BENCHMARK(IsZeroByOneLoop,          0,      "(A) Zero by one (loop)");
    BENCHMARK(IsZeroByOneUnrolled,      0,      "(A) Zero by one (unrolled)");
    BENCHMARK(IsZeroByOneOr,            0,      "(A) Zero by one (or)");
    BENCHMARK(IsZeroByFour,             0,      "(A) Zero by four");
    BENCHMARK(IsZeroByEight,            0,      "(A) Zero by eight");
    BENCHMARK(IsZeroFFS,                0,      "(A) Zero FFS");
    BENCHMARK(IsZeroByOneLoop,          1,      "(U) Zero by one (loop)");
    BENCHMARK(IsZeroByOneUnrolled,      1,      "(U) Zero by one (unrolled)");
    BENCHMARK(IsZeroByOneOr,            1,      "(U) Zero by one (or)");
    BENCHMARK(IsZeroByFour,             1,      "(U) Zero by four");
    BENCHMARK(IsZeroByEight,            1,      "(U) Zero by eight");
    BENCHMARK(IsZeroFFS,                1,      "(U) Zero FFS");
    return 0;
}
