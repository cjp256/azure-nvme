#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/nvme_ioctl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct nvme_lbaf {
    __le16 ms;
    __u8 ds;
    __u8 rp;
};

struct nvme_id_ns {
    __le64 nsze;
    __le64 ncap;
    __le64 nuse;
    __u8 nsfeat;
    __u8 nlbaf;
    __u8 flbas;
    __u8 mc;
    __u8 dpc;
    __u8 dps;
    __u8 nmic;
    __u8 rescap;
    __u8 fpi;
    __u8 dlfeat;
    __le16 nawun;
    __le16 nawupf;
    __le16 nacwu;
    __le16 nabsn;
    __le16 nabo;
    __le16 nabspf;
    __le16 noiob;
    __u8 nvmcap[16];
    __le16 npwg;
    __le16 npwa;
    __le16 npdg;
    __le16 npda;
    __le16 nows;
    __u8 rsvd74[18];
    __le32 anagrpid;
    __u8 rsvd96[3];
    __u8 nsattr;
    __le16 nvmsetid;
    __le16 endgid;
    __u8 nguid[16];
    __u8 eui64[8];
    struct nvme_lbaf lbaf[16];
    __u8 rsvd192[192];
    __u8 vs[3712];
};

#define MAX_PATH 4096

struct nvme_controller {
    char name[256];
    char dev_path[MAX_PATH];
    char sys_path[MAX_PATH];
    char model[MAX_PATH];
};

static bool debug = false;
static bool udev_mode = false;

#define DEBUG_PRINTF(fmt, ...)                                                 \
    do {                                                                       \
        if (debug == true) {                                                   \
            fprintf(stderr, "DEBUG: " fmt, ##__VA_ARGS__);                     \
        }                                                                      \
    } while (0)

/**
 * Given the path to a namespace, determine the namespace id.
 */
int get_namespace_id_for_path(const char *namespace_path) {
    int ctrl, nsid;

    sscanf(namespace_path, "/dev/nvme%dn%d", &ctrl, &nsid);
    return nsid;
}

/**
 * Dump environment variables.
 */
void debug_environment_variables() {
    extern char **environ;
    int i = 0;

    DEBUG_PRINTF("Environment Variables:\n");
    while (environ[i]) {
        DEBUG_PRINTF("%s\n", environ[i]);
        i++;
    }
}

/**
 * Query the NVMe namespace for the vendor specific data.
 *
 * For Azure devices, the vendor-specific data is current exposed as a string.
 * It will include the type of device and various properties in the format:
 * key1=value1,key2=value2,...\0.
 *
 * Anything beyond the terminating null-byte is currently undefined and
 * consequently ignored.
 */
char *query_namespace_vs(const char *namespace_path) {
    struct nvme_id_ns *ns = NULL;
    unsigned int nsid = get_namespace_id_for_path(namespace_path);

    int fd = open(namespace_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return NULL;
    }
    DEBUG_PRINTF("Opened device: %s with namespace id: %d...\n", namespace_path,
                 nsid);

    if (posix_memalign((void **)&ns, sysconf(_SC_PAGESIZE),
                       sizeof(struct nvme_id_ns)) != 0) {
        perror("posix_memalign");
        return NULL;
    }

    struct nvme_admin_cmd cmd = {
        .opcode = 0x06, // Identify command
        .nsid = nsid,
        .addr = (unsigned long)ns,
        .data_len = sizeof(ns),
    };

    int ret = ioctl(fd, NVME_IOCTL_ADMIN_CMD, &cmd);
    if (ret < 0) {
        perror("ioctl");
        return NULL;
    }
    close(fd);

    char *vs = strndup((const char *)ns->vs, sizeof(ns->vs));
    free(ns);

    return vs;
}

/**
 * Read file contents as a null-terminated string.
 *
 * For sysfs entries, etc. where the file contents are not null-terminated and
 * we know the contents are just a simple string (e.g. sysfs attributes).
 */
char *read_file_as_string(const char *path) {
    DEBUG_PRINTF("reading %s...\n", path);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("fopen");
        return NULL;
    }

    // Determine the file size
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate size of file plus one byte for null.
    char *contents = malloc(length + 1);
    if (contents == NULL) {
        perror("malloc");
        fclose(file);
        return NULL;
    }

    // Read file contents.
    fread(contents, 1, length, file);
    fclose(file);

    // Null-terminate the string.
    contents[length] = '\0';

    DEBUG_PRINTF("%s => %s\n", path, contents);
    return contents;
}

/**
 * Check if entry is a Microsoft Azure NVMe controller.
 */
int is_azure_nvme_controller(const struct dirent *entry) {
    char vendor_id_path[MAX_PATH];
    char *vendor_id = NULL;
    bool is_azure_nvme = true;

    // Name must start with "nvme".
    if (strncmp(entry->d_name, "nvme", 4) != 0) {
        return false;
    }

    // Vendor ID must be "0x1414".
    snprintf(vendor_id_path, sizeof(vendor_id_path),
             "/sys/class/nvme/%s/device/vendor", entry->d_name);

    vendor_id = read_file_as_string(vendor_id_path);
    if (vendor_id == NULL || strncmp(vendor_id, "0x1414", 6) != 0) {
        is_azure_nvme = false;
    }
    free(vendor_id);

    return is_azure_nvme;
}

/**
 * Check if entry is an NVMe namespace device.
 */
int is_nvme_namespace(const struct dirent *entry) {
    /*
     * NVMe namespace devices follow the format `/dev/nvme<#>n<#>`.
     * Partition devices may be exported which follow `/dev/nvme<#>n<#>p<#>`.
     */
    int ctrl, nsid;
    char p;

    return sscanf(entry->d_name, "nvme%dn%d%c", &ctrl, &nsid, &p) == 2;
}

/**
 * Enumerate namespaces for a controller.
 */
int enumerate_namespaces_for_controller(struct nvme_controller *ctrl) {
    struct dirent **namelist;

    int n = scandir(ctrl->sys_path, &namelist, is_nvme_namespace, alphasort);
    if (n < 0) {
        perror("scandir");
        return 1;
    }

    DEBUG_PRINTF("found %d namespace(s) for controller=%s:\n", n, ctrl->name);
    for (int i = 0; i < n; i++) {
        char namespace_path[MAX_PATH];
        const char *vs = NULL;

        snprintf(namespace_path, sizeof(namespace_path), "/dev/%s", namelist[i]->d_name);
        vs = query_namespace_vs(namespace_path);

        printf("%s: %s\n", namespace_path, vs);
        free(namelist[i]);
    }

    free(namelist);
    return 0;
}

/**
 * Enumerate Microsoft Azure NVMe controllers.
 */
int enumerate_azure_nvme_controllers() {
    struct dirent **namelist;

    int n = scandir("/sys/class/nvme", &namelist, is_azure_nvme_controller,
                    alphasort);
    if (n < 0) {
        perror("scandir");
        return 1;
    }

    DEBUG_PRINTF("found %d controllers\n", n);
    for (int i = 0; i < n; i++) {
        struct nvme_controller ctrl;
        strncpy(ctrl.name, namelist[i]->d_name, sizeof(ctrl.name));
        snprintf(ctrl.dev_path, sizeof(ctrl.dev_path), "/dev/%s", ctrl.name);
        snprintf(ctrl.sys_path, sizeof(ctrl.sys_path), "/sys/class/nvme/%s",
                 ctrl.name);

        enumerate_namespaces_for_controller(&ctrl);
        free(namelist[i]);
    }

    free(namelist);
    return 0;
}

/**
 * Print a udev key-value pair for environment variable.
 */
void print_udev_key_value(const char *key, const char *value) {
    printf("AZURE_NVME_");
    while (*key) {
        putchar(toupper((unsigned char)*key));
        key++;
    }
    printf("=%s\n", value);
}

/**
 * Print udev environment variables for all vs properties.
 * 
 * Example parsing for vs:
 *     type=local,index=2,name=nvme-600G-2
 * 
 * Environment variables printed include:
 *     AZURE_NVME_TYPE=local
 *     AZURE_NVME_INDEX=2
 *     AZURE_NVME_NAME=nvme-600G-2
 */
int parse_vs_for_udev_import(char *vs) {
    char *vs_copy = strdup(vs);
    char *outer_saveptr = NULL;
    char *inner_saveptr = NULL;

    char *pair = strtok_r(vs_copy, ",", &outer_saveptr);
    while (pair != NULL) {
        char *key = strtok_r(pair, "=", &inner_saveptr);
        char *value = strtok_r(NULL, "=", &inner_saveptr);

        if (key == NULL || value == NULL) {
            fprintf(stderr, "failed to parse key-value pair: %s\n", pair);
            free(vs_copy);
            return 1;
        }

        print_udev_key_value(key, value);
        pair = strtok_r(NULL, ",", &outer_saveptr);
    }

    free(vs_copy);
    return 0;
}

/**
 * Execute udev mode, printing out environment variables for import.
 * 
 * AZURE_NVME_VS: <vendor specific data>
 * AZURE_NVME_TYPE: <type, if specified>
 * AZURE_NVME_INDEX: <index for data disk, if specified>
 * AZURE_NVME_NAME: <name for disk, if specified>
 * etc...
 */
int execute_udev_import(void) {
    const char* dev_name = getenv("DEVNAME");
    if (dev_name == NULL) {
        fprintf(stderr, "environment variable 'DEVNAME' not set\n");
        return 1;
    }

    char *vs = query_namespace_vs(dev_name);
    if (vs == NULL) {
        fprintf(stderr, "failed to query namespace vendor-specific data\n");
        return 1;
    }

    printf("AZURE_NVME_VS=%s\n", vs);
    parse_vs_for_udev_import(vs);
    free(vs);
    return 0;
}

int main(int argc, const char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug = true;
            continue;
        }
        if (strcmp(argv[i], "--udev") == 0) {
            udev_mode = true;
            continue;
        }
    }

    if (debug) {
        debug_environment_variables();
    }

    if (udev_mode) {
        return execute_udev_import();
    } else {
        return enumerate_azure_nvme_controllers();
    }
}
