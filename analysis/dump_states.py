# @category PSXRecomp
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
fm = currentProgram.getFunctionManager()
af = currentProgram.getAddressFactory().getDefaultAddressSpace()
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
base = af.getAddress("0x800a8914")
n = 16
ptrs = []
for i in range(n):
    v = currentProgram.getMemory().getInt(base.add(i*4)) & 0xFFFFFFFF
    ptrs.append(v)
    fn = fm.getFunctionAt(af.getAddress("0x%08x" % v))
    print("STATE[%d] = 0x%08x  (%s)" % (i, v, fn.getName() if fn else "sem fn"))
for v in ptrs[:3]:
    fn = fm.getFunctionAt(af.getAddress("0x%08x" % v))
    if fn is None: continue
    res = di.decompileFunction(fn, 60, mon)
    print("\n===== %s =====" % fn.getName())
    if res.decompileCompleted():
        c = res.getDecompiledFunction().getC()
        print(c[:4000])
