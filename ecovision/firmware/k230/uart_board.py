"""Confirmed wiring: GPIO5 TX / GPIO6 RX, UART2, 115200 8N1."""
import time
from machine import FPIOA, UART
from runtime_identity import reserve_random_id
from figurine_uart_link import UartPublisher

class BoardUart:
    def __init__(self):
        self.fpioa=FPIOA()
        self.fpioa.set_function(5,FPIOA.UART2_TXD)
        self.fpioa.set_function(6,FPIOA.UART2_RXD)
        self.uart=UART(UART.UART2,baudrate=115200,bits=UART.EIGHTBITS,
                       parity=UART.PARITY_NONE,stop=UART.STOPBITS_ONE,timeout=0)
        try:
            self.publisher=UartPublisher(self.uart,reserve_random_id('/sdcard/ecovision_uart/.boot_id_ledger'))
        except Exception:
            self.uart.deinit()
            raise
        self.last_tick=time.ticks_ms()
        self.uptime=0
        print('UART2 ready: GPIO5 TX -> PD6, GPIO6 RX <- PD5; boot',self.publisher.link.boot_id)
    def now(self):
        tick=time.ticks_ms()
        self.uptime+=time.ticks_diff(tick,self.last_tick)
        self.last_tick=tick
        return self.uptime
    def close(self):
        self.uart.deinit()
