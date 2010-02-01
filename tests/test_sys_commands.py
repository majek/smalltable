import unittest
from utils import connect
import cPickle as pickle

from smalltable.binmemcache import OP_GET, OP_SET, OP_REPLACE, OP_DELETE, OP_NOOP, OP_VERSION, \
            MemcachedUnknownCommand, MemcachedInvalidArguments, \
            MemcachedItemNotStored, MemcachedKeyNotFoundError

class TestGlobal(unittest.TestCase):
    @connect
    def test_unknown_command(self, mc):
        self.assertRaises(MemcachedUnknownCommand,
                          mc.custom_command, opcode=0xFF, key='k', value='v')

    @connect
    def test_broken_get(self, mc):
        self.assertRaises(MemcachedInvalidArguments,
                          mc.custom_command, opcode=OP_GET, key='k', value='v')
        ##self.assertEqual(mc.add(key, 'a'), False)

    @connect
    def test_key_not_found(self, mc):
        self.assertEqual(mc.get('very.unknown.key'), None)

    @connect
    def test_broken_set(self, mc):
        self.assertRaises(MemcachedInvalidArguments,
                    mc.custom_command, opcode=OP_SET
                )

    @connect
    def test_key_not_found_cas_set(self, mc):
        key = 'very.unknown.key'
        self.assertRaises(MemcachedKeyNotFoundError,
                lambda :mc.custom_command(opcode=OP_SET, key=key, value=pickle.dumps('2',-1), cas=0xDEADED, extras='\x00\x00\x00\x01\x00\x00\x00\x00'),
            )

    @connect
    def test_cas_set(self, mc):
        key = 'a'
        mc.set(key, '1')
        self.assertEqual(mc.get(key), '1')
        self.assertRaises(MemcachedItemNotStored,
                lambda :mc.custom_command(opcode=OP_SET, key=key, value=pickle.dumps('2',-1), cas=0xDEADED, extras='\x00\x00\x00\x01\x00\x00\x00\x00'),
            )
        self.assertEqual(mc.get(key), '1')

    @connect
    def test_fail_replace(self, mc):
        key = 'very.unknown.key'
        self.assertRaises(MemcachedKeyNotFoundError,
                lambda :mc.custom_command(opcode=OP_REPLACE, key=key, value=pickle.dumps('2',-1), cas=0x0, extras='\x00\x00\x00\x01\x00\x00\x00\x00'),
            )

    @connect
    def test_broken_delete(self, mc):
        key = 'very.unknown.key'
        self.assertRaises(MemcachedInvalidArguments,
                lambda :mc.custom_command(opcode=OP_DELETE, key=key, value=pickle.dumps('2',-1)),
            )

    @connect
    def test_fail_delete(self, mc):
        key = 'very.unknown.key'
        mc.delete(key) # err is not raised.

    @connect
    def test_broken_noop(self, mc):
        self.assertRaises(MemcachedInvalidArguments,
                lambda :mc.custom_command(opcode=OP_NOOP, key='a'),
            )

    @connect
    def test_broken_version(self, mc):
        self.assertRaises(MemcachedInvalidArguments,
                lambda :mc.custom_command(opcode=OP_VERSION, key='a'),
            )
