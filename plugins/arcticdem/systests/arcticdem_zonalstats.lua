local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)


-- Unit Test --

local  lon = 180.00
local  lat = 66.34  -- Arctic Circle lat

local samplingAlg = "ZonalStats"
local demType = "arcticdem-mosaic"

print(string.format("\n-------------------------------------------------\nTest %s: Zonal Stats\n-------------------------------------------------", demType))
for radius = 50, 200, 50
do
    local dem = geo.vrt(demType, samplingAlg, radius)
    local tbl, status = dem:sample(lon, lat)
    if status ~= true then
        print(string.format("======> FAILED to read",lon, lat))
    else
        local el, min, max, mean, stdd, file
        for j, v in ipairs(tbl) do
            el = v["value"]
            min = v["min"]
            max = v["max"]
            mean = v["mean"]
            stdd = v["stdd"]
            fname = v["file"]
            print(string.format("(%02d) radius: (%03d) val: %.2f min: %.2f max: %.2f mean: %.2f stdd: %.2f", j, radius, el, min, max, mean, stdd))
        end
    end
end

-- os.exit()

demType = "arcticdem-strips"
lon = -178.0
lat =   51.8

print(string.format("\n-------------------------------------------------\nTest %s: Zonal Stats\n-------------------------------------------------", demType))
for radius = 50, 100, 50
do
    local dem = geo.vrt(demType, samplingAlg, radius)
    local tbl, status = dem:sample(lon, lat)
    if status ~= true then
        print(string.format("======> FAILED to read",lon, lat))
    else
        local el, min, max, mean, stdd, file
        for j, v in ipairs(tbl) do
            el = v["value"]
            min = v["min"]
            max = v["max"]
            mean = v["mean"]
            stdd = v["stdd"]
            fname = v["file"]
            print(string.format("(%02d) radius: (%03d) val: %.2f min: %.2f max: %.2f mean: %.2f stdd: %.2f", j, radius, el, min, max, mean, stdd))
        end
    end
    print('\n-------------------------------------------------')
end

os.exit()
