local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)

-- Setup --

local assets = asset.loaddir()

local _,td = runner.srcscript()
local geojsonfile = td.."../data/grand_mesa_1m_dem.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()


local _,td = runner.srcscript()
local poifile = td.."../../landsat/data/grand_mesa_poi.txt"
local f = io.open(poifile, "r")
-- read in array of POI from file
local arr = {}
for l in f:lines() do
  local row = {}
  for snum in l:gmatch("(%S+)") do
    table.insert(row, tonumber(snum))
  end
  table.insert(arr, row)
end
f:close()

-- show the collected array
--[[
for i=1, 10 do
    print( string.format("%.16f  %.16f", arr[i][1], arr[i][2]))
end
]]

print(string.format("\n-------------------------------------------------\n3dep 1meter DEM Grand Mesa test\n-------------------------------------------------"))

local demType = "3dep-1meter-dem"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents }))

local expectedFile = "HLS.S30.T12SYJ.2022004T180729.v2.0 {\"algo\": \"NDVI\"}"
local badFileCnt = 0
local badNDVICnt = 0
local samplesCnt = 0
local errorChecking = true

local starttime = time.latch();

for i=1,#arr do
-- for i=1000, 1002 do
    local  lon = arr[i][1]
    local  lat = arr[i][2]
    local  tbl, status = dem:sample(lon, lat)
    if status ~= true then
        print(string.format("======> FAILED to read", lon, lat))
    end

    samplesCnt = samplesCnt + 1

    local modulovalue = 3000
    if (i % modulovalue == 0) then
        print(string.format("Sample: %d", samplesCnt))
    end

end

local stoptime = time.latch();
local dtime = stoptime - starttime

print(string.format("\nSamples: %d, wrong NDVI: %d, wrong groupID: %d", samplesCnt, badNDVICnt, badFileCnt))
print(string.format("ExecTime: %f", dtime))

os.exit()
