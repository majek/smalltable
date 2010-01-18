
import subprocess
import socket
import signal
import os
import sys
import threading

# Server can send SIGURG to parent (us) once it's started. This can speedup
# startup phase, as we don't have to poll for open socket.
event = threading.Event()
def urg_handler(signum, frame):
    event.set()

def start_server(port, server_cmd, logname='tests.log'):
    signal.signal(signal.SIGURG, urg_handler)
    event.clear()

    fd = open(logname, 'w')
    proc = subprocess.Popen(server_cmd.split(), stdout=fd.fileno(), stderr=fd.fileno())

    for i in range(30):
        event.wait(0.5) # to be stopped by a signal SIGURG
        if event.isSet():
            event.clear()
        try:
            sd = socket.socket()
            sd.settimeout(1.0)
            sd.connect( ('127.0.0.1', port) )
        except socket.error:
            pass
        else:
            sd.close()
            break
    else:
        print >> sys.stderr, "cmd: %r" % (server_cmd, )
        raise Exception("Can't connect to port %i." % (port,))

    signal.signal(signal.SIGURG, signal.SIG_IGN)
    proc.fd = fd
    return proc


def stop_server(proc, signo=signal.SIGINT):
    os.kill(proc.pid, signo)
    pid, exit_code = os.waitpid(proc.pid, 0)
    proc.fd.close()


if __name__ == '__main__':
    port = int(sys.argv[1])
    server_cmd = sys.argv[2]
    test_cmd = sys.argv[3]

    srv = start_server(port, server_cmd)
    ret = os.system(test_cmd)
    stop_server(srv)
    sys.exit(ret)


