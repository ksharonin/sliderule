#include <iostream>
// #include "H5Coro.h"
#include "core.h"
#include "h5.h"

int main() {

    initcore();
    H5Coro::init(1);

    // TODO: resgister driver for "file"
    // bool status; // force ignore
    Asset::registerDriver(FileIODriver::FORMAT, FileIODriver::create);
    
    //const Asset* asset = nullptr;
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
    const char* datasetname = "/quality_assessment/gt2l/qa_total_signal_conf_ph_high"; // "/gt1r/geolocation/yaw";
    RecordObject::valType_t valtype = RecordObject::valType_t::DYNAMIC;
    long col = 0; // single dim
    long startrow = 0;
    long numrows = 1;
    H5Coro::context_t* context = nullptr; // simplify to no context
    bool meta_only = false; // t if metadata only

    std::cout << "Attempt H5Coro read\n";
    H5Future::info_t result = H5Coro::read(asset, url, datasetname,
    valtype, col, startrow, numrows, context, meta_only);
    std::cout << "Read complete; access data\n";
    unsigned int access = static_cast<unsigned int>(*result.data);
    std::cout << "uint8 access value: " << access << "\n";

    delete asset;
    H5Coro::deinit();
    deinitcore();

    return 0;
}
