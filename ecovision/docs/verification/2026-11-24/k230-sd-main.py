"""K230 SD-card entry for the existing EcoVision vision and UART application."""

entry = "/sdcard/ecovision_uart/figurine_uart.py"
with open(entry) as source:
    application = compile(source.read(), entry, "exec")
exec(application, {"__name__": "__main__", "__file__": entry})
