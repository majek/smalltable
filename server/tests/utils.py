
import functools
import smalltable
import os
from smalltable import simpleclient

server = '127.0.0.1'
port = os.getenv('SERVERPORT', '22122')

def new_connection():
    return smalltable.Client(['%s:%s' % (server, port)])

def connect(fun):
    @functools.wraps(fun)
    def wrapper(self, *args, **kwargs):
        mc = new_connection()
        return fun(self, mc, *args, **kwargs)
    return wrapper


def simple_connect(fun):
    @functools.wraps(fun)
    def wrapper(self, *args, **kwargs):
        mc = simpleclient.Client("%s:%s" % (server, port))
        return fun(self, mc, *args, **kwargs)
    return wrapper

