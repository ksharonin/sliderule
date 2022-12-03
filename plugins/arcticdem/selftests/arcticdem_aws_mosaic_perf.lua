local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Setup --

local assets = asset.loaddir() -- looks for asset_directory.csv in same directory this script is located in
local asset_name = "arcticdem-local"
local arcticdem_local = core.getbyname(asset_name)

local  lon   = -180.00
local  lat = 66.34  -- Arctic Circle lat
local _lon = lon
local _lat = lat

print('\n------------------\nTest: AWS mosaic\n------------')
local dem = arcticdem.raster("mosaic", "NearestNeighbour", 0)
local starttime = time.latch();
local tbl, status = dem:samples(lon, lat)
local stoptime = time.latch();
local dtime = stoptime - starttime

for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    print(string.format("(%02d) %20f     %s", i, el, fname))
end
print('ExecTime:', dtime, '\n')

local el, status
local max_cnt = 1000000
-- local max_cnt = 10000

--[[
--]]

print('\n------------------\nTest: Reading', max_cnt, '  points (THE SAME POINT)\n------------')

starttime = time.latch();
intervaltime = starttime

for i = 1, max_cnt
do
    tbl, status = dem:samples(lon, lat)
    if status ~= true then
        print(i, status)
    end
end

stoptime = time.latch();
dtime = stoptime-starttime
print('\n')
print(max_cnt, 'points read time', dtime)

-- os.exit()

--[[
--]]

-- About 500 points read from the same raster before opening a new one
max_cnt = 10000
print('\n------------------\nTest: Reading', max_cnt, '  different points (Average case of DIFFERENT RASTER)\n------------')
lon = _lon
lat = _lat
starttime = time.latch();
intervaltime = starttime

for i = 1, max_cnt
do
    tbl, status = dem:samples(lon, lat)
    if status ~= true then
        print(i, status)
    end

    lon = lon + 0.001

    modulovalue = 1000
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print('Points read:', i, dtime)
        intervaltime = time.latch();
    end

end

stoptime = time.latch();
dtime = stoptime-starttime
print('\n')
print(max_cnt, 'points read time', dtime)

-- os.exit();



-- Every point in different mosaic raster (absolute worse case performance)
-- Walking around arctic circle hitting different raster each time
max_cnt = 100
print('\n------------------\nTest: Reading', max_cnt, '  different points (DiFERENT RASTERs, walk around Arctic Circle)\n------------')
lon = _lon
lat = _lat
starttime = time.latch();
intervaltime = starttime

for i = 1, max_cnt
do
    tbl, status = dem:samples(lon, lat)
    if status ~= true then
        print(i, status)
    end

    -- NOTE: COGs are 1x1 degree but if I increment lon by 1.1 or 1.2
    --       ocasionally I hit the same raster twice.
    --  Incrementing by 1.5 degree gives absolute worse case scenario.
    --  Every point read is from different raster then the last one.
    lon = lon + 1.5

    modulovalue = 10
    if (i % modulovalue == 0) then
        midtime = time.latch();
        dtime = midtime-intervaltime
        print('Points read:', i, dtime)
        intervaltime = time.latch();
    end
end

stoptime = time.latch();
dtime = stoptime-starttime
print('\n')
print(max_cnt, 'points read time', dtime)

os.exit()

-- Clean Up --
-- Report Results --

runner.report()

