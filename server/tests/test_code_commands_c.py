import unittest
from utils import connect
import re
from pkg_resources import resource_filename
from smalltable.binmemcache import OP_CODE_LOAD, MemcachedError, MemcachedKeyExistsError, MemcachedInvalidArguments, MemcachedItemNotStored, MemcachedKeyNotFoundError

def flatten_run_error(mc, c):
    try:
        mc.code_load(c)
        assert(0)
    except MemcachedError, e:
        return e.value


class TestGlobal(unittest.TestCase):
    @connect
    def test_gcc_warning(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
        printf("Hello world!");
}
'''
        x = flatten_run_error(mc, c)
        print x
        self.assertEqual("warning: control reaches end of non-void function" in x, True)

    @connect
    def test_assert(self, mc):
        ''' test assert(0) '''
        c = r'''
#include <stdio.h>
#include <assert.h>

int main(int argc, char **argv) {
        assert(0);
        printf("Test is broken!\n");
        return(0);
}
'''
        x = flatten_run_error(mc, c)
        self.assertEqual('Error while running the binary' in x, True)

    @connect
    def test_segv(self, mc):
        ''' test segv'''
        c = r'''
#include <stdio.h>

int main(int argc, char **argv) {
        int volatile *a = NULL;
        *a = 15;
        printf("Test is broken!\n");
        return(0);
}
'''
        x = flatten_run_error(mc, c)
        self.assertEqual('Error while running the binary' in x, True)

    @connect
    def test02(self, mc):
        c = open(resource_filename(__name__, 'plugin_test02.c')).read()
        x = mc.code_load( c )
        r =  mc.custom_command(opcode=0x90 ,key="12345678", value="12345678")
        self.assertEqual(r[0], 'kalesony!')

        # now upload_code_with_this_key_again
        self.assertRaises(MemcachedKeyExistsError,
                mc.code_load, open(resource_filename(__name__, 'plugin_test02.c')).read()
            )
        mc.code_unload(c)

    @connect
    def test_busy_loop(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
        unsigned int i, j;
        char dst[512];
        char src[512];
        
        for(j=1; j > 0; j++) {
                for(i=1; i > 0; i++) {
                        memcpy(dst, src, sizeof(dst));
                }
        }
        printf("Test is broken!\n");
        return(0);
}
'''
        x = flatten_run_error(mc, c)
        self.assertEqual('Error while running the binary' in x, True)


    @connect
    def test_fail_compilation(self, mc):
        c = r'''
#include <stdio.h>
#include <broken_libarry.h>

int main(int argc, char **argv) {
        printf("Test is broken!\n");
        return(0);
}
'''
        self.assertRaises(MemcachedItemNotStored,
                            mc.code_load, c
                            )

    @connect
    def test_unregister_on_close(self, mc):
        c = r'''
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
        st_register(0, 0); // bad register
        st_register(0xFF, 0);
        st_register(0xFE, 0);
        return(0);
}
'''
        x = flatten_run_error(mc, c)
        # not really interested in error value here

    @connect
    def test_fail_rcode_load(self, mc):
        key = 'very.unknown.key'
        self.assertRaises(MemcachedInvalidArguments,
                mc.custom_command, opcode=OP_CODE_LOAD,
            )

    @connect
    def test_fail_check(self, mc):
        self.assertRaises(MemcachedKeyNotFoundError,
                mc.code_check,"abc"
            )

