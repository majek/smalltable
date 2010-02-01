import unittest
import struct
from utils import connect

from smalltable.binmemcache import OP_GET

class TestCommand(unittest.TestCase):
    @connect
    def test_big_transfer(self, mc):
        # move control flow from META_EVENT_ONLY_WRITE to META_EVENT_ONLY_READ
        key = 'a'
        val = 'a' * (256*1024*1)
        mc.set(key, val)
        x = mc.get(key)
        self.assertEqual(val, x)