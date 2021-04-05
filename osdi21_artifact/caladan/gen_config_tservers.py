# Usage: python gen_config_servers.py <num-servers> 

import sys

num_servers = int(sys.argv[1])

server_config="""host_addr 192.168.10.%d
host_netmask 255.255.255.0
host_gateway 192.168.10.1
runtime_kthreads 1
runtime_guaranteed_kthreads 1
runtime_spinning_kthreads 1
#enable_storage 1
enable_directpath 1
"""

for i in range(1, num_servers+1):
    f = open('thru_server%d.config'%(i), 'w')
    f.write(server_config % (150 + i))
    f.close()

print('Config files written')