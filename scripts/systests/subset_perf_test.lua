local runner = require("test_executive")
console = require("console")
asset = require("asset")
json = require("json")
local td = runner.rootdir(arg[0])

-- Setup --
-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

local GDT_datasize = {  1,  --GDT_Byte
                        2,  --GDT_UInt16
                        2,  --GDT_Int16
                        4,  --GDT_UInt32
                        4,  --GDT_Int32
                        4,  --GDT_Float32
                        8,  --GDT_Float64
                        8,  --GDT_CInt16
                        8,  --GDT_CInt32
                        10, --GDT_CFloat32
                        11, --GDT_CFloat64
                        12, --GDT_UInt64
                        13, --GDT_Int64
                        14, --GDT_Int8
                      }

local GDT_dataname = {  "GDT_Byte",
                        "GDT_UInt16",
                        "GDT_Int16",
                        "GDT_UInt32",
                        "GDT_Int32",
                        "GDT_Float32",
                        "GDT_Float64",
                        "GDT_CInt16",
                        "GDT_CInt32",
                        "GDT_CFloat32",
                        "GDT_CFloat64",
                        "GDT_UInt64",
                        "GDT_Int64",
                        "GDT_Int8",
                      }


-- AOI extent (extent of grandmesa.geojson)
local gm_llx = -108.3412
local gm_lly =   38.8236
local gm_urx = -107.7292
local gm_ury =   39.1956


local demType = "esa-worldcover-10meter"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))
local lon =    -108.1
local lat =      39.1
local height =    0.0
local starttime = time.latch();
local tbl, status = dem:sample(lon, lat, height)
local stoptime = time.latch();
threadCnt = 0
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, time
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

starttime = time.latch();
tbl, status = dem:subset(gm_llx, gm_lly, gm_urx, gm_ury)
stoptime = time.latch();

local threadCnt = 0
for i, v in ipairs(tbl) do
    threadCnt = threadCnt + 1
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

for i, v in ipairs(tbl) do
    local cols = v["cols"]
    local rows = v["rows"]
    local datatype = v["datatype"]

    local bytes = cols*rows* GDT_datasize[datatype]
    local mbytes = bytes / (1024*1024)
    print(string.format("AOI size: %6.1f MB   cols: %6d   rows: %6d   %s", mbytes, cols, rows, GDT_dataname[datatype]))
end


local demTypes = {"arcticdem-mosaic", "arcticdem-strips"}
for i = 1, #demTypes do
    demType = demTypes[i];
    print(string.format("\n--------------------------\n%s\n--------------------------", demType))
    dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))

    -- AOI extent
    local llx =  -145.81
    local lly =   70.00
    local urx =  -150.00
    local ury =   71.00

    starttime = time.latch();
    tbl, status = dem:sample(llx, lly, 0)
    stoptime = time.latch();
    threadCnt = 0
    if status ~= true then
        print(string.format("======> FAILED to read", lon, lat))
    else
        local value, fname, time
        for i, v in ipairs(tbl) do
            threadCnt = threadCnt + 1
        end
    end
    print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))


    starttime = time.latch();
    tbl, status = dem:subset(llx, lly, urx, ury)
    stoptime = time.latch();

    threadCnt = 0
    if tbl ~= nil then
        for i, v in ipairs(tbl) do
            threadCnt = threadCnt + 1
        end
    end
    print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

    if tbl ~= nil then
        for i, v in ipairs(tbl) do
            local cols = v["cols"]
            local rows = v["rows"]
            local datatype = v["datatype"]

            local bytes = cols*rows* GDT_datasize[datatype]
            local mbytes = bytes / (1024*1024)
            print(string.format("AOI size: %6.1f MB   cols: %6d   rows: %6d   %s", mbytes, cols, rows, GDT_dataname[datatype]))
        end
    end

    if i == 1 then  --Expecting 'throw' for mosaics, let it print correctly
        sys.wait(1)
    end
end


demTypes = {"rema-mosaic", "rema-strips"}
for i = 1, #demTypes do

    -- AOI extent
    llx =  149.80
    lly =  -70.00
    urx =  150.01
    ury =  -69.93

    demType = demTypes[i];
    print(string.format("\n--------------------------\n%s\n--------------------------", demType))
    dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour"}))
    starttime = time.latch();
    tbl, status = dem:sample(llx, lly, 0)
    stoptime = time.latch();
    threadCnt = 0
    for i, v in ipairs(tbl) do
        local el = v["value"]
        local fname = v["file"]
        -- print(string.format("(%02d) %10.2f  %s", i, el, fname))
        threadCnt = threadCnt + 1
    end
    print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

    starttime = time.latch();
    tbl, status = dem:subset(llx, lly, urx, ury)
    stoptime = time.latch();

    threadCnt = 0
    if tbl ~= nil then
        for i, v in ipairs(tbl) do
            threadCnt = threadCnt + 1
        end
    end
    print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

    if tbl ~= nil then
        for i, v in ipairs(tbl) do
            local cols = v["cols"]
            local rows = v["rows"]
            local datatype = v["datatype"]

            local bytes = cols*rows* GDT_datasize[datatype]
            local mbytes = bytes / (1024*1024)
            -- This results in 21 threads, all the same size, cols, buffs data type. Print only first one
            if i == 1 then
                print(string.format("AOI subset datasize: %.1f MB, cols: %d, rows: %d, datatype: %s", mbytes, cols, rows, GDT_dataname[datatype]))
            end
        end
    end
end


demType = "usgs3dep-1meter-dem"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))

local geojsonfile = td.."../../plugins/usgs3dep/data/grand_mesa_1m_dem.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents }))
starttime = time.latch();
local tbl, status = dem:sample(gm_llx, gm_lly, 0)
stoptime = time.latch();

threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("POI sample time: %.2f   (%d threads)\n", stoptime - starttime, threadCnt))


starttime = time.latch();
-- tbl, status = dem:subset(gm_llx, gm_lly, gm_urx, gm_ury)
tbl, status = dem:subset(gm_llx, gm_lly, gm_llx+0.1, gm_lly+0.1)
stoptime = time.latch();

threadCnt = 0
for i, v in ipairs(tbl) do
    threadCnt = threadCnt + 1
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

if tbl ~= nil then
    for i, v in ipairs(tbl) do
        local data = v["data"]
        local cols = v["cols"]
        local rows = v["rows"]
        local datatype = v["datatype"]

        local bytes = cols*rows* GDT_datasize[datatype]
        local mbytes = bytes / (1024*1024)
        -- print(string.format("(%02d) dataPtr: 0x%x, size: %d (%.2fMB), cols: %d, rows: %d, datatype: %d", i, data, bytes, mbytes, cols, rows, datatype))
        print(string.format("AOI subset datasize: %8.2f MB, cols: %6d, rows: %6d, datatype: %s", mbytes, cols, rows, GDT_dataname[datatype]))
    end
end

demType = "landsat-hls"
print(string.format("\n--------------------------\n%s\n--------------------------", demType))

local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms)):name("LpdaacAuthScript")
sys.wait(5)


local geojsonfile = td.."../../plugins/landsat/data/hls_trimmed.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- AOI extent (extent of hls_trimmed.geojson)
llx =  -179.87
lly =    50.45
urx =  -178.27
ury =    51.44


-- dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"B05"}, catalog = contents }))
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"NDVI"}, catalog = contents }))
starttime = time.latch();
tbl, status = dem:sample(-179, 51, 0)
stoptime = time.latch();

threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("POI sample time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

starttime = time.latch();
tbl, status = dem:subset(llx, lly, urx, ury)
stoptime = time.latch();

threadCnt = 0
if tbl ~= nil then
    for i, v in ipairs(tbl) do
        threadCnt = threadCnt + 1
    end
end
print(string.format("AOI subset time: %.2f   (%d threads)", stoptime - starttime, threadCnt))

if tbl ~= nil then
    for i, v in ipairs(tbl) do
        local cols = v["cols"]
        local rows = v["rows"]
        local datatype = v["datatype"]

        local bytes = cols*rows* GDT_datasize[datatype]
        local mbytes = bytes / (1024*1024)

        -- This results in 59 threads, all the same size, cols, buffs data type. Print only first one
        if i == 1 then
            print(string.format("AOI subset datasize: %.1f MB, cols: %d, rows: %d, datatype: %s", mbytes, cols, rows, GDT_dataname[datatype]))
        end
    end
end

local errors = runner.report()

-- Cleanup and Exit --
sys.quit( errors )

