#include <iostream>
// #include "H5Coro.h"
#include "core.h"
#include "h5.h"

int main() {

    H5Coro::init(1);
    
    const Asset* asset = nullptr;
    const char* url = "/home/ubuntu/Downloads/ATL03_20230816235231_08822014_006_01.h5"; // Replace with your H5 file URL
    const char* datasetname = "";
    RecordObject::valType_t valtype = RecordObject::valType_t::DYNAMIC;
    long col = 0; // single dim
    long startrow = 0; 
    long numrows = 10; 
    H5Coro::context_t* context = nullptr; // simplify to no context
    bool meta_only = false; // t if metadata only

    H5Future::info_t result = H5Coro::read(asset, url, datasetname, 
    valtype, col, startrow, numrows, context, meta_only);

    H5Coro::deinit();

    return 0;
}
