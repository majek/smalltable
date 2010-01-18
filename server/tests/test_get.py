import unittest
from utils import connect
import re

class TestGlobal(unittest.TestCase):
    @connect
    def test_version(self, mc):
        for v in mc.version():
            b = re.match('^([^.]+)[.]([^.]+)[.]([^.]+)$', v)
            self.assertNotEqual(b, None)

    @connect
    def test_add(self, mc):
        key = 'a'
        mc.delete(key)
        self.assertEqual(mc.add(key, 'a'), True)
        self.assertEqual(mc.add(key, 'a'), False)


