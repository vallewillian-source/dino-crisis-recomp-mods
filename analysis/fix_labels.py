# @category PSXRecomp
# Renomeia FUN_ -> func_XXXXXXXX conforme o ghidra_import.py (consistencia
# com o C gerado e o symbols.toml). Idempotente.
import re

fm = currentProgram.getFunctionManager()
af = currentProgram.getAddressFactory().getDefaultAddressSpace()
from ghidra.program.model.symbol import SourceType
lines = open("/home/willianvalle/Documentos/Recompilations/dino-crisis-recomp/repo/analysis/ghidra_import.py").read()
tuples = re.findall(r"\(0x([0-9A-Fa-f]{8}), (0x[0-9A-Fa-f]+), \"(func_[0-9A-Fa-f]{8})\", \"(\w+)\"\)", lines)
renamed = comments = 0
for hexa, _size, name, conf in tuples:
    ea = af.getAddress("0x" + hexa)
    fn = fm.getFunctionAt(ea)
    if fn is None:
        continue
    if fn.getName() != name:
        fn.setName(name, SourceType.IMPORTED)
        renamed += 1
    c = fn.getComment()
    want = "psxrecomp: confidence=%s" % conf
    if c != want:
        fn.setComment(want)
        comments += 1
print("fix_labels: %d renomeadas, %d comments atualizados (de %d entradas)" % (renamed, comments, len(tuples)))
