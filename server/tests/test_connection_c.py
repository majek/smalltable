import unittest
import struct
import socket
from utils import connect

from smalltable.binmemcache import OP_SET, OP_GET, OP_GETQ, OP_NOOP, MemcachedInvalidArguments

class TestConnection(unittest.TestCase):
    @connect
    def test_disconnect(self, mc):
        # dsconnect on read of a big value
        key = 'a'
        mc.set(key, 'a'*(1024*1024*1))

        sd = mc.servers[0].sd
        sd.setblocking(True)

        header = struct.pack('!BBHBBHIIQ',
            0x80, OP_GET, len(key),
            0, 0x00, 0x00,
            0 + len(key) + 0,
            0xDEAD,
            0x00)
        sd.send(header + key)
        self.assertEqual( sd.recv(32).encode('hex'), '8100000004000000001000040000dead00000000000000010000000261616161' )
        sd.close()
        # now, server shall kill the connectin

    @connect
    def test_broken_magic(self, mc):
        mc.version() # connect to server

        sd = mc.servers[0].sd
        sd.setblocking(True)
        header = struct.pack('!BBHBBHIIQ',
            0x01, OP_GET, 0,
            0, 0x00, 0x00,
            0,
            0xDEAD,
            0x00)
        sd.send(header + 'x')
        r = sd.recv(4096)
        self.assertEqual( len(r), 0)

    @connect
    def test_quiet_bulking(self, mc):
        key = 'a'
        mc.set(key, 'a')

        sd = mc.servers[0].sd
        sd.setblocking(False)

        header = struct.pack('!BBHBBHIIQ',
            0x80, OP_GETQ, len(key),
            0, 0x00, 0x00,
            len(key),
            0xDEAD,
            0x00)
        sd.send(header + key)
        self.assertRaises(socket.error, sd.recv, 4096)

        sd.send(header + key)
        self.assertRaises(socket.error, sd.recv, 4096)

        header = struct.pack('!BBHBBHIIQ',
            0x80, OP_NOOP, 0,
            0, 0x00, 0x00,
            0,
            0xDEAD,
            0x00)
        sd.send(header)
        sd.setblocking(True)
        r = sd.recv(4096)
        self.assertEqual( r.encode('hex'), '8109000004000000000000050000dead000000000000000100000002618109000004000000000000050000dead00000000000000010000000261810a000000000000000000000000dead0000000000000000')


    @connect
    def test_value_longer_than_allowed(self, mc):
        mc.version() # connect to server
        value = 'a'*(5*1024*1024)
        key = 'a'

        sd = mc.servers[0].sd
        sd.setblocking(True)
        header = struct.pack('!BBHBBHIIQ',
            0x80, OP_SET, len(key),
            0, 0x00, 0x00,
            0 + len(key) + len(value),
            0xDEAD,
            0x00)
        try:
            sd.sendall(header + key + value)
        except socket.error:
            pass
        self.assertEqual(sd.recv(4096), '')
        sd.close()
