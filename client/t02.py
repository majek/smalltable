import smalltable.simpleclient
mc = smalltable.simpleclient.Client('127.0.0.1:22122')

mc.delete('a')
m = 'd'*180
keys = ['%s%s%s' % (i,m,i) for i in range(30000)]
print sum([len(k) for k in keys])
for key in keys:
    mc.set(key, '1')
got_keys = mc.get_keys()
g_keys = [k for i_cas, k in got_keys]
diff = set(g_keys).symmetric_difference( keys )
print "%r" % (diff, )
for key in keys:
    mc.delete(key)
