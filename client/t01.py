import traceback
from smalltable import simpleclient as sc
a = sc.Client("127.0.0.1:22122")
print a.get_multi([chr(i) for i in range(256)])

while True:
    print "press any key to continue"
    raw_input()
    try:
        print a.get_multi(["%c%c%c" % (i,j,k) for i in range(256) for j in range(128) for k in range(1)])
    except:
        traceback.print_exc()

