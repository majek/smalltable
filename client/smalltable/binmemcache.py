#!/usr/bin/env python

DEFAULT_PORT=22122

try:
    import cPickle as pickle
except ImportError: #pragma: no cover
    import pickle

import sys
import socket
import struct
import select
import heapq
import time
import random
import collections
import fcntl
import os
import md5
import functools

import logging
log = logging.getLogger(__name__)

from . import simplebuffer


STATUS_NO_ERROR=0x0000
STATUS_KEY_NOT_FOUND=0x0001
STATUS_KEY_EXISTS=0x0002
STATUS_VALUE_TOO_BIG=0x0003
STATUS_INVALID_ARGUMENTS=0x0004
STATUS_ITEM_NOT_STORED=0x0005
STATUS_UNKNOWN_COMMAND=0x0081
STATUS_OUT_OF_MEMORY=0x0082


class MemcachedError(Exception):
    def __init__(self, opcode=None, status=None, value=None):
        self.opcode = opcode
        self.status = status
        self.value = value
        Exception.__init__(self)

    def __str__(self):
        return '<%s opcode=%i status=%i value=%r>' % (
                    self.__class__.__name__,
                    self.opcode,
                    self.status,
                    self.value)

    def __repr__(self):
        return self.__str__()

# Server exceptions
class MemcachedKeyError(MemcachedError): pass
class MemcachedKeyNotFoundError(MemcachedKeyError): pass
class MemcachedKeyExistsError(MemcachedKeyError): pass
class MemcachedValueTooBig(MemcachedError): pass
class MemcachedInvalidArguments(MemcachedError): pass
class MemcachedItemNotStored(MemcachedError): pass
class MemcachedUnknownCommand(MemcachedError): pass
class MemcachedOutOfMemory(MemcachedError): pass

# Client exceptions
class MemcachedEncodingError(MemcachedError): pass
class MemcacheKeyEncodingError(MemcachedError): pass

# Network exceptions
class ConnectionError(MemcachedError): pass
class ConnectionClosedError(ConnectionError): pass
class ConnectionRefusedError(ConnectionError): pass
class ConnectionTimeoutError(ConnectionError): pass

status_exceptions = {
    STATUS_KEY_NOT_FOUND:MemcachedKeyNotFoundError,
    STATUS_KEY_EXISTS:MemcachedKeyExistsError,
    STATUS_VALUE_TOO_BIG:MemcachedValueTooBig,
    STATUS_INVALID_ARGUMENTS:MemcachedInvalidArguments,
    STATUS_ITEM_NOT_STORED:MemcachedItemNotStored,
    STATUS_UNKNOWN_COMMAND:MemcachedUnknownCommand,
    STATUS_OUT_OF_MEMORY:MemcachedOutOfMemory,
}


OP_GET=0x00
OP_SET=0x01
OP_ADD=0x02
OP_REPLACE=0x03
OP_DELETE=0x04
OP_INCREMENT=0x05
OP_DECREMENT=0x06
OP_QUIT=0x07
OP_FLUSH=0x08
OP_GETQ=0x09        # not implemented
OP_NOOP=0x0A
OP_VERSION=0x0B
OP_GETK=0x0C        # not implemented
OP_GETKQ=0x0D       # not implemented
OP_APPEND=0x0E
OP_PREPEND=0x0F
OP_STAT=0x10

OP_CODE_LOAD=0x70
OP_CODE_UNLOAD=0x71
OP_CODE_CHECK=0x72

def _pack(opcode=0x0, reserved=0x0, opaque=0x0, cas=0x0, key='', extras='', value='', magic=0x80):
    ''' create request packet '''
    keylen=len(key)
    extraslen=len(extras)
    header = struct.pack('!BBHBBHIIQ',
        magic, opcode, keylen,
        extraslen, 0x00, reserved,
        extraslen + keylen + len(value),
        opaque,
        cas)
    return ''.join([header, extras, key, value])

def _unpack(buf):
    ''' unpack response blob '''
    magic, opcode,              key_length, \
    extras_length, data_type,   status,   \
    total_body_length, opaque,  cas         \
        = struct.unpack_from('!BBHBBHIIQ', buf, 0)
    assert(data_type == 0x00)

    value_length = total_body_length - key_length - extras_length
    assert(value_length >= 0)

    body = buf[24:24+total_body_length]
    extras, key, value = '', '', ''
    if extras_length:
        extras = body[:extras_length]
    if key_length:
        key = body[extras_length:extras_length+key_length]
    if value_length:
        value = body[extras_length+key_length:]
        assert(len(value) == value_length)
    return 24+total_body_length, opcode, status, opaque, cas, key, extras, value, magic

MAX_SOCKET_OVERCOMMIT_SIZE=65536

def split_requests(requests):
    x = []
    s = 0
    for dd in requests:
        x.append( dd )
        s += 24 + len(dd.get('value','')) + len(dd.get('key','')) + len(dd.get('extras',''))
        if s >= MAX_SOCKET_OVERCOMMIT_SIZE:
            x.append( {'opcode':OP_NOOP} )
            yield x
            x = []
            s = 0
    if x:
        x.append( {'opcode':OP_NOOP} )
        yield x

MAX_TIMEOUT=4.0

def set_ridiculously_high_buffers(sd):
    '''
    Set extremaly high tcp/ip buffers in the kernel. Let's move the complexity
    to the operating system! That's a wonderful idea!
    '''
    for flag in [socket.SO_SNDBUF, socket.SO_RCVBUF]:
        for i in range(10):
            bef = sd.getsockopt(socket.SOL_SOCKET, flag)
            try:
                sd.setsockopt(socket.SOL_SOCKET, flag, bef*2)
            except socket.error:
                break
            aft = sd.getsockopt(socket.SOL_SOCKET, flag)
            if aft <= bef or aft >= 16777216: # 16M
                break


class NetworkClientInterface:
    ''' Networking interface. Connection/disconnection, reading, writing stuff.
    All read/write functions are iterators that yield:
        'r' if waiting for reading
        'w' if waiting for writing
        's' if needed to sleep/wait for a time
        'o' if the value is a proper output
        'e' if network exception is passed down
    '''
    RESTART_DELAY_MIN=0.1
    RESTART_DELAY_MAX=3.0
    sd = None
    buf = ''
    counter = 0

    def __init__(self, master_host, engine=None):
        host, _, port = master_host.rpartition(':')
        if not port:
            port = DEFAULT_PORT
        else:
            port = int(port)
        self.address = (host, port)
        self.send_buffer = simplebuffer.SimpleBuffer()
        self.recv_buffer = simplebuffer.SimpleBuffer()
        if not engine:
            engine = Reactor()
        self.engine = engine

    def _new_connection(self, address=None):
        if address is None:
            address = self.address
        sd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sd.connect(address)
        except socket.error, (e, _):
            if e == 111: # connection refused
                raise ConnectionRefusedError()
            raise
        sd.setblocking(False)
        sd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        set_ridiculously_high_buffers(sd)

        fcntl.fcntl(sd, fcntl.F_SETFL, os.O_NDELAY)
        fcntl.fcntl(sd, fcntl.F_SETFL, os.O_NONBLOCK)
        return sd

    def _del_connection(self, sd):
        sd.close()
        sd = None

    def _recv_try(self):
        assert not self.send_buffer
        try:
            data = self.sd.recv(65536)
        except socket.error, (e, msg):
            if e == 11: # errno.EAGAIN
                log.error("buffer not ready. This shouldn't happen")
                return
            raise ConnectionClosedError( (e, msg) )
        if len(data) == 0:
            raise ConnectionClosedError()
        assert len(data) > 0
        self.recv_buffer.write(data)

    def _eat_one(self, buf, offset):
        if len(buf)-offset < 24:
            return False, None
        total_length, = struct.unpack_from('!I', buf, offset+8)
        if len(buf)-offset < (24 + total_length):
            return False, None
        return True, 24 + total_length

    def _recv_enough_clear(self):
        self._recv_cache = [0, 0]

    def _recv_enough(self):
        have_packets, offset = self._recv_cache
        buf = self.recv_buffer.read()
        while True:
            i, sh = self._eat_one(buf, offset)
            if not i:
                break
            have_packets += 1
            offset += sh
        self._recv_cache = [have_packets, offset]
        return have_packets

    def __nb_cmd_multi(self, requests):
        ''' iterator responsible for sending/receiving data
        assuming that the connection exists '''
        requests_sz = len(requests)

        assert not self.send_buffer
        for kwargs in requests:
            self.counter = (self.counter + 1) % 4294967296L
            kwargs['opaque'] = self.counter
        self.send_buffer.write( ''.join( (_pack(**kwargs) for kwargs in requests) ) )

        r = self.send_buffer.send_to_socket( self.sd )
        assert r > 0
        while self.send_buffer:
            yield 'w', self.sd
            r = self.send_buffer.send_to_socket( self.sd )
            assert r > 0

        self._recv_enough_clear()
        while len(self.recv_buffer) < (requests_sz*24) or self._recv_enough() < requests_sz:
            yield 'r', self.sd
            self._recv_try()

        r = []
        for i in range(requests_sz):
            sz, r_opcode, r_status, r_opaque, r_cas, r_key, r_extras, r_value, \
                                r_magic = _unpack(self.recv_buffer.read())
            self.recv_buffer.consume(sz)
            assert r_opcode == requests[i]['opcode']
            assert r_opaque == requests[i]['opaque'], "packet:%i 0x%x 0x%x" % (i, r_opaque, requests[i]['opaque'])
            if r_status != 0:
                r.append( ('e', status_exceptions[r_status](requests[i]['opcode'], r_status, r_value)) )
            else:
                r.append( (r_opcode, r_cas, r_key, r_extras, r_value) )
        yield 'o', r

    def _nb_cmd_multi(self, requests):
        ''' wrapping _nb_cmd with proper error/retry/connection restarting '''
        delay = self.RESTART_DELAY_MIN + random.random()*self.RESTART_DELAY_MIN
        slept = 0.0

        while True:
            try:
                if not self.sd:
                    self.sd = self._new_connection()
                    yield 'n', self.sd
                if slept:
                    log.debug('ConnectionSuccess, connected: %r' % (self.address, ))
                for i in self.__nb_cmd_multi(requests):
                    yield i
                return
            except ConnectionError, e:
                log.debug('ConnectionError, retrying: %r %r' % (self.address, e,))
                if self.sd: # after connection stage
                    yield 'd', self.sd
                    self.sd = self._del_connection(self.sd)
                self.send_buffer.flush()
                self.recv_buffer.flush()
                slept += delay
                if slept > self.RESTART_DELAY_MAX:
                    log.debug('ConnectionError, fatal: %r %r' % (self.address, e,))
                    raise ConnectionError((self.address, e))
                yield 's', delay
                delay *= 2

    def _nb_cmd(self, **kwargs):
        for action, rets in self._nb_cmd_multi([kwargs]):
            if action == 'o':
                assert(len(rets)==1)
                rets = rets[0]
                if rets[0] == 'e':
                    yield 'e', rets[1]
                else:
                    yield 'o', rets
            else:
                yield action, rets


class AsynchronousClient(NetworkClientInterface):
    ''' asynchronous interface to binary protocol '''
    def _get(self, key, default):
        for action, rets in self._nb_cmd(opcode=OP_GET, key=key):
            if action == 'o':
                r_opcode, r_cas, r_key, r_extras, r_value = rets
                flags, = struct.unpack('!I', r_extras)
                yield 'o', (r_value, flags, r_cas)
            elif action == 'e' and isinstance(rets, MemcachedKeyNotFoundError):
                yield 'o', (default, 0, 0)
            else:
                yield action, rets

    def _set(self, key, value, flags, expiration, cas):
        extras = struct.pack('!II', flags, expiration)
        for action, rets in self._nb_cmd(opcode=OP_SET, cas=cas, key=key, extras=extras, value=value):
            if action == 'o':
                r_opcode, r_cas, r_key, r_extras, r_value = rets
                yield 'o', 1
            else:
                yield action, rets

    def _add(self, key, value, flags, expiration):
        extras = struct.pack('!II', flags, expiration)
        for action, rets in self._nb_cmd(opcode=OP_ADD, cas=0, key=key, extras=extras, value=value):
            if action == 'o':
                r_opcode, r_cas, r_key, r_extras, r_value = rets
                yield 'o', True
            elif action == 'e' and isinstance(rets, MemcachedKeyExistsError):
                yield 'o', False
            else:
                yield action, rets

    def _delete(self, key):
        for action, rets in self._nb_cmd(opcode=OP_DELETE, key=key):
            if action == 'o':
                r_opcode, r_cas, r_key, r_extras, r_value = rets
                yield 'o', True
            elif action == 'e' and isinstance(rets, MemcachedKeyNotFoundError):
                yield 'o', False
            else:
                yield action, rets

    def _get_multi(self, keys, default):
        r = {}
        for key in keys:
            for action, rets in self._get(key, default):
                if action == 'o':
                    r[key] = rets # r_value, flags, r_cas
                else:
                    yield action, rets
        yield 'o', r

    def _set_multi(self, mapping):
        for key, (value, flags, expiration) in mapping.iteritems():
            for action, rets in self._set(key, value, flags, expiration, 0x0):
                if action == 'o':
                    pass
                else:
                    yield action, rets
        yield 'o', {}

    def _add_multi(self, mapping):
        r = {}
        for key, (value, flags, expiration) in mapping.iteritems():
            for action, rets in self._add(key, value, flags, expiration):
                if action == 'o':
                    r[key] = rets # True/False
                else:
                    yield action, rets
        yield 'o', r

    def _delete_multi(self, keys):
        for key in keys:
            for action, rets in self._delete(key):
                if action == 'o':
                    pass
                else:
                    yield action, rets
        yield 'o', {}

    def _version(self):
        for action, rets in self._nb_cmd(opcode=OP_VERSION):
            if action == 'o':
                r_opcode, r_cas, r_key, r_extras, r_value = rets
                yield 'o', r_value
            else:
                yield action, rets

    def _code_load(self, key, value):
        for action, rets in self._nb_cmd(opcode=OP_CODE_LOAD, key=key, value=value):
            if action == 'o':
                r_opcode, r_cas, r_key, r_extras, r_value = rets
                yield 'o', r_value
            else:
                yield action, rets

    def _custom_command(self, **kwargs):
        for action, rets in self._nb_cmd(**kwargs):
            if action == 'o':
                r_opcode, r_cas, r_key, r_extras, r_value = rets
                yield 'o', r_value
            else:
                yield action, rets

    def _close(self):
        ''' workaround, so that the 'd' message goes to engine reactor '''
        if self.sd:
            yield 'd', self.sd
            self.sd = self._del_connection(self.sd)
        yield 'o', None

    def _incr(self, key, amount, initial, expiration, opcode=OP_INCREMENT):
        extras = struct.pack('!QQI', amount, initial, expiration)
        for action, rets in self._nb_cmd(opcode=OP_INCREMENT, cas=0, key=key, extras=extras):
            if action == 'o':
                r_opcode, r_cas, r_key, r_extras, r_value = rets
                yield 'o', struct.unpack('!Q', r_value)[0]
            else:
                yield action, rets

    def _decr(self, key, amount, initial, expiration):
        return self._incr(key, amount, initial, expiration, opcode=OP_DECREMENT)




def pretend_synchronous(fun):
    @functools.wraps(fun)
    def wrapper(self, *args, **kwargs):
        return self.engine.execute_single(fun(self, *args, **kwargs), max_timeout=MAX_TIMEOUT)
    return wrapper


class PollEmulate:
    def __init__(self):
        self.toread=[]
        self.towrite=[]
        select.POLLIN=1
        select.POLLOUT=4

    def register(self, fd, flag):
        if flag == select.POLLIN:
            self.toread.append(fd)
        else:
            self.towrite.append(fd)

    def unregister(self, fd):
        if fd in self.toread:
            self.toread.remove( fd )
        if fd in self.towrite:
            self.towrite.remove( fd )

    def poll(self,timeout):
        r, w, x = select.select(self.toread, self.towrite, [], timeout/1000.0)
        return [(fd, select.POLLIN) for fd in r] + [(fd, select.POLLOUT) for fd in w]

class Reactor:
    def __init__(self):
        self.timeouts = []
        if getattr(select, 'poll', None):
            self.reactor = select.poll()
        else:
            self.reactor = PollEmulate()
        self.fd_map = {}
        self.fd_wait = {}

    def execute_single(self, iterator, max_timeout):
        for rets in self.execute([iterator], max_timeout):
            pass
        return rets

    def execute(self, iterators, max_timeout):
        toplay = iterators
        while True:
            now = time.time()
            ntoplay = []
            for iterator in toplay:
                action, rets = iterator.next()
                if action == 'o':
                    yield rets
                    continue
                elif action == 'e':
                    raise rets
                elif action == 's':
                    heapq.heappush(self.timeouts, (now + rets, iterator))
                    continue
                assert rets is not None, 'action=%r rets=%r' % (action, rets)
                fd = rets.fileno()
                #log.debug("action:%r fd:%r  fdmap=%r fdwait=%r" % (action, fd, self.fd_map, self.fd_wait))
                if action == 'n':
                    self.fd_wait[fd] = 'r'
                    self.reactor.register(fd, select.POLLIN)
                    ntoplay.append( iterator )
                elif action == 'd':
                    self.fd_wait[fd] = 'r'
                    del self.fd_wait[fd]
                    self.reactor.unregister(fd)
                    ntoplay.append( iterator )
                else:
                    assert action in ['r', 'w']
                    if action == 'r' and self.fd_wait[fd] != 'r':
                        self.fd_wait[fd] = 'r'
                        self.reactor.unregister(fd)
                        self.reactor.register(fd, select.POLLIN)
                    if action == 'w' and self.fd_wait[fd] != 'w':
                        self.fd_wait[fd] = 'w'
                        self.reactor.unregister(fd)
                        self.reactor.register(fd, select.POLLOUT)
                    self.fd_map[fd] = iterator
                #log.debug("      :%r fd:%r  fdmap=%r fdwait=%r" % (action, fd, self.fd_map, self.fd_wait))

            if ntoplay:
                toplay = ntoplay
                continue

            if self.timeouts:
                if self.timeouts[0][0] <= now:
                    toplay = [heapq.heappop(self.timeouts)[1]]
                    continue
                min_timeout = self.timeouts[0][0] - now
            else:
                min_timeout = max_timeout

            if not self.fd_map and not self.timeouts:
                break
            try:
                fds_sta = self.reactor.poll(min_timeout * 1000.0) # milliseconds
            except select.error, (e,_):
                if e == 4: # interrupted system call
                    continue
            toplay = [self.fd_map[fd] for fd, _ in fds_sta] # if fd in self.fd_map
            for fd, _ in fds_sta:
                del self.fd_map[fd]
        assert not self.fd_map, '%r' % (self.fd_map,)
        assert not self.timeouts, self.timeouts



class SingleClient(AsynchronousClient):
    ''' simplified synchronous interface '''

    @pretend_synchronous
    def get(self, *args, **kwargs):
        return self._get(*args, **kwargs)

    @pretend_synchronous
    def set(self, *args, **kwargs):
        return self._set(*args, **kwargs)

    @pretend_synchronous
    def delete(self, *args, **kwargs):
        return self._delete(*args, **kwargs)

    @pretend_synchronous
    def add(self, *args, **kwargs):
        return self._add(*args, **kwargs)

    @pretend_synchronous
    def incr(self, *args, **kwargs):
        return self._incr(*args, **kwargs)

    @pretend_synchronous
    def version(self, *args, **kwargs):
        return self._version(*args, **kwargs)

    @pretend_synchronous
    def close(self, *args, **kwargs):
        return self._close(*args, **kwargs)
    
'''
    def get_multi(self, *args, **kwargs):
        return self.engine.execute_single(self._get_multi(*args, **kwargs), max_timeout=MAX_TIMEOUT)

    def set_multi(self, *args, **kwargs):
        return self.engine.execute_single(self._set_multi(*args, **kwargs), max_timeout=MAX_TIMEOUT)
        

    def delete_multi(self, *args, **kwargs):
        self.engine.execute_single(self._delete_multi(*args, **kwargs), max_timeout=MAX_TIMEOUT)
        return 1
'''


from binascii import crc32
serverHashFunction = crc32

SERVER_MAX_KEY_LENGTH=256
FLAG_PICKLED=0x01
FLAG_RAWSTRING=0x02

def check_key(key):
    if not isinstance(key, str):
        raise MemcacheKeyEncodingError('key must be a string %r' % (key, ))
    if len(key) > SERVER_MAX_KEY_LENGTH:
        raise MemcacheKeyEncodingError('key is too long %r' % (key, ))
    return key

class Client:
    ''' dispatcher to multiple servers '''
    servers = None # list

    def _pack(self, value):
        if isinstance(value, str):
            return value, FLAG_RAWSTRING
        return pickle.dumps(value, pickle.HIGHEST_PROTOCOL), FLAG_PICKLED

    def _unpack(self, value, flags, default):
        if value is default:
            return value
        if flags == FLAG_RAWSTRING:
            return value
        if flags == FLAG_PICKLED:
            return pickle.loads(value)
        raise MemcachedEncodingError('unknown flags value 0x%x' % (flags,))

    def __init__(self, servers_addrs):
        self.engine = Reactor()
        self.servers = [SingleClient(s, engine=self.engine) for s in servers_addrs]
        self.servers_addrs = servers_addrs

    def clone(self):
        return Client(self.servers_addrs)

    def get(self, key, default=None):
        check_key(key)
        r_value, r_flags, r_cas = getattr(self.servers[serverHashFunction(key) % len(self.servers)], 'get')(key, default)
        return self._unpack(r_value, r_flags, default)

    def _set_add(self, fun, key, value, expiration):
        check_key(key)
        v, flags = self._pack(value)
        if fun == 'add':
            return getattr(self.servers[serverHashFunction(key) % len(self.servers)], fun)(key, v, flags, expiration)
        return getattr(self.servers[serverHashFunction(key) % len(self.servers)], fun)(key, v, flags, expiration, 0x0)

    def set(self, key, value, expiration=0x0):
        return self._set_add('set', key, value, expiration)
    def add(self, key, value, expiration=0x0):
        return self._set_add('add', key, value, expiration)

    def delete(self, key):
        return getattr(self.servers[serverHashFunction(key) % len(self.servers)], 'delete')(key)

    def _incr_decr(self, fun, key, amount, initial, expiration):
        return getattr(self.servers[serverHashFunction(key) % len(self.servers)], fun)(key, amount, initial, expiration)

    def incr(self, key, amount=1, initial=0, expiration=0x0):
        return self._incr_decr('incr', key, amount, initial, expiration)
    def decr(self, key, amount=1, initial=0, expiration=0x0):
        return self._incr_decr('decr', key, amount, initial, expiration)

    def close(self):
        for server in self.servers:
            server.close()

    def _mkeys_wrapper(self, mkeys, fun):
        srvno_keys = collections.defaultdict(lambda:[])
        for key in mkeys:
            srvno_keys[serverHashFunction(key) % len(self.servers)].append(key)
        iis = [ fun(self.servers[srvno], keys) \
                                    for srvno, keys in srvno_keys.iteritems() ]
        dd = {}
        for rets in self.engine.execute(iis, max_timeout=MAX_TIMEOUT):
            dd.update( rets )
        return dd

    def get_multi(self, mkeys, default=None):
        dd = self._mkeys_wrapper(mkeys, lambda srv, keys:srv._get_multi(keys, default))
        de = {}
        for key, (r_value, r_flags, r_cas) in dd.iteritems():
            de[key] = self._unpack(r_value, r_flags, default)
        return de

    def set_multi(self, mkeys, expiration=0x0):
        def foo(srv, keys):
            dd = {}
            for k in keys:
                v, flags = self._pack(mkeys[k])
                dd[check_key(k)] = (v, flags, expiration)
            return srv._set_multi(dd)
        r = self._mkeys_wrapper(mkeys, foo)
        return len(r)

    def add_multi(self, mkeys, expiration=0x0):
        def foo(srv, keys):
            dd = {}
            for k in keys:
                v, flags = self._pack(mkeys[k])
                dd[check_key(k)] = (v, flags, expiration)
            return srv._add_multi(dd)
        return self._mkeys_wrapper(mkeys, foo)

    def delete_multi(self, mkeys):
        _ = [check_key(key) for key in mkeys]
        self._mkeys_wrapper(mkeys, lambda srv, keys:srv._delete_multi(keys))
        return 1

    def version(self):
        iis = [srv._version() for srv in self.servers]
        return [rets for rets in self.engine.execute(iis, max_timeout=MAX_TIMEOUT)]

    def code_load(self, code, key=None):
        if key is None:
            key = md5.md5(code).hexdigest()
        iis = [srv._code_load(key, code) for srv in self.servers]
        return [rets for rets in self.engine.execute(iis, max_timeout=MAX_TIMEOUT)]

    def code_unload(self, code=None, key=None):
        if key is None:
            key = md5.md5(code).hexdigest()
        iis = [srv._custom_command(opcode=OP_CODE_UNLOAD, key=key) for srv in self.servers]
        return [rets for rets in self.engine.execute(iis, max_timeout=MAX_TIMEOUT)]

    def code_check(self, code=None, key=None):
        if key is None:
            key = md5.md5(code).hexdigest()
        iis = [srv._custom_command(opcode=OP_CODE_CHECK, key=key) for srv in self.servers]
        return [rets for rets in self.engine.execute(iis, max_timeout=MAX_TIMEOUT)]

    def custom_command(self, **kwargs):
        iis = [srv._custom_command(**kwargs) for srv in self.servers]
        return [rets for rets in self.engine.execute(iis, max_timeout=MAX_TIMEOUT)]
