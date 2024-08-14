#ifndef PROC_STAT_H
#define PROC_STAT_H

#include <stdint.h>

struct load_average_internal_t;
struct load_average_t
{
    struct load_average_internal_t *internal;
    uint64_t ncpu; // number of cpu lines in /proc/stat equal to number of CPUs+total
    double load_avg[];
};

struct load_average_t* init_load_average(void);
int get_load_average(struct load_average_t*);
void free_load_average(struct load_average_t*);


#endif // PROC_STAT_H
