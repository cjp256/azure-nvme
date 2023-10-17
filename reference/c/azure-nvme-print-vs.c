#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/nvme_ioctl.h>
#include <sys/ioctl.h>

struct nvme_lbaf {
	__le16			ms;
	__u8			ds;
	__u8			rp;
};

struct nvme_id_ns {
	__le64			nsze;
	__le64			ncap;
	__le64			nuse;
	__u8			nsfeat;
	__u8			nlbaf;
	__u8			flbas;
	__u8			mc;
	__u8			dpc;
	__u8			dps;
	__u8			nmic;
	__u8			rescap;
	__u8			fpi;
	__u8			dlfeat;
	__le16			nawun;
	__le16			nawupf;
	__le16			nacwu;
	__le16			nabsn;
	__le16			nabo;
	__le16			nabspf;
	__le16			noiob;
	__u8			nvmcap[16];
	__le16			npwg;
	__le16			npwa;
	__le16			npdg;
	__le16			npda;
	__le16			nows;
	__u8			rsvd74[18];
	__le32			anagrpid;
	__u8			rsvd96[3];
	__u8			nsattr;
	__le16			nvmsetid;
	__le16			endgid;
	__u8			nguid[16];
	__u8			eui64[8];
	struct nvme_lbaf	lbaf[16];
	__u8			rsvd192[192];
	__u8			vs[3712];
};

int main(int argc, const char **argv) {
    if (argc < 2) {
        fprintf(stderr, "error: specify device\n");
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct nvme_id_ns *ns;
    if (posix_memalign((void **)&ns, sysconf(_SC_PAGESIZE), sizeof(struct nvme_id_ns)) != 0) {
        perror("posix_memalign");
        return 1;
    }

    struct nvme_admin_cmd cmd = {
        .opcode = 0x06,  // Identify command
        .nsid = 1,       // Namespace ID
        .addr = (unsigned long)ns,
        .data_len = sizeof(ns),
    };

    int ret = ioctl(fd, NVME_IOCTL_ADMIN_CMD, &cmd);
    if (ret < 0) {
        perror("ioctl");
        return 1;
    }
 
    close(fd);

    if (ns->vs[0] != 0) {
        printf("%s\n", ns->vs);
    } else {
        fprintf(stderr, "error: no identifier found\n");
    }

    free(ns);
    return 0;
}
