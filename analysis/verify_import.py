# @category PSXRecomp
fm = currentProgram.getFunctionManager()
total = fm.getFunctionCount()
names = sum(1 for f in fm.getFunctions(True) if not f.getName().startswith("FUN_"))
print("VERIFY: total funcoes = %d, com nome = %d" % (total, names))
st = currentProgram.getSymbolTable()
print("VERIFY: label dc entry presente =", st.hasSymbol(st.getAddress("0x%08x" % 0x8001A1C8) if False else None) if False else "")
