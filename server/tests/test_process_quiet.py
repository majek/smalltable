import unittest
from utils import connect, new_connection
import re
from pkg_resources import resource_filename
from smalltable.binmemcache import OP_NOOP, MemcachedError, MemcachedKeyExistsError, MemcachedInvalidArguments, MemcachedItemNotStored
import struct
import socket

def flatten_run_error(mc, c):
    try:
        mc.code_load(c)
        assert(0)
    except MemcachedError, e:
        return e.value


class TestGlobal(unittest.TestCase):
    @connect
    def test03(self, mc):
        # test quiet process command
        code = open(resource_filename(__name__, 'plugin_test03.c')).read()
        x = mc.code_load(code)
        # command is quiet.
        sd = mc.servers[0].sd
        sd.setblocking(False)
        key = 'a'

        header = struct.pack('!BBHBBHIIQ',
            0x80, 0x91, len(key),
            0, 0x00, 0x00,
            0 + len(key) + 0,
            0xDEAD,
            0x00)
        sd.send(header + key)
        self.assertRaises(socket.error, sd.recv, 4096)

        header = struct.pack('!BBHBBHIIQ',
            0x80, 0x91, len(key),
            0, 0x00, 0x00,
            0 + len(key) + 0,
            0xDEAD,
            0x00)
        sd.send(header + key)
        self.assertRaises(socket.error, sd.recv, 4096)

        header = struct.pack('!BBHBBHIIQ',
            0x80, OP_NOOP, 0,
            0, 0x00, 0x00,
            0 + 0 + 0,
            0xDEAD,
            0x00)
        sd.send(header + key)
        sd.setblocking(True)
        res = sd.recv(4096)
        self.assertEqual( res.encode('hex'), '8191000000000000000000090000dead00000000000000006b616c65736f6e79218191000000000000000000090000dead00000000000000006b616c65736f6e7921810a000000000000000000000000dead0000000000000000' )

        mc = new_connection()
        mc.code_unload(code)
        mc.close()
