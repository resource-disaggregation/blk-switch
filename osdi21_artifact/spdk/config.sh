# Set this to the name of the interface on the NIC that should be used
# List of available interfaces can be obtained via `ifconfig`
# In our setup we use a 100Gbps NIC. Speed of an interface can be obtained via `ethtool <interface-name> | grep -i speed` 
IFACE="enp37s0f1"

# Set this to the IP address corresponding to the above interface
# This can be obtained via `ifconfig`
IP_ADDR="192.168.10.115"

# If you have an NVMe SSD attached to the machine, specify it's PCIe bus address here
# If there is not SSD, just set it to "none"
# One way to look for SSDs is to run `lspci | grep -i nvme`
# For example on our machine, the output of this is as follows:
# `dc:00.0 Non-Volatile memory controller: Samsung Electronics Co Ltd NVMe SSD Controller 172Xa/172Xb (rev 01)`
# Here the first column is showing the SSDs PCIe address. dc:00.0 => 0000:db:00.0 (you need to add the zeros in front)
#SSD_ADDR="none"
SSD_ADDR="0000:dc:00.0"