import unittest
from utils import connect
from smalltable.binmemcache import MemcachedError

class TestGlobal(unittest.TestCase):
    @connect
    def test_gcc_warning(self, mc):
        c = r'''
int main(int argc, char **argv) {
        return(0);
}
'''
        try:
            mc.code_load(c, key="+- *")
        except MemcachedError, e:
            pass

import ctypes
import os

dynlibname = os.getenv('TESTLIBNAME')

st = ctypes.CDLL(dynlibname)
st.read_full_file.argtypes = [ctypes.c_char_p]
st.read_full_file.restype = ctypes.c_char_p


class TestCommonCTypes(unittest.TestCase):
    def test_read_full_file(self):
        a = st.read_full_file(__file__)
        self.assertEquals(len(a) > 1, True)

        a = st.read_full_file("broken_file")
        self.assertEquals(a, None)
