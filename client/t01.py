import time
import traceback

if True:
    from smalltable import simpleclient as sc
    a = sc.Client("127.0.0.1:11212")
else:
    from smalltable import binmemcache as sc
    a = sc.Client(["127.0.0.1:11212"])

def main():

    #print a.get_multi([chr(i) for i in range(256)])

    key_values = dict(("xxx.%i" % (i,), 'a%i' %(i,)) for i in xrange(32768))

    keys = key_values.keys()
    y = [(k, key_values[k]) for k in keys]

    a.set_multi( key_values )

    for i in range(2):
        t0 = time.time()
        try:
            dx = a.get_multi(keys)
            x = [(k, dx[k]) for k in keys]
            diff = set(x) - set(y)
            print "%i items differ %.80r" %(len(diff), repr(diff))
            #print y[:10], x[:10]
            #print key_values, dx
        except:
            traceback.print_exc()
        t1 = time.time()
        print "took %.3f sec" % (t1-t0,)
        print "press any key to continue"
        #raw_input()

main()
