# @category PSXRecomp
# Aplica os nomes curados do symbols.toml no Ghidra (idempotente).
import tomllib, pathlib
fm = currentProgram.getFunctionManager()
af = currentProgram.getAddressFactory().getDefaultAddressSpace()
from ghidra.program.model.symbol import SourceType
data = tomllib.loads(pathlib.Path("/home/willianvalle/Documentos/Recompilations/dino-crisis-recomp/repo/symbols.toml").read_text())
ok = miss = ren = 0
for f in data.get("func", []):
    if f.get("status") == "guessed" and f["name"].startswith("func_"):
        continue
    ea = af.getAddress("0x%08x" % f["pc"])
    fn = fm.getFunctionAt(ea)
    if fn is None:
        miss += 1; continue
    if fn.getName() != f["name"]:
        fn.setName(f["name"], SourceType.IMPORTED); ren += 1
        fn.setComment(f"[{f.get('status','?')}] {f.get('note','')}"[:200])
    ok += 1
print("apply_symbols: %d ok, %d renomeadas, %d sem funcao no Ghidra" % (ok, ren, miss))
