import unittest
from utils import simple_connect
import struct
import socket

from smalltable import simpleclient
from smalltable.simpleclient import RESERVED_FLAG_PROXY_COMMAND, \
            MemcachedOutOfMemory, OP_GET, ConnectionClosedError

class TestGlobal(unittest.TestCase):
    @simple_connect
    def test_huge_request(self, mc):
        try:
            mc.get('a') # connect to server
        except MemcachedOutOfMemory:
            pass
        sd = mc.conn.sd
        sd.setblocking(True)

        value = 'a'*(1024*1024*5)

        header = struct.pack('!BBHBBHIIQ',
            0x80, OP_GET, 1,
            0, 0x00, 0x00,
            len(value),
            0xDEAD,
            0x00)
        try:
            sd.sendall(header + value)
        except socket.error:
            pass
        r = sd.recv(4096)
        self.assertEqual( r, '' )

    @simple_connect
    def test_huge_key(self, mc):
        key = 'a' * 257
        self.assertRaises(ConnectionClosedError,
            mc._custom_command, opcode=0x99, key=key, value='b')
