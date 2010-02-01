import unittest
import struct
from utils import connect

from smalltable.binmemcache import OP_GET

class TestCommand(unittest.TestCase):
    @connect
    def test_broken_value_sz(self, mc):
        # send negative value_sz
        # key_sz = 256
        # extras_sz = 0
        # data_sz = 0 -> should be key_sz + extra_sz
        mc.version() # connect to server

        sd = mc.servers[0].sd
        sd.setblocking(True)

        header = struct.pack('!BBHBBHIIQ',
            0x80, OP_GET, 255,
            0, 0x00, 0x00,
            1,
            0xDEAD,
            0x00)
        sd.send(header + 'x')
        r = sd.recv(4096)
        self.assertEqual( r, '' )

