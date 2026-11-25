#!/usr/bin/env python3
"""Generate supplemental observed functions with the SDK lifter, without a full scan.

The existing hash-pinned analysis and generated dispatch are read-only inputs.
Only functions absent from that dispatch are emitted. A full regeneration calls
this too; when its normal dispatch already contains an entry, no duplicate body
is emitted. Unsupported indirect jumps or unknown external branches fail closed.
"""
import argparse
import bisect
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--analysis-dir', type=Path, required=True)
    args = parser.parse_args()
    target = json.loads((ROOT / 'target.json').read_text())
    pin = json.loads((ROOT / 'toolchain.json').read_text())['xboxrecomp']['revision']
    sdk = Path(os.environ['XBOXRECOMP_DIR']).resolve()
    head = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', 'HEAD'], text=True).strip()
    if head != pin:
        raise RuntimeError('framework revision does not match toolchain.json')
    xbe = (args.game_dir / target['executable']).read_bytes()
    if hashlib.sha256(xbe).hexdigest() != target['sha256']:
        raise RuntimeError('unexpected XBE hash')
    provenance = json.loads((args.analysis_dir / 'pipeline.json').read_text())
    if provenance['xbe_sha256'] != target['sha256'] or provenance['framework_revision'] != pin:
        raise RuntimeError('base analysis belongs to another XBE/framework')

    sys.path.insert(0, str(sdk))
    import capstone
    from capstone.x86_const import X86_OP_IMM
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator, load_label_db, detect_seh_helpers, detect_setjmp_helpers
    config.configure_from_xbe(str(args.game_dir / target['executable']))
    gen = ROOT / 'src/recomp/gen'
    dispatch = dict((int(a, 16), n) for a, n in re.findall(
        r'\{\s*0x([0-9A-Fa-f]+)u,\s*\(recomp_func_t\)(\w+)\s*\}',
        (gen / 'recomp_dispatch.c').read_text()))
    if not dispatch:
        raise RuntimeError('base generated dispatch is empty')
    funcs = {}
    for entry in json.loads((args.analysis_dir / 'disasm/functions.json').read_text()):
        addr = int(entry['start'], 16)
        entry['end'] = int(entry['end'], 16)
        if addr in dispatch:
            entry['name'] = dispatch[addr]
        funcs[addr] = entry
    # The full SDK generation can recover extra entries beyond functions.json.
    # They are already compiled; preserve their actual generated names.
    for addr, name in dispatch.items():
        funcs.setdefault(addr, {'start': hex(addr), 'end': addr + 1, 'name': name})
    old_starts = sorted(funcs)
    pending = [int(e['start'], 16) for e in json.loads(
        (ROOT / 'tools/seeds' / ('table_slots_' + target['sha256'][:8] + '.json')).read_text())
        if e.get('observed') and int(e['start'], 16) not in dispatch]
    cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    cs.detail = True
    added = {}
    while pending:
        start = pending.pop(0)
        if start in added or start in dispatch:
            continue
        if len(added) >= 32:
            raise RuntimeError('more than 32 new functions: use the full pipeline')
        section = config.section_at(start)
        if section is None or not section.is_code:
            raise RuntimeError(f'entry outside code: {start:08X}')
        index = bisect.bisect_right(old_starts, start)
        upper = min(section.va + section.raw_size, start + 0x10000)
        if index < len(old_starts):
            upper = min(upper, old_starts[index])
        todo, seen, calls, end, returns = [start], {}, set(), start, 0
        while todo:
            pc = todo.pop()
            while pc not in seen:
                if not start <= pc < upper:
                    raise RuntimeError(f'branch outside bounded function: {start:08X} -> {pc:08X}')
                raw = section.raw_addr + pc - section.va
                ins = next(cs.disasm(xbe[raw:raw + min(15, upper - pc)], pc, count=1), None)
                if ins is None:
                    raise RuntimeError(f'instruction does not decode: {pc:08X}')
                seen[pc] = ins
                end = max(end, ins.address + ins.size)
                if ins.mnemonic in ('ret', 'retf'):
                    returns += 1
                    break
                if ins.mnemonic in ('ud2', 'hlt'):
                    raise RuntimeError(f'trap in reachable function body: {pc:08X}')
                if ins.group(capstone.CS_GRP_CALL) and ins.operands[0].type == X86_OP_IMM:
                    calls.add(ins.operands[0].imm & 0xFFFFFFFF)
                if ins.group(capstone.CS_GRP_JUMP):
                    if ins.operands[0].type != X86_OP_IMM:
                        raise RuntimeError(f'indirect jump requires full analysis: {pc:08X}')
                    dest = ins.operands[0].imm & 0xFFFFFFFF
                    if start <= dest < upper:
                        todo.append(dest)
                    elif dest not in dispatch:
                        raise RuntimeError(f'unknown external branch: {pc:08X} -> {dest:08X}')
                    if ins.mnemonic == 'jmp':
                        break
                pc += ins.size
        if not returns:
            raise RuntimeError(f'no reachable return in {start:08X}')
        info = {'start': hex(start), 'end': end, 'size': end - start,
                'name': f'sub_{start:08X}', 'num_instructions': len(seen),
                'detection_method': 'observed_runtime_bounded_cfg'}
        funcs[start] = info
        added[start] = info
        # Unknown direct callees are just as concrete as the observed entry.
        pending.extend(c for c in sorted(calls) if c not in dispatch and c not in added)
        print(f'observed function {start:08X}..{end:08X}, {len(seen)} reachable instructions', flush=True)

    def keyed(path, key):
        return {int(e[key], 16): e for e in json.loads(path.read_text())}
    ids = keyed(args.analysis_dir / 'func-id/identified_functions.json', 'start')
    abi = keyed(args.analysis_dir / 'abi/abi_functions.json', 'address')
    prologues, epilogue = detect_seh_helpers(funcs, xbe)
    setjmp, longjmp = detect_setjmp_helpers(funcs, xbe)
    translator = FunctionTranslator(xbe, funcs,
        load_label_db(str(args.analysis_dir / 'disasm/labels.json')), ids, abi,
        seh_prolog=prologues, seh_epilog=epilogue, setjmp_fn=setjmp, longjmp_fn=longjmp)
    original_lift = translator.lifter.lift_instruction
    def lift_with_traps(insn):
        # These are real conditional assertions in the observed body, not
        # padding. Keep them observable/fatal instead of the SDK's int3 nop.
        if insn.mnemonic == 'int3':
            return [f'obiwan_observed_trap(0x{insn.address:08X}u);']
        return original_lift(insn)
    translator.lifter.lift_instruction = lift_with_traps
    pieces = ['/* Generated by tools/generate_observed_entries.py. Do not edit. */\n',
              '#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n#include <math.h>\n#include <stdio.h>\n#include <stdlib.h>\n',
              'extern void obiwan_observed_trap(uint32_t va);\n']
    for addr in sorted(added):
        pieces.append(f'void sub_{addr:08X}(void);\n')
    for addr, info in sorted(added.items()):
        code = translator.translate_function(addr, info)
        if not code or 'recomp_unimpl(' in code or 'label not in function' in code:
            raise RuntimeError(f'incomplete translation at {addr:08X}')
        pieces.append(code + '\n')
    pieces.append('recomp_func_t recomp_lookup_observed(uint32_t va) {\n  switch (va) {\n')
    for addr in sorted(added):
        pieces.append(f'  case 0x{addr:08X}u:\n'
                      f'    if (getenv("RECOMP_OBSERVED_ENTRY_DIAG")) fprintf(stderr, "[OBSERVED] {addr:08X}\\n");\n'
                      f'    return sub_{addr:08X};\n')
    pieces.append('  default: return NULL;\n  }\n}\n')
    output = gen / 'recomp_observed_entries.c'
    output.write_text(''.join(pieces))
    report = {'xbe_sha256': target['sha256'], 'framework_revision': pin,
              'base_analysis': str(args.analysis_dir.resolve()),
              'functions': [dict(i, end=hex(i['end'])) for i in added.values()]}
    (ROOT / 'build/observed-generation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Wrote {output}: {len(added)} supplemental functions', flush=True)


if __name__ == '__main__':
    main()
