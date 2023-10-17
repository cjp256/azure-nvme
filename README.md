# Experimental udev rules for Azure NVMe devices

## Installation

Install rules and reboot:

```
git clone https://github.com/cjp256/azure-nvme
sudo cp 80-azure-nvme.rules /etc/udev/rules.d/
sudo cp azure-nvme-id /usr/sbin/azure-nvme-id
sudo reboot
```

## azure-nvme-id

azure-nvme-id supports two commands:

- identify: to identify devices

- parse: to parse Azure's vendor-specific id string

### azure-nvme-id identify

```
$ sudo azure-nvme-id identify
/dev/nvme0n1: type=local,name=nvme-150G-1
/dev/nvme1n3: type=unknown
/dev/nvme1n2: type=unknown
/dev/nvme1n1: type=unknown

$ sudo azure-nvme-id identify -o json
{
    "/dev/nvme0n1": {
        "name": "nvme-150G-1",
        "type": "local",
        "vs": "type=local,name=nvme-150G-1"
    },
    "/dev/nvme1n1": {
        "type": "unknown"
    },
    "/dev/nvme1n2": {
        "type": "unknown"
    },
    "/dev/nvme1n3": {
        "type": "unknown"
    }
}

$ sudo azure-nvme-id identify --device /dev/nvme0n1
/dev/nvme0n1: type=local,name=nvme-150G-1

$ sudo azure-nvme-id identify --device /dev/nvme0n1 -o raw
type=local,name=nvme-150G-1
```

### azure-nvme-id parse

```
$ azure-nvme-id parse --key name --vs "type=local,name=nvme-150G-1"
nvme-150G-1
```

## Links

### Using vendor-specific identifiers

`/dev/disk/azure/os`: Links to NVMe OS disk.

`/dev/disk/azure/data/by-lun/<lun>`: Links to NVMe data disks by user-configured lun.

`/dev/disk/azure/data/by-name/<name>`: Links to NVMe data disks by user-configured name.

`/dev/disk/azure/data/by-nguid/<nguid>`: Links to NVMe data disks by nguid.

`/dev/disk/azure/local/by-index/<index>`: Links to NVMe local disk by index.

`/dev/disk/azure/local/by-name/<name>`: Links to NVMe local disk by name.

`/dev/disk/azure/local/by-nguid/<nguid>`: Links to NVMe local disk by nguid.

### Using lun calculation method which we want to avoid

`/dev/disk/azure/xos`: Links to NVMe OS disk.

`/dev/disk/azure/xdata/by-xlun/<lun>`: Links to NVMe data disks by user-configured lun.

`/dev/disk/azure/xdata/by-nguid/<nguid>`: Links to NVMe data disks by nguid.

`/dev/disk/azure/xlocal/by-nguid/<nguid>`: Links to NVMe local disks by nguid.

### Example

```
$ find /dev/disk/azure/ -type l | xargs ls -al
lrwxrwxrwx 1 root root 19 Oct 17 15:42 /dev/disk/azure/local/by-name/nvme-150G-1 -> ../../../../nvme0n1
lrwxrwxrwx 1 root root 19 Oct 17 15:42 /dev/disk/azure/local/by-nguid/6aeb1017-eb3c-9959-9af6-7ca22be8b995 -> ../../../../nvme0n1
lrwxrwxrwx 1 root root 19 Oct 17 15:42 /dev/disk/azure/xdata/by-nguid/8013651d-5353-458a-8385-cb7a038eca01 -> ../../../../nvme1n2
lrwxrwxrwx 1 root root 19 Oct 17 15:42 /dev/disk/azure/xdata/by-nguid/9e1d99e5-1d7f-4d1c-a898-baf38df8cd01 -> ../../../../nvme1n3
lrwxrwxrwx 1 root root 19 Oct 17 15:42 /dev/disk/azure/xdata/by-xlun/0 -> ../../../../nvme1n2
lrwxrwxrwx 1 root root 19 Oct 17 15:42 /dev/disk/azure/xdata/by-xlun/1 -> ../../../../nvme1n3
lrwxrwxrwx 1 root root 13 Oct 17 15:42 /dev/disk/azure/xos -> ../../nvme1n1
```
