import time
import smalltable.simpleclient
mc = smalltable.simpleclient.Client('127.0.0.1:11211')


t0 = time.time()
a = mc.get_keys()
a = list(a)
t1 = time.time()
print t1-t0

print len(a)
