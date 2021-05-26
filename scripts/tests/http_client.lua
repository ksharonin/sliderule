local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Unit Test --

server = core.httpd(9081)
endpoint = core.endpoint()
server:attach(endpoint, "/source")

client = core.http("127.0.0.1", 9081)

print('\n------------------\nTest01: Return\n------------------')

rsps = client:request("GET", "/source/version", "{}")
print(rsps)
rsps_table = json.decode(rsps)
runner.check(rsps_table["server"]["packages"][1] == "core")

print('\n------------------\nTest02: Return\n------------------')

json_object = '{ \
    \"var1\": false, \
    \"var2\": \"*.rec\", \
    \"var3\": 4, \
    \"var4\": { \
        \"type\": \"FILE\", \
        \"files\": \"P01_01.dat\", \
        \"format\": \"CCSDS\" \
    } \
}'

rsps = client:request("GET", "/source/example_source_endpoint", json_object)
print(rsps)
rsps_table = json.decode(rsps)
runner.check(rsps_table["result"] == "Hello World")

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()
