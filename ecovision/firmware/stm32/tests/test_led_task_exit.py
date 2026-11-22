"""Host regression for the actual StartLedTask failure-return path.

Compiles the task entry extracted from main.c with a returning LED stub and
a non-returning CMSIS exit stub. This is not a FreeRTOS hardware test.
"""
import re
import subprocess
import tempfile
from pathlib import Path

source = (Path(__file__).resolve().parents[1] /
          'EcoVision_F413ZH/Src/main.c').read_text(encoding='utf-8-sig')
match = re.search(r'void\s+StartLedTask\(void\s*\*argument\)\s*\{', source)
assert match, 'Task entry definition not found'
end = match.end()
depth = 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
entry = source[match.start():end]
harness = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
static jmp_buf exit_target;
static int led_calls, exit_calls;
static int huart3;
static void AppLed_Run(void *port) { assert(port == &huart3); led_calls++; }
__declspec(noreturn) void osThreadExit(void) {
    exit_calls++;
    longjmp(exit_target, 1);
}
''' + entry + r'''
int main(void) {
    if (setjmp(exit_target) == 0) {
        StartLedTask(NULL);
        fputs("FAIL: LED task returned instead of exiting through CMSIS\n", stderr);
        return 1;
    }
    assert(led_calls == 1 && exit_calls == 1);
    puts("PASS: actual LED task entry exits through CMSIS when LED setup returns");
    return 0;
}
'''
vcvars = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
with tempfile.TemporaryDirectory(prefix='ecovision-led-exit-') as directory:
    work = Path(directory)
    c_file, executable = work / 'entry.c', work / 'entry.exe'
    c_file.write_text(harness, encoding='utf-8')
    command = (f'call "{vcvars}" >nul && cl /nologo /W4 /Od '
               f'"{c_file}" /Fo:"{work / "entry.obj"}" /Fe:"{executable}" '
               '/link /INCREMENTAL:NO')
    batch = work / 'compile.cmd'
    batch.write_text('@echo off\n' + command + '\n', encoding='utf-8')
    subprocess.run(['cmd.exe', '/d', '/c', str(batch)], check=True)
    subprocess.run([str(executable)], check=True)
