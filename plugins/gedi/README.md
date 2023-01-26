# SlideRule GEDI Plugin

This SlideRule plugin includes modules for subsetting and sampling the GEDI data products.

If you are a science user interested in processing GEDI data with SlideRule, please see the [sliderule-python client](https://github.com/ICESat2-SlideRule/sliderule-python).

## I. Prerequisites

The base build environment must be setup as described in the [SlideRule](https://github.com/ICESat2-SlideRule/sliderule) readme.  Follow the first three steps in the Quick Start section to install the necessary system packages.  See [SlideRule Readme: III. Quick Start](https://github.com/ICESat2-SlideRule/sliderule/blob/master/README.md#iii-quick-start).

## II. Building

Use the [Makefile](Makefile) to build and install the plugin.

For a development version of SlideRule that includes the plugin and is run locally:
```bash
$ make config-development
$ make
$ make install
```

## III. What Is Provided

The plugin supplies the following endpoints:
* [gedi04a](endpoints/gedi04a.lua): subset GEDI L4A footprints

This plugin supplies the following record types:
* `gedil4arec`: a footprint of GEDI data

## IV. Licensing

SlideRule is licensed under the 3-clause BSD license found in the LICENSE file at the root of this source tree.