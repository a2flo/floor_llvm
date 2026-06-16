//===-- metallib-dis.cpp - The low-level MetalLib + LLVM disassembler -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This utility may be invoked in the following manner:
//  metallib-dis [options]            - Read MetalLib from stdin, write asm
//                                      to stdout
//  metallib-dis [options] x.metallib - Read MetalLib from the x.metallib file,
//                                      write asm to the x.txt file.
//  Options:
//      --help   - Output information about command line switches
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/LLVMContext.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Transforms/LibFloor/MetalTypes.h"
#include "llvm/Transforms/LibFloor/metal_reflection.hpp"
#include "llvm/Transforms/LibFloor/metal_reflection_parsing.hpp"
#include "llvm/Transforms/LibFloor/metal_reflection_dumping.hpp"
#include <system_error>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <optional>
#include <span>
using namespace llvm;
using namespace std;
using namespace metal;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input metallib>"), cl::init("-"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Override output filename"),
               cl::value_desc("filename"));

static cl::opt<bool>
Force("f", cl::desc("Enable binary output on terminals"));

static cl::opt<bool>
DontPrint("disable-output", cl::desc("disable standard output (still prints errors)"), cl::init(false), cl::Hidden);

static cl::opt<bool>
ImmediatePrint("immediate-output", cl::desc("immediately write to standard output (incompatible with disable-output)"), cl::init(false), cl::Hidden);

static cl::opt<bool>
DisableReflection("disable-reflection", cl::desc("disable reflection parsing and printing"), cl::init(false));

static cl::opt<bool>
ShowAnnotations("show-annotations",
                cl::desc("Add informational comments to the LLVM IR output"));

static cl::opt<bool> PreserveAssemblyUseListOrder(
    "preserve-ll-uselistorder",
    cl::desc("Preserve use-list order when writing LLVM assembly."),
    cl::init(false), cl::Hidden);

static cl::opt<std::string>
FunctionFilter("filter", cl::desc("Only print/dump functions starting with <name>"),
               cl::value_desc("name"), cl::init(""));

static cl::opt<bool>
FuzzySearch("fuzzy",
            cl::desc("enable fuzzy searching of metallib data (MTLB blocks) in the input file"),
            cl::init(false));

/* .metallib layout
 
 versioning:
 [magic: char[4] = MTLB]
 [container version]
      [major version + macOS flag: uint16_t - MSB is macOS flag]
      [minor version: uint16_t]
      [bugfix version: uint16_t]
 [flags]
      [flags #1: 1:7 bit-field (uint8_t), MSB: stub flag, rest: file type (0 is metallib executable)s]
      [flags #2: 1:7 bit-field (uint8_t), MSB: 64-bit flag, rest: APPLE_PLATFORM (macOS: 1, iOS: 2, tvOS: 3, watchOS: 4, ...)]
 [platform version: 16:8:8 bit-field (32-bit) = major.minor.update]
 
 header:
 [program metadata offset: uint64_t]
 [program metadata length: uint64_t]
 [extended metadata offset: uint64_t]
 [extended metadata length: uint64_t]
 [debug metadata offset: uint64_t]
 [debug metadata length: uint64_t]
 [bitcode offset: uint64_t]
 [bitcode length: uint64_t]
 
 program metadata:
     [program count: uint32_t] // NOTE: not included by "program metadata length"
     [program metadata ...]
 
 additional program metadata (not included in "program metadata"!)
 [optional: embedded source code: char[4] = "HSRD"]
    [tag length: uint16_t = 16]
    [embedded source code offset: uint64_t]
    [embedded source code length: uint64_t]
 [optional: dynamic header: char[4] = "HYDN"]
    [tag length: uint16_t = 16]
    [dynamic header offset: uint64_t]
    [dynamic header length: uint64_t]
 [optional: metallib UUID: char[4] = "UUID"]
    [tag length: uint16_t = 16]
    [UUID: uint8_t[16]]
 [additional program metadata terminator: char[4] = "ENDT"]
 
 [extended metadata ...]
 [debug metadata ...]
 
 bitcode:
 [LLVM 5.0 (Metal 3.0) / 14.0 (Metal 3.1+) bitcode binaries ...]
 
 (opt) dynamic header:
 [metallib name tag: char[4] = "NAME"]
   [tag length: uint16_t = dynamic]
   [\0-terminated metallib file name...]
   [ENDT]
 
 (opt) embedded source code:
 [source archive count: uint32_t]
 [linker command line info: \0-terminated string]
 [working direction: \0-terminated string]
 source archives:
    [source archive length: uint32_t]
    [source archive...]
        [magic: char[4] = SARC] // NOTE: program source offsets point here (+"embedded source code offset")
        [archive length: uint32_t]
        [archive number in ASCII: uint16_t = 0x30/'0'...]
        [bzip2 compressed .a archive]
 
 (opt) reflection list:
 [reflection list entry count: uint32_t]
 reflection list:
    [reflection list entry length: uint32_t] // NOTE: program reflection offset point here
    [reflection list entry...]
        [tag: char[4] = "RBUF"]
        [tag length: uint32_t]
        [(opt) zero-padding for 16-byte alignment]
        [reflection/AIRR data] // NOTE: uses a flatbuffers format
            [root offset: uint32_t]
            [magic: char[4] = "AIRR"]
            [data...]
        [ENDT]
 */

//
static constexpr const char metallib_magic[4] { 'M', 'T', 'L', 'B' };

struct __attribute__((packed)) metallib_version {
	// container/file version
	uint16_t container_version_major : 15;
	uint16_t is_macos_target : 1;
	uint16_t container_version_minor;
	uint16_t container_version_bugfix;
	
	// flags
	uint8_t file_type : 7;
	uint8_t is_stub : 1;
	uint8_t platform : 7;
	uint8_t is_64_bit : 1;
	
	// platform version
	uint32_t platform_version_major : 16;
	uint32_t platform_version_minor : 8;
	uint32_t platform_version_update : 8;
};
static_assert(sizeof(metallib_version) == 12, "invalid version header length");

struct __attribute__((packed)) metallib_header_control {
	uint64_t programs_offset;
	uint64_t programs_length;
	uint64_t extended_md_offset;
	uint64_t extended_md_length;
	uint64_t debug_offset;
	uint64_t debug_length;
	uint64_t bitcode_offset;
	uint64_t bitcode_length;
};
static_assert(sizeof(metallib_header_control) == 64, "invalid program info length");

struct __attribute__((packed)) metallib_header {
	const char magic[4]; // == metallib_magic
	const metallib_version version;
	const uint64_t file_length;
	const metallib_header_control header_control;
};
static_assert(sizeof(metallib_header) == 4 + sizeof(metallib_version) + sizeof(uint64_t) + sizeof(metallib_header_control), "invalid metallib header size");

struct metallib_program_info {
	uint32_t length; // including length itself
	
	struct version_info {
		uint32_t major : 16;
		uint32_t minor : 8;
		uint32_t rev : 8;
	};
	
	struct offset_info {
		// NOTE: these are all relative offsets -> add to metallib_header_control offsets to get absolute offsets
		uint64_t extended_md_offset;
		uint64_t debug_offset;
		uint64_t bitcode_offset;
	};
	
	struct debug_entry {
		std::string source_file_name;
		uint32_t function_line { 0 };
		std::string dependent_file;
	};
	
	struct extended_md_entry {
		std::vector<vertex_attribute> vertex_attributes;
		std::vector<function_constant> function_constants;
	};
	
	struct entry {
		uint32_t length;
		string name; // NOTE: limited to 65536 - 1 ('\0')
		FUNCTION_TYPE type { FUNCTION_TYPE::NONE };
		uint8_t sha256_hash[32] {
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		};
		offset_info offset { 0, 0, 0 };
		uint64_t bitcode_size { 0 }; // always 8 bytes
		version_info metal_version { 0, 0, 0 };
		version_info metal_language_version { 0, 0, 0 };
		union tess_data_t {
			struct tess_info_t {
				uint8_t primitive_type : 2; // 1 = triangle, 2 = quad
				uint8_t control_point_count : 6; // hardware limit is 32
			} info;
			uint8_t data;
		} tess;
		union layer_data_t {
			// TODO: actual info
			uint8_t data;
		} layer;
		uint64_t source_offset { 0 };
		std::optional<debug_entry> debug;
		std::optional<extended_md_entry> extended_md;
		std::optional<uint64_t> reflection_offset;
		std::optional<metal::reflection::reflection_t> reflection;
	};
	vector<entry> entries;
};

//
namespace {

static void printDebugLoc(const DebugLoc &DL, formatted_raw_ostream &OS) {
  OS << DL.getLine() << ":" << DL.getCol();
  if (DILocation *IDL = DL.getInlinedAt()) {
    OS << "@";
    printDebugLoc(IDL, OS);
  }
}
class CommentWriter : public AssemblyAnnotationWriter {
public:
  void emitFunctionAnnot(const Function *F,
                         formatted_raw_ostream &OS) override {
    OS << "; [#uses=" << F->getNumUses() << ']';  // Output # uses
    OS << '\n';
  }
  void printInfoComment(const Value &V, formatted_raw_ostream &OS) override {
    bool Padded = false;
    if (!V.getType()->isVoidTy()) {
      OS.PadToColumn(50);
      Padded = true;
      // Output # uses and type
      OS << "; [#uses=" << V.getNumUses() << " type=" << *V.getType() << "]";
    }
    if (const Instruction *I = dyn_cast<Instruction>(&V)) {
      if (const DebugLoc &DL = I->getDebugLoc()) {
        if (!Padded) {
          OS.PadToColumn(50);
          Padded = true;
          OS << ";";
        }
        OS << " [debug line = ";
        printDebugLoc(DL,OS);
        OS << "]";
      }
      if (const DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(I)) {
        if (!Padded) {
          OS.PadToColumn(50);
          OS << ";";
        }
        OS << " [debug variable = " << DDI->getVariable()->getName() << "]";
      }
      else if (const DbgValueInst *DVI = dyn_cast<DbgValueInst>(I)) {
        if (!Padded) {
          OS.PadToColumn(50);
          OS << ";";
        }
        OS << " [debug variable = " << DVI->getVariable()->getName() << "]";
      }
    }
  }
};

struct MetalLibDisDiagnosticHandler : public DiagnosticHandler {
  char *Prefix;
  MetalLibDisDiagnosticHandler(char *PrefixPtr) : Prefix(PrefixPtr) {}
  bool handleDiagnostics(const DiagnosticInfo &DI) override {
    raw_ostream &OS = errs();
    OS << Prefix << ": ";
    switch (DI.getSeverity()) {
      case DS_Error: WithColor::error(OS); break;
      case DS_Warning: WithColor::warning(OS); break;
      case DS_Remark: OS << "remark: "; break;
      case DS_Note: WithColor::note(OS); break;
    }

    DiagnosticPrinterRawOStream DP(OS);
    DI.print(DP);
    OS << '\n';

    if (DI.getSeverity() == DS_Error)
      exit(1);
    return true;
  }
};
} // end anon namespace

static void hex_dump(raw_ostream& os, const char* ptr, const size_t length, const char* name) {
	os << '\n' << name << ":\n";
	static constexpr const uint32_t row_width = 16;
	for (uint32_t row = 0, row_count = ((length + row_width - 1) / row_width); row < row_count; ++row) {
		for (uint32_t i = 0; i < row_width; ++i) {
			const auto ch_idx = row * row_width + i;
			if (ch_idx < length) {
				const auto ch_ptr = (const uint8_t*)ptr + ch_idx;
				llvm::write_hex(os, (uint64_t)*ch_ptr, HexPrintStyle::Upper, 2);
				os << " ";
			} else {
				os << "   ";
			}
		}
		os << " |  ";
		for (uint32_t i = 0; i < row_width; ++i) {
			const auto ch_idx = row * row_width + i;
			if (ch_idx < length) {
				const auto ch_ptr = ptr + ch_idx;
				if (*ch_ptr >= 0x20) {
					os << *ch_ptr;
				} else if (*ch_ptr == 0) {
					os << "∅";
				} else {
					os << ' ';
				}
			}
		}
		os << '\n';
	}
}

//! parses RBUF contents
static Expected<bool> parse_reflection_buffer(std::span<const uint8_t> refl_buf, std::optional<metal::reflection::reflection_t>& refl_entry) {
	// skip initial zero padding (AIRR data is always 16-byte aligned inside the metallib)
	// NOTE: not checking the alignment requirement here, since our data might not actually be aligned to 16 bytes (just 4),
	//       and our parser technically doesn't need any alignment at all
	while (!refl_buf.empty() && refl_buf[0] == 0) {
		refl_buf = refl_buf.subspan(1);
	}
	if (refl_buf.empty()) {
		return make_error<StringError>("invalid initial zero padding", inconvertibleErrorCode());
	}
	assert((uintptr_t(refl_buf.data()) % 4u) == 0u);
	
	// must have 32-bit offset + AIRR magic
	if (refl_buf.size_bytes() < 8) {
		return make_error<StringError>("invalid reflection start block", inconvertibleErrorCode());
	}
	
	using table_vtable_t = metal::reflection::table_vtable_t;
	using table_root_t = metal::reflection::table_root_t;
	
	const auto refl_root_offset = *(const uint32_t*)refl_buf.data();
	if (refl_root_offset + sizeof(table_root_t) >= refl_buf.size_bytes()) {
		return make_error<StringError>("invalid reflection root offset", inconvertibleErrorCode());
	}
	
	if (strncmp((const char*)refl_buf.data() + 4, "AIRR", 4) != 0) {
		return make_error<StringError>("invalid reflection magic", inconvertibleErrorCode());
	}
	
	// get the root node + its vtable
	const auto& refl_root = *(const table_root_t*)(refl_buf.data() + refl_root_offset);
	const auto refl_vtable_offset = int(refl_root_offset) - refl_root.vtable_offset;
	if (refl_vtable_offset < 0 || uint32_t(refl_vtable_offset) + sizeof(table_vtable_t) >= refl_buf.size_bytes()) {
		return make_error<StringError>("invalid reflection root vtable offset", inconvertibleErrorCode());
	}
	const auto& refl_vtable = *(const table_vtable_t*)(refl_buf.data() + refl_vtable_offset);
	
	// parse all reflection
	refl_entry = metal::reflection::parse_reflection(refl_root, refl_vtable, refl_buf);
	
	return true;
}

static Expected<bool> parse_metallib(const std::span<const char> data_span, char* call_bin, raw_ostream& os) {
	const auto data = data_span.data();
	const auto buffer_size = data_span.size_bytes();
	
	// sanity check
	if(buffer_size < sizeof(metallib_header)) {
		return make_error<StringError>("invalid header size", inconvertibleErrorCode());
	}
	
	//
	const auto& header = *(const metallib_header*)data;
	if(memcmp(metallib_magic, header.magic, 4) != 0) {
		return make_error<StringError>("invalid magic", inconvertibleErrorCode());
	}
	
	// dump
	os << "[header]" << '\n';
	os << "container version: " << header.version.container_version_major << "." << header.version.container_version_minor << "." << header.version.container_version_bugfix << '\n';
	os << "macOS: " << (header.version.is_macos_target ? "yes" : "no") << '\n';
	os << "64-bit: " << (header.version.is_64_bit ? "yes" : "no") << '\n';
	os << "stub: " << (header.version.is_stub ? "yes" : "no") << '\n';
	os << "file-type: ";
	switch (header.version.file_type) {
		case 0:
			os << "execute";
			break;
		case 1:
			os << "CI";
			break;
		case 2:
			os << "dylib";
			break;
		case 3:
			os << "companion";
			break;
		default:
			os << "unknown";
			break;
	}
	os << '\n';
	os << "platform: ";
	switch (header.version.platform) {
		case uint32_t(metal::APPLE_PLATFORM::MACOS):
			os << "macOS";
			break;
		case uint32_t(metal::APPLE_PLATFORM::IOS):
			os << "iOS";
			break;
		case uint32_t(metal::APPLE_PLATFORM::TVOS):
			os << "tvOS";
			break;
		case uint32_t(metal::APPLE_PLATFORM::WATCHOS):
			os << "watchOS";
			break;
		case uint32_t(metal::APPLE_PLATFORM::BRIDGEOS):
			os << "bridgeOS";
			break;
		case uint32_t(metal::APPLE_PLATFORM::MAC_CATALYST):
			os << "Mac Catalyst";
			break;
		case uint32_t(metal::APPLE_PLATFORM::IOS_SIMULATOR):
			os << "iOS (Simulator)";
			break;
		case uint32_t(metal::APPLE_PLATFORM::TVOS_SIMULATOR):
			os << "tvOS (Simulator)";
			break;
		case uint32_t(metal::APPLE_PLATFORM::WATCHOS_SIMULATOR):
			os << "watchOS (Simulator)";
			break;
		case uint32_t(metal::APPLE_PLATFORM::DRIVERKIT):
			os << "DriverKit";
			break;
		case uint32_t(metal::APPLE_PLATFORM::XROS):
			os << "visionOS";
			break;
		case uint32_t(metal::APPLE_PLATFORM::XROS_SIMULATOR):
			os << "visionOS (Simulator)";
			break;
		default:
			os << "unknown (" << (uint32_t)header.version.platform << ")";
			break;
	}
	os << '\n';
	os << "platform version: " << header.version.platform_version_major << "." << header.version.platform_version_minor << "." << header.version.platform_version_update << '\n';
	os << "length: " << header.file_length << '\n';
	
	os << '\n';
	os << "programs_offset: " << header.header_control.programs_offset << '\n';
	os << "programs_length: " << header.header_control.programs_length << '\n';
	os << "extended_md_offset: " << header.header_control.extended_md_offset << '\n';
	os << "extended_md_length: " << header.header_control.extended_md_length << '\n';
	os << "debug_offset: " << header.header_control.debug_offset << '\n';
	os << "debug_length: " << header.header_control.debug_length << '\n';
	os << "bitcode_offset: " << header.header_control.bitcode_offset << '\n';
	os << "bitcode_length: " << header.header_control.bitcode_length << '\n';
	
	// read programs info
	if(buffer_size < header.header_control.programs_offset + header.header_control.programs_length + 4u) {
		return make_error<StringError>("invalid size", inconvertibleErrorCode());
	}
	
	metallib_program_info info;
	auto program_ptr = &data[header.header_control.programs_offset];
	const auto add_program_md_offset = header.header_control.programs_offset + 4 + header.header_control.programs_length;
	auto add_program_md_ptr = &data[add_program_md_offset];
	const auto add_program_md_end = find((const uint32_t*)add_program_md_ptr,
										 (const uint32_t*)add_program_md_ptr + (header.header_control.extended_md_offset -
																				add_program_md_offset) / 4u,
										 0x54444E45u /* rev(ENDT) */) + 1;
	const auto add_program_md_length = std::distance(add_program_md_ptr, (const char*)add_program_md_end);
	auto extended_md_ptr = &data[header.header_control.extended_md_offset];
	auto debug_ptr = &data[header.header_control.debug_offset];
	
	const auto program_count = *(const uint32_t*)program_ptr; program_ptr += 4;
	os << "program_count: " << program_count << '\n';
	info.entries.resize(program_count);
	
	if (FunctionFilter.empty()) {
		hex_dump(os, program_ptr, header.header_control.programs_length, "program metadata");
		hex_dump(os, add_program_md_ptr, add_program_md_length, "additional program metadata");
		hex_dump(os, extended_md_ptr, header.header_control.extended_md_length, "extended metadata");
		hex_dump(os, debug_ptr, header.header_control.debug_length, "debug metadata");
	}
	os.flush();
	
	for(uint32_t i = 0; i < program_count; ++i) {
		auto& entry = info.entries[i];
		
		entry.length = *(const uint32_t*)program_ptr; program_ptr += 4;
		
		bool found_end_tag = false;
		while(!found_end_tag) {
			const auto tag = *(const TAG_TYPE*)program_ptr; program_ptr += 4;
			uint32_t tag_length = 0;
			if(tag != TAG_TYPE::END) {
				tag_length = *(const uint16_t*)program_ptr;
				program_ptr += 2;
				
				if(tag_length == 0) {
					return make_error<StringError>("tag " + to_string(uint32_t(tag)) + " should not be empty",
												   inconvertibleErrorCode());
				}
			}
			
			switch(tag) {
				case TAG_TYPE::NAME: {
					entry.name = string((const char*)program_ptr, tag_length - 1u);
					break;
				}
				case TAG_TYPE::TYPE: {
					using underlying_func_type = underlying_type_t<FUNCTION_TYPE>;
					const auto func_type_val = *(const underlying_func_type*)program_ptr;
					if (func_type_val > underlying_func_type(FUNCTION_TYPE::OBJECT) &&
						func_type_val < underlying_func_type(FUNCTION_TYPE::NONE)) {
						return make_error<StringError>("unknown function type: " + to_string(func_type_val),
													   inconvertibleErrorCode());
					}
					entry.type = std::bit_cast<FUNCTION_TYPE>(func_type_val);
					break;
				}
				case TAG_TYPE::HASH: {
					if(tag_length != 32) {
						return make_error<StringError>("invalid hash size: " + to_string(tag_length),
													   inconvertibleErrorCode());
					}
					memcpy(entry.sha256_hash, program_ptr, 32u);
					break;
				}
				case TAG_TYPE::OFFSET: {
					entry.offset = *(const metallib_program_info::offset_info*)program_ptr;
					break;
				}
				case TAG_TYPE::VERSION: {
					entry.metal_version = *(const metallib_program_info::version_info*)program_ptr;
					entry.metal_language_version = *((const metallib_program_info::version_info*)program_ptr + 1u);
					break;
				}
				case TAG_TYPE::MD_SIZE: {
					entry.bitcode_size = *(const uint64_t*)program_ptr;
					break;
				}
				case TAG_TYPE::TESS: {
					if (tag_length != 1) {
						return make_error<StringError>("invalid tessellation info length: " + to_string(tag_length) + ", expected 1 byte",
													   inconvertibleErrorCode());
					}
					entry.tess.data = *(const uint8_t*)program_ptr;
					break;
				}
				case TAG_TYPE::SOFF: {
					if(tag_length != 8) {
						return make_error<StringError>("invalid SOFF size: " + to_string(tag_length),
													   inconvertibleErrorCode());
					}
					entry.source_offset = *(const uint64_t*)program_ptr;
					break;
				}
				case TAG_TYPE::RFLT: {
					if (tag_length != 8) {
						return make_error<StringError>("invalid RFLT size: " + to_string(tag_length),
													   inconvertibleErrorCode());
					}
					entry.reflection_offset = *(const uint64_t*)program_ptr;
					break;
				}
				case TAG_TYPE::LAYR: {
					if (tag_length != 1) {
						return make_error<StringError>("invalid LAYR size: " + to_string(tag_length),
													   inconvertibleErrorCode());
					}
					entry.layer.data = *(const uint8_t*)program_ptr;
					break;
				}
				case TAG_TYPE::END: {
					found_end_tag = true;
					break;
				}
				default:
					return make_error<StringError>("invalid program metadata tag: " + to_string((uint32_t)tag),
												   inconvertibleErrorCode());
			}
			program_ptr += tag_length;
		}
	}
	if(info.entries.size() != program_count) {
		return make_error<StringError>("invalid entry count", inconvertibleErrorCode());
	}
	
	// parse additional metadata
	std::optional<uint64_t> refl_list_offset;
	std::optional<uint64_t> refl_list_length;
	std::optional<uint64_t> i_list_offset;
	std::optional<uint64_t> i_list_length;
	std::optional<uint64_t> dyn_header_list_offset;
	std::optional<uint64_t> dyn_header_list_length;
	{
		bool found_end_tag = false;
		while (!found_end_tag) {
			const auto tag = *(const TAG_TYPE*)add_program_md_ptr; add_program_md_ptr += 4;
			uint32_t tag_length = 0;
			if (tag != TAG_TYPE::END) {
				tag_length = *(const uint16_t*)add_program_md_ptr;
				add_program_md_ptr += 2;
				
				if (tag_length == 0) {
					return make_error<StringError>("tag " + to_string(uint32_t(tag)) + " should not be empty",
												   inconvertibleErrorCode());
				}
			}
			
			switch(tag) {
				case TAG_TYPE::HSRD: {
					const auto src_archives_offset = *(const uint64_t*)add_program_md_ptr;
					const auto src_archives_length = *(const uint64_t*)(add_program_md_ptr + 8u);
					os << "\nembedded source archives:\n\toffset: " << src_archives_offset << "\n\tlength: " << src_archives_length << '\n';
					break;
				}
				case TAG_TYPE::UUID: {
					os << "\nUUID: ";
					raw_ostream::uuid_t uuid;
					memcpy(&uuid, add_program_md_ptr, sizeof(uuid));
					os.write_uuid(uuid);
					os << '\n';
					break;
				}
				case TAG_TYPE::RLST: {
					refl_list_offset = *(const uint64_t*)add_program_md_ptr;
					refl_list_length = *(const uint64_t*)(add_program_md_ptr + 8u);
					os << "\nreflection list:\n\toffset: " << *refl_list_offset << "\n\tlength: " << *refl_list_length << '\n';
					break;
				}
				case TAG_TYPE::ILST: {
					i_list_offset = *(const uint64_t*)add_program_md_ptr;
					i_list_length = *(const uint64_t*)(add_program_md_ptr + 8u);
					os << "\ni (?) list:\n\toffset: " << *i_list_offset << "\n\tlength: " << *i_list_length << '\n';
					break;
				}
				case TAG_TYPE::HDYN: {
					dyn_header_list_offset = *(const uint64_t*)add_program_md_ptr;
					dyn_header_list_length = *(const uint64_t*)(add_program_md_ptr + 8u);
					break;
				}
				case TAG_TYPE::END: {
					found_end_tag = true;
					break;
				}
				default:
					return make_error<StringError>("invalid additional program metadata tag: " + to_string((uint32_t)tag),
												   inconvertibleErrorCode());
			}
			add_program_md_ptr += tag_length;
		}
	}
	
	// parse debug info
	for (uint32_t i = 0; i < program_count; ++i) {
		auto& entry = info.entries[i];
		
		metallib_program_info::debug_entry dbg_entry {};
		//const auto dbg_entry_length = *(const uint32_t*)debug_ptr;
		debug_ptr += 4;
		
		bool found_end_tag = false;
		while (!found_end_tag) {
			const auto tag = *(const TAG_TYPE*)debug_ptr; debug_ptr += 4;
			uint32_t tag_length = 0;
			if(tag != TAG_TYPE::END) {
				tag_length = *(const uint16_t*)debug_ptr;
				debug_ptr += 2;
				
				if(tag_length == 0) {
					return make_error<StringError>("tag " + to_string(uint32_t(tag)) + " should not be empty",
												   inconvertibleErrorCode());
				}
			}
			
			switch(tag) {
				case TAG_TYPE::DEBI: {
					dbg_entry.function_line = *(const uint32_t*)debug_ptr;
					dbg_entry.source_file_name = string((const char*)debug_ptr + 4, tag_length - 5u);
					break;
				}
				case TAG_TYPE::DEPF: {
					dbg_entry.dependent_file = string((const char*)debug_ptr, tag_length - 1u);
					break;
				}
				case TAG_TYPE::END: {
					found_end_tag = true;
					break;
				}
				default:
					return make_error<StringError>("invalid debug metadata tag: " + to_string((uint32_t)tag),
												   inconvertibleErrorCode());
			}
			debug_ptr += tag_length;
		}
		
		// only set this debug entry if it actually contains anything
		if (!dbg_entry.source_file_name.empty() || !dbg_entry.dependent_file.empty()) {
			entry.debug = std::move(dbg_entry);
		}
	}
	
	// parse extended md info
	for (uint32_t i = 0; i < program_count; ++i) {
		auto& entry = info.entries[i];
		
		metallib_program_info::extended_md_entry ext_md_entry {};
		extended_md_ptr += 4;
		
		bool found_end_tag = false;
		while (!found_end_tag) {
			const auto tag = *(const TAG_TYPE*)extended_md_ptr; extended_md_ptr += 4;
			uint32_t tag_length = 0;
			if (tag != TAG_TYPE::END) {
				tag_length = *(const uint16_t*)extended_md_ptr;
				extended_md_ptr += 2;
				
				if (tag_length == 0) {
					return make_error<StringError>("tag " + to_string(uint32_t(tag)) + " should not be empty",
												   inconvertibleErrorCode());
				}
			}
			
			switch (tag) {
				case TAG_TYPE::VATT: {
					// initial vertex attribute data
					auto tag_extended_md_ptr = extended_md_ptr;
					const auto vattr_end_ptr = tag_extended_md_ptr + tag_length;
					const auto vattr_count = (uint32_t)*(const uint16_t*)tag_extended_md_ptr; tag_extended_md_ptr += 2;
					ext_md_entry.vertex_attributes.resize(vattr_count);
					for (auto& vattr : ext_md_entry.vertex_attributes) {
						// parse name
						const auto name_end_ptr = find(tag_extended_md_ptr, vattr_end_ptr, '\0');
						if (name_end_ptr == vattr_end_ptr) {
							return make_error<StringError>("failed to find name end terminator for vertex attribute",
														   inconvertibleErrorCode());
						}
						vattr.name = std::string((const char*)tag_extended_md_ptr, (const char*)name_end_ptr);
						tag_extended_md_ptr = name_end_ptr + 1;
						
						// parse other (16-bit)
						const auto other = *(const uint16_t*)tag_extended_md_ptr; tag_extended_md_ptr += 2;
						vattr.index = other & 0x1FFFu;
						vattr.use = (VERTEX_USE)((other & 0x6000u) >> 13u);
						vattr.active = ((other & 0x8000u) != 0u);
					}
					break;
				}
				case TAG_TYPE::VATY: {
					// vertex attribute type data
					if (ext_md_entry.vertex_attributes.empty()) {
						return make_error<StringError>("no prior vertex attribute data exists - can't add vertex types",
													   inconvertibleErrorCode());
					}
					if (tag_length - 2 != ext_md_entry.vertex_attributes.size()) {
						return make_error<StringError>("vertex attribute count mismatch: got " + to_string(uint32_t(tag_length - 2)) +
													   ", expected: " + to_string(ext_md_entry.vertex_attributes.size()),
													   inconvertibleErrorCode());
					}
					
					auto tag_extended_md_ptr = extended_md_ptr;
					const auto vattr_count = *(const uint16_t*)tag_extended_md_ptr; tag_extended_md_ptr += 2;
					if (vattr_count != ext_md_entry.vertex_attributes.size()) {
						return make_error<StringError>("vertex attribute type count mismatch: got " + to_string(uint32_t(vattr_count)) +
													   ", expected: " + to_string(ext_md_entry.vertex_attributes.size()),
													   inconvertibleErrorCode());
					}
					
					for (auto& vattr : ext_md_entry.vertex_attributes) {
						vattr.type = *(const DATA_TYPE*)tag_extended_md_ptr; ++tag_extended_md_ptr;
					}
					break;
				}
				case TAG_TYPE::CNST: {
					// function constant data
					auto tag_extended_md_ptr = extended_md_ptr;
					const auto cnst_end_ptr = tag_extended_md_ptr + tag_length;
					const auto cnst_count = (uint32_t)*(const uint16_t*)tag_extended_md_ptr; tag_extended_md_ptr += 2;
					ext_md_entry.function_constants.resize(cnst_count);
					for (auto& cnst : ext_md_entry.function_constants) {
						// parse name
						const auto name_end_ptr = find(tag_extended_md_ptr, cnst_end_ptr, '\0');
						if (name_end_ptr == cnst_end_ptr) {
							return make_error<StringError>("failed to find name end terminator for function constant",
														   inconvertibleErrorCode());
						}
						cnst.name = std::string((const char*)tag_extended_md_ptr, (const char*)name_end_ptr);
						tag_extended_md_ptr = name_end_ptr + 1;
						
						// parse other
						cnst.type = *(const DATA_TYPE*)tag_extended_md_ptr; ++tag_extended_md_ptr;
						cnst.index = *(const uint16_t*)tag_extended_md_ptr; tag_extended_md_ptr += 2;
						cnst.active = ((*tag_extended_md_ptr & 0x1u) != 0u); ++tag_extended_md_ptr;
					}
					break;
				}
				case TAG_TYPE::RETR: {
					// TODO: handle this
					break;
				}
				case TAG_TYPE::ARGR: {
					// TODO: handle this
					break;
				}
				case TAG_TYPE::END: {
					found_end_tag = true;
					break;
				}
				default:
					return make_error<StringError>("invalid extended metadata tag: " + to_string((uint32_t)tag),
												   inconvertibleErrorCode());
			}
			extended_md_ptr += tag_length;
		}
		
		// only set this extended md entry if it actually contains anything
		if (!ext_md_entry.vertex_attributes.empty() || !ext_md_entry.function_constants.empty()) {
			entry.extended_md = std::move(ext_md_entry);
		}
	}
	
	// parse reflection list
	if (refl_list_offset && refl_list_length) {
		if (*refl_list_offset + *refl_list_length > buffer_size) {
			return make_error<StringError>("reflection list data goes out-of-bounds: " +
										   to_string(*refl_list_offset + *refl_list_length) + " > " + to_string(buffer_size),
										   inconvertibleErrorCode());
		}
		for (uint32_t i = 0; i < program_count; ++i) {
			auto& entry = info.entries[i];
			if (!entry.reflection_offset) {
				continue;
			}
			if (*entry.reflection_offset >= *refl_list_length) {
				return make_error<StringError>("reflection offset is out-of-bounds: " + to_string(*entry.reflection_offset),
											   inconvertibleErrorCode());
			}
			
			auto refl_ptr = &data[*refl_list_offset + *entry.reflection_offset];
			const auto refl_len = *(const uint32_t*)refl_ptr;
			const auto refl_end_ptr = refl_ptr + refl_len;
			refl_ptr += 4;
			if (*entry.reflection_offset + refl_len > *refl_list_length) {
				return make_error<StringError>("reflection length is out-of-bounds: " +
											   to_string(*entry.reflection_offset + refl_len) + " > " + to_string(*refl_list_length),
											   inconvertibleErrorCode());
			}
			
			bool found_end_tag = false;
			while (!found_end_tag && refl_ptr < refl_end_ptr) {
				const auto tag = *(const TAG_TYPE*)refl_ptr; refl_ptr += 4;
				uint32_t tag_length = 0;
				if (tag != TAG_TYPE::END) {
					// NOTE: tag length is 32-bit here
					tag_length = *(const uint32_t*)refl_ptr;
					refl_ptr += 4;
					
					if (tag_length == 0) {
						return make_error<StringError>("tag " + to_string(uint32_t(tag)) + " should not be empty",
													   inconvertibleErrorCode());
					}
				}
				
				switch (tag) {
					case TAG_TYPE::RBUF: {
						if (!DisableReflection) {
							const std::span refl_buf { (const uint8_t*)refl_ptr, tag_length };
							if (auto success = parse_reflection_buffer(refl_buf, entry.reflection); !success) {
								std::string err_msg;
								handleAllErrors(success.takeError(), [&](ErrorInfoBase &EIB) {
									err_msg += EIB.message();
								});
								return make_error<StringError>("failed to parse reflection list: " + err_msg,
															   inconvertibleErrorCode());
							}
						}
						break;
					}
					case TAG_TYPE::RBUZ: {
						// NOTE/TODO: this seems to be bzip compressed data
						return make_error<StringError>("compressed reflection list not yet supported",
													   inconvertibleErrorCode());
					}
					case TAG_TYPE::END: {
						found_end_tag = true;
						break;
					}
					default:
						return make_error<StringError>("invalid reflection list tag: " + to_string((uint32_t)tag),
													   inconvertibleErrorCode());
				}
				refl_ptr += tag_length;
			}
			if (!found_end_tag) {
				return make_error<StringError>("reached the end of the reflection list data, but no end tag was found",
											   inconvertibleErrorCode());
			}
		}
	}
	
	// parse i (?) list
	if (i_list_offset && i_list_length) {
		if (*i_list_offset + *i_list_length > buffer_size) {
			return make_error<StringError>("i list data goes out-of-bounds: " +
										   to_string(*i_list_offset + *i_list_length) + " > " + to_string(buffer_size),
										   inconvertibleErrorCode());
		}
		// TODO: properly handle this
	}
	
	// parse hdyn / dynamic header
	if (dyn_header_list_offset && dyn_header_list_length) {
		if (*dyn_header_list_offset + *dyn_header_list_length > buffer_size) {
			return make_error<StringError>("hdyn data goes out-of-bounds: " +
										   to_string(*dyn_header_list_offset + *dyn_header_list_length) + " > " + to_string(buffer_size),
										   inconvertibleErrorCode());
		}
		
		auto hdyn_ptr = &data[*dyn_header_list_offset];
		const auto hdyn_end_ptr = hdyn_ptr + *dyn_header_list_length;
		
		bool found_end_tag = false;
		std::optional<std::string> hdyn_name;
		while (!found_end_tag && hdyn_ptr < hdyn_end_ptr) {
			const auto tag = *(const TAG_TYPE*)hdyn_ptr; hdyn_ptr += 4;
			uint32_t tag_length = 0;
			if (tag != TAG_TYPE::END) {
				tag_length = *(const uint16_t*)hdyn_ptr;
				hdyn_ptr += 2;
				
				if (tag_length == 0) {
					return make_error<StringError>("tag " + to_string(uint32_t(tag)) + " should not be empty",
												   inconvertibleErrorCode());
				}
			}
			
			switch (tag) {
				case TAG_TYPE::NAME: {
					if (tag_length >= 1u) {
						hdyn_name = string((const char*)hdyn_ptr, tag_length - 1u);
					}
					break;
				}
				case TAG_TYPE::END: {
					found_end_tag = true;
					break;
				}
				default:
					return make_error<StringError>("invalid hdyn list tag: " + to_string((uint32_t)tag),
												   inconvertibleErrorCode());
			}
			hdyn_ptr += tag_length;
		}
		
		os << "\ndynamic header:\n\toffset: " << *dyn_header_list_offset << "\n\tlength: " << *dyn_header_list_length << '\n';
		if (hdyn_name) {
			os << "\tname: " << *hdyn_name << '\n';
		}
		
		if (!found_end_tag) {
			return make_error<StringError>("reached the end of the hdyn data, but no end tag was found",
										   inconvertibleErrorCode());
		}
	}
	
	//
	for (const auto& prog : info.entries) {
		if (!FunctionFilter.empty()) {
			if (!prog.name.starts_with(FunctionFilter)) {
				continue;
			}
		}
		
		os << '\n';
		os << "################################################################################\n";
		os << '\n';
		os << "[program]" << '\n';
		os << "\tname: " << prog.name << '\n';
		os << "\ttype: ";
		switch (prog.type) {
			case FUNCTION_TYPE::FRAGMENT:
				os << "fragment";
				break;
			case FUNCTION_TYPE::VERTEX:
				os << "vertex";
				break;
			case FUNCTION_TYPE::KERNEL:
				os << "kernel";
				break;
			case FUNCTION_TYPE::UNQUALIFIED:
				os << "unqualified";
				break;
			case FUNCTION_TYPE::VISIBLE:
				os << "visible";
				break;
			case FUNCTION_TYPE::EXTERN:
				os << "extern";
				break;
			case FUNCTION_TYPE::INTERSECTION:
				os << "intersection";
				break;
			case FUNCTION_TYPE::MESH:
				os << "mesh";
				break;
			case FUNCTION_TYPE::OBJECT:
				os << "object";
				break;
			case FUNCTION_TYPE::NONE:
				os << "NONE";
				break;
		}
		os << '\n';
		os << "\tversion: " << prog.metal_version.major << "." << prog.metal_version.minor << "." << prog.metal_version.rev << '\n';
		os << "\tlanguage: " << prog.metal_language_version.major << "." << prog.metal_language_version.minor << "." << prog.metal_language_version.rev << '\n';
		os << "\trel offsets (ext, dbg, bc): " << prog.offset.extended_md_offset << ", " << prog.offset.debug_offset << ", " << prog.offset.bitcode_offset << '\n';
		os << "\tbitcode size: " << prog.bitcode_size << '\n';
		os << "\thash: ";
		
		stringstream hash_hex;
		hash_hex << hex << uppercase;
		for(uint32_t i = 0; i < 32; ++i) {
			if(prog.sha256_hash[i] < 0x10) {
				hash_hex << '0';
			}
			hash_hex << uint32_t(prog.sha256_hash[i]);
		}
		os << hash_hex.str() << '\n';
		if (prog.tess.data != 0) {
			os << "\ttessellation info: ";
			if (prog.tess.info.primitive_type == 1) {
				os << "triangle";
			} else if (prog.tess.info.primitive_type == 2) {
				os << "quad";
			} else {
				os << "<unknown: " << (uint32_t)prog.tess.info.primitive_type << ">";
			}
			os << ", " << (uint32_t)prog.tess.info.control_point_count << " control points\n";
		}
		if (prog.layer.data != 0) {
			os << "\tlayer info: " << uint32_t(prog.layer.data) << "\n";
		}
		os << "\tsource offset: " << uint32_t(prog.source_offset) << '\n';
		
		// TODO: could use stringref?
		auto bc_mem = WritableMemoryBuffer::getNewUninitMemBuffer(prog.bitcode_size, "bc_module");
		const auto bc_offset = header.header_control.bitcode_offset + prog.offset.bitcode_offset;
		os << "\toffset: " << bc_offset << '\n';
		os << "\tsize: " << bc_mem->getBufferSize();
		if (bc_mem->getBufferSize() != prog.bitcode_size) {
			os << " != expected: " << prog.bitcode_size << " (!!!)";
		}
		os << '\n';
		if (prog.extended_md) {
			if (!prog.extended_md->vertex_attributes.empty()) {
				os << "\tvertex attributes:\n";
				for (const auto& vattr : prog.extended_md->vertex_attributes) {
					// use: type name @idx
					os << "\t\t";
					switch (vattr.use) {
						case VERTEX_USE::STANDARD:
							os << "standard-vertex-data";
							break;
						case VERTEX_USE::PER_PATCH:
							os << "per-patch-vertex-data";
							break;
						case VERTEX_USE::CONRTOL_POINT:
							os << "control-point";
							break;
						default:
							os << "<unknown-use>";
							break;
					}
					os << "[" << vattr.index << "]: " << data_type_to_string(vattr.type) << " " << vattr.name;
					os << (vattr.active ? "" : " (inactive)") << '\n';
				}
			}
			if (!prog.extended_md->function_constants.empty()) {
				os << "\tfunction constants:\n";
				for (const auto& cnst : prog.extended_md->function_constants) {
					os << "\t\t";
					os << "[" << cnst.index << "]: " << data_type_to_string(cnst.type) << " " << cnst.name;
					os << (cnst.active ? "" : " (inactive)") << '\n';
				}
			}
		}
		if (prog.debug) {
			os << "\tdebug:\n";
			if (!prog.debug->source_file_name.empty()) {
				os << "\t\tsource file: " << prog.debug->source_file_name << '\n';
			}
			if (prog.debug->function_line != 0) {
				os << "\t\tsource line: " << prog.debug->function_line << '\n';
			}
			if (!prog.debug->dependent_file.empty()) {
				os << "\t\tdependent file: " << prog.debug->dependent_file << '\n';
			}
		}
		if (prog.reflection_offset) {
			os << "\treflection offset: " << *prog.reflection_offset << '\n';
		}
		if (prog.reflection) {
			os << "\treflection:\n";
			metal::reflection::dump(*prog.reflection, os, 2u);
		}
		os << '\n';
		
		// output LLVM IR
		memcpy((char*)bc_mem->getBufferStart(), data + bc_offset, bc_mem->getBufferSize());
		
		LLVMContext Context;
		Context.setDiagnosticHandler(std::make_unique<MetalLibDisDiagnosticHandler>(call_bin));
		auto bc_mod = parseBitcodeFile(*bc_mem, Context);
		if (bc_mod) {
			std::unique_ptr<AssemblyAnnotationWriter> Annotator;
			if (ShowAnnotations) {
				Annotator.reset(new CommentWriter());
			}
			
			if ((*bc_mod)->materializeAll()) {
				return make_error<StringError>("failed to materialize", inconvertibleErrorCode());
			}
			(*bc_mod)->print(os, Annotator.get(), PreserveAssemblyUseListOrder);
		} else {
			os << "bc parse error" << '\n';
			// TODO: better error handling
			handleAllErrors(bc_mod.takeError(), [&](ErrorInfoBase &EIB) {
				errs() << "bc module: ";
				EIB.log(errs());
				errs() << '\n';
			});
			return make_error<StringError>("failed to parse bitcode module", inconvertibleErrorCode());
		}
		
		os.flush();
	}
	
	return true;
}

static Expected<bool> disassembleInputFile(char** argv, std::unique_ptr<ToolOutputFile>& Out, const bool dont_print,
										   const bool immediate_print) {
	ErrorOr<std::unique_ptr<MemoryBuffer>> input_data = MemoryBuffer::getFileOrSTDIN(InputFilename);
	if (!input_data) {
		return errorCodeToError(input_data.getError());
	}
	const auto& buffer = (*input_data)->getBuffer();
	const auto& data = buffer.data();
	
	std::string output;
	raw_string_ostream str_os(output);
	auto& os = (!immediate_print ? (raw_ostream&)str_os : (raw_ostream&)Out->os());
	
	Expected<bool> ret { make_error<StringError>("", inconvertibleErrorCode()) };
	if (!FuzzySearch) {
		ret = parse_metallib(std::span<const char> { data, buffer.size() }, argv[0], os);
	} else {
		const std::string_view mtlb_header { "MTLB"sv };
		auto cur_data_span = std::span<const char> { data, buffer.size() };
		bool found_any = false;
		size_t total_offset = 0;
		for (;;) {
			const auto iter = std::search(cur_data_span.begin(), cur_data_span.end(), mtlb_header.begin(), mtlb_header.end());
			if (iter == cur_data_span.end()) {
				break;
			}
			const auto start_offset = std::distance(cur_data_span.begin(), iter);
			total_offset += start_offset;
			os << ">> found metal library @offset: rel " << start_offset << ", abs " << total_offset << "\n";
			cur_data_span = cur_data_span.subspan(start_offset);
			if (parse_metallib(cur_data_span, argv[0], os)) {
				found_any = true;
			}
			// offset span by 4 so that we don't match the same metallib
			cur_data_span = cur_data_span.subspan(4);
		}
		ret = found_any;
	}
	
	if (!dont_print && !immediate_print) {
		Out->os() << output;
	}
	
	return ret;
}

static ExitOnError ExitOnErr;

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  ExitOnErr.setBanner(std::string(argv[0]) + ": error: ");

  LLVMContext Context;
  Context.setDiagnosticHandler(
      std::make_unique<MetalLibDisDiagnosticHandler>(argv[0]));

  cl::ParseCommandLineOptions(argc, argv, ".metallib -> .txt disassembler\n");

  if (ImmediatePrint && DontPrint) {
    errs() << "incompatible options: can't use both immediate-output and disable-output\n";
    return -1;
  }

  // Just use stdout.  We won't actually print anything on it.
  if (DontPrint)
    OutputFilename = "-";

  if (OutputFilename.empty()) { // Unspecified output, infer it.
    if (InputFilename == "-") {
      OutputFilename = "-";
    } else {
      StringRef IFN = InputFilename;
      OutputFilename = (IFN.endswith(".metallib") ? IFN.drop_back(9) : IFN).str();
      OutputFilename += ".txt";
    }
  }

  std::error_code EC;
  std::unique_ptr<ToolOutputFile> Out(new ToolOutputFile(OutputFilename, EC, sys::fs::OF_None));
  if (EC) {
    errs() << EC.message() << '\n';
    return -1;
  }

  Expected<bool> SuccessOrErr = disassembleInputFile(argv, Out, DontPrint, ImmediatePrint);
  if (!SuccessOrErr) {
    handleAllErrors(SuccessOrErr.takeError(), [&](ErrorInfoBase &EIB) {
      errs() << argv[0] << ": ";
      EIB.log(errs());
      errs() << '\n';
    });
    return 1;
  }

  // Declare success.
  Out->keep();

  return 0;
}
