# List of available cores in NUMA order
# NUMA node to core mapping can be obtained by running `lscpu | grep "CPU(s)"`
# For example if the the output of the above command is as follows:
# NUMA node0 CPU(s):     0,2,4,6
# NUMA node1 CPU(s):     1,3,5,7
# Then you should set CORE_LIST="0,2,4,6,1,3,5,7"
CORE_LIST="0,4,8,12,16,20,1,5,9,13,17,21,2,6,10,14,18,22"

# You can just set this to the number of NUMA nodes on the machine (check `lscpu | grep "CPU(s)"`)
# TROUBLESHOOTING: Machines in our setup had 4 NUMA nodes. However, we ran into issues with getting Caladan
# to run on all 4 of the NUMA nodes. In this case, limiting the number the number of usable NUMA nodes
# solved the problem. The below variable can be used for this purpose. If you run into issues getting 
# Caladan to work, you may want to try reducing the NUMA_CAP, and seeing if that helps.
# Note: if you set this variable to a smaller value than the number of NUMA nodes on you machine, 
# then you also need to update CORE_LIST accordingly. 
NUMA_CAP=3

