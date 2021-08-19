# python
#
# In order to run this test:
#
# 1. Obtain the ATL06 science data file (referenced by the variable "resource") from NSIDC
#    and place in the local directory /data/ATLAS (referenced by the variable "url")
#
# 2. Build the SlideRule Python bindings via
#    $ make python-config
#    $ make
#
# 3. Copy this script to the <repo>/build directory and run there
#    OR setup the PYTHONPATH to point to the <repo>/build directory
#    OR install the srpybin.cpython-xxx-.so to a Python system folder
#

import sys
import srpybin

###############################################################################
# DATA
###############################################################################
""
# set resource parameters
resource1   = "ATL06_20200714160647_02950802_003_01.h5"
resource2   = "ATL06_20181019065445_03150111_003_01.h5"
format      = "file"        # "s3"
path        = "/data/ATLAS" # "icesat2-sliderule/data/ATLAS"
region      = ""            # "us-west-2"
endpoint    = ""            # "https://s3.us-west-2.amazonaws.com"

# expected single read
h_li_exp_1 = [3432.17578125, 3438.776611328125, 3451.01123046875, 3462.688232421875, 3473.559326171875]

# expected parallel read
h_li_exp_2 = {  '/gt1l/land_ice_segments/h_li': [3432.17578125, 3438.776611328125, 3451.01123046875, 3462.688232421875, 3473.559326171875],
                '/gt2l/land_ice_segments/h_li': [3263.659912109375, 3258.362548828125, 3.4028234663852886e+38, 3233.031494140625, 3235.200927734375],
                '/gt3l/land_ice_segments/h_li': [3043.489013671875, 3187.576171875, 3.4028234663852886e+38, 4205.04248046875, 2924.724365234375]}

# expected negative read
bsnow_conf_exp_3 = [-1, -1, -1, -1, -1]

###############################################################################
# UTILITY FUNCTIONS
###############################################################################

def check_results(act, exp):
    if type(exp) == dict:
        for dataset in exp:
            for i in range(len(exp[dataset])):
                if exp[dataset][i] != act[dataset][i]:
                    return False
        return True
    else:
        for i in range(len(exp)):
            if exp[i] != act[i]:
                return False
        return True

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    result = True

    # Open H5Coro File #
    h5file1 = srpybin.h5coro(resource1, format, path, region, endpoint)

    # Run Single and Parallel Tests #
    for test in range(100000):
        if test % 100 == 0:
            sys.stdout.write(".")
            sys.stdout.flush()

        # Perform Single Read #
        h_li_1 = h5file1.read("/gt1l/land_ice_segments/h_li", 0, 19, 5)
        result = result and check_results(h_li_1, h_li_exp_1)

        # Perform Parallel Read #
        datasets = [["/gt1l/land_ice_segments/h_li", 0, 19, 5],
                    ["/gt2l/land_ice_segments/h_li", 0, 19, 5],
                    ["/gt3l/land_ice_segments/h_li", 0, 19, 5],
                    ["/gt2r/land_ice_segments/geophysical/bsnow_conf", 0, 19, 5]]
        h_li_2 = h5file1.readp(datasets)
        result = result and check_results(h_li_2, h_li_exp_2)

        # check result for early exit from loop
        if not result:
            break

    # Run Negative Test #
    h5file2 = srpybin.h5coro(resource2, format, path, region, endpoint)
    bsnow_conf = h5file2.read("/gt2r/land_ice_segments/geophysical/bsnow_conf", 0, 19, 5)
    result = result and check_results(bsnow_conf, bsnow_conf_exp_3)

    # Display Results #
    if result:
        print("\nPassed H5Coro Test")
    else:
        print("\nFailed H5Coro Test")