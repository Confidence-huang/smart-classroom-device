"""Read ELF32 section headers without fromelf or third-party packages."""
import json
import struct
from pathlib import Path

root=Path(__file__).resolve().parents[2]
path=root/'tests/algorithms/build/ecovision_algorithm.arm.o'
data=path.read_bytes()
assert data[:7] == b'\x7fELF\x01\x01\x01'
header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
offset, size, count, names_index=header[6],header[11],header[12],header[13]
sections=[struct.unpack_from('<IIIIIIIIII',data,offset+i*size) for i in range(count)]
names_section=sections[names_index]
names=data[names_section[4]:names_section[4]+names_section[5]]
result={}
for section in sections:
    name=names[section[0]:].split(b'\0',1)[0].decode()
    if name.startswith(('.text','.rodata','.data','.bss')):
        result[name]=section[5]
report={'compiler':'Keil AC6 armclang 6.16; Cortex-M4 Thumb; C99 -Oz -Wall -Wextra -Werror',
        'kind':'relocatable object, not linked firmware', 'sections_bytes':result,
        'stack_wcet':'NOT MEASURED on target',
        'host_struct_bytes':{'EvState':276,'EvInput':72,'EvOutput':14,'EvDebounce':8}}
out=root/'tests/algorithms/evidence/arm_resources.json'
out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(out.read_text())
