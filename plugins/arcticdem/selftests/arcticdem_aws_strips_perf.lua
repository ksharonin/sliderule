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


local  lon = -178.0
local  lat =   51.7
local _lon = lon
local _lat = lat

print('\n------------------\nTest: AWS strips\n------------')
local dem = arcticdem.raster("strip", "NearestNeighbour", 0)
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
local max_cnt = 1000
-- local max_cnt = 500

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

    modulovalue = 100
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


print('\n------------------\nTest: Reading', max_cnt, '  different points (THE SAME RASTER)\n------------')
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

    if (i % 100 == 0) then
        lon = _lon
        lat = _lat
    else
        lon = lon + 0.01
        lat = lat + 0.01
    end

    modulovalue = 100
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

