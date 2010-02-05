import unittest
import ctypes
import os

st = ctypes.CDLL('./smalltable-cov.so')
st.net_connect.argtypes=[ctypes.c_char_p, ctypes.c_int]
st.net_connect.restype = ctypes.c_int

class TestNetwork(unittest.TestCase):
    def test_one(self):
        fd = st.net_connect('127.0.0.1', 1)
        self.assertEqual(fd >= 0, True)
        os.close( fd )

        fd = st.net_connect('brokenaddress', 1)
        self.assertEqual(fd, -1)
