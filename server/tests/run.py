#!/usr/bin/env python

import sys
import unittest
import glob
import time

import os, os.path
import sys


def main_coverage(test_names):
    print "***** Unittest *****"
    suite = unittest.TestLoader().loadTestsFromNames(test_names)
    a = unittest.TextTestRunner().run(suite)
    if a.wasSuccessful():
        sys.exit(0)
    else:
        sys.exit(1)


def my_import(name):
    mod = __import__(name)
    components = name.split('.')
    for comp in components[1:]:
        mod = getattr(mod, comp)
    return mod

def mass_import(names):
    return [my_import(name) for name in names]


def list_files(directory, file_mask):
    cwd = os.getcwd()
    os.chdir(directory)
    files = [os.path.split(f)[1].rpartition('.')[0] \
                                                for f in glob.glob(file_mask)]
    os.chdir(cwd)
    return sorted(files)

def get_module_names(module_name, directory, file_mask, skip_modules):
    module_names = list_files(directory, file_mask)
    if module_name != "-":
        module_names = [module_name + "." + mn for mn in module_names]
    module_names = filter(lambda i: i not in skip_modules, module_names)
    return sorted(module_names)


if __name__ == '__main__':
    test_module, test_dir = sys.argv[1:]
    test_names   = get_module_names(test_module, test_dir, "test_*.py", [])
    #print test_names
    main_coverage(test_names)
