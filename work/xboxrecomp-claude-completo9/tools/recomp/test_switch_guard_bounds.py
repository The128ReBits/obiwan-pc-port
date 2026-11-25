"""A switch table is as long as its range guard says, not a fixed cap.

Star Wars: Obi-Wan dispatches its script commands through one 615-case
switch (`cmp eax, 0x266 / ja default / jmp [eax*4 + table]`). The disassembler
measured jump tables up to 512 slots and the lifter read 256, so every case
past the 256th was lifted as an indirect jump nothing could resolve, and the
last 103 slots were decoded as a function of their own.
"""
import struct
from types import SimpleNamespace

import pytest

from . import config
from .disasm import Disassembler, switch_bounds
from .lifter import Lifter
from tools.disasm.test_returning_body_switch import BASE, _engine

CASES = 600                      # past both old caps (256 and 512)
JMP = BASE + 0x0D
ARM_A, ARM_B, ARM_LAST = BASE + 0x14, BASE + 0x17, BASE + 0x1D
TABLE = BASE + 0x40
SIZE = 0xA00


def _image():
    code = bytearray(b"\xcc" * SIZE)
    body = (b"\x3d" + struct.pack("<I", CASES - 1)      # cmp eax, CASES-1
            + b"\x8b\xf1"                                # mov esi, ecx
            + b"\x0f\x87" + struct.pack("<i", 0x23 - 0x0D)  # ja default
            + b"\xff\x24\x85" + struct.pack("<I", TABLE)    # jmp [eax*4+TABLE]
            + b"\x31\xc0\xc3"                            # arm A
            + b"\xb8\x01\x00\x00\x00\xc3"                # arm B
            + b"\xb8\x02\x00\x00\x00\xc3"                # last arm
            + b"\xc3")                                   # default
    code[:len(body)] = body
    for i in range(CASES):
        arm = ARM_LAST if i == CASES - 1 else (ARM_A, ARM_B)[i & 1]
        struct.pack_into("<I", code, TABLE - BASE + 4 * i, arm)
    code[TABLE - BASE + 4 * CASES] = 0xC3
    return bytes(code)


@pytest.fixture(autouse=True)
def layout(monkeypatch):
    monkeypatch.setattr(config, "_SECTIONS", [
        config.Section(".text", BASE, SIZE, 0, SIZE, True),
    ])


def _switch_arms(bounds):
    data = _image()
    lifter = Lifter()
    lifter.xbe_data = data
    lifter.func_start, lifter.func_end = BASE, TABLE
    lifter.switch_bounds = bounds
    op = SimpleNamespace(type="mem", mem_disp=TABLE, mem_index="eax",
                         mem_base=None)
    return set(lifter._analyze_switch_table([op], JMP))


def test_guard_gives_the_table_length():
    data = _image()
    insns = Disassembler().disassemble_function(data[:0x24], BASE, BASE + 0x24)
    assert switch_bounds(insns) == {JMP: CASES}


def test_lifter_reaches_the_cases_past_256():
    assert ARM_LAST in _switch_arms({JMP: CASES})
    assert ARM_LAST not in _switch_arms({})     # the old fixed cap


def test_disassembler_measures_the_whole_table():
    engine, _ = _engine(_image())
    assert engine.jump_tables.get(TABLE) == TABLE + 4 * CASES
