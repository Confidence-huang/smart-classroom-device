"""SIMULATED counts over REAL UART. Not a visual accuracy test."""
import sys
sys.path.insert(0,'/sdcard/ecovision_uart')
import os
import time
from uart_board import BoardUart
board=BoardUart()
try:
    print('UART SMOKE: simulated 0,3,12,invalid; 4 seconds each')
    while True:
        os.exitpoint()
        now=board.now()
        stage=(now//4000)%4
        count=(0,3,12,0)[stage]
        board.publisher.service(now,count,stage!=3)
        time.sleep_ms(10)
except KeyboardInterrupt:
    print('UART smoke stopped')
finally:
    board.close()
