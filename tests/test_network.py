import unittest
import ctypes
import os

dynlibname = os.getenv('TESTLIBNAME')

st = ctypes.CDLL(dynlibname)
st.net_connect.argtypes=[ctypes.c_char_p, ctypes.c_int]
st.net_connect.restype = ctypes.c_int

st.net_bind.argtypes = [ctypes.c_char_p, ctypes.c_int]
st.net_bind.restype = ctypes.c_int

class TestNetworkCTypes(unittest.TestCase):
    def test_connect(self):
        fd = st.net_connect('127.0.0.1', 1)
        self.assertNotEqual(fd, -1)
        os.close( fd )

    def test_connect(self):
        fd = st.net_connect('brokenaddress.google.co.ch', 1)
        self.assertEqual(fd, -1)
        fd = st.net_connect('localhost', 1)
        self.assertNotEqual(fd, -1)
        os.close( fd )

    def test_bind(self):
        fd = st.net_bind('brokenaddress.google.co.ch', 1)
        self.assertEqual(fd, -1)
