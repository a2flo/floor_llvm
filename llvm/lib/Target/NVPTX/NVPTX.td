//===- NVPTX.td - Describe the NVPTX Target Machine -----------*- tblgen -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This is the top level entry point for the NVPTX target.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

include "NVPTXRegisterInfo.td"
include "NVPTXInstrInfo.td"

//===----------------------------------------------------------------------===//
// Subtarget Features.
// - We use the SM version number instead of explicit feature table.
// - Need at least one feature to avoid generating zero sized array by
//   TableGen in NVPTXGenSubtarget.inc.
//===----------------------------------------------------------------------===//

// SM Versions
// Fermi
def SM20 : SubtargetFeature<"sm_20", "FullSmVersion", "200",
                            "Target SM 2.0">;
def SM21 : SubtargetFeature<"sm_21", "FullSmVersion", "210",
                            "Target SM 2.1">;
// Kepler
def SM30 : SubtargetFeature<"sm_30", "FullSmVersion", "300",
                            "Target SM 3.0">;
def SM32 : SubtargetFeature<"sm_32", "FullSmVersion", "320",
                            "Target SM 3.2">;
def SM35 : SubtargetFeature<"sm_35", "FullSmVersion", "350",
                            "Target SM 3.5">;
def SM37 : SubtargetFeature<"sm_37", "FullSmVersion", "370",
                            "Target SM 3.7">;
// Maxwell
def SM50 : SubtargetFeature<"sm_50", "FullSmVersion", "500",
                            "Target SM 5.0">;
def SM52 : SubtargetFeature<"sm_52", "FullSmVersion", "520",
                            "Target SM 5.2">;
def SM53 : SubtargetFeature<"sm_53", "FullSmVersion", "530",
                            "Target SM 5.3">;
// Pascal
def SM60 : SubtargetFeature<"sm_60", "FullSmVersion", "600",
                            "Target SM 6.0">;
def SM61 : SubtargetFeature<"sm_61", "FullSmVersion", "610",
                            "Target SM 6.1">;
def SM62 : SubtargetFeature<"sm_62", "FullSmVersion", "620",
                            "Target SM 6.2">;
// Volta
def SM70 : SubtargetFeature<"sm_70", "FullSmVersion", "700",
                            "Target SM 7.0">;
def SM72 : SubtargetFeature<"sm_72", "FullSmVersion", "720",
                            "Target SM 7.2">;
def SM73 : SubtargetFeature<"sm_73", "FullSmVersion", "730",
                            "Target SM 7.3">;
// Turing
def SM75 : SubtargetFeature<"sm_75", "FullSmVersion", "750",
                            "Target SM 7.5">;
// Ampere
def SM80 : SubtargetFeature<"sm_80", "FullSmVersion", "800",
                            "Target SM 8.0">;
def SM82 : SubtargetFeature<"sm_82", "FullSmVersion", "820",
                            "Target SM 8.2">;
def SM86 : SubtargetFeature<"sm_86", "FullSmVersion", "860",
                            "Target SM 8.6">;
def SM87 : SubtargetFeature<"sm_87", "FullSmVersion", "870",
                            "Target SM 8.7">;
def SM88 : SubtargetFeature<"sm_88", "FullSmVersion", "880",
                            "Target SM 8.8">;
// Ada
def SM89 : SubtargetFeature<"sm_89", "FullSmVersion", "890",
                            "Target SM 8.9">;
// Hopper
def SM90 : SubtargetFeature<"sm_90", "FullSmVersion", "900",
                             "Target SM 9.0">;
def SM90a : SubtargetFeature<"sm_90a", "FullSmVersion", "901",
                             "Target SM 9.0a">;
// Blackwell
def SM100 : SubtargetFeature<"sm_100", "FullSmVersion", "1000",
                             "Target SM 10.0">;
def SM100a : SubtargetFeature<"sm_100a", "FullSmVersion", "1001",
                              "Target SM 10.0a">;
// deprecated, renamed to sm_110
def SM101 : SubtargetFeature<"sm_101", "FullSmVersion", "1010",
                             "Target SM 10.1">;
// deprecated, renamed to sm_110a
def SM101a : SubtargetFeature<"sm_101a", "FullSmVersion", "1011",
                              "Target SM 10.1a">;
def SM103 : SubtargetFeature<"sm_103", "FullSmVersion", "1030",
                             "Target SM 10.3">;
def SM103a : SubtargetFeature<"sm_103a", "FullSmVersion", "1031",
                              "Target SM 10.3a">;
def SM110 : SubtargetFeature<"sm_110", "FullSmVersion", "1100",
                             "Target SM 11.0">;
def SM110a : SubtargetFeature<"sm_110a", "FullSmVersion", "1101",
                              "Target SM 11.0a">;
def SM120 : SubtargetFeature<"sm_120", "FullSmVersion", "1200",
                             "Target SM 12.0">;
def SM120a : SubtargetFeature<"sm_120a", "FullSmVersion", "1201",
                              "Target SM 12.0a">;
def SM121 : SubtargetFeature<"sm_121", "FullSmVersion", "1210",
                             "Target SM 12.1">;
def SM121a : SubtargetFeature<"sm_121a", "FullSmVersion", "1211",
                              "Target SM 12.1a">;

// PTX Versions
def PTX32 : SubtargetFeature<"ptx32", "PTXVersion", "32",
                             "Use PTX version 3.2">;
def PTX40 : SubtargetFeature<"ptx40", "PTXVersion", "40",
                             "Use PTX version 4.0">;
def PTX41 : SubtargetFeature<"ptx41", "PTXVersion", "41",
                             "Use PTX version 4.1">;
def PTX42 : SubtargetFeature<"ptx42", "PTXVersion", "42",
                             "Use PTX version 4.2">;
def PTX43 : SubtargetFeature<"ptx43", "PTXVersion", "43",
                             "Use PTX version 4.3">;
def PTX50 : SubtargetFeature<"ptx50", "PTXVersion", "50",
                             "Use PTX version 5.0">;
def PTX60 : SubtargetFeature<"ptx60", "PTXVersion", "60",
                             "Use PTX version 6.0">;
def PTX61 : SubtargetFeature<"ptx61", "PTXVersion", "61",
                             "Use PTX version 6.1">;
def PTX62 : SubtargetFeature<"ptx62", "PTXVersion", "62",
                             "Use PTX version 6.2">;
def PTX63 : SubtargetFeature<"ptx63", "PTXVersion", "63",
                             "Use PTX version 6.3">;
def PTX64 : SubtargetFeature<"ptx64", "PTXVersion", "64",
                             "Use PTX version 6.4">;
def PTX65 : SubtargetFeature<"ptx65", "PTXVersion", "65",
                             "Use PTX version 6.5">;
def PTX70 : SubtargetFeature<"ptx70", "PTXVersion", "70",
                             "Use PTX version 7.0">;
def PTX71 : SubtargetFeature<"ptx71", "PTXVersion", "71",
                             "Use PTX version 7.1">;
def PTX72 : SubtargetFeature<"ptx72", "PTXVersion", "72",
                             "Use PTX version 7.2">;
def PTX73 : SubtargetFeature<"ptx73", "PTXVersion", "73",
                             "Use PTX version 7.3">;
def PTX74 : SubtargetFeature<"ptx74", "PTXVersion", "74",
                             "Use PTX version 7.4">;
def PTX75 : SubtargetFeature<"ptx75", "PTXVersion", "75",
                             "Use PTX version 7.5">;
def PTX76 : SubtargetFeature<"ptx76", "PTXVersion", "76",
                             "Use PTX version 7.6">;
def PTX77 : SubtargetFeature<"ptx77", "PTXVersion", "77",
                             "Use PTX version 7.7">;
def PTX78 : SubtargetFeature<"ptx78", "PTXVersion", "78",
                             "Use PTX version 7.8">;
def PTX80 : SubtargetFeature<"ptx80", "PTXVersion", "80",
                             "Use PTX version 8.0">;
def PTX81 : SubtargetFeature<"ptx81", "PTXVersion", "81",
                             "Use PTX version 8.1">;
def PTX82 : SubtargetFeature<"ptx82", "PTXVersion", "82",
                             "Use PTX version 8.2">;
def PTX83 : SubtargetFeature<"ptx83", "PTXVersion", "83",
                             "Use PTX version 8.3">;
def PTX84 : SubtargetFeature<"ptx84", "PTXVersion", "84",
                             "Use PTX version 8.4">;
def PTX85 : SubtargetFeature<"ptx85", "PTXVersion", "85",
                             "Use PTX version 8.5">;
def PTX86 : SubtargetFeature<"ptx86", "PTXVersion", "86",
                             "Use PTX version 8.6">;
def PTX87 : SubtargetFeature<"ptx87", "PTXVersion", "87",
                             "Use PTX version 8.7">;
def PTX88 : SubtargetFeature<"ptx88", "PTXVersion", "88",
                             "Use PTX version 8.8">;
def PTX90 : SubtargetFeature<"ptx90", "PTXVersion", "90",
                             "Use PTX version 9.0">;

//===----------------------------------------------------------------------===//
// NVPTX supported processors.
//===----------------------------------------------------------------------===//

class Proc<string Name, list<SubtargetFeature> Features>
 : Processor<Name, NoItineraries, Features>;

def : Proc<"sm_20", [SM20]>;
def : Proc<"sm_21", [SM21]>;
def : Proc<"sm_30", [SM30]>;
def : Proc<"sm_32", [SM32, PTX40]>;
def : Proc<"sm_35", [SM35]>;
def : Proc<"sm_37", [SM37, PTX41]>;
def : Proc<"sm_50", [SM50, PTX40]>;
def : Proc<"sm_52", [SM52, PTX41]>;
def : Proc<"sm_53", [SM53, PTX42]>;
def : Proc<"sm_60", [SM60, PTX50]>;
def : Proc<"sm_61", [SM61, PTX50]>;
def : Proc<"sm_62", [SM62, PTX50]>;
def : Proc<"sm_70", [SM70, PTX60]>;
def : Proc<"sm_72", [SM72, PTX61]>;
def : Proc<"sm_73", [SM73, PTX61]>;
def : Proc<"sm_75", [SM75, PTX63]>;
def : Proc<"sm_80", [SM80, PTX70]>;
def : Proc<"sm_82", [SM82, PTX71]>;
def : Proc<"sm_86", [SM86, PTX71]>;
def : Proc<"sm_87", [SM87, PTX76]>;
def : Proc<"sm_88", [SM88, PTX90]>;
def : Proc<"sm_89", [SM88, PTX78]>;
def : Proc<"sm_90", [SM90, PTX78]>;
def : Proc<"sm_90a", [SM90a, PTX78]>;
def : Proc<"sm_100", [SM100, PTX86]>;
def : Proc<"sm_100a", [SM100a, PTX86]>;
// deprecated, renamed to sm_110
def : Proc<"sm_101", [SM101, PTX86]>;
// deprecated, renamed to sm_110a
def : Proc<"sm_101a", [SM101a, PTX86]>;
def : Proc<"sm_103", [SM103, PTX88]>;
def : Proc<"sm_103a", [SM103a, PTX88]>;
def : Proc<"sm_110", [SM110, PTX90]>;
def : Proc<"sm_110a", [SM110a, PTX90]>;
def : Proc<"sm_120", [SM120, PTX87]>;
def : Proc<"sm_120a", [SM120a, PTX87]>;
def : Proc<"sm_121", [SM121, PTX88]>;
def : Proc<"sm_121a", [SM121a, PTX88]>;

def NVPTXInstrInfo : InstrInfo {
}

def NVPTX : Target {
  let InstructionSet = NVPTXInstrInfo;
}
