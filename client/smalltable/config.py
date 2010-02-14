
from smalltable.simpleclient import Client, ProxyClient

def only_lines(prefix, config):
    for line in config.split('\n'):
        if line.startswith(prefix):
            line, _, _ = line.strip().partition('#')
            yield line

def connect_proxies(config):
    hosts = []
    for line in only_lines("proxy", config):
        _, host = line.split()
        hosts.append( (host, ProxyClient(host, restart_delay_max=0.2)) )
    return dict( hosts )

def connect_servers(config):
    hosts = []
    for line in only_lines("server", config):
        c = line.replace(',', ' ').replace('\t', ' ').split()
        if len(c) == 3:
            _, host, weight = c
            hex_key = ''
        else:
            _, host, weight, hex_key = c
        weight = int(weight)
        hosts.append( (host, (Client(host, restart_delay_max=0.2), weight, hex_key.decode('hex')) ) )
    hosts.sort( key=lambda (h, (c,w,key)):key )
    for i in range(len(hosts)):
        host, (c, w, l_key) = hosts[i]
        if i != len(hosts)-1:
           _, (_, _, r_key) = hosts[i+1]
        else:
            r_key = None
        hosts[i] = (host, (c, w, (l_key, r_key)))
    return dict( hosts )

def dump_config(options):
    l = []
    for host, mc in options.proxies.iteritems():
        l.append("proxy\t%s" % (host,))
    for host, (mc, weight, (l_key, r_key)) in options.servers.iteritems():
        l.append("server\t%s, %i, %s" % (host, weight, l_key.encode('hex')))
    return '\n'.join(l) + '\n'

def drop_comments(config):
    new_lines = []
    for line in config.split('\n'):
        if not line.strip().startswith('#'):
            new_lines.append( line )
    return '\n'.join(new_lines)


def matches(local, remote):
    if drop_comments(local) == drop_comments(remote):
        return True
    return False


