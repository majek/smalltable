import unittest
from utils import simple_connect
import struct
import socket

from smalltable import simpleclient
from smalltable.simpleclient import RESERVED_FLAG_PROXY_COMMAND, \
            MemcachedOutOfMemory, OP_GET, ConnectionClosedError

class TestGlobal(unittest.TestCase):
    @simple_connect
    def test_remote_server(self, mc):
        key = 'a'
        val = 'b'
        mc.set(key, val)
        v = mc.get(key)
        self.assertEqual(v, val)
        mc.delete(key)
        v = mc.get(key)
        self.assertEqual(v, None)
