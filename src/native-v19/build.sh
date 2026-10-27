#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mcpc_compiler="${CC:-i686-w64-mingw32-gcc}"
python3 - <<'PY'
from pathlib import Path
for source,target,symbol in [('official-ui.html','ui.inc','UI_HTML'),('history-ui.html','history-ui.inc','HISTORY_HTML')]:
 b=Path(source).read_bytes()
 lines=['static const unsigned char '+symbol+'[] = {']
 for i in range(0,len(b),24):lines.append(','.join(str(x) for x in b[i:i+24])+',')
 lines.append('0};')
 Path(target).write_text('\n'.join(lines))
PY
"$mcpc_compiler" -std=c99 -Os -s -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables \
 -DNDEBUG -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ZLIB_APIS -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_TIME \
 -nostartfiles -static-libgcc -Wl,-e,_NativeEntry -Wl,--image-base,0x400000 \
 -Wl,--section-start,.text=0x1174000 -Wl,--enable-reloc-section -Wl,-u,__pei386_runtime_relocator \
 -Wl,-Map,payload.map -Wl,--major-os-version,6,--minor-os-version,1,--major-subsystem-version,6,--minor-subsystem-version,1 \
 -o payload.exe native.c core.c adapter.c network.c third_party/miniz_tinfl.c -lwinhttp -lws2_32 -ladvapi32 -lkernel32 -luser32 -lwininet -lshell32 -lole32
python3 merge_payload.py "$@"
