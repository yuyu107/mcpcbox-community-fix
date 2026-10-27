"""Execute the adapter machine code with mocked Windows and original box APIs.
This verifies ABI/routing, not the real Windows downloader or UI."""
from pathlib import Path
import pefile,struct,re,hashlib,functools,os
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *

pe=pefile.PE('MCPCBox_NativeAdapter_v18/MCPCBox.exe')
option_bytes=Path(os.environ.get('MCPC_OPTION_DLL','upload/mcoption.dll')).read_bytes();assert hashlib.sha1(option_bytes).hexdigest()=='e568c557abf461fc5d8f1918b3d45412f9a01976'
option_pe=pefile.PE(data=option_bytes)
task_pe=pefile.PE(os.environ.get('MCPC_TASK_DLL','original/extract/mcpcbox/2.0.1762993.606/taskmodulehttp.dll'))
linkmap=Path('native-v18/payload.map').read_text()
@functools.lru_cache(None)
def symbol(name):
 m=re.search(r'^\s*(0x[0-9a-f]+)\s+'+re.escape(name)+r'\s*$',linkmap,re.M);assert m,name;return int(m[1],16)-0x400000

def run(base):
 delta=base-0x400000;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x1000)
 u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
 image=bytearray(pe.get_memory_mapped_image())
 for block in pe.DIRECTORY_ENTRY_BASERELOC:
  for e in block.entries:
   if e.type==3:struct.pack_into('<I',image,e.rva,(struct.unpack_from('<I',image,e.rva)[0]+delta)&0xffffffff)
 u.mem_write(base,bytes(image));u.mem_map(0x60000000,0x20000);u.mem_map(0x61000000,0x200000);u.mem_map(0x70000000,0x200000)
 dllbase=0x10000000 if base==0x400000 else 0x18000000
 dllimage=bytearray(option_pe.get_memory_mapped_image())
 for block in option_pe.DIRECTORY_ENTRY_BASERELOC:
  for e in block.entries:
   if e.type==3:struct.pack_into('<I',dllimage,e.rva,(struct.unpack_from('<I',dllimage,e.rva)[0]+dllbase-0x10000000)&0xffffffff)
 u.mem_map(dllbase,(option_pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(dllbase,bytes(dllimage))
 taskbase=0x20000000;u.mem_map(taskbase,(task_pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(taskbase,task_pe.get_memory_mapped_image())
 heap=[0x61010000];imports={};handles={'wnd':0,'ui_pending':None};posts=[];creates=[];native_rows=[];dtor=[];controls=[];snapshots=[];events={};closed_events=[];waits=[];stop_points={base+0x2c5578};reached=[]
 RET=0x6001f000;UI_RET=0x6001f010;ROOT_FN=0x6001f020;ADD_FN=0x6001f030
 CONNECT_FN=0x6001e100;REQUEST_FN=0x6001e110;CLOSE_FN=0x6001e120
 net_connects=[];net_requests=[];net_closed=[];net_roots=[];limits={73:2,74:2}
 fake_option=0x61001000;fake_vt=dllbase+0x6ae2c;game_record=0x61008000;manager=base+0x3c8978;rows=0x61004000
 def dw(a):return struct.unpack('<I',u.mem_read(a,4))[0]
 def put(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def alloc(n):p=heap[0];heap[0]+=(n+15)&~15;assert heap[0]<0x61200000;return p
 def wide(a):
  b=bytearray()
  for i in range(4096):
   c=u.mem_read(a+i*2,2)
   if c==b'\0\0':return b.decode('utf-16le')
   b.extend(c)
  raise AssertionError('Unterminated wide string')
 def wideput(a,s):u.mem_write(a,s.encode('utf-16le')+b'\0\0')
 def narrow(a):
  data=bytearray()
  while True:
   ch=bytes(u.mem_read(a+len(data),1))
   if ch==b'\0':return bytes(data)
   data.extend(ch)
 def ws(a,s):
  u.mem_write(a,bytes(28));put(a+20,len(s))
  if len(s)<8:wideput(a+4,s);put(a+24,7)
  else:p=alloc((len(s)+1)*2);wideput(p,s);put(a+4,p);put(a+24,len(s))
 def wsread(a):return wide(dw(a+4) if dw(a+24)>=8 else a+4)
 def ret(value=0,cleanup=0,high=0):
  sp=u.reg_read(UC_X86_REG_ESP);dest=dw(sp);u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_EDX,high&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+4+cleanup);u.reg_write(UC_X86_REG_EIP,dest)
 for mod in pe.DIRECTORY_ENTRY_IMPORT:
  for imp in mod.imports:
   if not imp.name:continue
   name=imp.name.decode();address=0x60000000+len(imports)*16;imports[address]=name;u.mem_write(address,b'\xc3')
   if name in ['__mb_cur_max','_iob']:
    ptr=0x61000000+(0 if name=='__mb_cur_max' else 0x100);put(ptr,1)
   else:ptr=address
   put(base+imp.address-0x400000,ptr)
 task_slots={}
 for mod in task_pe.DIRECTORY_ENTRY_IMPORT:
  if mod.dll.lower()==b'wininet.dll':
   for imp in mod.imports:
    if imp.name and imp.name.decode() in {'InternetConnectW','HttpOpenRequestW','InternetCloseHandle'}:
     name=imp.name.decode();slot=taskbase+imp.address-task_pe.OPTIONAL_HEADER.ImageBase;task_slots[name]=slot
     put(slot,{'InternetConnectW':CONNECT_FN,'HttpOpenRequestW':REQUEST_FN,'InternetCloseHandle':CLOSE_FN}[name])
 put(fake_option,fake_vt);put(fake_vt+0x5c,ROOT_FN);put(fake_vt+0x9c,ADD_FN);put(fake_vt+0x94,0x6001f050)
 put(base+0x3c8974,1);u.mem_write(base+0x3c8a18,b'\1')
 def hook(uc,address,size,data):
  if address in stop_points:reached.append(address);u.emu_stop();return
  sp=u.reg_read(UC_X86_REG_ESP);this=u.reg_read(UC_X86_REG_ECX)
  def arg(n):return dw(sp+4+n*4)
  if address==base+symbol('__mingw_vsprintf'):
   value=dw(arg(2));u.mem_write(arg(0),('%02x'%value).encode()+b'\0');ret(2);return
  if address==CONNECT_FN:
   net_connects.append([arg(i) for i in range(8)]);ret(0x75000000+len(net_connects)*16,32);return
  if address==REQUEST_FN:
   net_requests.append([arg(i) for i in range(8)]);ret(0x76000000+len(net_requests)*16,32);return
  if address==CLOSE_FN:net_closed.append(arg(0));ret(1,4);return
  if address==base+0x98f0:ret(manager);return
  if address==base+0x6a80 and handles.get('track_compaction'):handles.setdefault('record_dtors',[]).append(arg(0))
  if address==base+0xb93c0:
   assert this==manager and arg(0)==16
   handles['batch_publishes']=handles.get('batch_publishes',0)+1;put(manager+0x70,dw(manager+0x58));put(manager+0x74,dw(manager+0x5c));ret(0,4);return

  if address==base+0xb9f00 and handles.get('history_remove'):
   assert this==manager and arg(2)==1
   begin,end=dw(manager+0x58),dw(manager+0x5c)
   for row in range(begin,end,0x7c):
    if dw(row)==arg(1) and dw(row+4)==arg(0):
     u.mem_write(row,bytes(u.mem_read(row+0x7c,end-row-0x7c)));put(manager+0x5c,end-0x7c);put(manager+0x74,end-0x7c);break
   handles.setdefault('history_removed',[]).append((arg(1),arg(0)));ret(0,12);return
  if address==base+0xb9f00 and arg(0)==handles.get('completed_retire_id'):
   assert this==manager and arg(1)==3 and arg(2)==1
   begin,end=dw(manager+0x58),dw(manager+0x5c);u.mem_write(begin,bytes(u.mem_read(begin+0x7c,end-begin-0x7c)));put(manager+0x5c,end-0x7c)
   put(manager+0x74,end-0x7c);handles['completed_retired']=True;ret(0,12);return
  if address==base+0xb9640 and arg(0)==handles.get('stale_id'):
   assert this==manager and arg(1)==3;put(manager+0x2c,handles['stale_row']);handles['stale_retired']=True;ret(0,8);return
  if address==base+0xb9640 and arg(0)==handles.get('retire_id'):
   assert this==manager and arg(1)==3;handles['retired']=True;ret(0,8);return
  if address==0x6001f050:
   assert this==fake_option and arg(0)==123 and arg(1)==0
   put(game_record+0xc,0);wideput(game_record+0xd8,'');wideput(game_record+0x268,'');handles['unregistered']=handles.get('unregistered',0)+1;ret(0,8);return
  if address==ROOT_FN:assert this==fake_option;ws(arg(0),'D:\\MCBox\\download');ret(0,4);return
  if address==base+symbol('NativeOptionGuard'):assert arg(0)==dllbase;ret(0 if handles.get('reject_option') else 1);return
  if address==dllbase+0xa350:
   assert this==fake_option and u.mem_read(game_record+4,1)==b'\0';handles['saved']=handles.get('saved',0)+1;ret();return
  if address==dllbase+0x10070:assert arg(0)==fake_option;handles['notified']=True;ret(0,4);return
  if address==dllbase+0x5ff70:
   name=bytes(u.mem_read(arg(0),100)).split(b'\0')[0].decode();handles.setdefault('xml_attrs',{})[name]=arg(1);ret(0,8);return
  if address==ADD_FN:
   assert this==fake_option;handles['registered']=wide(arg(0));handles['imports']=handles.get('imports',0)+1
   u.mem_write(game_record,bytes(2*0x4b0));put(fake_option+0x148,game_record);put(fake_option+0x14c,game_record+2*0x4b0)
   put(game_record,123);u.mem_write(game_record+4,b'\1');put(game_record+0xc,2);wideput(game_record+0xd8,'1.8.9');wideput(game_record+0x268,wide(arg(0))+'\\')
   other=game_record+0x4b0;put(other,124);u.mem_write(other+4,b'\1');put(other+0xc,2);wideput(other+0xd8,'1.8.9');wideput(other+0x268,'D:\\OtherLocal')
   ret(1,4);return
  if address==0x6001f040:
   original_sp=handles.pop('history_wait_sp');u.reg_write(UC_X86_REG_ESP,original_sp);ret(0,8);return
  if address==UI_RET:
   p=handles.pop('ui_pending');put(p['out'],u.reg_read(UC_X86_REG_EAX));u.reg_write(UC_X86_REG_ESP,p['sp']);ret(1,28);return
  if address in [base+0xb9280,base+0xb9320]:
   assert this==manager;handles['ui_observers']=handles.get('ui_observers',0)+1;ret();return
  if address in [base+0xba120,base+0x2f7e0]:
   stride=0x88 if address==base+0xba120 else 0x7c
   assert dw(dw(this))==this, 'source vector proxy must reference source view'
   begin,end=dw(this+12),dw(this+16);assert end>=begin and (end-begin)%stride==0
   dest=arg(0);count=(end-begin)//stride;copied=alloc(max(count*stride,16));proxy=alloc(4);put(proxy,dest);u.mem_write(dest,bytes(24));put(dest,proxy);put(dest+12,copied);put(dest+16,copied+count*stride);put(dest+20,copied+count*stride)
   for i in range(count):
    r=begin+i*stride;d=copied+i*stride;u.mem_write(d,bytes(u.mem_read(r,stride)))
    for off in ([8,0x24,0x40,0x5c] if stride==0x88 else [8,0x24,0x40,0x60]):ws(d+off,wsread(r+off))
   snapshots.append((stride,count));ret(dest,4);return
  if address==base+0xb71a0:
   request=this;assert u.reg_read(UC_X86_REG_EDI)==manager
   assert arg(0)==request+0x78
   entry={'id':dw(request+4),'url':wsread(request+0x24),'target':wsread(request+0x40),'name':wsread(request+8),'nativeId':500+len(native_rows)}
   assert entry['url'].startswith('https://piston-data.mojang.com/')
   assert entry['target'].startswith('D:\\MCBox\\download\\game\\1.8.9\\.minecraft\\')
   assert wsread(request+0x5c)==''
   row=rows+len(native_rows)*0x88;u.mem_write(row,bytes(u.mem_read(request,0x78))+bytes(16));put(row+0x78,entry['nativeId']);native_rows.append(entry)
   put(manager+0x28,rows);put(manager+0x2c,rows+len(native_rows)*0x88);creates.append(entry);ret(0,4);return
  if address==base+0x4ac0:dtor.append(arg(0));ret(0,4);return
  if address not in imports:return
  name=imports[address]
  if name=='GetModuleHandleW':ret((dllbase if wide(arg(0)).lower()=='mcoption.dll' else taskbase if wide(arg(0)).lower()=='taskmodulehttp.dll' else base) if arg(0) else base)
  elif name=='InternetQueryOptionW':
   handle,opt,buffer,length=[arg(i) for i in range(4)]
   if handles.get('query_fail') and handle:ret(0,16)
   elif not handle and opt in limits:put(buffer,limits[opt]);put(length,4);ret(1,16)
   elif opt==41:wideput(buffer,'');put(length,2);ret(1,16)
   elif opt==38:
    proxy=handles.get('net_proxy');put(buffer,3 if proxy else 1);put(buffer+4,buffer+32 if proxy else 0);put(buffer+8,0)
    if proxy:wideput(buffer+32,proxy)
    put(length,12);ret(1,16)
   elif opt in [2,5,6]:put(buffer,handles.get('net_timeout',10000));put(length,4);ret(1,16)
   else:raise AssertionError(('unknown net query',opt))
  elif name=='InternetSetOptionW':
   if not arg(0) and arg(1) in limits:limits[arg(1)]=dw(arg(2))
   ret(1,16)
  elif name=='InternetOpenW':
   assert arg(1) in [1,3] and arg(4)==0;root=0x74000000+len(net_roots)*16;net_roots.append(root);ret(root,20)
  elif name=='InitializeCriticalSection':u.mem_write(arg(0),bytes(24));ret(0,4)
  elif name in ['EnterCriticalSection','LeaveCriticalSection']:ret(0,4)
  elif name=='GetModuleFileNameW':s='C:\\Box\\MCPCBox.exe';wideput(arg(1),s);ret(len(s),12)
  elif name=='GetCurrentProcess':ret(0xffffffff)
  elif name=='IsWow64Process':put(arg(1),1);ret(1,8)
  elif name=='CryptAcquireContextW':put(arg(0),123);ret(1,20)
  elif name=='CryptGenRandom':u.mem_write(arg(2),bytes(range(arg(1))));ret(1,12)
  elif name=='CryptReleaseContext':ret(1,8)
  elif name=='CreateThread':ret(0xbeef,24)
  elif name=='CloseHandle':
   if arg(0) in events:closed_events.append(arg(0));events.pop(arg(0))
   ret(1,4)
  elif name=='CreateEventW':
   assert arg(1)==1 and arg(2)==0;event=alloc(4);events[event]=False;ret(event,16)
  elif name=='SetEvent':assert arg(0) in events;events[arg(0)]=True;ret(1,4)
  elif name=='WaitForSingleObject':
   waits.append((arg(0),arg(1)))
   if not events.get(arg(0)) and not handles.get('history_timeout'):
    pending=[p for p in posts if p[1]==0x3ee and dw(p[2]+4)==0x64000001 and p[2] not in handles.setdefault('history_markers',set())]
    if pending:
     post=pending[-1];handles['history_markers'].add(post[2]);handles['history_wait_sp']=sp
     raw=bytes(u.mem_read(post[2],0x78))+bytes(u.mem_read(post[3],8));u.mem_write(sp-512,struct.pack('<I',0x6001f040)+raw)
     u.reg_write(UC_X86_REG_ESP,sp-512);u.reg_write(UC_X86_REG_ECX,manager);u.reg_write(UC_X86_REG_EDX,0);u.reg_write(UC_X86_REG_EIP,base+0xb7580);return
   ret(0 if events.get(arg(0)) else 258,8)
  elif name=='VirtualQuery':u.mem_write(arg(1),struct.pack('<IIIIIII',base,base,0x40,pe.OPTIONAL_HEADER.SizeOfImage,0x1000,0x40,0x1000000));ret(28,12)
  elif name=='VirtualProtect':put(arg(3),0x40);ret(1,16)
  elif name in ['GetLastError']:ret(0)
  elif name=='SetLastError':ret(0,4)
  elif name=='strlen':ret(len(narrow(arg(0))))
  elif name in ['strcmp','_stricmp']:
   a,b=narrow(arg(0)),narrow(arg(1))
   if name=='_stricmp':a,b=a.lower(),b.lower()
   ret((a>b)-(a<b))
  elif name=='memcmp':
   a,b=bytes(u.mem_read(arg(0),arg(2))),bytes(u.mem_read(arg(1),arg(2)));ret((a>b)-(a<b))
  elif name=='_errno':ret(0x61000400)
  elif name=='strcpy':
   src=arg(1);data=bytearray()
   while True:
    ch=bytes(u.mem_read(src+len(data),1));data.extend(ch)
    if ch==b'\0':break
   u.mem_write(arg(0),bytes(data));ret(arg(0))
  elif name=='wcslen':ret(len(wide(arg(0))))
  elif name=='wcsrchr':
   i=wide(arg(0)).rfind(chr(arg(1)));ret(arg(0)+2*i if i>=0 else 0)
  elif name=='wcsncmp':a,b=wide(arg(0))[:arg(2)],wide(arg(1))[:arg(2)];ret((a>b)-(a<b))
  elif name in ['wcscmp','_wcsicmp']:a,b=wide(arg(0)),wide(arg(1));ret((a>b)-(a<b))
  elif name=='wcscpy':dest=arg(0);wideput(dest,wide(arg(1)));ret(dest)
  elif name=='wcscat':dest=arg(0);wideput(dest,wide(dest)+wide(arg(1)));ret(dest)
  elif name=='memset':dest=arg(0);u.mem_write(dest,bytes([arg(1)&255])*arg(2));ret(dest)
  elif name=='memcpy':dest=arg(0);u.mem_write(dest,bytes(u.mem_read(arg(1),arg(2))));ret(dest)
  elif name=='malloc' or name=='calloc':ret(alloc(arg(0)*(arg(1) if name=='calloc' else 1)))
  elif name=='free':ret(0)
  elif name.startswith('??0?$basic_string@_W'):ws(this,'');ret(this)
  elif name.startswith('??4?$basic_string@_W') and 'PB_W' in name:ws(this,wide(arg(0)));ret(this,4)
  elif name.startswith('??4?$basic_string@_W') and 'ABV01' in name:
   if handles.get('track_compaction'):handles['string_assigns']=handles.get('string_assigns',0)+1
   ws(this,wsread(arg(0)));ret(this,4)

  elif name.startswith('??1?$basic_string@_W'):ret(0)
  elif name.startswith('??0CallbackBase') and 'PAVBindState' in name:u.mem_write(this,bytes(8));ret(this,4)
  elif name.startswith('??1CallbackBase'):ret(0)
  elif name.startswith('?Acquire@Lock') or name.startswith('?Release@Lock'):ret(0)
  elif name=='??2@YAPAXI@Z':ret(alloc(arg(0)))
  elif name=='??3@YAXPAX@Z':ret(0)
  elif name.startswith('?getInterface@MCOPTION'):ret(fake_option)
  elif name=='FindWindowW':ret(0x3333,8)
  elif name=='IsWindow':ret(1,4)
  elif name=='GetWindowThreadProcessId':put(arg(1),4444);ret(99,8)
  elif name=='GetCurrentProcessId':ret(4444)
  elif name=='SetWindowLongW':handles['wnd']=arg(2);ret(0x6001e000,12)
  elif name=='SendMessageTimeoutW':
   if arg(1)==0x8372:
    assert arg(2)==0x4f463731 and arg(3)==0 and arg(5)==2000
    if handles.get('refresh_timeout'):ret(0,28);return
   handles['ui_pending']={'sp':sp,'out':arg(6)}
   args=[arg(i) for i in range(4)]
   ns=sp-20;u.mem_write(ns,struct.pack('<IIIII',UI_RET,*args));u.reg_write(UC_X86_REG_ESP,ns);u.reg_write(UC_X86_REG_EIP,handles['wnd'])
  elif name=='CallWindowProcW':ret(1,20)
  elif name=='PostMessageW':
   failed=handles.get('history_post_fail') and arg(1)==0x3ee and dw(arg(2)+4)==0x64000001
   if not failed:posts.append((arg(0),arg(1),arg(2),arg(3)))
   ret(0 if failed else 1,16)
  elif name=='GetTaskMaxSize':ret(123456)
  elif name in ['PauseTask','StartTask']:controls.append((name,arg(0)));ret(1)
  elif name=='GetTaskCurSize':assert arg(0)>=500;ret(12345)
  else:raise AssertionError('Unexpected API: '+name)
 u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_X86_REG_ESP,0x701ff000)
 u.emu_start(base+pe.OPTIONAL_HEADER.AddressOfEntryPoint,base+pe.OPTIONAL_HEADER.SizeOfImage,count=2000000)
 assert reached==[base+0x2c5578]
 stop_points.clear();stop_points.add(RET)
 def call(address,args=(),ecx=0,raw=None,cleanup=0):
  payload=raw if raw is not None else struct.pack('<'+'I'*len(args),*args)
  sp=0x701f0000;u.mem_write(sp,struct.pack('<I',RET)+payload);u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,ecx);u.reg_write(UC_X86_REG_EDX,0);reached.clear()
  u.emu_start(address,base+pe.OPTIONAL_HEADER.SizeOfImage,count=2000000)
  assert reached==[RET],(hex(address),reached)
  assert u.reg_read(UC_X86_REG_ESP)==sp+4+cleanup,(hex(address),hex(u.reg_read(UC_X86_REG_ESP)))
  return u.reg_read(UC_X86_REG_EAX)
 def badge():
  sp=0x701f0000;u.mem_write(sp,struct.pack('<I',RET));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EBX,4)
  stop_points.clear();stop_points.add(base+0x75637);reached.clear()
  u.emu_start(base+0x75632,base+pe.OPTIONAL_HEADER.SizeOfImage,count=2000000)
  assert reached==[base+0x75637] and u.reg_read(UC_X86_REG_ESP)==sp
  assert u.reg_read(UC_X86_REG_EAX)==manager
  value=u.reg_read(UC_X86_REG_EBX);stop_points.clear();stop_points.add(RET);return value
 out=alloc(8192);err=alloc(512)
 assert call(base+symbol('adapter_root'),[out,4096,err])==1
 assert wide(out)=='D:\\MCBox\\download'
 def submit(target):
  name=alloc(512);url=alloc(1024);dest=alloc(1024);task=alloc(4)
  wideput(name,'Minecraft 1.8.9 - client.jar');wideput(url,'https://piston-data.mojang.com/v1/objects/abcdef/client.jar');wideput(dest,target)
  assert call(base+symbol('adapter_submit'),[name,url,dest,task,err])==1
  post=posts[-1];assert post[0]==0x3333 and post[1]==0x3ee
  request=post[2];cb=post[3];assert dw(request)==3
  stack=bytes(u.mem_read(request,0x78))+bytes(u.mem_read(cb,8))
  before=len(dtor)
  call(base+0xb7580,ecx=manager,raw=stack,cleanup=0x80)
  assert len(dtor)==before+1
  return dw(task),creates[-1],dest
 group_name=alloc(512);group_dir=alloc(1024);wideput(group_name,'Minecraft 1.8.9');wideput(group_dir,'D:\\MCBox\\download\\game\\1.8.9')
 assert call(base+symbol('adapter_group_begin'),[group_name,group_dir,1000000,0,err])==1
 task,record,dest=submit('D:\\MCBox\\download\\game\\1.8.9\\.minecraft\\versions\\1.8.9\\1.8.9.jar')
 size=alloc(8);error=alloc(4)
 call(base+symbol('adapter_wait'),[task,500]);assert waits[-1][1]==500 and not events[waits[-1][0]]
 assert call(base+symbol('adapter_status'),[task,size,error])==1
 assert struct.unpack('<Q',u.mem_read(size,8))[0]==12345
 def event(rva,args,resume):
  sp=0x701f0000;u.mem_write(sp,struct.pack('<I',RET)+struct.pack('<'+'I'*len(args),*args));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,manager);old_ebp=0x7abcabcd;u.reg_write(UC_X86_REG_EBP,old_ebp)
  stop_points.clear();stop_points.add(base+resume);reached.clear();u.emu_start(base+rva,base+pe.OPTIONAL_HEADER.SizeOfImage,count=2000000)
  assert reached==[base+resume];assert u.reg_read(UC_X86_REG_EBP)==sp-4;assert u.reg_read(UC_X86_REG_ESP)==((sp-4)&~7)
  assert dw(sp-4)==old_ebp
  stop_points.clear();stop_points.add(RET)
 event(0xb7c80,[record['nativeId'],dest,0],0xb7c86)
 assert call(base+symbol('adapter_status'),[task,size,error])==2
 assert events[waits[-1][0]], 'completion must wake the waiting worker immediately'
 assert call(base+symbol('adapter_task_pending'),[task])==1, 'callback precedes native cleanup'
 put(manager+0x2c,rows);assert call(base+symbol('adapter_task_pending'),[task])==0
 put(manager+0x2c,rows+0x88)
 call(base+symbol('adapter_release'),[task]);assert closed_events, 'release completion event after the worker exits'
 event(0xb7c80,[record['nativeId'],dest,0],0xb7c86)
 closed_before=len(closed_events);call(base+symbol('adapter_release'),[task]);assert len(closed_events)==closed_before, 'late callbacks and repeated release must not use a closed event'
 task2,record2,_=submit('D:\\MCBox\\download\\game\\1.8.9\\.minecraft\\versions\\1.8.9\\1.8.9.jar')
 # Delayed callbacks for an earlier attempt at the SAME target cannot poison
 # the replacement or rewrite its native ID; terminal results are monotonic.
 event(0xb7c80,[record['nativeId'],dest,0],0xb7c86)
 assert call(base+symbol('adapter_status'),[task2,size,error])==1
 event(0xb7c80,[9999,dest,0],0xb7c86)
 assert call(base+symbol('adapter_status'),[task2,size,error])==1
 event(0xb7d50,[record['nativeId'],12029],0xb7d56)
 assert call(base+symbol('adapter_status'),[task,size,error])==2
 assert call(base+symbol('adapter_status'),[task2,size,error])==1
 event(0xb7d50,[record2['nativeId'],12029],0xb7d56)
 assert call(base+symbol('adapter_status'),[task2,size,error])==0xffffffff
 assert dw(error)==12029
 regpath=alloc(1024);wideput(regpath,'D:\\MCBox\\download\\game\\1.8.9')
 assert call(base+symbol('adapter_register'),[regpath,err])==1
 assert handles['registered']=='D:\\MCBox\\download\\game\\1.8.9'
 assert handles['saved']==1 and handles['notified'] and handles['imports']==1
 assert u.mem_read(game_record+4,1)==b'\0' and dw(game_record+8)&0xf0000000==0x50000000
 assert u.mem_read(game_record+0x4b0+4,1)==b'\1', 'unrelated local game must retain its category'
 assert dw(game_record+0x4a8)==1, 'pure UI also requires versionType=1'
 assert dw(game_record+0x4b0+0x4a8)==0, 'unrelated versionType must remain unchanged'
 assigned=dw(game_record+8);assert call(base+symbol('adapter_register'),[regpath,err])==1
 assert handles['imports']==1 and dw(game_record+8)==assigned, 'cached install must not duplicate or renumber'
 handles['reject_option']=True;assert call(base+symbol('adapter_register'),[regpath,err])==0
 handles['reject_option']=False
 # Execute the supplied DLL's actual serializer instructions for its category,
 # status and resource ID, stubbing only the XML SetIntAttribute method.
 owner=alloc(24);proxy=alloc(4);put(owner+0x10,game_record+2*0x4b0);put(proxy,owner)
 sp=0x701f0000;u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EDI,game_record);u.reg_write(UC_X86_REG_ESI,proxy);u.reg_write(UC_X86_REG_EBP,0x12340000)
 stop_points.clear();stop_points.add(dllbase+0xaeca);reached.clear()
 u.emu_start(dllbase+0xae2c,dllbase+option_pe.OPTIONAL_HEADER.SizeOfImage,count=2000000)
 assert reached==[dllbase+0xaeca]
 assert handles['xml_attrs']['islocal']==0 and handles['xml_attrs']['status']==2 and handles['xml_attrs']['resid']==assigned
 # Execute the actual DLL serializer for the previously omitted versionType.
 sp=0x701f0000;u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EDI,game_record);u.reg_write(UC_X86_REG_EBP,0x12340000)
 stop_points.clear();stop_points.add(dllbase+0xaf5f);reached.clear()
 u.emu_start(dllbase+0xaf4c,dllbase+option_pe.OPTIONAL_HEADER.SizeOfImage,count=2000000)
 assert handles['xml_attrs']['versionType']==1
 stop_points.clear();stop_points.add(RET)
 # Execute original EXE category predicate, stubbing only insertion into its vector.
 # 0=online, 1=pure, 2=local. This catches the v11 omission.
 predicate_state=alloc(8);put(predicate_state,alloc(24))
 original_insert_hook=base+0x61080
 def accepted(row,category):
  put(predicate_state+4,category);sp=0x701f0000;u.mem_write(sp,struct.pack('<I',RET))
  u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,row);u.reg_write(UC_X86_REG_EDX,predicate_state)
  stop_points.clear();stop_points.update([RET,original_insert_hook]);reached.clear()
  u.emu_start(base+0x874c0,base+pe.OPTIONAL_HEADER.SizeOfImage,count=2000000)
  return reached==[original_insert_hook]
 assert accepted(game_record,1), 'official install must enter original pure list'
 assert not accepted(game_record,0) and not accepted(game_record,2)
 assert accepted(game_record+0x4b0,2) and not accepted(game_record+0x4b0,1)
 put(game_record+0x4a8,0)
 assert not accepted(game_record,1), 'reproduce v11 omission with original UI code'
 put(game_record+0x4a8,1)
 stop_points.clear();stop_points.add(RET)
 # One native center row for two real engine tasks, plus unrelated rows unchanged.
 def snapshot(completed=False):
  outv=alloc(24);call(base+(0xb95b0 if completed else 0xb9520),[manager,outv],cleanup=8)
  stride=0x7c if completed else 0x88
  return [dw(outv+12)+i*stride for i in range((dw(outv+16)-dw(outv+12))//stride)]
 put(manager+0x40,rows);put(manager+0x44,rows+len(native_rows)*0x88)
 visible=snapshot();assert len(visible)==1
 gid=dw(visible[0]+4);assert (gid&0xff000000)==0x62000000 and wsread(visible[0]+8)=='Minecraft 1.8.9'
 assert call(dw(base+0x30a608),[gid])==1000000
 call(base+symbol('adapter_group_progress'),[250000,0])
 assert call(base+0xb82b0,[manager,gid,3],cleanup=12)==25
 assert badge()==1
 # Leave a real child running so pause/resume propagation can be observed.
 task3,record3,_=submit('D:\\MCBox\\download\\game\\1.8.9\\.minecraft\\libraries\\active.jar')
 call(base+0xb9910,[gid,3],ecx=manager,cleanup=8)
 assert call(base+symbol('adapter_group_paused'))==1
 assert ('PauseTask',record3['nativeId']) in controls
 visible=snapshot();assert dw(visible[0]+0x84)==2
 call(base+0xb9b00,[gid,3],ecx=manager,cleanup=8)
 assert call(base+symbol('adapter_group_paused'))==0
 assert ('StartTask',record3['nativeId']) in controls
 # A non-adapter native task must remain visible and contribute to badge count.
 ordinary=rows+len(native_rows)*0x88;u.mem_write(ordinary,bytes(0x88));put(ordinary,3);put(ordinary+4,999)
 for off in [8,0x24,0x40,0x5c]:ws(ordinary+off,'ordinary' if off==8 else '')
 put(manager+0x44,ordinary+0x88)
 assert len(snapshot())==2
 # Completed native child history is hidden, ordinary completed entries preserved.
 done_rows=alloc(2*0x7c)
 for i,logical in enumerate([record['id'],777]):
  r=done_rows+i*0x7c;u.mem_write(r,bytes(0x7c));put(r,3);put(r+4,logical)
  for off in [8,0x24,0x40,0x60]:ws(r+off,'ordinary completed' if i and off==8 else '')
 put(manager+0x70,done_rows);put(manager+0x74,done_rows+2*0x7c)
 assert len(snapshot(True))==1 and dw(snapshot(True)[0]+4)==777
 assert badge()==2
 call(base+symbol('adapter_group_progress'),[1000000,0])
 assert call(base+0xb82b0,[manager,gid,3],cleanup=12)==99
 call(base+symbol('adapter_group_finish'),[4])
 assert len(snapshot())==1, 'finished group must leave active tab'
 done=snapshot(True);assert len(done)==1 and dw(done[-1]+4)==gid and dw(done[-1]+0x5c)==1000000
 assert call(base+0xb82b0,[manager,gid,3],cleanup=12)==100
 before_observers=handles.get('ui_observers',0)
 assert call(base+symbol('adapter_group_sync'))==1 and handles['ui_observers']==before_observers+2
 handles['refresh_timeout']=True;assert call(base+symbol('adapter_group_sync'))==0
 handles['refresh_timeout']=False;assert call(base+symbol('adapter_root'),[out,4096,err])==1, 'refresh timeout must not poison UI commands'
 # New completed APIs run against the real adapter machine code.
 put(manager+0x58,done_rows);put(manager+0x5c,done_rows+2*0x7c)
 ws(done_rows+0x7c+0x24,'D:\\MCBox\\download\\file.zip')
 ws(done_rows+0x7c+0x40,'https://official.example/file.zip')
 # The original DeleteDownloadedFile binding passes completed-row +0x24
 # to internal::DeleteFileSync. Execute that untouched original slice.
 sp=0x701f0000;u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EBX,done_rows+0x7c)
 stop_points.clear();stop_points.add(base+0xbdb10);reached.clear()
 u.emu_start(base+0xb9e7c,base+pe.OPTIONAL_HEADER.SizeOfImage,count=2000000)
 assert reached==[base+0xbdb10] and dw(u.reg_read(UC_X86_REG_ESP)+12)==done_rows+0x7c+0x24
 stop_points.clear();stop_points.add(RET)

 def history_exec(operation,type=0,id=0):
  cmd=alloc(2596);u.mem_write(cmd,bytes(2596));put(cmd+12,operation);put(cmd+24,type);put(cmd+28,id)
  call(base+symbol('HistoryExecute'),[manager,cmd]);return cmd
 handles['completed_retire_id']=record['id']
 history_out=alloc(4);history_count=alloc(4);history_path=alloc(2048)
 before_posts=len(posts);before_events=len(events)
 assert call(base+symbol('adapter_history_list'),[history_out,history_count,err])==1 and dw(history_count)==2
 assert len(posts)==before_posts and len(events)==before_events, 'read-only history must not allocate events or post UI/download work'
 assert call(base+symbol('adapter_history_path'),[3,gid,history_path,err])==1 and wide(history_path).endswith('1.8.9')
 handles['history_timeout']=True
 assert call(base+symbol('adapter_history_path'),[3,gid,history_path,err])==0
 handles['history_timeout']=False
 assert call(base+symbol('adapter_history_path'),[3,gid,history_path,err])==1, 'late queued completion must not reference freed event/storage'
 handles['history_post_fail']=True
 assert call(base+symbol('adapter_history_path'),[3,gid,history_path,err])==0
 handles['history_post_fail']=False
 # Queue dispatch cleans the consumed internal child. Recreate the source view.
 put(manager+0x58,done_rows);put(manager+0x5c,done_rows+0x7c)
 hc=history_exec(1);assert dw(hc+16)==1 and dw(hc+20)==2
 hr=dw(hc+32);assert dw(hr+12)==777 and wide(hr+1044)=='D:\\MCBox\\download\\file.zip'
 assert dw(hr+3096+12)==gid and dw(hr+3096+16)==1 and wide(hr+3096+1044)=='D:\\MCBox\\download\\game\\1.8.9'
 hc=history_exec(3,3,gid);assert dw(hc+16)==1 and wide(hc+36)=='D:\\MCBox\\download\\game\\1.8.9'
 hc=history_exec(3,3,777);assert dw(hc+16)==1 and wide(hc+36).endswith('file.zip')
 hc=history_exec(2,3,gid);assert dw(hc+16)==1 and len(snapshot(True))==1
 hc=history_exec(2,3,gid);assert dw(hc+16)==0, 'repeat removal must report missing record'
 hc=history_exec(2,3,record['id']);assert dw(hc+16)==0, 'internal child rows cannot be removed as user history'
 handles['history_remove']=True
 hc=history_exec(2,3,777);assert dw(hc+16)==1 and handles['history_removed']==[(3,777)]
 hc=history_exec(1);assert dw(hc+16)==1 and dw(hc+20)==0
 handles['history_remove']=False
 large=alloc(100*0x7c)
 for i in range(100):
  r=large+i*0x7c;u.mem_write(r,bytes(0x7c));put(r,3);put(r+4,10000+i)
  for off in [8,0x24,0x40,0x60]:ws(r+off,('D:\\MCBox\\download\\file%d.zip'%i) if off==0x24 else '')
 put(manager+0x70,large);put(manager+0x74,large+100*0x7c)
 before_posts=len(posts);before_events=len(events)
 assert call(base+symbol('adapter_history_list'),[history_out,history_count,err])==1 and dw(history_count)==100
 assert len(posts)==before_posts and len(events)==before_events
 assert len(snapshot(True))==1, 'hidden native host must build at most one presence row, while public API retains all 100'
 rev=call(base+symbol('adapter_history_revision'));assert call(base+symbol('adapter_history_revision'))==rev
 put(manager+0x74,large+99*0x7c);assert call(base+symbol('adapter_history_revision'))!=rev
 # Execute the real native record-copy and record-destructor machine code.
 # MSVC string allocation/assignment and snapshot publication APIs are mocked.
 put(manager+0x58,large);put(manager+0x5c,large+100*0x7c)
 handles['track_compaction']=True;handles['record_dtors']=[];handles['string_assigns']=0;handles['batch_publishes']=0
 def mutations(keys):
  entries=alloc(len(keys)*524)
  for i,(type,id) in enumerate(keys):u.mem_write(entries+i*524,bytes(524));put(entries+i*524,type);put(entries+i*524+4,id)
  return entries
 selected=mutations([(3,10000+2*i) for i in range(50)])
 before_markers=sum(1 for p in posts if p[1]==0x3ee and dw(p[2]+4)==0x64000001)
 assert call(base+symbol('adapter_history_remove_batch'),[selected,50,err])==1
 assert all(dw(selected+i*524+8)==1 for i in range(50))
 assert sum(1 for p in posts if p[1]==0x3ee and dw(p[2]+4)==0x64000001)==before_markers+1
 assert handles['batch_publishes']==1 and handles['string_assigns']==50*4
 assert len(handles['record_dtors'])==50 and len(set(handles['record_dtors']))==50
 assert dw(manager+0x5c)==large+50*0x7c
 for i in range(50):
  r=large+i*0x7c;assert dw(r+4)==10001+2*i and wsread(r+0x24).endswith('file%d.zip'%(2*i+1))
 handles['record_dtors']=[];handles['string_assigns']=0;handles['batch_publishes']=0
 selected=mutations([(3,10001+2*i) for i in range(50)])
 assert call(base+symbol('adapter_history_remove_batch'),[selected,50,err])==1
 assert dw(manager+0x5c)==large and handles['batch_publishes']==1
 assert len(handles['record_dtors'])==50 and handles['string_assigns']==0, 'delete-all must destroy rows once, with zero survivor copies'
 handles['track_compaction']=False
 assert call(base+symbol('adapter_group_begin'),[group_name,group_dir,1000000,0,err])==1
 call(base+symbol('adapter_group_finish'),[4]);batch_gid=dw(snapshot(True)[0]+4)
 selected=mutations([(3,batch_gid),(3,1234567),(3,record['id'])]);publishes=handles['batch_publishes']
 assert call(base+symbol('adapter_history_remove_batch'),[selected,3,err])==1
 assert dw(selected+8)==1 and dw(selected+524+8)==0 and dw(selected+1048+8)==0
 assert handles['batch_publishes']==publishes, 'group-only erasure must not rebuild native history'
 put(manager+0x70,done_rows);put(manager+0x74,done_rows+2*0x7c)
 put(manager+0x58,done_rows);put(manager+0x5c,done_rows+2*0x7c)
 # Restore fixture metadata for existing native cleanup tests below.
 for i,logical in enumerate([record['id'],777]):
  r=done_rows+i*0x7c;put(r,3);put(r+4,logical)
  for off in [8,0x24,0x40,0x60]:ws(r+off,'ordinary completed' if i and off==8 else '')

 # New group cancellation removes its one parent row and signals the job.
 assert call(base+symbol('adapter_group_begin'),[group_name,group_dir,1000000,0,err])==1
 gid2=dw(snapshot()[-1]+4);call(base+0xb9640,[gid2,3],ecx=manager,cleanup=8)
 assert len(snapshot())==1
 # Startup marker cleans only orphaned adapter task IDs, through the manager.
 stale=ordinary+0x88;u.mem_write(stale,bytes(0x88));put(stale,3);put(stale+4,0x61000055)
 for off in [8,0x24,0x40,0x5c]:ws(stale+off,'Minecraft 1.8.9 - old file' if off==8 else '')
 put(manager+0x2c,stale+0x88);handles['stale_id']=0x61000055;handles['stale_row']=stale
 handles['completed_retire_id']=record['id'];put(manager+0x58,done_rows);put(manager+0x5c,done_rows+2*0x7c)
 marker=[p for p in posts if p[1]==0x3ee and dw(p[2]+4)==0x64000000][-1]
 stack=bytes(u.mem_read(marker[2],0x78))+bytes(u.mem_read(marker[3],8))
 before=len(creates);call(base+0xb7580,ecx=manager,raw=stack,cleanup=0x80)
 assert handles.get('stale_retired') and len(creates)==before and dw(ordinary+4)==999
 assert handles.get('completed_retired') and dw(done_rows+4)==777, 'remove consumed child history, retain ordinary history'
 # Cancel/retry releases the original task on its own dispatch thread.
 handles['retire_id']=record3['id'];call(base+symbol('adapter_cancel'),[task3])
 post=posts[-1];assert post[1]==0x3ee and dw(post[2]+4)==record3['id']
 stack=bytes(u.mem_read(post[2],0x78))+bytes(u.mem_read(post[3],8))
 call(base+0xb7580,ecx=manager,raw=stack,cleanup=0x80)
 assert handles.get('retired')
 # Ordinary native controls must continue through their original prologues.
 for rva,resume in [(0xb9910,0xb9916),(0xb9b00,0xb9b06),(0xb9640,0xb9646),(0xb9d40,0xb9d46)]:event(rva,[999,3],resume)
 # Exercise the compiled WinINet hooks through the real task DLL's IAT.
 assert call(base+symbol('native_network_init'))==1
 assert limits=={73:32,74:32}
 host=alloc(512);verb=alloc(32);obj=alloc(64);wideput(host,'resources.download.minecraft.net');wideput(verb,'GET');wideput(obj,'/test')
 def connect(original=0x7100,user=0):return call(dw(task_slots['InternetConnectW']),[original,host,443,user,0,3,0,0],cleanup=32)
 def request(conn):return call(dw(task_slots['HttpOpenRequestW']),[conn,verb,obj,0,0,0,0x84800000,0],cleanup=32)
 c1=connect();q1=request(c1);first_root=net_connects[-1][0]
 assert first_root==net_roots[0] and net_requests[-1][6]==0x84c00000
 call(dw(task_slots['InternetCloseHandle']),[q1],cleanup=4);call(dw(task_slots['InternetCloseHandle']),[c1],cleanup=4);call(dw(task_slots['InternetCloseHandle']),[0x7100],cleanup=4)
 c2=connect(0x7200);q2=request(c2);assert len(net_roots)==1 and net_connects[-1][0]==first_root
 assert c2==c1 and len(net_connects)==1 and c1 not in net_closed
 # A connection closed with an active request is actually closed, preserving cancel.
 call(dw(task_slots['InternetCloseHandle']),[c2],cleanup=4);assert c2 in net_closed
 call(dw(task_slots['InternetCloseHandle']),[q2],cleanup=4)
 assert first_root not in net_closed, 'native per-file session closure must not close the shared official session'
 wideput(host,'resources.download.minecraft.net.evil');c3=connect();request(c3)
 assert net_connects[-1][0]==0x7100 and net_requests[-1][6]==0x84800000
 wideput(host,'resources.download.minecraft.net');user=alloc(32);wideput(user,'login');c4=connect(user=user);request(c4)
 assert net_connects[-1][0]==0x7100 and net_requests[-1][6]==0x84800000
 handles['net_timeout']=20000;c5=connect();assert len(net_roots)==2 and net_connects[-1][0]!=first_root
 handles['net_proxy']='proxy.example:8080';c6=connect();assert len(net_roots)==3
 handles['query_fail']=True;c7=connect();assert net_connects[-1][0]==0x7100
 handles['query_fail']=False
 # A leased connection is never handed to two active file tasks.
 c8=connect();q8=request(c8);c9=connect();assert c8!=c9
 call(dw(task_slots['InternetCloseHandle']),[q8],cleanup=4);call(dw(task_slots['InternetCloseHandle']),[c8],cleanup=4)
 call(dw(task_slots['InternetCloseHandle']),[c9],cleanup=4)
 # A failed version must no longer count/render as downloading, and it must
 # not be fabricated as a successfully completed entry.
 assert call(base+symbol('adapter_group_begin'),[group_name,group_dir,1000000,0,err])==1
 fail_gid=dw(snapshot()[-1]+4);before_count=badge()
 call(base+symbol('adapter_group_finish'),[3])
 assert badge()==before_count-1 and all(dw(r+4)!=fail_gid for r in snapshot())
 assert all(dw(r+4)!=fail_gid for r in snapshot(True))
 # UI refresh notifications are marshaled by an independent posted message.
 assert any(p[1]==0x8372 for p in posts)
 assert call(handles['wnd'],[0x3333,0x8372,0x4f463731,0],cleanup=16)==1
 # File deletion clears only the exact pure-version registration on the UI thread.
 handles['reject_option']=True;assert call(base+symbol('adapter_unregister'),[regpath,err])==0
 handles['reject_option']=False
 assert call(base+symbol('adapter_unregister'),[regpath,err])==1
 assert handles['unregistered']==1 and dw(game_record+0xc)==0 and u.mem_read(game_record+0x4b0+4,1)==b'\1'
 assert call(base+symbol('adapter_unregister'),[regpath,err])==1 and handles['unregistered']==1
 # Unrelated resources replay the overwritten original SEH prologue.
 sp=0x701f0000;u.mem_write(sp,struct.pack('<III',RET,1,123));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,manager)
 stop_points.clear();stop_points.add(base+0xb7587);reached.clear();u.emu_start(base+0xb7580,base+pe.OPTIONAL_HEADER.SizeOfImage,count=2000000)
 assert reached==[base+0xb7587];assert u.reg_read(UC_X86_REG_ESP)==sp-8
 assert dw(sp-8)==base+0x2ff9bf and dw(sp-4)==0xffffffff
 print('PASS same-target late callback isolation/monotonic terminal result, callback vs native cleanup, synchronous terminal UI flush/timeout isolation, failed groups removed from active; batch single-publication/original native compaction+destructors, fast snapshots and adapter ABI at',hex(base),': configured root, WM 0x3ee, native manager call/target, stack cleanup, progress, success/error, registration, grouped center snapshots/count/total/percentage/pause/resume/cancel/completion, unrelated-task passthrough; supplied-DLL category serialization; official WinINet root reuse, keep-alive, credential/domain/proxy/timeout isolation and query-failure fallback.')

for address in [0x400000,0x2000000]:run(address)
print('Mocked original/Windows APIs: no real native transfer, Windows UI, or launch test.')
