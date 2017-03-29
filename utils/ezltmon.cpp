#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#define MAX_TZ_COUNT         10
#define THERMAL_SYSFS_PATH  "/sys/class/thermal"

struct tz_data {
    char type[32];
    int temp_fd;
};

static struct tz_data tz_array[MAX_TZ_COUNT];
static int tz_count;

int get_temp(int id)
{
    char buffer[32];

    if (tz_array[id].temp_fd < 0) {
        return -1;
    }

    lseek(tz_array[id].temp_fd, 0, SEEK_SET);
    read(tz_array[id].temp_fd, buffer, 32);
    return atoi(buffer);
}

void print_temps(int log_fid)
{
    int temps[MAX_TZ_COUNT];
    char buffer[512];

    for (int i = 0; i < tz_count; i++) {
        temps[i] = get_temp(i);
    }

    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    unsigned long long timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    snprintf(buffer, 512, "%llu", timestamp);
    for (int i = 0; i < tz_count; i++) {
        char temp_str[16];
        snprintf(temp_str, 16, "%d", temps[i]);
        strcat(buffer, ", ");
        strcat(buffer, temp_str);
    }
    strcat(buffer, "\n");
    write(log_fid, buffer, strlen(buffer));

    for (int i = 0; i < tz_count; i++) {
        printf("%s: %d\n", tz_array[i].type, temps[i]);
    }
}

void clear_lines()
{
    for (int i = 0; i < tz_count; i++) {
        printf("\033[A");
        printf("\33[2K");
    }
}

int open_tz_files()
{
    for (int i = 0; i < tz_count; i++) {
        char filename[80];

        snprintf(filename, 80, "%s/thermal_zone%d/temp", THERMAL_SYSFS_PATH, i);

        int fd = open(filename, O_RDONLY);
        if (fd >= 0) {
            tz_array[i].temp_fd = fd;
        }

        snprintf(filename, 80, "%s/thermal_zone%d/type", THERMAL_SYSFS_PATH, i);
        fd = open(filename, O_RDONLY);
        if (fd >= 0) {
            read(fd, tz_array[i].type, 32);
            char *nl;
            if ((nl = strchr(tz_array[i].type, '\n')) != NULL) {
                *nl = '\0';
            }
            close(fd);
        }
    }

    return 0;
}

int close_tz_files()
{
    for (int i = 0; i < tz_count; i++) {
        if (tz_array[i].temp_fd >= 0) {
            close(tz_array[i].temp_fd);
        }
    }

    return 0;
}

int log_init(char *filename)
{
    int fid = open(filename, O_CREAT | O_TRUNC | O_RDWR);

    if (fid < 0) {
        printf("Failed to open file \"%s\" for logging (%d)\n", filename, fid);
    } else {
        printf("Opened file \"%s\" for logging\n", filename);

        char buffer[512];
        snprintf(buffer, 512, "Time");
        for (int i = 0; i < tz_count; i++) {
            strcat(buffer, ", ");
            strcat(buffer, tz_array[i].type);
        }
        strcat(buffer, "\n");
        write(fid, buffer, strlen(buffer));
    }

    return fid;
}

int find_tzs()
{
    DIR *thermal_dir = opendir(THERMAL_SYSFS_PATH);
    if (thermal_dir == NULL)
        return -1;

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(thermal_dir)) != NULL) {
        if (strncmp(entry->d_name, "thermal_zone", 12) == 0) {
            count++;
        }

        if (count == MAX_TZ_COUNT) {
            printf("Reached max of %d thermal zones\n", count);
            break;
        }
    }

    return count;
}

int main(int argc, char **argv) {
    int poll_delay_ms = 100;
    char log_filename[80];
    bool log_enable = false;
    int log_fid = -1;
    int c;

    // get command-line arguments
    while ((c = getopt(argc, argv, ":d:l:")) != -1) {
        switch (c) {
        case 'd':
            poll_delay_ms = atoi(optarg);
            break;
        case 'l':
            strncpy(log_filename, optarg, 80);
            log_enable = true;
            break;
        case ':':
            printf("Option '-%c' requires an argument\n", optopt);
            return -1;
        case '?':
            printf("Unknown option '-%c'\n", c);
            return -1;
        default:
            return -1;
        }
    }

    tz_count = find_tzs();
    printf("Found %d thermal zones\n", tz_count);
    if (tz_count <= 0) {
        return -1;
    }

    open_tz_files();

    if (log_enable)
        log_fid = log_init(log_filename);


    for (int i = 0; i < tz_count; i++) {
        printf("Thermal Zone %d: \"%s\"\n", i, tz_array[i].type);
    }

    while (1) {
        print_temps(log_fid);
        usleep(poll_delay_ms * 1000);
        clear_lines();
    }

    close_tz_files();
    close(log_fid);

    return 0;
}

