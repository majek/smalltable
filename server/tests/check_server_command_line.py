import os
import sys
import random
from server_runner import start_server, stop_server

cmd = ' '.join(sys.argv[1:])

suffix = " >/dev/null 2>/dev/null"

#start_server(port, server_cmd, logname='tests-cmdline.log')
#stop_server(proc, signo=signal.SIGINT)

os.system(cmd + " --listen an.invalid.host.name.indeed" + suffix)
os.system(cmd + " -v --port 65537" + suffix)
os.system(cmd + " -v --port \\-1" + suffix)
os.system(cmd + " --wrong-option" + suffix)
os.system(cmd + " --vx32sdk /tmp " + suffix)


port = random.randint(32768, 65534)
srv = start_server(port, '%s -x --port %i --listen *' % (cmd, port), logname='tests-cmdline.log')

#bind second time on the same port
os.system(cmd + " --port %s " % (port,)  + suffix) # let bind fail
stop_server(srv)

