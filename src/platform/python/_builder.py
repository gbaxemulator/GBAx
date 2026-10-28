import cffi
import os, os.path
import shlex
import subprocess
import sys

ffi = cffi.FFI()
pydir = os.path.dirname(os.path.abspath(__file__))
srcdir = os.path.join(pydir, "..", "..")
incdir = os.path.join(pydir, "..", "..", "..", "include")
bindir = os.environ.get("BINDIR", os.path.join(os.getcwd(), ".."))
libdir = os.environ.get("LIBDIR")

cpp = shlex.split(os.environ.get("CPP", "cc -E"))
cppflags = shlex.split(os.environ.get("CPPFLAGS", ""))
cppflags.extend(["-I" + incdir, "-I" + srcdir, "-I" + bindir])

ffi.set_source("gbax._pylib", """
#define static
#define inline
#define GBAX_EXPORT
#include <gbax/flags.h>
#define OPAQUE_THREADING
#include <gbax/core/cache-set.h>
#include <gbax-util/common.h>
#include <gbax/core/core.h>
#include <gbax/core/map-cache.h>
#include <gbax/core/log.h>
#include <gbax/core/mem-search.h>
#include <gbax/core/thread.h>
#include <gbax/core/version.h>
#include <gbax/debugger/debugger.h>
#include <gbax/gba/interface.h>
#include <gbax/internal/arm/arm.h>
#include <gbax/internal/debugger/cli-debugger.h>
#include <gbax/internal/gba/gba.h>
#include <gbax/internal/gba/input.h>
#include <gbax/internal/gba/renderers/cache-set.h>
#include <gbax/internal/sm83/sm83.h>
#include <gbax/internal/gb/gb.h>
#include <gbax/internal/gb/renderers/cache-set.h>
#include <gbax-util/image/png-io.h>
#include <gbax-util/vfs.h>

#define PYEXPORT
#include "platform/python/core.h"
#include "platform/python/log.h"
#include "platform/python/vfs-py.h"
#undef PYEXPORT
""", include_dirs=[incdir, srcdir],
     extra_compile_args=cppflags,
     libraries=["gbax"],
     library_dirs=[bindir],
     runtime_library_dirs=[libdir],
     sources=[os.path.join(pydir, path) for path in ["vfs-py.c", "core.c", "log.c"]])

preprocessed = subprocess.check_output(cpp + ["-fno-inline", "-P"] + cppflags + [os.path.join(pydir, "_builder.h")], universal_newlines=True)

lines = []
for line in preprocessed.splitlines():
    line = line.strip()
    if line.startswith('#'):
        continue
    lines.append(line)
ffi.cdef('\n'.join(lines))

preprocessed = subprocess.check_output(cpp + ["-fno-inline", "-P"] + cppflags + [os.path.join(pydir, "lib.h")], universal_newlines=True)

lines = []
for line in preprocessed.splitlines():
    line = line.strip()
    if line.startswith('#'):
        continue
    lines.append(line)
ffi.embedding_api('\n'.join(lines))

ffi.embedding_init_code("""
    import os, os.path
    from gbax._pylib import ffi, lib
    symbols = {}
    globalSyms = {
        'symbols': symbols
    }
    pendingCode = []

    @ffi.def_extern()
    def mPythonSetDebugger(debugger):
        from gbax.debugger import NativeDebugger, CLIDebugger
        oldDebugger = globalSyms.get('debugger')
        if oldDebugger and oldDebugger._native == debugger:
            return
        if oldDebugger and not debugger:
            del globalSyms['debugger']
            return
        if debugger.type == lib.DEBUGGER_CLI:
            debugger = CLIDebugger(debugger)
        else:
            debugger = NativeDebugger(debugger)
        globalSyms['debugger'] = debugger

    @ffi.def_extern()
    def mPythonLoadScript(name, vf):
        from gbax.vfs import VFile
        vf = VFile(vf)
        name = ffi.string(name)
        source = vf.read_all().decode('utf-8')
        try:
            code = compile(source, name, 'exec')
            pendingCode.append(code)
        except:
            return False
        return True

    @ffi.def_extern()
    def mPythonRunPending():
        global pendingCode
        for code in pendingCode:
            exec(code, globalSyms, {})
        pendingCode = []

    @ffi.def_extern()
    def mPythonDebuggerEntered(reason, info):
        debugger = globalSyms['debugger']
        if not debugger:
            return
        if info == ffi.NULL:
            info = None
        for cb in debugger._cbs:
            cb(reason, info)

    @ffi.def_extern()
    def mPythonLookupSymbol(name, outptr):
        name = ffi.string(name).decode('utf-8')
        if name not in symbols:
            return False
        sym = symbols[name]
        val = None
        try:
            val = int(sym)
        except:
            try:
                val = sym()
            except:
                pass
        if val is None:
            return False
        try:
            outptr[0] = ffi.cast('int32_t', val)
            return True
        except:
            return False
""")

if __name__ == "__main__":
    ffi.emit_c_code("lib.c")
