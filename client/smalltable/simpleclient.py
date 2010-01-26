#!/usr/bin/env python
'''

    >>> st.set('a', 123)
    True
    >>> st.set('a', 124)
    True
    >>> st.get('a')
    124
    >>> st.delete('a')
    True
    >>> st.get('a') is None
    True


    >>> st.incr('a', initial=123, amount=3)
    123
    >>> st.incr('a', initial=123, amount=3)
    126
    >>> st.decr('a', initial=123, amount=3)
    123
'''
DEFAULT_PORT = 22122

try:
    import cPickle as pickle
except ImportError: #pragma: no cover
    import pickle

try:
    import hashlib
    do_md5 = lambda v:hashlib.md5(v).hexdigest()
except ImportError:
    import md5
    do_md5 = lambda v:md5.md5(v).hexdigest()

import socket
import struct
import select
import time
import random
import fcntl
import os
import functools
import pkg_resources
import logging
log = logging.getLogger(__name__)

import simplebuffer

# It's not the list of implemented commands.
# It's just list copied from the protocol draft.
OP_GET = 0x00
OP_SET = 0x01
OP_ADD = 0x02
OP_REPLACE = 0x03
OP_DELETE = 0x04
OP_INCREMENT = 0x05
OP_DECREMENT = 0x06
OP_QUIT = 0x07
OP_FLUSH = 0x08
#OP_GETQ = 0x09
OP_NOOP = 0x0A
OP_VERSION = 0x0B
OP_GETK = 0x0C
#OP_GETKQ = 0x0D
OP_APPEND = 0x0E
OP_PREPEND = 0x0F
OP_STAT = 0x10
#OP_SETQ = 0x11
#OP_ADDQ = 0x12
#OP_REPLACEQ = 0x13
#OP_DELETEQ = 0x14
#OP_INCREMENTQ = 0x15
#OP_DECREMENTQ = 0x16
#OP_QUITQ = 0x17       # you have to be fucked up to think of QUITQ
#OP_FLUSHQ = 0x18
#OP_APPENDQ = 0x19
#OP_PREPENDQ = 0x1A

OP_CODE_LOAD = 0x70
OP_CODE_UNLOAD = 0x71
OP_CODE_CHECK = 0x72


STATUS_NO_ERROR = 0x0000
STATUS_KEY_NOT_FOUND = 0x0001
STATUS_KEY_EXISTS = 0x0002
STATUS_VALUE_TOO_BIG = 0x0003
STATUS_INVALID_ARGUMENTS = 0x0004
STATUS_ITEM_NOT_STORED = 0x0005
STATUS_ITEM_NON_NUMERIC = 0x0006

STATUS_UNKNOWN_COMMAND = 0x0081
STATUS_OUT_OF_MEMORY = 0x0082

RESERVED_FLAG_QUIET = 1 << 0
RESERVED_FLAG_PROXY_COMMAND = 1 << 1

class MemcachedError(Exception):
    def __init__(self, *args, **kwargs):
        for k in ['opcode', 'extras', 'key', 'status', 'value']:
            if k in kwargs:
                setattr(self, k, kwargs[k])
                del kwargs[k]
            else:
                setattr(self, k, None)
        self._exc = True if args else False
        Exception.__init__(self, *args, **kwargs)

    def __str__(self):
        if self._exc:
            return Exception.__str__(self)
        x = []
        for k in ['opcode', 'extras', 'key', 'status', 'value']:
            if getattr(self, k, None) is not None:
                x.append( "%s=%r" % (k, getattr(self, k)) )
        return '<%s %s>' % (self.__class__.__name__, " ".join(x))

    def __repr__(self):
        if self._exc:
            return Exception.__repr__(self)
        return self.__str__()

# Server exceptions
class MemcachedKeyError(MemcachedError): pass
class MemcachedKeyNotFoundError(MemcachedKeyError): pass
class MemcachedKeyExistsError(MemcachedKeyError): pass
class MemcachedValueTooBig(MemcachedError): pass
class MemcachedInvalidArguments(MemcachedError): pass
class MemcachedItemNotStored(MemcachedError): pass
class MemcachedItemNonNumeric(MemcachedError): pass
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
    STATUS_ITEM_NON_NUMERIC:MemcachedItemNonNumeric,
    STATUS_UNKNOWN_COMMAND:MemcachedUnknownCommand,
    STATUS_OUT_OF_MEMORY:MemcachedOutOfMemory,
}


def _pack(opcode=0x0, key='', extras='', value='', opaque=0x0, cas=0x0, reserved=0x0):
    ''' create request packet '''
    keylen = len(key)
    extraslen = len(extras)
    header = struct.pack('!BBHBBHIIQ',
        0x80, opcode, keylen,
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

    value_length = total_body_length - key_length - extras_length

    extras, key, value = '', '', ''
    if extras_length:
        extras = buf[24:24+extras_length]
    if key_length:
        key = buf[24+extras_length:24+extras_length+key_length]
    if value_length:
        value = buf[24+extras_length+key_length:24+total_body_length]
    return opcode, status, opaque, cas, extras, key, value


def _set_ridiculously_high_buffers(sd):
    '''
    Set extremely high tcp/ip buffers in the kernel. Let's move the complexity
    to the operating system! That must be a good idea!
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


def _encode(value):
    if isinstance(value, str):
        return value, FLAG_RAWSTRING
    return pickle.dumps(value, pickle.HIGHEST_PROTOCOL), FLAG_PICKLED

def _decode(value, flags):
    if flags == FLAG_RAWSTRING:
        return value
    if flags == FLAG_PICKLED:
        return pickle.loads(value)
    raise MemcachedEncodingError('unknown flags 0x%x' % (flags,))


class NetworkConnecton:
    RESTART_DELAY_MIN = 0.1
    RESTART_DELAY_MAX = 3.0

    def __init__(self, server_addr):
        host, port = server_addr.split(':', 1)
        if not port:
            port = DEFAULT_PORT
        else:
            port = int(port)
        self.address = (host, port)
        self.sd = None
        self.send_buffer = simplebuffer.SimpleBuffer()
        self.recv_buffer = simplebuffer.SimpleBuffer()
        self.opaque = random.randint(0, 65535)

    def _new_connection(self):
        sd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sd.connect(self.address)
        except socket.error, (e, _):
            if e == 111: # connection refused
                raise ConnectionRefusedError()
            raise
        sd.setblocking(False)
        sd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        _set_ridiculously_high_buffers(sd)
        fcntl.fcntl(sd, fcntl.F_SETFL, os.O_NDELAY)
        fcntl.fcntl(sd, fcntl.F_SETFL, os.O_NONBLOCK)
        return sd

    def new_connection(self):
        delay = self.RESTART_DELAY_MIN + random.random()*self.RESTART_DELAY_MIN
        slept = 0.0

        while True:
            try:
                return self._new_connection()
            except ConnectionError, e:
                log.debug('ConnectionError, retrying: %r %r' % (self.address, e))
                self.send_buffer.flush()
                self.recv_buffer.flush()
                slept += delay
                if slept > self.RESTART_DELAY_MAX:
                    log.error('ConnectionError, fatal: %r %r' % (self.address, e,))
                    raise ConnectionError((self.address, e))
                time.sleep(delay)
                delay *= 2

    def send_with_noop(self, requests_iter):
        def x(opaque):
            for kwargs in requests_iter:
                kwargs['opaque'] = opaque
                yield _pack(**kwargs)
                opaque += 1
            yield _pack(opcode=OP_NOOP, opaque=opaque)
        self.send_iter = x

    def recv_till_noop(self):
        for r_opcode, r_status, r_cas, r_extras, r_key, r_value in self.recv():
            if r_opcode == OP_NOOP:
                continue
            yield r_status, r_cas, r_extras, r_key, r_value
        assert len(self.send_buffer) == 0
        assert len(self.recv_buffer) == 0

    def single_cmd(self, **kwargs):
        def x(opaque):
            kwargs['opaque'] = opaque
            yield _pack(**kwargs)
            opaque += 1
        self.send_iter = x
        ret = list( self.recv() )[0]
        return ret

    def recv(self):
        if not self.sd:
            self.sd = self.new_connection()

        assert len(self.send_buffer) == 0
        assert len(self.recv_buffer) == 0
        opaques = []
        group_sz = 0
        group = []
        for data in self.send_iter(self.opaque):
            opaques.append( self.opaque )
            self.opaque = (self.opaque+1) % 65536
            group.append( data )
            group_sz += len( data )
            if group_sz > 65536: # Every 65k try to fill tcp/ip buffers.
                self.send_buffer.write( ''.join(group) )
                self.send_buffer.send_to_socket( self.sd )
                group = []
                group_sz = 0
        self.send_buffer.write( ''.join(group) )
        self.send_buffer.send_to_socket( self.sd )

        opaques.reverse()

        res_len = 24
        while opaques:
            if len(self.send_buffer):
                wlist = [self.sd.fileno()]
            else:
                wlist = []
            (rlist, wlist, _) = select.select([self.sd.fileno()], wlist, [])
            if wlist:
                self.send_buffer.send_to_socket( self.sd )
            if rlist:
                c = self.recv_buffer.recv_from_socket( self.sd )
                if c == 0:
                    raise ConnectionClosedError()
                while True:
                    buf_len = len(self.recv_buffer)
                    if buf_len >= res_len:
                        total_length, = struct.unpack_from('!I', self.recv_buffer.read(8+4), 8)
                        res_len = total_length+24
                        if buf_len >= res_len:
                            r_opcode, r_status, r_opaque, r_cas, r_extras, r_key, r_value = \
                                _unpack(self.recv_buffer.read_and_consume(res_len))
                            opaque = opaques.pop()
                            assert(r_opaque == opaque, '0x%x != 0x%x' % (r_opaque, opaque))
                            yield r_opcode, r_status, r_cas, r_extras, r_key, r_value
                            res_len = 24
                            continue
                    break
        assert len(self.send_buffer) == 0
        assert len(self.recv_buffer) == 0


FLAG_PICKLED = 0x01
FLAG_RAWSTRING = 0x02

class Client:
    server_addr = None
    conn = None

    def __init__(self, server_addr):
        self.conn = NetworkConnecton( server_addr )

    def get_multi(self, keys, default=None):
        self.conn.send_with_noop( {'opcode':OP_GET, 'key':key, 'reserved':RESERVED_FLAG_QUIET}  for key in keys )
        key_map = {}
        for i, (r_status, r_cas, r_extras, r_key, r_value) in enumerate(self.conn.recv_till_noop()):
            if r_status is STATUS_NO_ERROR:
                r_flags, = struct.unpack('!I', r_extras)
                key_map[keys[i]] = _decode(r_value, r_flags)
            elif r_status is STATUS_KEY_NOT_FOUND:
                key_map[keys[i]] = default
            else:
                raise status_exceptions[r_status](key=keys[i])
        return key_map

    def set_multi(self, key_map):
        items = key_map.items()
        def _req(key, value):
            enc_val, flags = _encode(value)
            return {'opcode':OP_SET, 'key':key, 'extras': struct.pack('!II', flags, 0x0), 'value':enc_val, 'reserved':RESERVED_FLAG_QUIET}
        self.conn.send_with_noop( _req(key, value) for key, value in items )
        for i, (r_status, r_cas, r_extras, r_key, r_value) in enumerate(self.conn.recv_till_noop()):
            if r_status is not STATUS_NO_ERROR:
                raise status_exceptions[r_status](key=items[i])
        return True

    def delete_multi(self, keys):
        self.conn.send_with_noop( {'opcode':OP_DELETE, 'key':key} for key in keys )
        for i, (r_status, r_cas, r_extras, r_key, r_value) in enumerate(self.conn.recv_till_noop()):
            if r_status is not STATUS_NO_ERROR and r_status is not STATUS_KEY_NOT_FOUND:
                raise status_exceptions[r_status](key=keys[i])
        return True

    def get(self, key, default=None):
        return self.get_multi((key,), default=default)[key]

    def set(self, key, value):
        return self.set_multi( {key:value} )

    def delete(self, key):
        return self.delete_multi( (key,) )

    def code_load(self, code, key=None):
        if key is None:
            key = do_md5(code)
        r_opcode, r_status, r_cas, r_extras, r_key, r_value = \
                            self.conn.single_cmd(opcode=OP_CODE_LOAD, key=key, value=code)
        if r_status is not STATUS_NO_ERROR and r_status is not STATUS_KEY_EXISTS:
            raise status_exceptions[r_status](key=r_key, value=r_value)
        return True

    def code_unload(self, code=None, key=None):
        if key is None:
            key = do_md5(code)
        r_opcode, r_status, r_cas, r_extras, r_key, r_value = \
                            self.conn.single_cmd(opcode=OP_CODE_UNLOAD, key=key)
        if r_status is not STATUS_NO_ERROR:
            raise status_exceptions[r_status](key=key, value=r_value)
        return True

    def code_check(self, code=None, key=None):
        if key is None:
            key = do_md5(code)
        r_opcode, r_status, r_cas, r_extras, r_key, r_value = \
                            self.conn.single_cmd(opcode=OP_CODE_CHECK, key=key)
        if r_status is not STATUS_KEY_FOUND:
            return True
        elif r_status is not STATUS_KEY_NOT_FOUND:
            return False
        raise status_exceptions[r_status](key=key, value=r_value)

    def _custom_command(self, **kwargs):
        r_opcode, r_status, r_cas, r_extras, r_key, r_value = \
                            self.conn.single_cmd(**kwargs)
        if r_status is not STATUS_NO_ERROR:
            raise status_exceptions[r_status](value=r_value)
        return r_value

def code_loader(filename):
    def decor(fun):
        @functools.wraps(fun)
        def wrapper(self, *args, **kwargs):
            try:
                return fun(self, *args, **kwargs)
            except MemcachedUnknownCommand:
                code = open(pkg_resources.resource_filename(__name__, filename)).read()
                self.code_load( code )
                return fun(self, *args, **kwargs)
        return wrapper
    return decor


class IncrClient(Client):
    @code_loader('plugin_incr.c')
    def _do_incr(self, opcode, key, amount, initial, expiration):
        extras = struct.pack("!QQI", amount, initial, expiration)
        r_opcode, r_status, r_cas, r_extras, r_key, r_value = \
                            self.conn.single_cmd(opcode=opcode, key=key, extras=extras)
        if r_status is not STATUS_NO_ERROR:
            raise status_exceptions[r_status](key=key)
        return struct.unpack('!Q', r_value)[0]

    def incr(self, key, amount=1, initial=0x0, expiration=0x0):
        return self._do_incr(OP_INCREMENT, key, amount, initial, expiration)

    def decr(self, key, amount=1, initial=0x0, expiration=0x0):
        return self._do_incr(OP_DECREMENT, key, amount, initial, expiration)


if __name__ == '__main__':
    import doctest, simpleclient
    FORMAT_CONS = '%(asctime)s %(name)-12s %(levelname)8s\t%(message)s'
    logging.basicConfig(level=logging.DEBUG, format=FORMAT_CONS)

    server = "127.0.0.1:22122"
    st = IncrClient(server)
    doctest.testmod(simpleclient, globs={"st": st})

