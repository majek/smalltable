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
