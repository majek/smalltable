import unittest
from utils import simple_connect

from smalltable import simpleclient
from smalltable.simpleclient import RESERVED_FLAG_PROXY_COMMAND, MemcachedOutOfMemory

class TestGlobal(unittest.TestCase):
    @simple_connect
    def test_unknown_server_command(self, mc):
        self.assertRaises(simpleclient.MemcachedOutOfMemory,
            mc._custom_command, opcode=0x99, key='a', value='b' )

    @simple_connect
    def test_unknown_proxy_command(self, mc):
        self.assertRaises(MemcachedOutOfMemory,
            mc._custom_command, opcode=0x99, key='a', value='b', reserved=RESERVED_FLAG_PROXY_COMMAND )
