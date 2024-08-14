#include "proc_stat.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cpuline_t {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
};

struct load_average_internal_t {
    FILE *proc_stat;
    struct cpuline_t cpuline[];
};

static int parse_cpu_line(const char *line, struct cpuline_t *cpuline);


struct load_average_t *init_load_average() {
    FILE *proc_stat_file = fopen("/proc/stat", "rt");
    if (proc_stat_file == NULL)
        return NULL;

    // Get number of CPU strings
    int ncpu = 0;
    char line[256];
    while (fgets(line, sizeof(line), proc_stat_file) != 0) {
        if (strncmp(line, "cpu", 3) == 0) {
            ncpu++;
        }
    }

    // Initialize structures
    struct load_average_t *load_avg =
        malloc(sizeof(struct load_average_t) + sizeof(double) * ncpu);

    if (!load_avg) // Not enogh memory?
        goto init_failure; // NB: The one and only reason to use goto is to emulate C++ try/catch

    load_avg->internal = malloc(sizeof(struct load_average_internal_t) +
                                ncpu * sizeof(struct cpuline_t));
    if(!load_avg->internal) // Not enogh memory?
        goto init_failure;
    load_avg->ncpu = ncpu;
    load_avg->internal->proc_stat = proc_stat_file;

    rewind(proc_stat_file);

    while (fgets(line, sizeof(line), proc_stat_file) != 0) {
        if (strncmp(line, "cpu", 3) == 0) {
            char *endptr = line+4; // move to end of "cpu "
            unsigned long cpu_index = 0; // CPU number. 1-based. Zero for total
            if(line[3] != ' ') { // cpu-specific line. Get CPU index and move to end of first column
                cpu_index = strtoul(line+3, &endptr, 10)+1;
            }
            if(!parse_cpu_line(endptr, &load_avg->internal->cpuline[cpu_index])) {
                // Error on CPU line?!
                goto init_failure;
            }
        }
        else // CPU's are on top. so we can break as soon as the ended
            break;
    }

    return load_avg;

init_failure:
    if (load_avg) {
        free(load_avg->internal);
        free(load_avg);
    }
    if (proc_stat_file) {
        fclose(proc_stat_file);
    }
    return NULL;
}

int get_load_average(struct load_average_t *load_avg)
{
    freopen(NULL, "r", load_avg->internal->proc_stat);
    char line[256];
    while (fgets(line, sizeof(line), load_avg->internal->proc_stat) != 0) {
        if (strncmp(line, "cpu", 3)==0) {
            char *endptr = line+4; // move to end of "cpu "
            unsigned long cpu_index = 0; // CPU number. 1-based. Zero for total
            if(line[3] != ' ') { // cpu-specific line. Get CPU index and move to end of first column
                cpu_index = strtoul(line+3, &endptr, 10)+1;
            }
            struct cpuline_t cpuline;
            if(!parse_cpu_line(endptr, &cpuline)) {
                // Error on CPU line?!
                return 0;
            }
            struct cpuline_t* last_cpuline = &load_avg->internal->cpuline[cpu_index];
            struct cpuline_t delta = {
                .user = cpuline.user - last_cpuline->user,
                .system = cpuline.system - last_cpuline->system,
                .nice = cpuline.nice - last_cpuline->nice,
                .idle = cpuline.idle - last_cpuline->idle,
            };
            uint64_t total = delta.user+delta.system+delta.nice+delta.idle;
            uint64_t busy = delta.user+delta.system+delta.nice;
            load_avg->load_avg[cpu_index] = busy*1.0/total;
            load_avg->internal->cpuline[cpu_index] = cpuline;
        }
        else // CPU's are on top. so we can break as soon as the ended
            break;
    }
    return 1;
}

int parse_cpu_line(const char *line, struct cpuline_t *cpuline)
{
    int ret =
        sscanf(line,
                     "%" __UINT64_FMTu__ " %" __UINT64_FMTu__ " %" __UINT64_FMTu__
                     " %" __UINT64_FMTu__,
                     &cpuline->user, &cpuline->system, &cpuline->nice, &cpuline->idle);
    if (ret != 4)
        return 0;
    return 1;
}


void free_load_average(struct load_average_t *load_avg)
{
    if(load_avg) {
        if(load_avg->internal) {
            fclose(load_avg->internal->proc_stat);
            free(load_avg->internal);
        }
        free(load_avg);
    }
}
