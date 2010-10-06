import optparse
import os
from smalltable.simpleclient import ProxyClient, Client
from smalltable.simpleclient import ConnectionError
import sys

from . import config

import logging
log = logging.getLogger(os.path.basename(__file__))
FORMAT_CONS = '%(asctime)s %(name)-12s %(levelname)8s\t%(message)s'
logging.basicConfig(level=logging.DEBUG, format=FORMAT_CONS)


def check_health(options):
    online = {}
    for host, mc in options.proxies.iteritems():
        try:
            mc.noop()
            online[host] = True
        except ConnectionError:
            online[host] = False

    for host, (mc, weight, key) in options.servers.iteritems():
        try:
            mc.noop()
            online[host] = True
        except ConnectionError:
            online[host] = False

    failed = False
    if sum([1 for host, is_online in online.items() if is_online is False]):
        failed = True

    print "\n [*] proxies"
    for host, mc in options.proxies.iteritems():
        if not online[host]:
            print "     %s %s" % (host, 'OFFLINE')
        else:
            local_config = options.config
            remote_config = mc.get_config()
            matches = config.matches(local_config, remote_config)
            if not matches:
                failed = True
            print  "     %s %s config=%s" % (host, 'online',
                    'ok' if matches else 'FAILED',
                    )

    print "\n [*] servers"
    for host, (mc, weight, key) in options.servers.iteritems():
        print "     %s %s %s %r" % (host,
                    'online' if online[host] else 'OFFLINE',
                    weight, key
                )
    print
    return failed


def do_health(options):
    return

def key_difference(old, new, ratio):
    print "getting keys"
    old_cas_keys = set(old.get_keys())
    old_keys = sorted(k for i_cas, k in old_cas_keys)
    
    boundry_idx = int(ratio * float(len(old_keys)))
    boundry = old_keys[ boundry_idx ]
    print "Boundry is %r %.3f (will have keys old=%i new=%i)" % (boundry, ratio, boundry_idx, len(old_keys)-boundry_idx)

    old_keys = set(old_keys[boundry_idx:])
    new_keys = set(k for i_cas, k in new.get_keys())

    to_copy = old_keys - new_keys
    were_ok = len(old_keys) - len(to_copy)
    to_del = new_keys - old_keys
    
    print "Copying %i keys from old server, %i keys assumed to be up-to-date on new server, removing %i keys from new server." % (len(to_copy), were_ok, len(to_del))
    print "Press enter to continue."
    raw_input()
    
    to_copy = list(to_copy)
    while to_copy:
        to_copy_now, to_copy = to_copy[:64000], to_copy[64000:]
        a = old.clone_get_multi(to_copy_now)
        new.clone_set_multi(a)
        print >> sys.stderr, 'c',
    
    while to_del:
        to_del_now, to_del = to_del[:64000], to_del[64000:]
        new.delete_multi( to_del_now )
        print >> sys.stderr, 'd',
    print >> sys.stderr, ''
    return boundry, set([(i_cas, k) for i_cas, k in old_cas_keys if k >= boundry])


def do_add(options, oldhost, newhost, new_weight=10):
    new_weight = int(new_weight)
    assert oldhost in options.servers
    old, old_weight, (l_key, r_key) = options.servers[oldhost]
    assert ':' in newhost
    assert newhost not in options.servers
    new = Client(newhost)
    new.noop()
    boundry, old_cas_key = key_difference(old, new, float(old_weight)/(old_weight+new_weight))

    print "Locking all proxies"
    for host, mc in options.proxies.iteritems():
        print "    %s" % (host,),
        mc.stop()
        print "ok"

    upd_cas_key = set((i, k) for i, k in old.get_keys() if k >= boundry)
    new_keys = [k for _,k in (upd_cas_key - old_cas_key)]
    del_keys = list(set([k for _,k in old_cas_key]) - set([k for _,k in upd_cas_key]))
    print "Updated items %i, removed items %i" % (len(new_keys), len(del_keys))
    a = old.clone_get_multi(new_keys)
    new.clone_set_multi(a)
    new.delete_multi( del_keys )

    options.servers[oldhost] = (old, old_weight, (l_key, boundry) )
    options.servers[newhost] = (new, new_weight, (boundry, r_key) )

    new_conf = config.dump_config(options)
    print new_conf
    print "Updating proxy config"
    for host, mc in options.proxies.iteritems():
        print "%s" % (host,),
        mc.set_config(new_conf)
        print "ok"

    print "Unlocking all proxies"
    for host, mc in options.proxies.iteritems():
        print "    %s" % (host,),
        mc.start()
        print "ok"

    del_keys = [k for _,k in upd_cas_key]
    print "Removing %i keys from old server" % (len(del_keys),)
    while del_keys:
        del_keys_now, del_keys = del_keys[:64000], del_keys[64000:]
        old.delete_multi( del_keys_now )

    return

ACTIONS = {
    'HEALTH' : do_health,
    'ADD': do_add,
}

def main(args):
    usage = "usage: %prog [options] CONFIGHOST ACTION"
    desc = "smalltable-manager - reconfigure your smalltable cluster"
    parser = optparse.OptionParser(usage, description=desc)
    parser.add_option("-v", "--verbose",
                      action="count", dest="verbosity",
                      help="increase the verbosity of debugging")

    (options, left_args) = parser.parse_args(args=args)
    #if len(left_args) != 2:
    #    parser.error("Incorrect number of arguments. "
    #                 "Specify at least HOST and ACTION.")

    (host, action), action_args = left_args[:2], left_args[2:]
    if action.upper() not in ACTIONS:
        parser.error("ACTION must be one of: %s" % ('|'.join(ACTIONS.keys()),) )

    mc = ProxyClient(host)
    options.config = mc.get_config()
    mc.close()
    options.proxies = config.connect_proxies(options.config)
    options.servers = config.connect_servers(options.config)

    failed = check_health(options)
    if failed:
        print "Problems found. QUITTING."
        sys.exit(-1)
    ACTIONS[action.upper()](options, *action_args)


if __name__ == '__main__':
    main(sys.argv[1:])
