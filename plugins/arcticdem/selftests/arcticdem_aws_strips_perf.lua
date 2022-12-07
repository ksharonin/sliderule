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

local failedSamples = 0
-- local verbose = true
local verbose = false

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

if status ~= true then
    failedSamples = failedSamples + 1
    print(string.format("Failed to read point lon: %f, lat: %f", lon, lat))
else
    if verbose then
        for j, v in ipairs(tbl) do
            local el = v["value"]
            local fname = v["file"]
            print(string.format("(%02d) %20f     %s", j, el, fname))
        end
    end
end

print(string.format("ExecTime: %f, failed reads: %d", dtime, failedSamples))

local el, status
local max_cnt = 1000
failedSamples = 0

--[[
--]]

print('\n------------------\nTest: Reading', max_cnt, '  points (THE SAME POINT)\n------------')

starttime = time.latch();
intervaltime = starttime


for i = 1, max_cnt
do
    tbl, status = dem:samples(lon, lat)
    if status ~= true then
        failedSamples = failedSamples + 1
        print(string.format("Failed to read point# %d, lon: %f, lat: %f",i, lon, lat))
    end

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
print(string.format("\n%d points read, time: %f, failed reads: %d", max_cnt, dtime, failedSamples))

-- os.exit()

failedSamples = 0
-- max_cnt = 1000
max_cnt = 100
print('\n------------------\nTest: Reading', max_cnt, '  different points (THE SAME RASTER)\n------------')
lon = _lon
lat = _lat
starttime = time.latch();
intervaltime = starttime

for i = 1, max_cnt
do
    tbl, status = dem:samples(lon, lat)
    if status ~= true then
        failedSamples = failedSamples + 1
        print(string.format("Failed to read point# %d, lon: %f, lat: %f",i, lon, lat))
    else
        if verbose then
            for i, v in ipairs(tbl) do
                local el = v["value"]
                local fname = v["file"]
                print(string.format("(%02d) %20f     %s", i, el, fname))
            end
        end
    end

    if (i % 40 == 0) then
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
print(string.format("\n%d points read, time: %f, failed reads: %d", max_cnt, dtime, failedSamples))

os.exit()

-- Clean Up --
-- Report Results --

runner.report()

