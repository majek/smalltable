import os
import signal
import unittest

class TestStorage(unittest.TestCase):
    def test_sync(self):
        pids = os.popen('pgrep smalltable').read()
        for pid in map(int, pids.split()):
            try:
                os.kill(pid, signal.SIGHUP)
            except:
                pass
