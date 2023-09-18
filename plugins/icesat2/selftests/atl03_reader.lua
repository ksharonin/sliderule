local runner = require("test_executive")
-- local console = require("console")
local asset = require("asset")
local json = require("json")

-- Setup --

local assets = asset.loaddir()
local asset_name = "icesat2"
local nsidc_s3 = core.getbyname(asset_name)
local name, identity, driver = nsidc_s3:info()

local creds = aws.csget(identity)
if not creds then
    local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
    local response, _ = netsvc.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(identity, credential)
end

-- Unit Test --

print('\n------------------\nTest01: Atl03 Reader \n------------------')

local f1_0 = icesat2.atl03(nsidc_s3, "missing_file", "tmpq", icesat2.parms({srt=icesat2.SRT_SEA_ICE, cnf=icesat2.CNF_NOT_CONSIDERED, track=icesat2.RPT_1}))
local p1_0 = f1_0:parms()

runner.check(p1_0.srt == icesat2.SRT_SEA_ICE, "Failed to set surface type")
runner.check(not p1_0.cnf[icesat2.CNF_POSSIBLE_TEP], "Failed to not set _tep_")
runner.check(p1_0.cnf[icesat2.CNF_NOT_CONSIDERED], "Failed to set _not considered_")
runner.check(p1_0.cnf[icesat2.CNF_BACKGROUND], "Failed to set _background_")
runner.check(p1_0.cnf[icesat2.CNF_WITHIN_10M], "Failed to set _within 10m_")
runner.check(p1_0.cnf[icesat2.CNF_SURFACE_LOW], "Failed to set _low_")
runner.check(p1_0.cnf[icesat2.CNF_SURFACE_MEDIUM], "Failed to set _medium_")
runner.check(p1_0.cnf[icesat2.CNF_SURFACE_HIGH], "Failed to set _high_")

local f1_1 = icesat2.atl03(nsidc_s3, "missing_file", "tmpq", icesat2.parms({srt=icesat2.SRT_SEA_ICE, cnf={icesat2.CNF_NOT_CONSIDERED}, track=icesat2.RPT_1}))
local p1_1 = f1_1:parms()

runner.check(not p1_1.cnf[icesat2.CNF_POSSIBLE_TEP], "Failed to not set _tep_")
runner.check(p1_1.cnf[icesat2.CNF_NOT_CONSIDERED], "Failed to set _not considered_")
runner.check(not p1_1.cnf[icesat2.CNF_BACKGROUND], "Failed to not set _background_")
runner.check(not p1_1.cnf[icesat2.CNF_WITHIN_10M], "Failed to not set _within 10m_")
runner.check(not p1_1.cnf[icesat2.CNF_SURFACE_LOW], "Failed to not set _low_")
runner.check(not p1_1.cnf[icesat2.CNF_SURFACE_MEDIUM], "Failed to not set _medium_")
runner.check(not p1_1.cnf[icesat2.CNF_SURFACE_HIGH], "Failed to not set _high_")

local f1_2 = icesat2.atl03(nsidc_s3, "missing_file", "tmpq", icesat2.parms({srt=icesat2.SRT_SEA_ICE, cnf={"atl03_not_considered"}, track=icesat2.RPT_1}))
local p1_2 = f1_2:parms()

runner.check(not p1_2.cnf[icesat2.CNF_POSSIBLE_TEP], "Failed to not set _tep_")
runner.check(p1_2.cnf[icesat2.CNF_NOT_CONSIDERED], "Failed to set _not considered_")
runner.check(not p1_2.cnf[icesat2.CNF_BACKGROUND], "Failed to not set _background_")
runner.check(not p1_2.cnf[icesat2.CNF_WITHIN_10M], "Failed to not set _within 10m_")
runner.check(not p1_2.cnf[icesat2.CNF_SURFACE_LOW], "Failed to not set _low_")
runner.check(not p1_2.cnf[icesat2.CNF_SURFACE_MEDIUM], "Failed to not set _medium_")
runner.check(not p1_2.cnf[icesat2.CNF_SURFACE_HIGH], "Failed to not set _high_")

local f1_3 = icesat2.atl03(nsidc_s3, "missing_file", "tmpq",icesat2.parms({srt=icesat2.SRT_SEA_ICE, cnf={"atl03_low", "atl03_medium", "atl03_high"}, track=icesat2.RPT_1}))
local p1_3 = f1_3:parms()

runner.check(not p1_3.cnf[icesat2.CNF_POSSIBLE_TEP], "Failed to not set _tep_")
runner.check(not p1_3.cnf[icesat2.CNF_NOT_CONSIDERED], "Failed to not set _not considered_")
runner.check(not p1_3.cnf[icesat2.CNF_BACKGROUND], "Failed to not set _background_")
runner.check(not p1_3.cnf[icesat2.CNF_WITHIN_10M], "Failed to not set _within 10m_")
runner.check(p1_3.cnf[icesat2.CNF_SURFACE_LOW], "Failed to set _low_")
runner.check(p1_3.cnf[icesat2.CNF_SURFACE_MEDIUM], "Failed to set _medium_")
runner.check(p1_3.cnf[icesat2.CNF_SURFACE_HIGH], "Failed to set _high_")

print('\n------------------\nTest02: Atl03 Extent Record\n------------------')

local recq = msg.subscribe("atl03-reader-recq")
local tstart = time.latch()
local f2 = icesat2.atl03(nsidc_s3, "ATL03_20200304065203_10470605_005_01.h5", "atl03-reader-recq", icesat2.parms({cnf=4, track=icesat2.RPT_1}))
local extentrec = recq:recvrecord(15000)
print("Time to execute: "..tostring(time.latch() - tstart))

runner.check(extentrec, "Failed to read an extent record")

if extentrec then
    runner.check(extentrec:getvalue("track") == 1, extentrec:getvalue("track"))
    runner.check(extentrec:getvalue("segment_id") == 555765, extentrec:getvalue("segment_id"))

    local t2 = extentrec:tabulate()
    runner.check(t2.segment_id == extentrec:getvalue("segment_id"))
end

print('\n------------------\nTest03: Atl03 Extent Definition\n------------------')

local def = msg.definition("atl03rec")
print("atl03rec", json.encode(def))

-- Clean Up --

recq:destroy()
f1_0:destroy() 
f1_1:destroy() 
f1_2:destroy() 
f1_3:destroy() 
f2:destroy()

-- Report Results --

runner.report()

