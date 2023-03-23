# Experimental udev rules for Azure NVMe devices

## Installation

Download rules:

```
wget -O 80-azure-nvme.rules https://raw.githubusercontent.com/cjp256/azure-nvme/main/80-azure-nvme.rules
```

Sanity check rules:

```
cat 80-azure-nvme.rules
```

Install rules and reboot:

```
sudo cp 80-azure-nvme.rules /lib/udev/rules.d/
sudo reboot
```

## Links

`/dev/disk/azure/os`: Links to NVMe OS disk.

`/dev/disk/azure/data/by-lun/<lun>`: Links to NVMe data disks by user-configured lun.

`/dev/disk/azure/data/by-nguid/<nguid>`: Links to NVMe data disks by nguid.

`/dev/disk/azure/temp/by-nguid/<nguid>`: Links to NVMe local temp/resource disks by nguid.

