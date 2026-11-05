//===- MetalLibWriterPass.cpp - Metal Library writing pass ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// MetalLibWriterPass implementation.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Triple.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Bitcode/BitcodeWriterPass.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "../Writer140/ValueEnumerator140.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VersionTuple.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/LibFloor/MetalTypes.h"
#include "llvm/Transforms/LibFloor/FloorUtils.h"
#include "MetalLibReflection.hpp"
#include "sha256.hpp"
#define BZ_NO_STDIO 1
#include "bzip2/bzlib.h"
#include "tar/microtar.h"
#include <string>
#include <limits>
#include <unordered_set>
#include <unordered_map>
#include <random>
#include <fstream>
#include <sstream>
using namespace llvm;
using namespace std;
using namespace metal;

// workaround Windows stupidity ...
#if defined(uuid_t)
#undef uuid_t
#endif

PreservedAnalyses MetalLibWriterPass::run(Module &M, ModuleAnalysisManager &) {
  if (!WriteMetalLibToFile(M, OS)) {
    M.getContext().emitError("failed to emit metallib");
  }
  return PreservedAnalyses::all();
}

namespace {
class WriteMetalLibPass : public ModulePass {
  raw_ostream &OS; // raw_ostream to print on

public:
  static char ID; // Pass identification, replacement for typeid
  explicit WriteMetalLibPass(raw_ostream &o) : ModulePass(ID), OS(o) {}

  StringRef getPassName() const override { return "Metal Library Writer"; }

  bool runOnModule(Module &M) override {
    if (!WriteMetalLibToFile(M, OS)) {
      M.getContext().emitError("failed to emit metallib");
    }
    return false;
  }
};
} // namespace

char WriteMetalLibPass::ID = 0;

ModulePass *llvm::createMetalLibWriterPass(raw_ostream &Str) {
  return new WriteMetalLibPass(Str);
}

//
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
static_assert(sizeof(metallib_header_control) == 64,
              "invalid program info length");

struct __attribute__((packed)) metallib_header {
  const char magic[4]; // == metallib_magic
  const metallib_version version;
  const uint64_t file_length;
  const metallib_header_control header_control;
};
static_assert(sizeof(metallib_header) == 4 + sizeof(metallib_version) +
                                             sizeof(uint64_t) +
                                             sizeof(metallib_header_control),
              "invalid metallib header size");

struct metallib_program_info {
  struct version_info {
    uint32_t major : 16;
    uint32_t minor : 8;
    uint32_t rev : 8;
  };
  static_assert(sizeof(version_info) == sizeof(uint32_t),
                "invalid offset_info size");

  struct offset_info {
    // NOTE: these are all relative offsets -> add to metallib_header_control
    // offsets to get absolute offsets
    uint64_t extended_md_offset{0u};
    uint64_t debug_offset{0u};
    uint64_t bitcode_offset{0u};
  };
  static_assert(sizeof(offset_info) == 3 * sizeof(uint64_t),
                "invalid offset_info size");

  struct entry {
    uint32_t length{0};

    string name; // NOTE: limited to 65536 - 1 ('\0')

    FUNCTION_TYPE type{FUNCTION_TYPE::NONE};

    sha256_hash hash;

    offset_info offset{};

    // we need a separate stream for the actual bitcode data, since we need to
    // know
    // the size of each module/file (no way to know this beforehand)
    string bitcode_data{""}; // -> used via raw_string_ostream later on
    uint64_t bitcode_size{0};

    // same for extended metadata and debug data
    string extended_md_data{""};
    uint64_t extended_md_size{0};
    string debug_data{""};
    uint64_t debug_size{0};

    version_info metal_version;
    version_info metal_language_version;

    bool emit_debug_info{false};
    uint64_t debug_source_offset{0u};

    union tess_data_t {
      struct tess_info_t {
        uint8_t primitive_type : 2;      // 1 = triangle, 2 = quad
        uint8_t control_point_count : 6; // hardware limit is 32
      } info;
      uint8_t data;
    } tess;

    std::vector<vertex_attribute> vertex_attributes;
    std::vector<function_constant> function_constants;

    // reflection flatbuffer data
    std::vector<uint8_t> reflection_data;
    // data size + container/wrapper/padding
    uint32_t reflection_size{0u};
    // offset within the RLST
    uint64_t reflection_offset{0u};
    // amount of initial padding bytes
    uint32_t reflection_padding{0u};

    // output in same order as Apple:
    //  * NAME
    //  * TYPE
    //  * HASH
    //  * OFFT
    //  * VERS
    //  * MDSZ
    //  * SOFF
    //  * RFLT
    //  * ENDT
    void update_length() {
      length = 4;                     // length info itself
      length += 8 * sizeof(TAG_TYPE); // 8 tags
      length += 7 * sizeof(uint16_t); // tag lengths (except ENDT)
      if (emit_debug_info) {
        length += 1 * (sizeof(TAG_TYPE) + sizeof(uint16_t)); // SOFF tag
      }

      length += name.size() + 1;          // name length + \0
      length += 1;                        // type
      length += sizeof(sha256_hash);      // hash
      length += sizeof(offset_info);      // offset
      length += sizeof(version_info) * 2; // both versions
      length += sizeof(uint64_t);         // module size, always 8 bytes
      if (tess.data != 0u) {
        length += sizeof(TAG_TYPE); // TESS
        length += sizeof(uint16_t); // tag length
        length += sizeof(uint8_t);  // tessellation info
      }
      if (emit_debug_info) {
        length += sizeof(uint64_t); // SOFF, always 8 bytes
      }
      length += sizeof(uint64_t); // RFLT, always 8 bytes

      bitcode_size = bitcode_data.size();
      extended_md_size = extended_md_data.size();
      debug_size = debug_data.size();
      // reflection data itself + container/wrapper (padding later)
      reflection_size =
          (reflection_data.size() + sizeof(TAG_TYPE) * 2u /* RBUF+ENDT */ +
           sizeof(uint32_t) * 2u /* sizes */);
    }

    void update_offsets(uint64_t &running_ext_md_size,
                        uint64_t &running_dbg_size, uint64_t &running_bc_size) {
      offset.extended_md_offset = running_ext_md_size;
      offset.debug_offset = running_dbg_size;
      offset.bitcode_offset = running_bc_size;

      running_ext_md_size += extended_md_size;
      running_dbg_size += debug_size;
      running_bc_size += bitcode_size;
    }

    void update_reflection_placement(uint64_t &running_reflection_offset,
                                     const uint64_t front_file_length) {
      // reflection size and offset handling is a bit special, since it requires
      // the flatbuffer payload to be 16-byte aligned *within* the metallib
      const auto fb_pos =
          (front_file_length + running_reflection_offset +
           sizeof(TAG_TYPE) /* RBUF */ + sizeof(uint32_t) * 2u /* sizes */);
      if (const auto rem = fb_pos % 16u; rem != 0u) {
        reflection_padding = 16u - rem;
      }
      reflection_size += reflection_padding;
      reflection_offset = running_reflection_offset;
      running_reflection_offset += reflection_size;
    }

    template <typename data_type>
    static inline void write_value(raw_ostream &OS, const data_type &value) {
      OS.write((const char *)&value, sizeof(data_type));
    }

    void write_header(raw_ostream &OS) const {
      write_value(OS, length);

      // NAME
      write_value(OS, TAG_TYPE::NAME);
      write_value(OS, uint16_t(name.size() + 1));
      OS << name << '\0';

      // TYPE
      write_value(OS, TAG_TYPE::TYPE);
      write_value(OS, uint16_t(sizeof(uint8_t)));
      write_value(OS, uint8_t(type));

      // HASH
      write_value(OS, TAG_TYPE::HASH);
      write_value(OS, uint16_t(sizeof(sha256_hash)));
      OS.write((const char *)&hash, sizeof(sha256_hash));

      // OFFT
      write_value(OS, TAG_TYPE::OFFSET);
      write_value(OS, uint16_t(sizeof(offset_info)));
      write_value(OS, offset.extended_md_offset);
      write_value(OS, offset.debug_offset);
      write_value(OS, offset.bitcode_offset);

      // VERS
      write_value(OS, TAG_TYPE::VERSION);
      write_value(OS, uint16_t(2 * sizeof(version_info)));
      write_value(OS, *(const uint32_t *)&metal_version);
      write_value(OS, *(const uint32_t *)&metal_language_version);

      if (tess.data != 0u) {
        // TESS
        write_value(OS, TAG_TYPE::TESS);
        write_value(OS, uint16_t(1u));
        write_value(OS, tess.data);
      }

      // MDSZ
      write_value(OS, TAG_TYPE::MD_SIZE);
      write_value(OS, uint16_t(sizeof(uint64_t)));
      write_value(OS, uint64_t(bitcode_data.size()));

      if (emit_debug_info) {
        // SOFF
        write_value(OS, TAG_TYPE::SOFF);
        write_value(OS, uint16_t(sizeof(uint64_t)));
        write_value(OS, debug_source_offset);
      }

      // RFLT
      write_value(OS, TAG_TYPE::RFLT);
      write_value(OS, uint16_t(sizeof(uint64_t)));
      write_value(OS, reflection_offset);

      // ENDT
      write_value(OS, TAG_TYPE::END);
    }

    void write_module(raw_ostream &OS) const {
      OS.write(bitcode_data.data(), bitcode_data.size());
    }

    void write_extended_md(raw_ostream &OS) const {
      OS.write(extended_md_data.data(), extended_md_data.size());
    }

    void write_debug(raw_ostream &OS) const {
      OS.write(debug_data.data(), debug_data.size());
    }

    void write_reflection(raw_ostream &OS) const {
      const auto embedded_refl_data_size =
          uint32_t(reflection_data.size() + reflection_padding);
      write_value(OS, uint32_t(embedded_refl_data_size + sizeof(TAG_TYPE) * 2u +
                               sizeof(uint32_t) * 2u));
      write_value(OS, TAG_TYPE::RBUF);
      write_value(OS, embedded_refl_data_size);
      for (uint32_t i = 0; i < reflection_padding; ++i) {
        OS.write(0u); // alignment padding
      }
      OS.write((const char *)reflection_data.data(),
               uint32_t(reflection_data.size()));
      write_value(OS, TAG_TYPE::END);
    }
  };
  vector<entry> entries;
};

// version -> { AIR version, language version }
static const unordered_map<uint32_t,
                           pair<array<uint32_t, 3>, array<uint32_t, 3>>>
    metal_versions{
        // Metal 3.2 uses AIR 2.7
        {270, {{{2, 7, 0}}, {{3, 2, 0}}}},
        // Metal 4.0 uses AIR 2.8
        {280, {{{2, 8, 0}}, {{4, 0, 0}}}},
        // Metal 4.1 uses AIR 2.9
        {290, {{{2, 9, 0}}, {{4, 1, 0}}}},
    };

static std::string make_abs_file_name(const std::string &file_name_in) {
  std::string file_name_out = file_name_in;
  SmallVector<char> file_name(file_name_out.size());
  file_name.assign(file_name_out.begin(), file_name_out.end());
  sys::fs::make_absolute(file_name);
  file_name_out.resize(file_name.size(), '\0');
  file_name_out.assign(file_name.begin(), file_name.end());
  return file_name_out;
}

struct tar_stream_t {
  static int read(void *stream, void *data, uint32_t size) {
    assert(false && "should not call this");
    return 0;
  }

  static int seek(void *stream, uint32_t offset) {
    assert(false && "should not call this");
    return MTAR_ESEEKFAIL;
  }

  static int write(void *stream, const void *data, uint32_t size) {
    auto &str = ((tar_stream_t *)stream)->str;
    str.write((const char *)data, size);
    return size;
  }

  static int close(void *stream) {
    (void)stream; // nop
    return MTAR_ESUCCESS;
  }

  static constexpr const mtar_ops_t ops{
      .read = &read,
      .write = &write,
      .seek = &seek,
      .close = &close,
  };

  std::stringstream str;
};
static std::string create_tar(
    const std::unordered_map<std::string /* abs path */,
                             std::unique_ptr<MemoryBuffer> /* data */> &files) {
  // create tar
  tar_stream_t tar_stream;
  mtar_t tar;
  mtar_init(&tar, MTAR_WRITE, &tar_stream_t::ops, &tar_stream);

  for (const auto &file : files) {
    const auto file_size = file.second->getBufferSize();
    mtar_write_file_header(&tar, file.first.c_str(), file_size);
    mtar_write_data(&tar, file.second->getBufferStart(), file_size);
    mtar_end_data(&tar);
  }

  mtar_finalize(&tar);
  mtar_close(&tar);

  return tar_stream.str.str();
}

static std::pair<std::unique_ptr<uint8_t[]>, uint32_t>
compress_source_archive(const void *archive_ptr, const uint32_t archive_size) {
  auto dst_len = archive_size + (archive_size / 50u) +
                 600u; // provision orig + 2% + advised 600 bytes
  auto dst = make_unique<uint8_t[]>(dst_len);
  const auto bz2_ret = BZ2_bzBuffToBuffCompress(
      (char *)dst.get(), &dst_len,
      const_cast<char *>((const char *)archive_ptr), archive_size, 6, 0, 30);
  if (bz2_ret != BZ_OK) {
    errs() << "failed to perform bz2 compression on source archive: " << bz2_ret
           << '\n';
    return {nullptr, 0};
  }
  return {std::move(dst), dst_len};
}

// Public/private metadata lengths exclude the four-byte length prefix.
// Derive the payload length from emitted bytes, including ENDT and all strings,
// rather than duplicate the serializer's variable-length arithmetic.
static void finish_metadata_length(std::string &block) {
  if (block.size() < 4 || block.size() - 4 > std::numeric_limits<uint32_t>::max())
    report_fatal_error("Metal metadata block length is out of range");
  const auto size = static_cast<uint32_t>(block.size() - 4);
  for (unsigned i = 0; i != 4; ++i)
    block[i] = static_cast<char>((size >> (8 * i)) & 0xff);
}

// A library-wide constructor is not evidence that a particular entry uses a
// function constant. Find uses in the entry's call graph before GlobalOpt sees
// llvm.global_ctors, which otherwise keeps every initializer alive.
static bool collect_entry_functions(Function &Entry,
                                    SmallPtrSetImpl<const Function *> &Functions) {
  SmallVector<const Function *, 16> Worklist{&Entry};
  while (!Worklist.empty()) {
    const Function *F = Worklist.pop_back_val();
    if (!Functions.insert(F).second)
      continue;
    for (const BasicBlock &BB : *F)
      for (const Instruction &I : BB)
        if (const auto *Call = dyn_cast<CallBase>(&I)) {
          if (Call->isInlineAsm())
            continue;
          const auto *Callee = dyn_cast<Function>(Call->getCalledOperand()->stripPointerCasts());
          if (!Callee)
            return false; // An indirect call may reach a function using any constant.
          if (!Callee->isDeclaration())
            Worklist.push_back(Callee);
        }
  }
  return true;
}

static bool used_by_entry(const GlobalVariable &GV,
                          const SmallPtrSetImpl<const Function *> &Functions) {
  SmallVector<const Value *, 16> Worklist{&GV};
  SmallPtrSet<const Value *, 32> Seen;
  while (!Worklist.empty()) {
    const Value *Current = Worklist.pop_back_val();
    if (!Seen.insert(Current).second)
      continue;
    for (const User *User : Current->users()) {
      if (const auto *I = dyn_cast<Instruction>(User)) {
        if (Functions.count(I->getFunction()))
          return true;
      } else if (const auto *C = dyn_cast<Constant>(User)) {
        // Includes constant expressions, aliases and globals initialized with
        // a pointer to this constant. Follow their instruction users as well.
        Worklist.push_back(C);
      }
    }
  }
  return false;
}

static void collect_entry_predicates(const MDNode &Node,
    SmallPtrSetImpl<const MDNode *> &Visited,
    SmallPtrSetImpl<const GlobalVariable *> &Predicates,
    std::unordered_set<std::string> &Names) {
  if (!Visited.insert(&Node).second)
    return;
  for (unsigned I = 0; I < Node.getNumOperands(); ++I) {
    auto *Operand = Node.getOperand(I).get();
    if (const auto *Key = dyn_cast_or_null<MDString>(Operand);
        Key && Key->getString() == "air.function_constant" && I + 1 < Node.getNumOperands()) {
      if (const auto *Predicate = dyn_cast_or_null<MDNode>(Node.getOperand(I + 1).get());
          Predicate && Predicate->getNumOperands() == 3) {
        if (const auto *Value = dyn_cast_or_null<ConstantAsMetadata>(Predicate->getOperand(0).get()))
          if (const auto *GV = dyn_cast<GlobalVariable>(Value->getValue()))
            Predicates.insert(GV);
        if (const auto *Name = dyn_cast_or_null<MDString>(Predicate->getOperand(2).get()))
          Names.insert(Name->getString().str());
      }
    }
    if (const auto *Child = dyn_cast_or_null<MDNode>(Operand))
      collect_entry_predicates(*Child, Visited, Predicates, Names);
  }
}

static void replace_global_array(GlobalVariable &Old, ArrayRef<Constant *> Elements) {
  auto *Type = ArrayType::get(Old.getValueType()->getArrayElementType(), Elements.size());
  auto *Initializer = ConstantArray::get(Type, Elements);
  auto *Replacement = new GlobalVariable(Type, Old.isConstant(), Old.getLinkage(),
      Initializer, "", Old.getThreadLocalMode());
  Old.getParent()->getGlobalList().insert(Old.getIterator(), Replacement);
  Replacement->takeName(&Old);
  Replacement->setSection(Old.getSection());
  if (auto Alignment = Old.getAlign())
    Replacement->setAlignment(Alignment);
  if (!Old.use_empty()) {
    Constant *AsOldType = Replacement;
    if (AsOldType->getType() != Old.getType())
      AsOldType = ConstantExpr::getBitCast(AsOldType, Old.getType());
    Old.replaceAllUsesWith(AsOldType);
  }
  Old.eraseFromParent();
}

static void prepare_function_constants(Module &M, Function &Entry,
    SmallPtrSetImpl<const GlobalVariable *> &Selected) {
  const auto *Constants = M.getNamedMetadata("air.function_constants");
  if (!Constants)
    return;
  SmallPtrSet<const Function *, 32> Reachable;
  const bool DirectCallsOnly = collect_entry_functions(Entry, Reachable);
  SmallPtrSet<const MDNode *, 32> Visited;
  SmallPtrSet<const GlobalVariable *, 8> Predicates;
  std::unordered_set<std::string> PredicateNames;
  for (const char *Kind : {"air.kernel", "air.vertex", "air.fragment", "air.object", "air.mesh"})
    if (const auto *Entries = M.getNamedMetadata(Kind))
      for (const auto *Node : Entries->operands()) {
        const auto *FunctionMD = Node->getNumOperands() ?
            dyn_cast_or_null<ValueAsMetadata>(Node->getOperand(0).get()) : nullptr;
        if (FunctionMD && FunctionMD->getValue() == &Entry)
          collect_entry_predicates(*Node, Visited, Predicates, PredicateNames);
      }

  std::unordered_set<std::string> SelectedCtorNames;
  for (const auto *Node : Constants->operands()) {
    if (Node->getNumOperands() != 5)
      continue; // The writer reports malformed entries before filtering them.
    const auto *Value = dyn_cast_or_null<ConstantAsMetadata>(Node->getOperand(0).get());
    const auto *Placeholder = Value ? dyn_cast<GlobalVariable>(Value->getValue()) : nullptr;
    const auto *Index = mdconst::dyn_extract_or_null<ConstantInt>(Node->getOperand(3).get());
    const auto *Name = dyn_cast_or_null<MDString>(Node->getOperand(2).get());
    if (!Placeholder || !Index || Index->getValue().getActiveBits() > 16)
      continue;
    const std::string CtorName = "__metal_fc_init_" + std::to_string(Index->getZExtValue());
    const Function *Ctor = M.getFunction(CtorName);
    const GlobalVariable *Backing = nullptr;
    if (Ctor)
      for (const BasicBlock &BB : *Ctor)
        for (const Instruction &I : BB)
          if (const auto *Store = dyn_cast<StoreInst>(&I))
            Backing = dyn_cast<GlobalVariable>(Store->getPointerOperand()->stripPointerCasts());
    const bool Used = !DirectCallsOnly || used_by_entry(*Placeholder, Reachable) ||
        (Backing && used_by_entry(*Backing, Reachable)) ||
        (Name && PredicateNames.count(Name->getString().str()));
    if (Used) {
      Selected.insert(Placeholder);
      SelectedCtorNames.insert(CtorName);
    }
  }

  // The frontend places conditional-interface predicate globals in
  // llvm.compiler.used. Drop other entries' predicates from this clone too,
  // so removing their constructors can actually remove their storage.
  if (auto *CompilerUsed = M.getGlobalVariable("llvm.compiler.used"))
    if (const auto *Array = dyn_cast_or_null<ConstantArray>(CompilerUsed->getInitializer())) {
      SmallVector<Constant *, 16> Kept;
      for (unsigned I = 0; I < Array->getNumOperands(); ++I) {
        auto *Item = cast<Constant>(Array->getOperand(I));
        const auto *GV = dyn_cast<GlobalVariable>(Item->stripPointerCasts());
        if (!GV || !GV->getName().startswith("__metal_implicit_fc_pred") ||
            Predicates.count(GV))
          Kept.push_back(Item);
      }
      if (Kept.size() != Array->getNumOperands()) {
        // GlobalOpt's collectUsedGlobalVariables expects a nonempty array.
        // An empty llvm.compiler.used has no effect and must be removed.
        if (Kept.empty()) {
          CompilerUsed->eraseFromParent();
        } else {
          replace_global_array(*CompilerUsed, Kept);
        }
      }
    }

  auto *Ctors = M.getGlobalVariable("llvm.global_ctors");
  if (!Ctors || !Ctors->hasInitializer())
    return;
  const auto *Array = dyn_cast<ConstantArray>(Ctors->getInitializer());
  if (!Array)
    return;
  SmallVector<Constant *, 16> Kept;
  for (unsigned I = 0; I < Array->getNumOperands(); ++I) {
    auto *Item = cast<Constant>(Array->getOperand(I));
    const auto *Fields = dyn_cast<ConstantStruct>(Item);
    const auto *Ctor = Fields ? dyn_cast<Function>(Fields->getOperand(1)->stripPointerCasts()) : nullptr;
    bool Keep = true;
    if (Ctor && Ctor->getName().startswith("__metal_fc_init_"))
      Keep = SelectedCtorNames.count(Ctor->getName().str()) != 0;
    else if (Ctor && Ctor->getName().startswith("__metal_fc_pred_init")) {
      Keep = false;
      for (const BasicBlock &BB : *Ctor)
        for (const Instruction &Inst : BB)
          if (const auto *Store = dyn_cast<StoreInst>(&Inst))
            if (const auto *GV = dyn_cast<GlobalVariable>(Store->getPointerOperand()->stripPointerCasts()))
              Keep |= Predicates.count(GV) != 0;
    }
    if (Keep)
      Kept.push_back(Item);
  }
  if (Kept.size() == Array->getNumOperands())
    return;
  replace_global_array(*Ctors, Kept);
}

//
bool llvm::WriteMetalLibToFile(Module &M, raw_ostream &OS) {
  // get metal version
  Triple TT(M.getTargetTriple());
  uint32_t target_air_version = 270;
  if (TT.isiOS()) {
    auto ios_version = TT.getiOSVersion();
    if (ios_version.getMajor() == 18) {
      target_air_version = 270;
    } else if (ios_version.getMajor() >= 27) {
      target_air_version = 290;
    } else if (ios_version.getMajor() >= 26) {
      target_air_version = 280;
    }
  } else if (TT.isXROS()) {
    auto xros_version = TT.getXROSVersion();
    if (xros_version.getMajor() == 2) {
      target_air_version = 270;
    } else if (xros_version.getMajor() >= 27) {
      target_air_version = 290;
    } else if (xros_version.getMajor() >= 26) {
      target_air_version = 280;
    }
  } else {
    VersionTuple osx_version{};
    TT.getMacOSXVersion(osx_version);
    auto osx_major = osx_version.getMajor();
    if (osx_major == 15) {
      target_air_version = 270;
    } else if (osx_major >= 27) {
      target_air_version = 290;
    } else if (osx_major == 16 || osx_major >= 26) {
      target_air_version = 280;
    }
  }
  const auto &metal_version = *metal_versions.find(target_air_version);

  // gather entry point functions that we want to clone/emit
  unordered_map<string, FUNCTION_TYPE> function_set;
  // -> first pass to gather all entry points specified in metadata lists
  static constexpr const std::array<FUNCTION_TYPE, 5> func_types{{
      FUNCTION_TYPE::KERNEL,
      FUNCTION_TYPE::VERTEX,
      FUNCTION_TYPE::FRAGMENT,
      FUNCTION_TYPE::OBJECT,
      FUNCTION_TYPE::MESH,
  }};
  for (const auto func_type : func_types) {
    const NamedMDNode *func_list = nullptr;
    switch (func_type) {
    case FUNCTION_TYPE::KERNEL:
      func_list = M.getNamedMetadata("air.kernel");
      break;
    case FUNCTION_TYPE::VERTEX:
      func_list = M.getNamedMetadata("air.vertex");
      break;
    case FUNCTION_TYPE::FRAGMENT:
      func_list = M.getNamedMetadata("air.fragment");
      break;
    case FUNCTION_TYPE::OBJECT:
      func_list = M.getNamedMetadata("air.object");
      break;
    case FUNCTION_TYPE::MESH:
      func_list = M.getNamedMetadata("air.mesh");
      break;
    case FUNCTION_TYPE::UNQUALIFIED:
    case FUNCTION_TYPE::VISIBLE:
    case FUNCTION_TYPE::EXTERN:
    case FUNCTION_TYPE::INTERSECTION:
    case FUNCTION_TYPE::NONE:
      llvm_unreachable("invalid type");
    }
    if (func_list == nullptr) {
      // no functions of this type
      continue;
    }

    for (const auto &op : func_list->operands()) {
      const auto &op_0 = op->getOperand(0);
      if (auto const_md = dyn_cast<ConstantAsMetadata>(op_0)) {
        if (auto func = dyn_cast<Function>(const_md->getValue())) {
          function_set.emplace(func->getName().str(), func_type);
        }
      }
    }
  }
  // -> second pass to actually gather all functions
  // NOTE: we do it this way so that we maintain the order of functions
  vector<pair<const Function *, FUNCTION_TYPE>> functions;
  for (const auto &func : M.functions()) {
    if (!func.hasName()) {
      continue;
    }

    const auto func_iter = function_set.find(func.getName().str());
    if (func_iter == function_set.end()) {
      continue; // not an entry point
    }
    functions.emplace_back(&func, func_iter->second);
  }
  // sort by function type
  std::stable_sort(functions.begin(), functions.end(),
                   [](const auto &lhs, const auto &rhs) {
                     return uint8_t(lhs.second) < uint8_t(rhs.second);
                   });
  const uint32_t function_count = uint32_t(functions.size());

  // program info
  metallib_header_control ctrl;
  metallib_program_info prog_info;
  prog_info.entries.resize(function_count);

  // absolute source file name
  const std::string src_file_name = make_abs_file_name(M.getSourceFileName());
  const uint32_t src_file_name_length = src_file_name.length() + 1u /* \0 */;
  std::string file_name_stem = M.getSourceFileName();
  if (const auto file_name_dot_pos = file_name_stem.rfind('.');
      file_name_dot_pos != std::string::npos) {
    file_name_stem.erase(file_name_dot_pos,
                         file_name_stem.size() - file_name_dot_pos);
  }
  if (const auto file_name_folder_pos = file_name_stem.rfind('/');
      file_name_folder_pos != std::string::npos) {
    file_name_stem.erase(0, file_name_folder_pos + 1);
  }

  // if we're building with debug info, emit .metallib specific debug info
  const bool emit_debug_info =
      (M.debug_compile_units_begin() != M.debug_compile_units_end() &&
       function_count > 0);

  // handle source files / archive creation
  std::pair<std::unique_ptr<uint8_t[]>, uint32_t> source_archive_data{nullptr,
                                                                      0};
  std::unordered_map<std::string /* abs path */,
                     std::unique_ptr<MemoryBuffer> /* source code */>
      source_files;
  std::string working_dir;
  std::string linker_cmd;
  std::string dependent_bc_file_name = src_file_name + ".air";
  if (emit_debug_info) {
    // * get the working directory
    if (auto working_dir_md = M.getNamedMetadata("llvm_utils.workingdir");
        working_dir_md) {
      if (working_dir_md->getNumOperands() >= 1) {
        auto md_node = working_dir_md->getOperand(0);
        if (md_node->getNumOperands() >= 1) {
          if (const auto working_dir_str_md =
                  dyn_cast_or_null<llvm::MDString>(md_node->getOperand(0));
              working_dir_str_md) {
            working_dir = working_dir_str_md->getString().str();
          }
        }
      }
    }

    // * gather all source files
    // * ensure that all DIFile metadata entries use absolute file names +
    //   directory is the working directory
    auto &Context = M.getContext();
    // somewhat overkill, but I don't know of a better way
    const auto md_difile_exec = [&source_files, &Context,
                                 &working_dir](const auto &md) {
      if (const DIFile *difile_node = dyn_cast_or_null<DIFile>(md.first);
          difile_node) {
        const auto orig_file_name = difile_node->getFilename().str();
        auto abs_file_name = make_abs_file_name(orig_file_name);
        source_files.emplace(abs_file_name, nullptr);
        auto mod_difile_node = const_cast<DIFile *>(difile_node);
        mod_difile_node->replaceOperandWith(
            0, llvm::MDString::get(Context, abs_file_name));
        const auto path_sep = abs_file_name.rfind('/');
        if (path_sep != std::string::npos) {
          abs_file_name.erase(path_sep, abs_file_name.size() - path_sep);
        }
        mod_difile_node->replaceOperandWith(
            1, llvm::MDString::get(Context, working_dir));
      }
    };

    ValueEnumerator140 VE140(M, false);
    for (const auto &md : VE140.getMetadataMap()) {
      md_difile_exec(md);
    }

    // * read all source code (drop files that we can't read)
    // * emit code for all valid source files ("recompile_info")
    llvm::NamedMDNode *recompile_info =
        M.getOrInsertNamedMetadata("recompile_info");
    SmallVector<llvm::Metadata *, 8> recompile_info_list;
    for (auto src_iter = source_files.begin();
         src_iter != source_files.end();) {
      auto source = MemoryBuffer::getFile(src_iter->first, true /* is text */,
                                          true /* requires \0 */,
                                          false /* not volatile */);
      if (!source) {
        src_iter = source_files.erase(src_iter);
      } else {
        src_iter->second = std::move(*source);
        ++src_iter;
      }
    }
    for (const auto &source_file : source_files) {
      SmallVector<llvm::Metadata *, 2> recompile_info_file;
      recompile_info_file.push_back(
          llvm::MDString::get(Context, source_file.first));
      recompile_info_file.push_back(
          llvm::MDString::get(Context, source_file.second->getBuffer()));
      recompile_info_list.push_back(
          llvm::MDNode::get(Context, recompile_info_file));
    }
    recompile_info->addOperand(llvm::MDNode::get(Context, recompile_info_list));

    // * create additional source archive entries
    if (working_dir.empty()) {
      errs() << "no valid 'llvm_utils.workingdir' metadata entry!\n";
      return false;
    }
    source_files.emplace("metal-working-dir.txt",
                         MemoryBuffer::getMemBuffer(working_dir));

    StringRef cmd_line;
    if (auto cmd_line_md = M.getNamedMetadata("llvm.commandline");
        cmd_line_md) {
      if (cmd_line_md->getNumOperands() >= 1) {
        auto md_node = cmd_line_md->getOperand(0);
        if (md_node->getNumOperands() >= 1) {
          if (auto cmd_line_str_md =
                  dyn_cast_or_null<llvm::MDString>(md_node->getOperand(0));
              cmd_line_str_md) {
            cmd_line = cmd_line_str_md->getString();
          }
        }
      }
    }
    if (cmd_line.empty()) {
      errs() << "no valid 'llvm.commandline' metadata entry!\n";
      return false;
    }
    source_files.emplace("metal-options.txt",
                         MemoryBuffer::getMemBuffer(cmd_line));
    linker_cmd = cmd_line;

    source_files.emplace("original-input-filename.txt",
                         MemoryBuffer::getMemBuffer(dependent_bc_file_name));

    // * create .tar.bz2
    auto tar_data = create_tar(source_files);
#if 0
    {
      error_code ec;
      raw_fd_ostream tar_file(src_file_name + ".tar", ec, sys::fs::CreationDisposition::CD_CreateAlways);
      tar_file.write(tar_data.data(), tar_data.size());
    }
#endif
    source_archive_data =
        compress_source_archive(tar_data.data(), tar_data.size());
    if (!source_archive_data.first) {
      return false;
    }
#if 0
    {
      error_code ec;
      raw_fd_ostream bz2_file(src_file_name + ".tar.bz2", ec, sys::fs::CreationDisposition::CD_CreateAlways);
      bz2_file.write((const char*)source_archive_data.first.get(), source_archive_data.second);
    }
#endif

    // * export the original complete bitcode file as well
    error_code ec;
    raw_fd_ostream dependent_bc_file(
        dependent_bc_file_name, ec,
        sys::fs::CreationDisposition::CD_CreateAlways);
    if (!ec) {
      WriteBitcodeToFile140(M, dependent_bc_file);
      dependent_bc_file.flush();
    } else {
      errs() << "failed to write dependent debug file "
             << dependent_bc_file_name << "\n";
      return false;
    }
  }

  // we can now remove any metadata that we no longer need
  static constexpr array<const char *, 5> drop_mds{
      "llvm_utils.workingdir", "recompile_info",   "air.source_file_name",
      "llvm.linker.options",   "llvm.commandline",
  };
  for (const auto &drop_md : drop_mds) {
    if (auto md = M.getNamedMetadata(drop_md); md) {
      md->dropAllReferences();
      md->eraseFromParent();
    }
  }

  // create per-function modules and fill entries
  uint64_t entries_size = 0;
  uint64_t extended_md_data_size = 0;
  uint64_t debug_data_size = 0;
  uint64_t bitcode_data_size = 0;
  for (uint32_t i = 0; i < function_count; ++i) {
    auto &entry = prog_info.entries[i];
    auto &func = functions[i].first;

    entry.type = functions[i].second;
    entry.name = func->getName().str();
    entry.metal_version.major = metal_version.second.first[0];
    entry.metal_version.minor = metal_version.second.first[1];
    entry.metal_version.rev = metal_version.second.first[2];
    entry.metal_language_version.major = metal_version.second.second[0];
    entry.metal_language_version.minor = metal_version.second.second[1];
    entry.metal_language_version.rev = metal_version.second.second[2];
    entry.emit_debug_info = emit_debug_info;

    // clone the module with the current entry point function and any global
    // vars that we need
    ValueToValueMapTy VMap;
    auto cloned_mod = CloneModule(M, VMap);
    SmallPtrSet<const GlobalVariable *, 8> selected_function_constants;
    prepare_function_constants(*cloned_mod, *cloned_mod->getFunction(func->getName()),
                               selected_function_constants);

    // metallib uses the function name as the source file name
    cloned_mod->setSourceFileName(func->getName());

    // update data layout
    cloned_mod->setDataLayout(
        "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-"
        "f64:64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:"
        "128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:"
        "1024-n8:16:32");

    // Internalize and remove globals unused by this entry, including the
    // function constant initializers removed from llvm.global_ctors above.
    llvm::legacy::PassManager mpm;

    mpm.add(createInternalizePass([&func](const GlobalValue& GV) -> bool {
        return GV.getName() == func->getName();
    }));

    mpm.add(createGlobalOptimizerPass());

    mpm.run(*cloned_mod);

    // * strip floor_* calling convention from all functions and their users (replace it with C CC)
    for (auto func_iter = cloned_mod->begin(); func_iter != cloned_mod->end();) {
      auto& func = *func_iter;

      if (func.getCallingConv() != CallingConv::C) {
        func.setCallingConv(CallingConv::C);
        for (auto user : func.users()) {
          if (auto CB = dyn_cast<CallBase>(user)) {
            CB->setCallingConv(CallingConv::C);
          }
        }
      }

      ++func_iter;
    }

    // clean up metadata
    // * metadata of all entry points that no longer exist
    static constexpr const std::array<const char *, 5> entry_point_md_names{{
        "air.kernel",
        "air.vertex",
        "air.fragment",
        "air.object",
        "air.mesh",
    }};
    for (const auto &entry_point_md_name : entry_point_md_names) {
      if (auto func_entries =
              cloned_mod->getNamedMetadata(entry_point_md_name)) {
        vector<MDNode *> kept_nodes;
        for (auto op_iter = func_entries->op_begin();
             op_iter != func_entries->op_end(); ++op_iter) {
          MDNode *node = *op_iter;
          if (node->getNumOperands() < 3) {
            continue;
          }
          if (node->getOperand(0).get() == nullptr) {
            continue;
          }
          kept_nodes.emplace_back(node);
        }

        // need to drop all references to existing nodes, b/c we can't directly
        // remove operands
        func_entries->dropAllReferences();
        if (kept_nodes.empty()) {
          // remove air.sampler_states altogether
          func_entries->eraseFromParent();
        } else {
          // now, only add ones we want to keep
          for (auto node : kept_nodes) {
            func_entries->addOperand(node);
          }
        }
      }
    }
    // * sample states of entry points that no longer exist
    if (auto sampler_states =
            cloned_mod->getNamedMetadata("air.sampler_states")) {
      vector<MDNode *> kept_nodes;
      for (auto op_iter = sampler_states->op_begin();
           op_iter != sampler_states->op_end(); ++op_iter) {
        MDNode *node = *op_iter;
        if (node->getNumOperands() != 2 ||
            node->getOperand(1).get() == nullptr) {
          continue;
        }
        kept_nodes.emplace_back(node);
      }

      // need to drop all references to existing nodes, b/c we can't directly
      // remove operands
      sampler_states->dropAllReferences();
      if (kept_nodes.empty()) {
        // remove air.sampler_states altogether
        sampler_states->eraseFromParent();
      } else {
        // now, only add ones we want to keep
        for (auto node : kept_nodes) {
          sampler_states->addOperand(node);
        }
      }
    }
    // * set fake compiler ident
    if (auto llvm_ident = cloned_mod->getNamedMetadata("llvm.ident")) {
      if (MDNode *ident_op = llvm_ident->getOperand(0)) {
        static const std::unordered_map<uint32_t, const char *> ident_versions{
            {270, "Apple metal version 32023.620 (metalfe-32023.620)"},
            {280, "Apple metal version 32023.883 (metalfe-32023.883)"},
            {290, "Apple metal version 32023.921 (metalfe-32023.921.6)"},
        };
        ident_op->replaceOperandWith(
            0, llvm::MDString::get(cloned_mod->getContext(),
                                   ident_versions.at(target_air_version)));
      }
    }
    // * kill other named metadata that is no longer needed
    for (auto nmd_iter = cloned_mod->named_metadata_begin();
         nmd_iter != cloned_mod->named_metadata_end();) {
      if (nmd_iter->getName().startswith("floor.")) {
        auto erase_nmd = &*nmd_iter++;
        cloned_mod->eraseNamedMetadata(erase_nmd);
      } else {
        ++nmd_iter;
      }
    }

    // modify local and constant memory GVs
    const auto &DL = cloned_mod->getDataLayout();
    for (auto I = cloned_mod->global_begin(), E = cloned_mod->global_end();
         I != E;) {
      GlobalVariable &GV = *I++;
      if (GV.getAddressSpace() == 2 /* constant memory */ ||
          GV.getAddressSpace() == 3 /* local memory */) {
        auto value_type = GV.getValueType();
        if (value_type && value_type->isSized() &&
            DL.getTypeStoreSize(value_type) >= 16 && GV.getAlignment() < 16 &&
            !(GV.hasName() && GV.getName().startswith("__air_sampler_state"))) {
          // use at least 16-byte alignment (unless sampler state)
          GV.setAlignment(MaybeAlign{16u});
        }
      }
      if (GV.getAddressSpace() == 3 /* local memory */) {
        // always use undef initializer (instead of zeroinitializer)
        GV.setInitializer(UndefValue::get(GV.getValueType()));
      }
    }

    // rename __air_sampler_state to have ascending names
    uint32_t sampler_state_count = 0u;
    for (auto &GV : cloned_mod->globals()) {
      if (!GV.hasName() || !GV.getName().startswith("__air_sampler_state")) {
        continue;
      }
      if (sampler_state_count == 0) {
        GV.setName("__air_sampler_state");
      } else {
        GV.setName("__air_sampler_state." +
                   std::to_string(sampler_state_count));
      }
      ++sampler_state_count;
    }

    // extract tessellation info
    if (auto vertex_md = cloned_mod->getNamedMetadata("air.vertex");
        vertex_md && vertex_md->getNumOperands() > 0) {
      // there should only be one vertex function entry
      assert(vertex_md->getNumOperands() == 1u);
      auto tess_func_md = vertex_md->getOperand(0);
      for (auto op_iter = tess_func_md->op_begin() + 1;
           op_iter != tess_func_md->op_end(); ++op_iter) {
        auto node = dyn_cast_or_null<MDNode>(*op_iter);
        if (!node) {
          continue;
        }
        if (auto first_md = dyn_cast_or_null<MDString>(node->getOperand(0));
            first_md && first_md->getString().equals("air.patch")) {
          auto prim_md = dyn_cast_or_null<MDString>(node->getOperand(1));
          if (!prim_md) {
            continue;
          }
          auto ctrl_pnts_md =
              dyn_cast_or_null<ConstantAsMetadata>(node->getOperand(3));
          if (!ctrl_pnts_md) {
            continue;
          }
          auto ctrl_pnts =
              dyn_cast_or_null<ConstantInt>(ctrl_pnts_md->getValue());
          if (!ctrl_pnts) {
            continue;
          }

          auto prim_str = prim_md->getString();
          if (prim_str.startswith("triangle")) {
            entry.tess.info.primitive_type = 1u;
          } else if (prim_str.startswith("quad")) {
            entry.tess.info.primitive_type = 2u;
          }
          entry.tess.info.control_point_count = min(
              (uint32_t)ctrl_pnts->getValue().getZExtValue(), 63u /* 6 bits */);
        } else if (auto ctrl_pnt_md =
                       dyn_cast_or_null<MDNode>(node->getOperand(0));
                   ctrl_pnt_md) {
          if (ctrl_pnt_md->getNumOperands() < 4) {
            continue;
          }
          // op #0: arg index (should be 0)
          // op #1: "air.patch_control_point_input"
          if (auto patch_ctrl_pnt_str =
                  dyn_cast_or_null<MDString>(ctrl_pnt_md->getOperand(1));
              !patch_ctrl_pnt_str || !patch_ctrl_pnt_str->getString().equals(
                                         "air.patch_control_point_input")) {
            continue;
          }
          // op #2: specifies the patch_control_point_function
          // ops #3 and higher specify all attributes
          for (auto attr_iter = ctrl_pnt_md->op_begin() + 3;
               attr_iter != ctrl_pnt_md->op_end(); ++attr_iter) {
            const auto attr_md = dyn_cast_or_null<MDNode>(*attr_iter);
            if (!attr_md || attr_md->getNumOperands() < 7) {
              continue;
            }
            auto attr_idx =
                dyn_cast_or_null<ConstantAsMetadata>(attr_md->getOperand(1));
            auto attr_type = dyn_cast_or_null<MDString>(attr_md->getOperand(4));
            auto attr_name = dyn_cast_or_null<MDString>(attr_md->getOperand(6));
            if (!attr_idx || !attr_type || !attr_name) {
              continue;
            }
            auto attr_idx_int =
                dyn_cast_or_null<ConstantInt>(attr_idx->getValue());
            if (!attr_idx_int) {
              continue;
            }
            vertex_attribute vattr{
                .name = attr_name->getString().str(),
                .index = (uint32_t)attr_idx_int->getZExtValue(),
                .type = data_type_from_string(attr_type->getString().str()),
                .use = VERTEX_USE::CONRTOL_POINT,
                .active = true, // always flag as active
            };
            if (vattr.type == DATA_TYPE::INVALID) {
              errs() << "invalid data type in control point vertex attribute: "
                     << vattr.name << ", index " << vattr.index << "\n";
              return false;
            }
            entry.vertex_attributes.emplace_back(std::move(vattr));
          }
        }
      }
    }

    // function constants
    if (auto fc_md = cloned_mod->getNamedMetadata("air.function_constants");
        fc_md && fc_md->getNumOperands() > 0) {
      vector<MDNode *> kept_fcs;

      for (auto op_iter = fc_md->op_begin();
           op_iter != fc_md->op_end(); ++op_iter) {
        auto node = dyn_cast_or_null<MDNode>(*op_iter);
        // Named metadata is not shape-checked by the IR verifier. Validate
        // before indexing; reflection's later validation cannot protect this loop.
        if (!node || node->getNumOperands() != 5u) {
          M.getContext().emitError("invalid Metal function constant metadata");
          return false;
        }
        auto cnst_idx =
            dyn_cast_or_null<ConstantAsMetadata>(node->getOperand(3));
        auto cnst_type =
            dyn_cast_or_null<MDString>(node->getOperand(1));
        auto cnst_name =
            dyn_cast_or_null<MDString>(node->getOperand(2));
        if (!cnst_idx || !cnst_type || !cnst_name) {
          M.getContext().emitError("invalid Metal function constant metadata");
          return false;
        }
        auto cnst_idx_int =
            dyn_cast_or_null<ConstantInt>(cnst_idx->getValue());
        if (!cnst_idx_int || cnst_idx_int->getValue().getActiveBits() > 16u) {
          M.getContext().emitError("invalid Metal function constant metadata");
          return false;
        }
        const auto *value = dyn_cast_or_null<ConstantAsMetadata>(node->getOperand(0).get());
        const auto *placeholder = value ? dyn_cast<GlobalVariable>(value->getValue()) : nullptr;
        if (!placeholder || !selected_function_constants.count(placeholder))
          continue;

        kept_fcs.push_back(node);
        function_constant cnst{
            .name = cnst_name->getString().str(),
            .index = (uint32_t)cnst_idx_int->getZExtValue(),
            .type = data_type_from_string(cnst_type->getString().str()),
            .active = true
        };
        entry.function_constants.emplace_back(std::move(cnst));
      }

      fc_md->dropAllReferences();
      if (kept_fcs.empty()) {
        fc_md->eraseFromParent();
      }
      else {
        for (auto node : kept_fcs) {
          fc_md->addOperand(node);
        }
      }
    }

    // extract metadata and build reflection data from it
    entry.reflection_data = metal::reflection::create_reflection(*cloned_mod);
    if (entry.reflection_data.empty()) {
      errs() << "failed to create reflection data for " << M.getSourceFileName()
             << "\n";
      return false;
    }

    // write module / bitcode
    raw_string_ostream bitcode_stream{entry.bitcode_data};
    WriteBitcodeToFile140(*cloned_mod, bitcode_stream);
    bitcode_stream.flush();

    // hash module
    entry.hash = compute_sha256_hash((const uint8_t *)entry.bitcode_data.data(),
                                     entry.bitcode_data.size());

    // write extended metadata and debug data (just ENDT right now)
    static const auto end_tag = TAG_TYPE::END;
    static const auto dbg_tag = TAG_TYPE::DEBI;
    static const auto dep_tag = TAG_TYPE::DEPF;
    static const uint32_t tag_length = sizeof(TAG_TYPE);

    raw_string_ostream ext_md_stream{entry.extended_md_data};
    const uint32_t length_placeholder = 0;

    uint16_t attr_count = 0;
    uint16_t vatt_len = 0;
    uint16_t vaty_len = 0;

    if (!entry.vertex_attributes.empty()) {
      attr_count = (uint16_t)entry.vertex_attributes.size();
      vatt_len =
          sizeof(uint16_t) /* attr count */ +
          2u * attr_count /* per-attr info */;
      vaty_len =
          sizeof(uint16_t) /* attr count */ + attr_count /* per-attr type */;
      for (const auto &vattr : entry.vertex_attributes) {
        vatt_len += vattr.name.size() + 1u;
      }
    }

    uint16_t cnst_count = 0;
    uint16_t cnst_len = 0;

    if (!entry.function_constants.empty()) {
      // CNST has 16-bit count and payload-length fields. Do not let either
      // wrap while still writing the complete records into the container.
      constexpr size_t max_cnst_field = std::numeric_limits<uint16_t>::max();
      const size_t count = entry.function_constants.size();
      auto reject_cnst_size = [&]() {
        M.getContext().emitError("Metal function constant metadata exceeds CNST limits");
        return false;
      };
      if (count > max_cnst_field)
        return reject_cnst_size();
      size_t length = sizeof(uint16_t) +
          (sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint8_t)) * count;
      if (length > max_cnst_field)
        return reject_cnst_size();
      for (const auto &cnst : entry.function_constants) {
        // Include the terminator without allowing size_t addition to overflow.
        if (length == max_cnst_field ||
            cnst.name.size() > max_cnst_field - length - 1u)
          return reject_cnst_size();
        length += cnst.name.size() + 1u;
      }
      cnst_count = static_cast<uint16_t>(count);
      cnst_len = static_cast<uint16_t>(length);
    }

    ext_md_stream.write((const char *)&length_placeholder, sizeof(length_placeholder));

    if (!entry.vertex_attributes.empty()) {
      static const auto vatt_tag = TAG_TYPE::VATT;
      static const auto vaty_tag = TAG_TYPE::VATY;

      // VATT
      ext_md_stream.write((const char *)&vatt_tag, tag_length);
      ext_md_stream.write((const char *)&vatt_len, sizeof(vatt_len));
      ext_md_stream.write((const char *)&attr_count, sizeof(attr_count));
      for (const auto &vattr : entry.vertex_attributes) {
        ext_md_stream.write(vattr.name.c_str(), vattr.name.size());
        ext_md_stream.write('\0');

        uint16_t info = vattr.index & 0x1FFFu;
        info |= (uint16_t(vattr.use) & 0x3u) << 13u;
        info |= (vattr.active ? 0x8000u : 0u);
        ext_md_stream.write((const char *)&info, sizeof(info));
      }

      // VATY
      ext_md_stream.write((const char *)&vaty_tag, tag_length);
      ext_md_stream.write((const char *)&vaty_len, sizeof(vaty_len));
      ext_md_stream.write((const char *)&attr_count, sizeof(attr_count));
      for (const auto &vattr : entry.vertex_attributes) {
        ext_md_stream.write((const char *)&vattr.type, sizeof(DATA_TYPE));
      }
    }

    if (!entry.function_constants.empty()) {
      static const auto cnst_tag = TAG_TYPE::CNST;

      // CNST
      ext_md_stream.write((const char *)&cnst_tag, tag_length);
      ext_md_stream.write((const char *)&cnst_len, sizeof(cnst_len));
      ext_md_stream.write((const char *)&cnst_count, sizeof(cnst_count));
      for (const auto& cnst : entry.function_constants) {
        ext_md_stream.write(cnst.name.c_str(), cnst.name.size());
        ext_md_stream.write('\0');

        ext_md_stream.write((const char *)&cnst.type, sizeof(DATA_TYPE));

        uint16_t index = (uint16_t)cnst.index;
        ext_md_stream.write((const char *)&index, sizeof(index));
        ext_md_stream.write((const char *)&cnst.active, sizeof(uint8_t));
      }
    }

    ext_md_stream.write((const char *)&end_tag, tag_length);
    ext_md_stream.flush();
    finish_metadata_length(entry.extended_md_data);

    raw_string_ostream dbg_stream{entry.debug_data};
    dbg_stream.write((const char *)&length_placeholder, sizeof(length_placeholder));
    if (emit_debug_info) {
      dbg_stream.write((const char *)&dbg_tag, tag_length);
      const uint16_t src_file_info_len = uint16_t(4u + src_file_name_length);
      dbg_stream.write((const char *)&src_file_info_len,
                       sizeof(src_file_info_len));
      uint32_t function_line = 1;
      if (DISubprogram *sub_prog = func->getSubprogram(); sub_prog) {
        function_line = sub_prog->getLine();
      }
      dbg_stream.write((const char *)&function_line, sizeof(function_line));
      dbg_stream.write(src_file_name.c_str(), src_file_name_length - 1u);
      dbg_stream.write('\0');
      if (!dependent_bc_file_name.empty()) {
        dbg_stream.write((const char *)&dep_tag, tag_length);
        const uint16_t dep_info_length =
            uint16_t(dependent_bc_file_name.size() + 1 /* \0 */);
        dbg_stream.write((const char *)&dep_info_length,
                         sizeof(dep_info_length));
        dbg_stream.write(dependent_bc_file_name.c_str(), dep_info_length - 1u);
        dbg_stream.write('\0');
      }
    }
    dbg_stream.write((const char *)&end_tag, tag_length);
    dbg_stream.flush();
    finish_metadata_length(entry.debug_data);

    // finish
    entry.update_length();
    entries_size += entry.length;
    extended_md_data_size += entry.extended_md_size;
    debug_data_size += entry.debug_size;
    bitcode_data_size += entry.bitcode_size;
  }

  // now that we have created all data/info, update all offsets
  uint64_t running_ext_md_size = 0, running_dbg_size = 0, running_bc_size = 0;
  for (uint32_t i = 0; i < function_count; ++i) {
    auto &entry = prog_info.entries[i];
    entry.update_offsets(running_ext_md_size, running_dbg_size,
                         running_bc_size);
  }
  assert(extended_md_data_size == running_ext_md_size);
  assert(debug_data_size == running_dbg_size);
  assert(bitcode_data_size == running_bc_size);

  //// start writing
  // header
  OS.write("MTLB", 4);

  uint16_t container_version_bugfix = 8;
  if (target_air_version >= 280) {
    container_version_bugfix = 9;
  }
  metallib_version header{
      .container_version_major = 1,
      .is_macos_target = TT.isMacOSX(),
      .container_version_minor = 2,
      .container_version_bugfix = container_version_bugfix,
      .file_type = 0,    // always "execute"
      .is_stub = false,  // never stub
      .is_64_bit = true, // always 64-bit
  };
  VersionTuple platform_version;
  if (TT.isMacOSX()) {
    header.platform = uint32_t(metal::APPLE_PLATFORM::MACOS);
    TT.getMacOSXVersion(platform_version);
  } else if (TT.isiOS()) {
    header.platform = uint32_t(TT.isSimulatorEnvironment()
                                   ? metal::APPLE_PLATFORM::IOS_SIMULATOR
                                   : metal::APPLE_PLATFORM::IOS);
    platform_version = TT.getiOSVersion();
  } else if (TT.isTvOS()) {
    header.platform = uint32_t(TT.isSimulatorEnvironment()
                                   ? metal::APPLE_PLATFORM::TVOS_SIMULATOR
                                   : metal::APPLE_PLATFORM::TVOS);
    platform_version = TT.getiOSVersion();
  } else if (TT.isWatchOS()) {
    header.platform = uint32_t(TT.isSimulatorEnvironment()
                                   ? metal::APPLE_PLATFORM::WATCHOS_SIMULATOR
                                   : metal::APPLE_PLATFORM::WATCHOS);
    platform_version = TT.getWatchOSVersion();
  } else if (TT.isXROS()) {
    header.platform = uint32_t(TT.isSimulatorEnvironment()
                                   ? metal::APPLE_PLATFORM::XROS_SIMULATOR
                                   : metal::APPLE_PLATFORM::XROS);
    platform_version = TT.getXROSVersion();
  } else {
    header.platform = 0u;
  }
  header.platform_version_major = platform_version.getMajor();
  header.platform_version_minor = platform_version.getMinor().hasValue()
                                      ? platform_version.getMinor().getValue()
                                      : 0;
  header.platform_version_update =
      platform_version.getSubminor().hasValue()
          ? platform_version.getSubminor().getValue()
          : 0;

  OS.write((const char *)&header, sizeof(metallib_version));

  // file length
  uint64_t ext_program_md_size = sizeof(TAG_TYPE) /* ENDT */;
  ext_program_md_size += (4 + 2 + 16) /* UUID */;
  ext_program_md_size += (4 + 2 + 16) /* HDYN */;
  if (emit_debug_info) {
    ext_program_md_size += (4 + 2 + 16) /* HSRD */;
  }
  ext_program_md_size += (4 + 2 + 16) /* RLST */;
  const uint32_t src_archive_header_length =
      (emit_debug_info
           ? (sizeof(uint32_t) /* count */ + (linker_cmd.size() + 1) +
              (working_dir.size() + 1) + sizeof(uint32_t) /* length */)
           : 0u);
  const uint32_t src_archive_length =
      (emit_debug_info
           ? (sizeof(TAG_TYPE) /* magic/tag */ +
              sizeof(uint32_t) /* archive length */ +
              sizeof(uint16_t) /* "0" */ + source_archive_data.second +
              sizeof(TAG_TYPE) /* end tag */)
           : 0u);
  const auto metallib_file_name = file_name_stem + ".metallib";
  const auto metallib_file_name_size =
      metallib_file_name.size() + sizeof(uint8_t) /* "0" */;
  const uint32_t dyn_header_block_length =
      (sizeof(TAG_TYPE) /* magic/tag */ + sizeof(uint16_t) /* name length */ +
       metallib_file_name_size + sizeof(TAG_TYPE) /* end tag */);
  uint64_t file_length =
      (sizeof(metallib_header) + sizeof(uint32_t) /* #programs */ +
       entries_size + ext_program_md_size + extended_md_data_size +
       debug_data_size + bitcode_data_size + dyn_header_block_length +
       src_archive_header_length + src_archive_length);

  // compute total reflection size + individual entry offsets
  uint64_t reflection_data_size = sizeof(uint32_t) /* #entries */;
  for (uint32_t i = 0; i < function_count; ++i) {
    prog_info.entries[i].update_reflection_placement(reflection_data_size,
                                                     file_length);
  }
  file_length += reflection_data_size;

  OS.write((const char *)&file_length, sizeof(uint64_t));

  // header/block control
  ctrl.programs_offset = sizeof(metallib_header);
  ctrl.programs_length = entries_size;
  ctrl.extended_md_offset = ctrl.programs_offset + sizeof(uint32_t) +
                            ctrl.programs_length + ext_program_md_size;
  ctrl.extended_md_length = extended_md_data_size;
  ctrl.debug_offset = ctrl.extended_md_offset + ctrl.extended_md_length;
  ctrl.debug_length = debug_data_size;
  ctrl.bitcode_offset = ctrl.debug_offset + ctrl.debug_length;
  ctrl.bitcode_length = bitcode_data_size;
  const uint64_t dyn_header_offset = ctrl.bitcode_offset + ctrl.bitcode_length;
  const uint64_t dyn_header_length = dyn_header_block_length;
  const uint64_t src_archives_offset = dyn_header_offset + dyn_header_length;
  const uint64_t src_archives_length =
      (src_archive_header_length + src_archive_length);
  const uint64_t reflection_offset = src_archives_offset + src_archives_length;
  OS.write((const char *)&ctrl, sizeof(metallib_header_control));

  // write entry headers/info
  OS.write((const char *)&function_count, sizeof(function_count));
  for (auto &entry : prog_info.entries) {
    if (emit_debug_info) {
      entry.debug_source_offset = src_archive_header_length;
    }
    entry.write_header(OS);
  }

  // write additional program metadata
  {
    // write embedded source code archive metadata
    if (emit_debug_info) {
      const auto HSRD_tag = TAG_TYPE::HSRD;
      OS.write((const char *)&HSRD_tag, sizeof(TAG_TYPE));
      OS.write(0x10);
      OS.write(0x0);
      OS.write((const char *)&src_archives_offset, sizeof(uint64_t));
      OS.write((const char *)&src_archives_length, sizeof(uint64_t));
    }

    // write dynamic header metadata
    const auto HDYN_tag = TAG_TYPE::HDYN;
    OS.write((const char *)&HDYN_tag, sizeof(TAG_TYPE));
    OS.write(0x10);
    OS.write(0x0);
    OS.write((const char *)&dyn_header_offset, sizeof(uint64_t));
    OS.write((const char *)&dyn_header_length, sizeof(uint64_t));

    // write RLST
    const auto RLST_tag = TAG_TYPE::RLST;
    OS.write((const char *)&RLST_tag, sizeof(TAG_TYPE));
    OS.write(0x10);
    OS.write(0x0);
    OS.write((const char *)&reflection_offset, sizeof(uint64_t));
    OS.write((const char *)&reflection_data_size, sizeof(uint64_t));

    // write UUID
    // NOTE: Apple doesn't actually care about UUID variants and versions,
    // so just fill this with random, but still signal variant 1 + version 4
    random_device rd{};
    mt19937 gen{rd()};
    uniform_int_distribution<uint8_t> dist(0u, 0xFFu);
    raw_ostream::uuid_t program_uuid;
    for (auto &ch : program_uuid) {
      ch = dist(gen);
    }
    program_uuid[6] = (4u /* version */ << 4u) | (program_uuid[6] & 0x0Fu);
    program_uuid[8] = (0b10 /* variant */ << 6u) | (program_uuid[8] & 0x3Fu);

    const auto UUID_tag = TAG_TYPE::UUID;
    OS.write((const char *)&UUID_tag, sizeof(TAG_TYPE));
    OS.write(0x10);
    OS.write(0x0);
    OS.write((const char *)&program_uuid, sizeof(program_uuid));

    // write ENDT
    // NOTE: this is not included by the "programs_length"
    const auto END_tag = TAG_TYPE::END;
    OS.write((const char *)&END_tag, sizeof(TAG_TYPE));
  }

  // write extended metadata
  for (const auto &entry : prog_info.entries) {
    entry.write_extended_md(OS);
  }

  // write debug data
  for (const auto &entry : prog_info.entries) {
    entry.write_debug(OS);
  }

  // write bitcode data
  for (const auto &entry : prog_info.entries) {
    entry.write_module(OS);
  }

  // write dynamic header
  const auto NAME_tag = TAG_TYPE::NAME;
  OS.write((const char *)&NAME_tag, sizeof(TAG_TYPE));
  const auto len = (uint16_t)metallib_file_name_size;
  OS.write((const char *)&len, sizeof(len));
  OS.write(metallib_file_name.c_str(), metallib_file_name_size - 1);
  OS.write(0x0); // \0
  const auto END_tag = TAG_TYPE::END;
  OS.write((const char *)&END_tag, sizeof(TAG_TYPE));

  // write embedded source code archives
  if (emit_debug_info) {
    const uint32_t src_archive_count = 1;
    OS.write((const char *)&src_archive_count, sizeof(src_archive_count));

    // linker command + linker working dir (assume same as compiler)
    OS.write(linker_cmd.c_str(), linker_cmd.size());
    OS.write(0);
    OS.write(working_dir.c_str(), working_dir.size());
    OS.write(0);

    OS.write((const char *)&src_archive_length, sizeof(src_archive_length));

    const auto SARC_tag = TAG_TYPE::SARC;
    OS.write((const char *)&SARC_tag, sizeof(TAG_TYPE));

    const uint32_t source_archive_data_size =
        source_archive_data.second + sizeof(uint16_t);
    OS.write((const char *)&source_archive_data_size, sizeof(uint32_t));
    const char archive_number[] = "0";
    OS.write((const char *)&archive_number, 2);
    OS.write((const char *)source_archive_data.first.get(),
             source_archive_data.second);

    const auto END_tag = TAG_TYPE::END;
    OS.write((const char *)&END_tag, sizeof(TAG_TYPE));
  }

  // write reflection data
  const auto reflection_entry_count = uint32_t(prog_info.entries.size());
  OS.write((const char *)&reflection_entry_count,
           sizeof(reflection_entry_count));
  for (const auto &entry : prog_info.entries) {
    entry.write_reflection(OS);
  }

  return true;
}
