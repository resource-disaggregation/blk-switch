# Usage: python gen_config_lserver.py <num-cores> <ssd> 

import sys

num_cores = int(sys.argv[1])
storage = sys.argv[2]

ssd = False
if(storage.strip() == 'ssd'):
    ssd = True

server_config="""host_addr 192.168.10.116
host_netmask 255.255.255.0
host_gateway 192.168.10.1
runtime_kthreads %d
runtime_guaranteed_kthreads %d
runtime_spinning_kthreads %d
enable_directpath 1
"""

if ssd:
    server_config += "\nenable_storage 1\n"

f = open('lat_server.config', 'w')
f.write(server_config % (num_cores, num_cores, num_cores))
f.close()

print('L-app server config file written')