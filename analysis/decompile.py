# @category PSXRecomp
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
fm = currentProgram.getFunctionManager()
af = currentProgram.getAddressFactory().getDefaultAddressSpace()
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
for a in getScriptArgs():
    ea = af.getAddress(a)
    fn = fm.getFunctionAt(ea)
    if fn is None:
        print("===== %s: SEM FUNCAO =====" % a); continue
    res = di.decompileFunction(fn, 90, mon)
    print("\n===== %s (%s) =====" % (a, fn.getName()))
    print(res.getDecompiledFunction().getC() if res.decompileCompleted() else "DECOMPILACAO FALHOU: " + str(res.getErrorMessage()))
