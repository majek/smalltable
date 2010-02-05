import os
import sys
import random
import signal
from server_runner import start_server, stop_server

cmd = ' '.join(sys.argv[1:])

suffix = " >/dev/null 2>/dev/null"

os.system(cmd + " --help" + suffix)
os.system(cmd + " --listen an.invalid.host.name.indeed --tmpdir /tmp" + suffix)
os.system(cmd + " -v --port 65537" + suffix)
os.system(cmd + " -v --port \\-1" + suffix)
os.system(cmd + " --wrong-option" + suffix)


port = random.randint(32768, 65534)
srv = start_server(port, '%s -x --port %i --listen *' % (cmd, port), logname='tests-cmdline.log')

#bind second time on the same port
os.system(cmd + " --port %s " % (port,)  + suffix) # let bind fail

# test sigint in the meantime
os.kill(srv.pid, signal.SIGHUP)

stop_server(srv)

