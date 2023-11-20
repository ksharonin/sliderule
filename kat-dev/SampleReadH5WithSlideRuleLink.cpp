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

    // args for func
    // (const Asset* asset, const char* resource, 
    // const char* datasetname, RecordObject::valType_t valtype,
    // long col, long startrow, long numrows, context_t* context, 
    // bool _meta_only, uint32_t parent_trace_id)

    // Call the H5Coro::read function
    H5Future::info_t result = H5Coro::read(asset, url, datasetname, 
    valtype, col, startrow, numrows, context, meta_only);

    // Handle the result as needed
    // Example: Print some information from the result
    // std::cout << "Info: DataType = " << result.dataType << ", DataSize = " << result.dataSize << std::endl;

    H5Coro::deinit();

    return 0;
}
