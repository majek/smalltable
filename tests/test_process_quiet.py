import unittest
from utils import connect, new_connection
from utils import simple_connect
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
    @simple_connect
    def test04(self, mc):
        code = open(resource_filename(__name__, 'plugin_test04.c')).read()
        x = mc.code_load(code)
        x = mc._custom_command(opcode=0x91, key="a", value="a")
        self.assertEqual(x, 'kalesony!')
        mc.code_unload(code)
