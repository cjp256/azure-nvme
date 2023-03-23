# Experimental udev rules for Azure

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


