from pathlib import Path
import pefile,struct,re
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *

path=Path('MCPCBox_NativeAdapter_v19/MCPCBox.exe');pe=pefile.PE(str(path))
linkmap=Path('native-v19/payload.map').read_text()
fmt_match=re.search(r'^\s*(0x[0-9a-f]+)\s+__mingw_vsprintf\s*$',linkmap,re.M)
assert fmt_match
printf_preferred=int(fmt_match[1],16)
regs=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
def run(base,crypto_ok):
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
 image=bytearray(pe.get_memory_mapped_image());delta=base-pe.OPTIONAL_HEADER.ImageBase
 for block in pe.DIRECTORY_ENTRY_BASERELOC:
  for entry in block.entries:
   if entry.type==3:struct.pack_into('<I',image,entry.rva,(struct.unpack_from('<I',image,entry.rva)[0]+delta)&0xffffffff)
 u.mem_write(base,bytes(image));u.mem_map(0x60000000,0x20000);u.mem_map(0x61000000,0x100000);u.mem_map(0x70000000,0x200000)
 def dword(a):return struct.unpack('<I',u.mem_read(a,4))[0]
 def put(a,v):u.mem_write(a,struct.pack('<I',v))
 def wide(a):
  data=bytearray()
  for i in range(4096):
   b=u.mem_read(a+i*2,2)
   if b==b'\0\0':return data.decode('utf-16le')
   data.extend(b)
  raise AssertionError('unterminated wide string')
 def narrow(a):
  data=bytearray()
  for i in range(4096):
   b=u.mem_read(a+i,1)
   if b==b'\0':return data.decode('ascii')
   data.extend(b)
  raise AssertionError('unterminated string')
 def writewide(a,s):u.mem_write(a,s.encode('utf-16le')+b'\0\0')
 imports={};index=0
 for module in pe.DIRECTORY_ENTRY_IMPORT:
  for imp in module.imports:
   if imp.name is None:continue
   name=imp.name.decode();stub=0x60000000+16*index;index+=1;imports[stub]=name;u.mem_write(stub,b'\xc3')
   if name in ['__mb_cur_max','_iob']:
    ptr=0x61000000+(0 if name=='__mb_cur_max' else 0x100);put(ptr,1);put(base+imp.address-pe.OPTIONAL_HEADER.ImageBase,ptr)
   else:put(base+imp.address-pe.OPTIONAL_HEADER.ImageBase,stub)
 reached=[];threads=[];heap=[0x61010000]
 def ret(value,cleanup=0):
  sp=u.reg_read(UC_X86_REG_ESP);target=dword(sp);u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+4+cleanup);u.reg_write(UC_X86_REG_EIP,target)
 def hook(uc,addr,size,data):
  if addr==base+0x2c5578:reached.append(addr);u.emu_stop();return
  sp=u.reg_read(UC_X86_REG_ESP)
  def arg(n):return dword(sp+4+4*n)
  if addr==printf_preferred+delta:
   assert narrow(arg(1))=='%02x';v=dword(arg(2));out=('%02x'%v).encode();u.mem_write(arg(0),out+b'\0');ret(len(out));return
  if addr not in imports:return
  name=imports[addr]
  if name=='GetModuleHandleW':ret(base,4)
  elif name=='InitializeCriticalSection':u.mem_write(arg(0),bytes(24));ret(0,4)
  elif name=='GetModuleFileNameW':
   s='C:\\测试\\MCPCBox.exe';writewide(arg(1),s);ret(len(s),12)
  elif name=='GetCurrentProcess':ret(0xffffffff)
  elif name=='IsWow64Process':put(arg(1),1);ret(1,8)
  elif name=='CryptAcquireContextW':
   if crypto_ok:put(arg(0),123)
   ret(1 if crypto_ok else 0,20)
  elif name=='CryptGenRandom':u.mem_write(arg(2),bytes(range(arg(1))));ret(1,12)
  elif name=='CryptReleaseContext':ret(1,8)
  elif name=='CreateThread':threads.append(arg(2));ret(0xbeef,24)
  elif name=='CloseHandle':ret(1,4)
  elif name=='VirtualQuery':
   u.mem_write(arg(1),struct.pack('<IIIIIII',base,base,0x40,pe.OPTIONAL_HEADER.SizeOfImage,0x1000,0x40,0x1000000));ret(28,12)
  elif name=='VirtualProtect':put(arg(3),0x40);ret(1,16)
  elif name=='GetLastError':ret(0)
  elif name=='wcslen':ret(len(wide(arg(0))))
  elif name=='wcsrchr':
   s=wide(arg(0));i=s.rfind(chr(arg(1)));ret(arg(0)+i*2 if i>=0 else 0)
  elif name=='wcsncmp':
   a,b=wide(arg(0))[:arg(2)],wide(arg(1))[:arg(2)];ret((a>b)-(a<b))
  elif name=='wcscpy':dest=arg(0);writewide(dest,wide(arg(1)));ret(dest)
  elif name=='wcscat':dest=arg(0);writewide(dest,wide(dest)+wide(arg(1)));ret(dest)
  elif name=='memset':dest=arg(0);u.mem_write(dest,bytes([arg(1)&255])*arg(2));ret(dest)
  elif name=='memcpy':dest=arg(0);u.mem_write(dest,bytes(u.mem_read(arg(1),arg(2))));ret(dest)
  elif name=='malloc':
   p=heap[0];heap[0]+=(arg(0)+15)&~15;ret(p)
  elif name=='free':ret(0)
  else:raise AssertionError('Unexpected API during native entry: '+name)
 u.hook_add(UC_HOOK_CODE,hook)
 initial=[0x11111111,0x22222222,0x33333333,0x44444444,0x55555555,0x66666666,0x77777777]
 for reg,value in zip(regs,initial):u.reg_write(reg,value)
 esp=0x701ff000;u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EFLAGS,0x202)
 u.emu_start(base+pe.OPTIONAL_HEADER.AddressOfEntryPoint,base+pe.OPTIONAL_HEADER.SizeOfImage,count=2000000)
 assert reached==[base+0x2c5578]
 assert [u.reg_read(r) for r in regs]==initial
 assert u.reg_read(UC_X86_REG_ESP)==esp
 assert u.reg_read(UC_X86_REG_EFLAGS)==0x202
 assert len(threads)==(1 if crypto_ok else 0)
 if threads:assert base+pe.sections[-2].VirtualAddress<=threads[0]<base+pe.sections[-2].VirtualAddress+pe.sections[-2].Misc_VirtualSize
 print('PASS: native entry + real pseudo-relocation helper at',hex(base),'crypto',crypto_ok,'registers/flags/stack preserved, original entry reached.')
for base in [0x400000,0x2000000]:
 for crypto in [False,True]:run(base,crypto)
print('Windows APIs were stubbed; this does not test Windows networking, UI, or game launch.')
