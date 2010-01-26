import unittest
from utils import simple_connect

from smalltable import simpleclient

class TestGlobal(unittest.TestCase):
    @simple_connect
    def test_unknown_command(self, mc):
        self.assertRaises(simpleclient.MemcachedOutOfMemory,
            mc._custom_command, opcode=0x99, key='a', value='b' )
