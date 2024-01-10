#include <iostream>
#include <cassert>
#include "core.h"
#include "h5.h"

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
const bool SYS_LITTLE_ENDIAN = true;
#else
const bool SYS_LITTLE_ENDIAN = false;
#endif

int main() {

    assert(SYS_LITTLE_ENDIAN);

    initcore();
    H5Coro::init(1);

    Asset::registerDriver(FileIODriver::FORMAT, FileIODriver::create);
    
    std::vector<const char*> attr_in = {
        "local",
        "local",
        "file",
        "./data", // "/host/kat-dev"
        "nil",
        "nil",
        "nil"
    };

    // TODO: JSON KERCHUNK METADATA EXTRACTION
    // for now, assume the json lies in the specified attr_in directory 
    // no control under the hood aside conditioning redirecting

    const Asset* asset = Asset::assetFactory(NULL, attr_in);
    // the path to the file that is appended to the path parameter of the Asset object you created
    const char* url = "ATL03_20230816235231_08822014_006_01.h5";
    const char* datasetname = "/ancillary_data/calibrations/first_photon_bias/gt1l/ffb_corr";
    // "/ancillary_data/calibrations/first_photon_bias/cal19_product"; <- valid read as string
    //"/ancillary_data/calibrations/dead_time_radiometric_signal_loss/gt1l/rad_corr";
    // "/gt1l/heights/signal_conf_ph";

    // RecordObject::valType_t valtype = RecordObject::valType_t::INTEGER; //DYNAMIC;
    RecordObject::valType_t valtype = RecordObject::valType_t::DYNAMIC;

    long col = 1;
    long startrow = 0;
    long numrows = 4;
    H5Coro::context_t* context = nullptr; // simplify to no context
    bool meta_only = false; // t if metadata only

    std::cout << "Attempt H5Coro read\n";
    H5Future::info_t result = H5Coro::read(asset, url, datasetname,
    valtype, col, startrow, numrows, context, meta_only);
    
    std::cout << "Read complete; access data\n";
    // int8_t* recast = reinterpret_cast<int8_t*>(result.data);
    double* recast = reinterpret_cast<double*>(result.data);
    // std::cout << "access value: " << static_cast<float>(*recast) << "\n";
    std::cout << "access value: " << recast[2] << "\n";

    delete asset;
    H5Coro::deinit();
    deinitcore();

    return 0;
}
