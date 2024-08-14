#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "proc_stat.h"


struct data_message_t { // all fields are 64bit long to simplify data alignment in communication
    uint64_t secs; //timespec.tv_sec
    uint64_t nsecs; //timespec.tv_usec
    uint64_t ncpu; // number of cpu lines in /proc/stat equal to number of CPUs+total
    double load_avg[]; // load averages
};

void print_usage(const char * appname);

int main(int argc, char *argv[])
{
    // default arguments

    double freq=1.0;
    int verbosity = 0;
    struct sockaddr_in receiver_addr = {
        .sin_family = AF_INET,
        .sin_addr = {htonl(INADDR_LOOPBACK)},
        .sin_port = htons(1234)
    };
    int sender_socket = -1;


    // parse arguments
    static struct option long_option[] = {
        {"freq", required_argument, 0, 'f'},
        {"host", required_argument, 0, 's'},
        {"port", required_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0},
        };
    int opt;
    while ((opt = getopt_long(argc, argv, "f:s:p:vh", long_option, 0)) != -1) {
        switch (opt) {
        case 'f':
        {
            char *endptr = 0;
            freq = strtod(optarg, &endptr);
            if((errno == ERANGE) ||
                (strlen(optarg) != (endptr-optarg)) ||
                (freq == 0)) {
                fprintf(stderr, "Error: Invalid frequency\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
        case 's':
        {
            int ret = inet_pton(AF_INET, optarg, &receiver_addr.sin_addr);
            if (ret < 0) {
                perror("inet_pton");
                exit(EXIT_FAILURE);
            }
            else if (ret == 0) {
                fprintf(stderr, "Invalid address format");
                exit(EXIT_FAILURE);
            }
            break;
        }
        case 'p':
        {
            char *endptr = 0;
            unsigned long port = strtoul(optarg, &endptr, 0);
            if((errno == ERANGE) ||
                (strlen(optarg) != (endptr-optarg)) ||
                port > 0xFFFF) {
                fprintf(stderr, "Error: Invalid port\n");
                exit(EXIT_FAILURE);
            }
            receiver_addr.sin_port = htons(port);
            break;
        }
        case 'v':
            verbosity++;
            break;
        case 'h':
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;
        case '?':
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
            break;
        case -1:
            break;
        }
    }
    if(optind < argc) {
        fprintf(stderr, "Unknown non-option arguments passed\n");
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(verbosity > 0) {
        char receiver_addrstr[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &receiver_addr.sin_addr, receiver_addrstr,
                      sizeof(receiver_addrstr)) == NULL) {
            perror("inet_ntop");
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "Query processor load average at %f Hz\n", freq);
        fprintf(stderr, "Send data to %s:%d\n", receiver_addrstr,
                ntohs(receiver_addr.sin_port));
    }

    // create socket
    sender_socket = socket(AF_INET, SOCK_DGRAM, 0);
    // skip socket binding, since outgoing port has not been specified in terms of reference
    struct load_average_t *load_avg = init_load_average();
    if(!load_avg) {
        perror("init_load_average");
        exit(EXIT_FAILURE);
    }

    double delay = 1.0/freq;
    struct timespec duration = {
        .tv_sec = (int)floor(delay),
        .tv_nsec = floor((delay-floor(delay))*1000000000)
    };

    size_t msgSize = sizeof(struct data_message_t)+sizeof(double)*load_avg->ncpu;
    struct data_message_t* msg = malloc(msgSize);

    while(1) {
        int sleep_result = 0;
        struct timespec remainder = duration;
        do {
            sleep_result = nanosleep(&remainder, &remainder);
            if(sleep_result == -1 && errno != EINTR) {
                perror("nanoslep");
                exit(EXIT_FAILURE);
            }
        } while(sleep_result != 0);

        if(!get_load_average(load_avg)) {
            fprintf(stderr, "Error parsing /proc/stat file");
            exit(EXIT_FAILURE);
        }
        struct timespec curtime;
        clock_gettime(CLOCK_REALTIME, &curtime);
        if (verbosity > 1) {
            struct tm localtime_tm;
            localtime_r(&curtime.tv_sec, &localtime_tm);
            fprintf(stderr, "\nTime: %04d-%02d-%02d %02d:%02d:%02d.%03ld\n", localtime_tm.tm_year+1900,
                    localtime_tm.tm_mon, localtime_tm.tm_mday,
                    localtime_tm.tm_hour, localtime_tm.tm_min,
                    localtime_tm.tm_sec, curtime.tv_nsec / 1000000);
            fprintf(stderr, "cpu total load: %.2f%%\n",
                    load_avg->load_avg[0] * 100);
            for (int cpu = 1; cpu < load_avg->ncpu; ++cpu) {
                fprintf(stderr, "cpu%d load: %.2f%%\n", cpu - 1,
                        load_avg->load_avg[cpu] * 100);
            }
        }

        msg->secs = htobe64(curtime.tv_sec);
        msg->nsecs = htobe64(curtime.tv_nsec);
        msg->ncpu = htobe64(load_avg->ncpu);
        memcpy(msg->load_avg, load_avg->load_avg, sizeof(double)*load_avg->ncpu);

        ssize_t send_ret = sendto(sender_socket, msg, msgSize, 0, (struct sockaddr*)&receiver_addr, sizeof(receiver_addr));
        if(send_ret < 0) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }

    }

    free(msg);
    free_load_average(load_avg);
    close(sender_socket);
    return EXIT_SUCCESS;
}

void print_usage(const char * appname)
{
    fprintf(stderr,
            "Usage: %s [-h] [-f FREQ] [-s HOST] [-p PORT] [-v]:\n"
            "options:\n"
            "  -h, --help            show thuis help message and exit\n"
            "  -f FREQ, --freq FREQ  CPU load average query frequency (default 1Hz)\n"
            "  -s HOST, --host HOST  Receiver host (default localhost)\n"
            "  -p PORT, --port PORT  Receiver port (default 123)\n"
            "  -v                    Output verbosity\n",
            appname);
}
