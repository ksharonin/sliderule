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
        "/host/kat-dev/data", // "/home/ubuntu/Downloads/sliderule/kat-dev/data"
        "nil",
        "nil",
        "nil"
    };

    const Asset* asset = Asset::assetFactory(NULL, attr_in);
    // the path to the file that is appended to the path parameter of the Asset object you created
    const char* url = "ATL03_20230816235231_08822014_006_01.h5";
    const char* datasetname = "/gt1l/heights/signal_conf_ph";
    // "/quality_assessment/gt2l/qa_total_signal_conf_ph_high"; 
    // "/gt1r/geolocation/yaw";
    RecordObject::valType_t valtype = RecordObject::valType_t::INTEGER; //DYNAMIC;
    long col = 0; //4; // single dim
    long startrow = 0;
    long numrows = 1;
    H5Coro::context_t* context = nullptr; // simplify to no context
    bool meta_only = false; // t if metadata only

    std::cout << "Attempt H5Coro read\n";
    H5Future::info_t result = H5Coro::read(asset, url, datasetname,
    valtype, col, startrow, numrows, context, meta_only);
    std::cout << "Read complete; access data\n";

    // try: recast to i1 as expected then float
    int8_t* recast = reinterpret_cast<int8_t*>(result.data);
    std::cout << "int8 access value: " << static_cast<float>(*recast) << "\n";

    delete asset;
    H5Coro::deinit();
    deinitcore();

    return 0;
}

