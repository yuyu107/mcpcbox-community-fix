from pathlib import Path
import argparse,struct,pefile,hashlib,json,re,xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description='Merge the native module into the exact v4 single-list base executable.')
parser.add_argument('--base',type=Path,default=ROOT.parent/'MCPCBox_Pure_UI_v4_SingleList/MCPCBox.exe')
parser.add_argument('--out-dir',type=Path,default=ROOT.parent/'MCPCBox_NativeAdapter_v18')
args=parser.parse_args()
BASE_EXE=args.base
OUT=args.out_dir
OUT.mkdir(exist_ok=True)
base=pefile.PE(str(BASE_EXE));payload=pefile.PE(str(ROOT/'payload.exe'))
assert base.FILE_HEADER.Machine==payload.FILE_HEADER.Machine==0x14c
assert base.OPTIONAL_HEADER.ImageBase==payload.OPTIONAL_HEADER.ImageBase==0x400000
assert base.OPTIONAL_HEADER.AddressOfEntryPoint==0x2c5578
assert not hasattr(payload,'DIRECTORY_ENTRY_TLS')
data=bytearray(BASE_EXE.read_bytes());original=bytes(data)
assert hashlib.sha256(original).hexdigest()=='3f8209b850663b7e64e2244a5ecaf8f6e061bfea0975f00a02de11eaad08ccdc','Expected exact v4 single-list base executable'
def align(x,n):return (x+n-1)//n*n

linkmap=(ROOT/'payload.map').read_text()
def symbol(name):
 m=re.search(r'^\s*(0x[0-9a-f]+)\s+'+re.escape(name)+r'\s*$',linkmap,re.M)
 assert m,name
 return int(m[1],16)
patches=[]
for rva,size,name in [(0xb7580,7,'AdapterResourceGate'),(0xb7c80,6,'AdapterSuccessGate'),(0xb7d50,6,'AdapterErrorGate'),(0xb9520,7,'AdapterDownloading@8'),(0xb95b0,7,'AdapterDownloaded@8'),(0xb82b0,7,'AdapterPercentGate'),(0xb9910,6,'AdapterPauseGate'),(0xb9b00,6,'AdapterResumeGate'),(0xb9640,6,'AdapterDeleteGate'),(0xb9d40,6,'AdapterDeleteDoneGate'),(0x75632,5,'AdapterBadgeGate')]:
 off=base.get_offset_from_rva(rva)
 seh={0xb7580:0x6ff9bf,0xb9520:0x6f10f2,0xb95b0:0x6f4b42,0xb82b0:0x6f11e8}
 expected=b'\xe8'+struct.pack('<i',0x4098f0-(0x475632+5)) if rva==0x75632 else (b'\x6a\xff\x68'+struct.pack('<I',seh[rva])) if rva in seh else b'\x56\x8b\xf0\x57\x8d\x7e' if rva==0xba0b0 else b'\x55\x8b\xec\x83\xe4\xf8'
 assert original[off:off+size]==expected
 target=symbol(name)
 raw=b'\xe9'+struct.pack('<i',target-(base.OPTIONAL_HEADER.ImageBase+rva+5))+b'\x90'*(size-5)
 data[off:off+size]=raw
 patches.append({'rva':rva,'offset':off,'size':size,'target':target,'name':name})

resources=[]
for kind in ['', '_SMALL']:
 name='MCBOX_TAB_GAME_PAGE'+kind+'.XML'
 candidates=[lang.data.struct for t in base.DIRECTORY_ENTRY_RESOURCE.entries if str(t.name)=='XML' for n in t.directory.entries if str(n.name)==name for lang in n.directory.entries if lang.id==2052]
 assert len(candidates)==1
 res=candidates[0];old=base.get_data(res.OffsetToData,res.Size)
 text=old.decode('utf-8').rstrip();url='https://yuyu107.github.io/mcpcbox-downloads/pure-ui-v4/'
 assert text.count(url)==1
 text=text.replace(url,'http://127.0.0.1:19481/')
 raw=text.encode('utf-8');raw+=b' '*(len(old)-len(raw));assert len(raw)==len(old)
 ET.fromstring(raw);start=0;count=0
 while True:
  start=original.find(old,start)
  if start<0:break
  data[start:start+len(old)]=raw;resources.append((start,len(old)));start+=len(old);count+=1
 assert count==2

# Preserve native controls for their existing callbacks, hide the old completed
# list and embed the new selection UI in the same original downloaded pane.
for name,pane in [('MCBOX_TAB_DOWNLOAD_CENTER.XML','downloaded_page'),('IDR_DUI_DOWNLOADDLG','download_tab_1')]:
 candidates=[lang.data.struct for t in base.DIRECTORY_ENTRY_RESOURCE.entries if str(t.name)=='XML' for n in t.directory.entries if str(n.name)==name for lang in n.directory.entries if lang.id==2052]
 assert len(candidates)==1
 res=candidates[0];old=base.get_data(res.OffsetToData,res.Size)
 tree=ET.fromstring(old.decode('utf-8-sig'))
 parent=next(x for x in tree.iter('dlg') if x.get('name')==pane)
 item=next(x for x in parent if x.get('name')=='download_finish_itembox')
 item.set('show','0');item.set('pos','0,0,@1,@1')
 ET.SubElement(parent,'realwnd',{'name':'native_history_web','pos':'0,0,-0,-0','wndclass':'IECTRL','wndname':'http://127.0.0.1:19481/completed.html'})
 # Compact whitespace within the fixed original resource allocation.
 for node in tree.iter():node.text=node.text.strip() if node.text and node.text.strip() else None;node.tail=None
 raw=ET.tostring(tree,encoding='utf-8');assert len(raw)<=len(old),(name,len(raw),len(old))
 raw+=b' '*(len(old)-len(raw));ET.fromstring(raw);start=0;count=0
 while True:
  start=original.find(old,start)
  if start<0:break
  data[start:start+len(old)]=raw;resources.append((start,len(old)));start+=len(old);count+=1
 assert count==2,(name,count)

code=payload.sections[0];assert code.Name.rstrip(b'\0')==b'.text'
assert code.VirtualAddress==base.OPTIONAL_HEADER.SizeOfImage
other=[s for s in payload.sections if s is not code and s.Name.rstrip(b'\0')!=b'.reloc']
data_rva=min(s.VirtualAddress for s in other)
data_end=max(s.VirtualAddress+max(s.SizeOfRawData,s.Misc_VirtualSize) for s in other)
native_data=bytearray(align(data_end-data_rva,4))
for section in other:
 raw=section.get_data();off=section.VirtualAddress-data_rva
 native_data[off:off+len(raw)]=raw

old_imports=[base.__data__[d.struct.get_file_offset():d.struct.get_file_offset()+20] for d in base.DIRECTORY_ENTRY_IMPORT]
new_imports=[payload.__data__[d.struct.get_file_offset():d.struct.get_file_offset()+20] for d in payload.DIRECTORY_ENTRY_IMPORT]
imports_rva=data_rva+len(native_data)
imports=b''.join(old_imports+new_imports)+bytes(20)
native_data.extend(imports)
while len(native_data)%4:native_data.append(0)
relocs_rva=data_rva+len(native_data)
blocks={}
for pe in [base,payload]:
 for block in pe.DIRECTORY_ENTRY_BASERELOC:
  for entry in block.entries:
   if entry.type and not any(x["rva"]<=entry.rva<x["rva"]+x["size"] for x in patches):
    assert entry.type==3,entry.type
    blocks.setdefault(entry.rva&~0xfff,set()).add((entry.type<<12)|(entry.rva&0xfff))
relocs=bytearray()
for page,entries in sorted(blocks.items()):
 entries=sorted(entries)
 if len(entries)%2:entries.append(0)
 relocs.extend(struct.pack('<II',page,8+2*len(entries)))
 relocs.extend(struct.pack('<'+'H'*len(entries),*entries))
native_data.extend(relocs)

fa=base.OPTIONAL_HEADER.FileAlignment;sa=base.OPTIONAL_HEADER.SectionAlignment
code_raw=code.get_data();code_size=align(len(code_raw),fa);code_raw+=bytes(code_size-len(code_raw))
code_offset=align(len(data),fa);data.extend(bytes(code_offset-len(data)));data.extend(code_raw)
data_offset=align(len(data),fa);data.extend(bytes(data_offset-len(data)))
data_size=align(len(native_data),fa);native_data.extend(bytes(data_size-len(native_data)));data.extend(native_data)

section_header=base.sections[-1].get_file_offset()+40
assert section_header+80<=base.OPTIONAL_HEADER.SizeOfHeaders
def section(name,vs,rva,size,offset,flags):return struct.pack('<8sIIIIIIHHI',name,vs,rva,size,offset,0,0,0,0,flags)
data[section_header:section_header+40]=section(b'.mcx\0\0\0\0',code.Misc_VirtualSize,code.VirtualAddress,code_size,code_offset,0x60000020)
data[section_header+40:section_header+80]=section(b'.mcd\0\0\0\0',len(native_data),data_rva,data_size,data_offset,0xc0000040)
def set_field(structure,name,value):struct.pack_into('<I',data,structure.get_field_absolute_offset(name),value)
struct.pack_into('<H',data,base.FILE_HEADER.get_field_absolute_offset('NumberOfSections'),len(base.sections)+2)
set_field(base.OPTIONAL_HEADER,'AddressOfEntryPoint',payload.OPTIONAL_HEADER.AddressOfEntryPoint)
set_field(base.OPTIONAL_HEADER,'SizeOfImage',align(data_rva+len(native_data),sa))
set_field(base.OPTIONAL_HEADER,'SizeOfCode',base.OPTIONAL_HEADER.SizeOfCode+code_size)
set_field(base.OPTIONAL_HEADER,'SizeOfInitializedData',base.OPTIONAL_HEADER.SizeOfInitializedData+data_size)
for index,rva,size in [(1,imports_rva,len(imports)),(5,relocs_rva,len(relocs)),(4,0,0),(11,0,0)]:
 directory=base.OPTIONAL_HEADER.DATA_DIRECTORY[index]
 set_field(directory,'VirtualAddress',rva);set_field(directory,'Size',size)
set_field(base.OPTIONAL_HEADER,'CheckSum',0)
final=pefile.PE(data=bytes(data));set_field(base.OPTIONAL_HEADER,'CheckSum',final.generate_checksum())
out=OUT/'MCPCBox.exe';out.write_bytes(data)
final=pefile.PE(str(out))
assert final.verify_checksum()
assert len(final.DIRECTORY_ENTRY_IMPORT)==len(old_imports)+len(new_imports)
assert final.OPTIONAL_HEADER.DllCharacteristics==base.OPTIONAL_HEADER.DllCharacteristics
assert len(final.sections)==len(base.sections)+2
assert all(not(s.Characteristics&0x80000000 and s.Characteristics&0x20000000) for s in final.sections[-2:])
for old in base.sections:
 if old.Name.rstrip(b'\0')!=b'.rsrc':
  at=old.PointerToRawData
  for offset in range(at,at+old.SizeOfRawData):
   if any(x['offset']<=offset<x['offset']+x['size'] for x in patches):continue
   assert data[offset]==original[offset],hex(offset)
assert sorted((e.rva,e.type) for b in final.DIRECTORY_ENTRY_BASERELOC for e in b.entries if e.type)==sorted((page+(e&0xfff),e>>12) for page,values in blocks.items() for e in values)
assert b'__MCPC_TOKEN__' in bytes(data)
assert b'OfficialDownload.exe' not in bytes(data[code_offset:])
report={'base':'MCPCBox_Pure_UI_v4_SingleList','sha256':hashlib.sha256(data).hexdigest(),'size':len(data),'originalEntryRva':hex(base.OPTIONAL_HEADER.AddressOfEntryPoint),'newEntryRva':hex(final.OPTIONAL_HEADER.AddressOfEntryPoint),'originalCodeChangesRestrictedToNativeTaskHooks':patches,'nativePayloadSections':[{'name':s.Name.rstrip(b'\0').decode(),'rva':hex(s.VirtualAddress),'size':s.Misc_VirtualSize,'flags':hex(s.Characteristics)} for s in final.sections[-2:]],'extraImports':[d.dll.decode() for d in payload.DIRECTORY_ENTRY_IMPORT],'relocationsMerged':sum(len(x) for x in blocks.values()),'aslrPreserved':True,'checksumVerified':True,'managedRuntimeRequired':False,'externalDownloadProcessRequired':False,'windowsRuntimeTested':False,'nativeDownloadCenterSubmissionImplemented':True,'nativeVersionTaskAggregationImplemented':True,'automaticVersionRegistrationAttemptImplemented':True,'nativeTaskRuntimeVerified':False,'gameDownloadEngine':'original CDownloadManager / taskmodulehttp.dll','gamePathSource':'IMCOption download path + game/version'}
(OUT/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
