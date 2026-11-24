# DMA and watchdog implementation plan
Goal: reduce business warnings, add circular DMA/IDLE UART reception and supervised IWDG without changing public protocol or pins.
Architecture: HAL ReceiveToIdle DMA + HT/TC copy bounded spans to existing SPSC rings. Tasks retain parsing, errors discard and recover. A single heartbeat supervisor feeds IWDG only when all critical tasks advance; modem waits beat in bounded sleep chunks.
Tech stack: F413 C99/HAL, CMSIS-RTOS2, AC6, native C host tests.
1. Add failing host regressions for circular cursor (wrap/duplicate/invalid positions) and health policy (startup/missing/stale/wrap/latching).
2. Implement portable policies and UART HAL adapter, link USART2/6 and preserve stream7.
3. Add task beats, chunk modem sleeps, reset-cause report and delayed supervised IWDG startup. Use no teammate alert/TTS code; before future integration consolidate IWDG to one owner.
4. Fix observed signed CRC and misleading-indentation warnings; build full project and run host regression.
5. Deliver a separately named HEX, hash, configuration-sector range check and tomorrow's hardware test checklist. No board attached, no flash, no push/merge into module PR.
