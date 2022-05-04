local console_monitor = core.monitor(core.LOG, core.INFO, core.FMT_TEXT)
console_monitor:name("console.monitor")
console_monitor:tail(1024)

local console_dispatcher = core.dispatcher(core.EVENTQ)
console_dispatcher:name("console.dispatcher")
console_dispatcher:attach(console_monitor, "eventrec")
console_dispatcher:run()

local package = {
    dispatcher = console_dispatcher,
    monitor = console_monitor,
}

return package
