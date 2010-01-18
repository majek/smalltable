import unittest
from utils import connect
import re
from pkg_resources import resource_filename
from smalltable.binmemcache import OP_CODE_LOAD, MemcachedError, MemcachedKeyExistsError, MemcachedInvalidArguments, MemcachedItemNotStored, MemcachedUnknownCommand

def flatten_run_error(mc, c):
    try:
        mc.code_load(c)
        assert(0)
    except MemcachedError, e:
        return e.value


class TestGlobal(unittest.TestCase):
    @connect
    def test_crash_inside_handler(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smalltable.h>

int main(int argc, char **argv) {
        static char req_buf[READ_REQUESTS_BUF_SZ];      // not on stack
        st_register(0x91, 0);
        while(1) {
                st_read_requests(req_buf, sizeof(req_buf));
                // crash
                int *a = NULL;
                *a = 1;
        }
        return(0);
}
'''
        mc.code_load( c )
        self.assertRaises(MemcachedUnknownCommand,
                    mc.custom_command, opcode=0x91, key="12345678", value="12345678"
                )


    @connect
    def test_small_send_buffer(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smalltable.h>

int main(int argc, char **argv) {
        static char req_buf[1];      // not on stack
        st_register(0x91, 0);
        while(1) {
                st_read_requests(req_buf, sizeof(req_buf));
                printf("Test is broken!\n");
        }
        return(0);
}
'''
        self.assertRaises(MemcachedItemNotStored, mc.code_load, c)
        #self.assertRaises(MemcachedUnknownCommand,
        #            mc.custom_command, opcode=0x91, key="12345678", value="12345678"
        #        )

    @connect
    def test_small_flood_syscalls(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smalltable.h>

int main(int argc, char **argv) {
        setvbuf(stdout, (char *) NULL, _IONBF, 0);
        static char req_buf[READ_REQUESTS_BUF_SZ];      // not on stack
        st_register(0x91, 0);
        while(1) {
                st_read_requests(req_buf, sizeof(req_buf));
                while(1)
                    st_write_responses("abc", 0);
        }
        return(0);
}
'''
        mc.code_load(c)
        self.assertRaises(MemcachedUnknownCommand,
                    mc.custom_command, opcode=0x91, key="12345678", value="12345678"
                )

    @connect
    def test_no_response(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smalltable.h>

int main(int argc, char **argv) {
        setvbuf(stdout, (char *) NULL, _IONBF, 0);
        static char req_buf[READ_REQUESTS_BUF_SZ];      // not on stack
        st_register(0x91, 0);
        while(1) {
                st_read_requests(req_buf, sizeof(req_buf));
        }
        return(0);
}
'''
        mc.code_load(c)
        self.assertRaises(MemcachedUnknownCommand,
                    mc.custom_command, opcode=0x91, key="12345678", value="12345678"
                )

    @connect
    def test_partial_response_and_crash(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smalltable.h>

int main(int argc, char **argv) {
        setvbuf(stdout, (char *) NULL, _IONBF, 0);
        static char req_buf[READ_REQUESTS_BUF_SZ];      // not on stack
        st_register(0x91, 0);
        while(1) {
                st_read_requests(req_buf, sizeof(req_buf));
                /* btw. let's check wrong segment and zero length */
                st_write_responses("abc", 0);
                st_write_responses((void*)1, 3);
                int r  = st_write_responses("abc", 3);
                int *a = NULL;
                *a = 1;
        }
        return(0);
}
'''
        mc.code_load(c)
        self.assertRaises(MemcachedUnknownCommand,
                    mc.custom_command, opcode=0x91, key="12345678", value="12345678"
                )

    @connect
    def test_too_much_memory(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
        while(1)
            malloc(512*1024*1024);
        return(0);
}
'''
        self.assertRaises(MemcachedItemNotStored,
                    mc.code_load, c
                )

    @connect
    def test_write_and_register_corner_cases(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
        setvbuf(stdout, (char *) NULL, _IONBF, 0);
        write(1, (void*)1, 512);
        write(1, "abcd\n", 5);
        write(1, "", 0);
        int i = st_register(-1, 0);
        int j = st_register(655364, 0);
        printf("i=%i j=%i\n",i, j);
        return(0);
}
'''
        self.assertRaises(MemcachedItemNotStored,
                    mc.code_load, c
                )

    @connect
    def test_take_over_command(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smalltable.h>

int main(int argc, char **argv) {
        static char req_buf[READ_REQUESTS_BUF_SZ];      // not on stack
        st_register(0x91, 0);
        while(1) {
                st_read_requests(req_buf, sizeof(req_buf));
        }
        return(0);
}
'''
        mc.code_load( c )
        d = r'''
// difference for md5 sum
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smalltable.h>

int main(int argc, char **argv) {
        static char req_buf[READ_REQUESTS_BUF_SZ];      // not on stack
        st_register(0x91, 0);
        while(1) {
                st_read_requests(req_buf, sizeof(req_buf));
        }
        return(0);
}
'''
        mc.code_load( d )


    @connect
    def test_unregister(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smalltable.h>

int main(int argc, char **argv) {
        st_register(0x91, 0);
        st_unregister(0x91);
        st_unregister(0x81);
        st_unregister(0);
        st_unregister(-1);
        st_unregister(655532);
        st_read_requests("", 1000000000); // unregistered
        st_register(0x91, 0);
        st_read_requests("", 1000000000); // not write segment
        return(0);
}
'''
        self.assertRaises(MemcachedItemNotStored,
                    mc.code_load, c
                )
