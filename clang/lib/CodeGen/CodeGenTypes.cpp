//===--- CodeGenTypes.cpp - Type translation for LLVM CodeGen -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the code that handles AST -> LLVM type lowering.
//
//===----------------------------------------------------------------------===//

#include "CodeGenTypes.h"
#include "CGCXXABI.h"
#include "CGCall.h"
#include "CGOpenCLRuntime.h"
#include "CGRecordLayout.h"
#include "TargetInfo.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecordLayout.h"
#include "clang/CodeGen/CGFunctionInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"

#include "llvm/Transforms/LibFloor/FloorImageType.h"

using namespace clang;
using namespace CodeGen;

#ifndef NDEBUG
#include "llvm/Support/CommandLine.h"
// TODO: turn on by default when defined(EXPENSIVE_CHECKS) once check-clang is
// -verify-type-cache clean.
static llvm::cl::opt<bool> VerifyTypeCache(
    "verify-type-cache",
    llvm::cl::desc("Verify that the type cache matches the computed type"),
    llvm::cl::init(false), llvm::cl::Hidden);
#endif

CodeGenTypes::CodeGenTypes(CodeGenModule &cgm)
  : CGM(cgm), Context(cgm.getContext()), TheModule(cgm.getModule()),
    Target(cgm.getTarget()), TheCXXABI(cgm.getCXXABI()),
    TheABIInfo(cgm.getTargetCodeGenInfo().getABIInfo()) {
  SkippedLayout = false;
}

CodeGenTypes::~CodeGenTypes() {
  for (llvm::FoldingSet<CGFunctionInfo>::iterator
       I = FunctionInfos.begin(), E = FunctionInfos.end(); I != E; )
    delete &*I++;
}

const CodeGenOptions &CodeGenTypes::getCodeGenOpts() const {
  return CGM.getCodeGenOpts();
}

void CodeGenTypes::addRecordTypeName(const RecordDecl *RD,
                                     llvm::StructType *Ty,
                                     StringRef suffix, const bool is_io_type) {
  SmallString<256> TypeName;
  llvm::raw_svector_ostream OS(TypeName);
  OS << RD->getKindName() << '.';
  if (is_io_type) {
    OS << "floor.io.";
  }

  // FIXME: We probably want to make more tweaks to the printing policy. For
  // example, we should probably enable PrintCanonicalTypes and
  // FullyQualifiedNames.
  PrintingPolicy Policy = RD->getASTContext().getPrintingPolicy();
  Policy.SuppressInlineNamespace = false;

  // Name the codegen type after the typedef name
  // if there is no tag type name available
  if (RD->getIdentifier()) {
    // FIXME: We should not have to check for a null decl context here.
    // Right now we do it because the implicit Obj-C decls don't have one.
    if (RD->getDeclContext())
      RD->printQualifiedName(OS, Policy);
    else
      RD->printName(OS);
  } else if (const TypedefNameDecl *TDD = RD->getTypedefNameForAnonDecl()) {
    // FIXME: We should not have to check for a null decl context here.
    // Right now we do it because the implicit Obj-C decls don't have one.
    if (TDD->getDeclContext())
      TDD->printQualifiedName(OS, Policy);
    else
      TDD->printName(OS);
  } else
    OS << "anon";

  if (!suffix.empty())
    OS << suffix;

  Ty->setName(OS.str());
}

/// ConvertTypeForMem - Convert type T into a llvm::Type.  This differs from
/// ConvertType in that it is used to convert to the memory representation for
/// a type.  For example, the scalar representation for _Bool is i1, but the
/// memory representation is usually i8 or i32, depending on the target.
llvm::Type *CodeGenTypes::ConvertTypeForMem(QualType T, bool ForBitField,
                                            bool ForRecordField,
                                            type_conversion_opts_t opts) {
  const auto is_task_payload = T.getAddressSpace() == LangAS::task_payload;
  opts.vector_compat_conversion |= is_task_payload; // always want this if task payload

  if (T->isConstantMatrixType()) {
    const Type *Ty = Context.getCanonicalType(T).getTypePtr();
    const ConstantMatrixType *MT = cast<ConstantMatrixType>(Ty);
    return llvm::ArrayType::get(ConvertType(MT->getElementType()),
                                MT->getNumRows() * MT->getNumColumns());
  } else if (T->isFloorArgBufferType() || is_task_payload) {
    if (auto io_type = convert_io_type_or_null(T, true); io_type) {
      return io_type;
    }
  } else if (const auto cxx_rdecl = T->getAsCXXRecordDecl();
             cxx_rdecl && opts.vector_compat_conversion && cxx_rdecl->hasAttr<VectorCompatAttr>()) {
    return ConvertType(Context.get_compat_vector_type(cxx_rdecl));
  }

  auto fwd_conv_opts = opts;
  fwd_conv_opts.convert_array_image_or_buffer_type = !ForRecordField;
  llvm::Type *R = ConvertType(T, fwd_conv_opts);
  if (!R) {
    assert(false && "type conversion failed");
    return nullptr;
  }

  // If this is a bool type, or a bit-precise integer type in a bitfield
  // representation, map this integer to the target-specified size.
  if ((ForBitField && T->isBitIntType()) ||
      (!T->isBitIntType() && R->isIntegerTy(1)))
    return llvm::IntegerType::get(getLLVMContext(),
                                  (unsigned)Context.getTypeSize(T));

  // Else, don't map it.
  return R;
}

llvm::Type* CodeGenTypes::convert_io_type_or_null(QualType Ty, bool indirect_io_type_conversion) {
  llvm::Type* llvm_type = nullptr;
  if (const auto cxx_rdecl = Ty->getAsCXXRecordDecl(); cxx_rdecl) {
    llvm_type = CGM.getTypes().getAnyFlattenedType(cxx_rdecl, true);
  } else if (indirect_io_type_conversion && (Ty->isPointerType() || Ty->isReferenceType())) {
    auto pointee_type = Ty->getPointeeType();
    if (const auto cxx_rdecl = pointee_type->getAsCXXRecordDecl(); cxx_rdecl) {
      llvm_type = CGM.getTypes().getAnyFlattenedType(cxx_rdecl, true);
      if (llvm_type) {
        llvm_type = llvm::PointerType::get(llvm_type, CGM.getContext().getTargetAddressSpace(Ty.getAddressSpace()));
      }
    }
  }
  return llvm_type;
}

/// isRecordLayoutComplete - Return true if the specified type is already
/// completely laid out.
bool CodeGenTypes::isRecordLayoutComplete(const Type *Ty) const {
  const auto I = RecordDeclTypes.find(Ty);
  const auto IOI = GraphicsIORecordDeclTypes.find(Ty);
  return (I != RecordDeclTypes.end() && !I->second->isOpaque()) ||
         (IOI != GraphicsIORecordDeclTypes.end() && !IOI->second->isOpaque());
}

static bool
isSafeToConvert(QualType T, CodeGenTypes &CGT,
                llvm::SmallPtrSet<const RecordDecl*, 16> &AlreadyChecked);


/// isSafeToConvert - Return true if it is safe to convert the specified record
/// decl to IR and lay it out, false if doing so would cause us to get into a
/// recursive compilation mess.
static bool
isSafeToConvert(const RecordDecl *RD, CodeGenTypes &CGT,
                llvm::SmallPtrSet<const RecordDecl*, 16> &AlreadyChecked) {
  // If we have already checked this type (maybe the same type is used by-value
  // multiple times in multiple structure fields, don't check again.
  if (!AlreadyChecked.insert(RD).second)
    return true;

  const Type *Key = CGT.getContext().getTagDeclType(RD).getTypePtr();

  // If this type is already laid out, converting it is a noop.
  if (CGT.isRecordLayoutComplete(Key)) return true;

  // If this type is currently being laid out, we can't recursively compile it.
  if (CGT.isRecordBeingLaidOut(Key))
    return false;

  // If this type would require laying out bases that are currently being laid
  // out, don't do it.  This includes virtual base classes which get laid out
  // when a class is translated, even though they aren't embedded by-value into
  // the class.
  if (const CXXRecordDecl *CRD = dyn_cast<CXXRecordDecl>(RD)) {
    for (const auto &I : CRD->bases())
      if (!isSafeToConvert(I.getType()->castAs<RecordType>()->getDecl(), CGT,
                           AlreadyChecked))
        return false;
  }

  // If this type would require laying out members that are currently being laid
  // out, don't do it.
  for (const auto *I : RD->fields())
    if (!isSafeToConvert(I->getType(), CGT, AlreadyChecked))
      return false;

  // If there are no problems, lets do it.
  return true;
}

/// isSafeToConvert - Return true if it is safe to convert this field type,
/// which requires the structure elements contained by-value to all be
/// recursively safe to convert.
static bool
isSafeToConvert(QualType T, CodeGenTypes &CGT,
                llvm::SmallPtrSet<const RecordDecl*, 16> &AlreadyChecked) {
  // Strip off atomic type sugar.
  if (const auto *AT = T->getAs<AtomicType>())
    T = AT->getValueType();

  // If this is a record, check it.
  if (const auto *RT = T->getAs<RecordType>())
    return isSafeToConvert(RT->getDecl(), CGT, AlreadyChecked);

  // If this is an array, check the elements, which are embedded inline.
  if (const auto *AT = CGT.getContext().getAsArrayType(T))
    return isSafeToConvert(AT->getElementType(), CGT, AlreadyChecked);

  // Otherwise, there is no concern about transforming this.  We only care about
  // things that are contained by-value in a structure that can have another
  // structure as a member.
  return true;
}


/// isSafeToConvert - Return true if it is safe to convert the specified record
/// decl to IR and lay it out, false if doing so would cause us to get into a
/// recursive compilation mess.
static bool isSafeToConvert(const RecordDecl *RD, CodeGenTypes &CGT) {
  // If no structs are being laid out, we can certainly do this one.
  if (CGT.noRecordsBeingLaidOut()) return true;

  llvm::SmallPtrSet<const RecordDecl*, 16> AlreadyChecked;
  return isSafeToConvert(RD, CGT, AlreadyChecked);
}

/// isFuncParamTypeConvertible - Return true if the specified type in a
/// function parameter or result position can be converted to an IR type at this
/// point.  This boils down to being whether it is complete, as well as whether
/// we've temporarily deferred expanding the type because we're in a recursive
/// context.
bool CodeGenTypes::isFuncParamTypeConvertible(QualType Ty) {
  // Some ABIs cannot have their member pointers represented in IR unless
  // certain circumstances have been reached.
  if (const auto *MPT = Ty->getAs<MemberPointerType>())
    return getCXXABI().isMemberPointerConvertible(MPT);

  // If this isn't a tagged type, we can convert it!
  const TagType *TT = Ty->getAs<TagType>();
  if (!TT) return true;

  // Incomplete types cannot be converted.
  if (TT->isIncompleteType())
    return false;

  // If this is an enum, then it is always safe to convert.
  const RecordType *RT = dyn_cast<RecordType>(TT);
  if (!RT) return true;

  // Otherwise, we have to be careful.  If it is a struct that we're in the
  // process of expanding, then we can't convert the function type.  That's ok
  // though because we must be in a pointer context under the struct, so we can
  // just convert it to a dummy type.
  //
  // We decide this by checking whether ConvertRecordDeclType returns us an
  // opaque type for a struct that we know is defined.
  return isSafeToConvert(RT->getDecl(), *this);
}


/// Code to verify a given function type is complete, i.e. the return type
/// and all of the parameter types are complete.  Also check to see if we are in
/// a RS_StructPointer context, and if so whether any struct types have been
/// pended.  If so, we don't want to ask the ABI lowering code to handle a type
/// that cannot be converted to an IR type.
bool CodeGenTypes::isFuncTypeConvertible(const FunctionType *FT) {
  if (!isFuncParamTypeConvertible(FT->getReturnType()))
    return false;

  if (const FunctionProtoType *FPT = dyn_cast<FunctionProtoType>(FT))
    for (unsigned i = 0, e = FPT->getNumParams(); i != e; i++)
      if (!isFuncParamTypeConvertible(FPT->getParamType(i)))
        return false;

  return true;
}

/// UpdateCompletedType - When we find the full definition for a TagDecl,
/// replace the 'opaque' type we previously made for it if applicable.
void CodeGenTypes::UpdateCompletedType(const TagDecl *TD) {
  // If this is an enum being completed, then we flush all non-struct types from
  // the cache.  This allows function types and other things that may be derived
  // from the enum to be recomputed.
  if (const EnumDecl *ED = dyn_cast<EnumDecl>(TD)) {
    // Only flush the cache if we've actually already converted this type.
    if (TypeCache.count(ED->getTypeForDecl())) {
      // Okay, we formed some types based on this.  We speculated that the enum
      // would be lowered to i32, so we only need to flush the cache if this
      // didn't happen.
      if (!ConvertType(ED->getIntegerType())->isIntegerTy(32))
        TypeCache.clear();
    }
    // If necessary, provide the full definition of a type only used with a
    // declaration so far.
    if (CGDebugInfo *DI = CGM.getModuleDebugInfo())
      DI->completeType(ED);
    return;
  }

  // If we completed a RecordDecl that we previously used and converted to an
  // anonymous type, then go ahead and complete it now.
  const RecordDecl *RD = cast<RecordDecl>(TD);
  if (RD->isDependentType()) return;

  // Only complete it if we converted it already.  If we haven't converted it
  // yet, we'll just do it lazily.
  if (RecordDeclTypes.count(Context.getTagDeclType(RD).getTypePtr()) ||
      GraphicsIORecordDeclTypes.count(Context.getTagDeclType(RD).getTypePtr()))
    ConvertRecordDeclType(RD);

  // If necessary, provide the full definition of a type only used with a
  // declaration so far.
  if (CGDebugInfo *DI = CGM.getModuleDebugInfo())
    DI->completeType(RD);
}

void CodeGenTypes::RefreshTypeCacheForClass(const CXXRecordDecl *RD) {
  QualType T = Context.getRecordType(RD);
  T = Context.getCanonicalType(T);

  const Type *Ty = T.getTypePtr();
  if (RecordsWithOpaqueMemberPointers.count(Ty)) {
    TypeCache.clear();
    RecordsWithOpaqueMemberPointers.clear();
  }
}

static llvm::Type *getTypeForFormat(llvm::LLVMContext &VMContext,
                                    const llvm::fltSemantics &format,
                                    bool UseNativeHalf = false) {
  if (&format == &llvm::APFloat::IEEEhalf()) {
    if (UseNativeHalf)
      return llvm::Type::getHalfTy(VMContext);
    else
      return llvm::Type::getInt16Ty(VMContext);
  }
  if (&format == &llvm::APFloat::BFloat())
    return llvm::Type::getBFloatTy(VMContext);
  if (&format == &llvm::APFloat::IEEEsingle())
    return llvm::Type::getFloatTy(VMContext);
  if (&format == &llvm::APFloat::IEEEdouble())
    return llvm::Type::getDoubleTy(VMContext);
  if (&format == &llvm::APFloat::IEEEquad())
    return llvm::Type::getFP128Ty(VMContext);
  if (&format == &llvm::APFloat::PPCDoubleDouble())
    return llvm::Type::getPPC_FP128Ty(VMContext);
  if (&format == &llvm::APFloat::x87DoubleExtended())
    return llvm::Type::getX86_FP80Ty(VMContext);
  llvm_unreachable("Unknown float format!");
}

llvm::Type *CodeGenTypes::ConvertFunctionTypeInternal(QualType QFT) {
  assert(QFT.isCanonical());
  const Type *Ty = QFT.getTypePtr();
  const FunctionType *FT = cast<FunctionType>(QFT.getTypePtr());
  // First, check whether we can build the full function type.  If the
  // function type depends on an incomplete type (e.g. a struct or enum), we
  // cannot lower the function type.
  if (!isFuncTypeConvertible(FT)) {
    // This function's type depends on an incomplete tag type.

    // Force conversion of all the relevant record types, to make sure
    // we re-convert the FunctionType when appropriate.
    if (const RecordType *RT = FT->getReturnType()->getAs<RecordType>())
      ConvertRecordDeclType(RT->getDecl());
    if (const FunctionProtoType *FPT = dyn_cast<FunctionProtoType>(FT))
      for (unsigned i = 0, e = FPT->getNumParams(); i != e; i++)
        if (const RecordType *RT = FPT->getParamType(i)->getAs<RecordType>())
          ConvertRecordDeclType(RT->getDecl());

    SkippedLayout = true;

    // Return a placeholder type.
    return llvm::StructType::get(getLLVMContext());
  }

  // While we're converting the parameter types for a function, we don't want
  // to recursively convert any pointed-to structs.  Converting directly-used
  // structs is ok though.
  if (!RecordsBeingLaidOut.insert(Ty).second) {
    SkippedLayout = true;
    return llvm::StructType::get(getLLVMContext());
  }

  // The function type can be built; call the appropriate routines to
  // build it.
  const CGFunctionInfo *FI;
  if (const FunctionProtoType *FPT = dyn_cast<FunctionProtoType>(FT)) {
    FI = &arrangeFreeFunctionType(
        CanQual<FunctionProtoType>::CreateUnsafe(QualType(FPT, 0)));
  } else {
    const FunctionNoProtoType *FNPT = cast<FunctionNoProtoType>(FT);
    FI = &arrangeFreeFunctionType(
        CanQual<FunctionNoProtoType>::CreateUnsafe(QualType(FNPT, 0)));
  }

  llvm::Type *ResultType = nullptr;
  // If there is something higher level prodding our CGFunctionInfo, then
  // don't recurse into it again.
  if (FunctionsBeingProcessed.count(FI)) {

    ResultType = llvm::StructType::get(getLLVMContext());
    SkippedLayout = true;
  } else {

    // Otherwise, we're good to go, go ahead and convert it.
    ResultType = GetFunctionType(*FI);
  }

  RecordsBeingLaidOut.erase(Ty);

  if (RecordsBeingLaidOut.empty())
    while (!DeferredRecords.empty())
      ConvertRecordDeclType(DeferredRecords.pop_back_val());
  return ResultType;
}

/// ConvertType - Convert the specified type to its LLVM form.
llvm::Type *CodeGenTypes::ConvertType(QualType T, type_conversion_opts_t opts) {
  T = Context.getCanonicalType(T);

  const Type *Ty = T.getTypePtr();

  // NOTE: any custom handling must be done *prior* to any cached type handling
  //       also: don't cache anything if this is an I/O or vector compat conversion
  const auto skip_cache = (opts.io_type_conversion || opts.vector_compat_conversion);

  // intercept image arrays before RT conversion
  // NOTE: we do not want this when this is part of a record/struct (default single_field_array_image_or_buffer_only == true)
  if (opts.convert_array_image_or_buffer_type && Ty->isArrayImageType(opts.single_field_array_image_or_buffer_only)) {
    return ConvertArrayImageType(Ty);
  }

  // same thing for buffer arrays (Vulkan only)
  if (Context.getLangOpts().Vulkan && opts.convert_array_image_or_buffer_type && Ty->isArrayBufferType()) {
    return ConvertArrayBufferType(Ty);
  }

  // and for arrays if specified
  if (opts.convert_array_type && Ty->isAggregateArrayType()) {
    return ConvertArrayType(Ty);
  }

  // handle vector compat conversions
  if (opts.vector_compat_conversion) {
    if (const auto cxx_rdecl = T->getAsCXXRecordDecl(); cxx_rdecl && cxx_rdecl->hasAttr<VectorCompatAttr>()) {
      return ConvertType(Context.get_compat_vector_type(cxx_rdecl));
    }
  }

  // For the device-side compilation, CUDA device builtin surface/texture types
  // may be represented in different types.
  if (Context.getLangOpts().CUDAIsDevice) {
    if (T->isCUDADeviceBuiltinSurfaceType()) {
      if (auto *Ty = CGM.getTargetCodeGenInfo()
                         .getCUDADeviceBuiltinSurfaceDeviceType())
        return Ty;
    } else if (T->isCUDADeviceBuiltinTextureType()) {
      if (auto *Ty = CGM.getTargetCodeGenInfo()
                         .getCUDADeviceBuiltinTextureDeviceType())
        return Ty;
    }
  }

  // RecordTypes are cached and processed specially.
  if (const RecordType *RT = dyn_cast<RecordType>(Ty))
    return ConvertRecordDeclType(RT->getDecl(), opts);

  // The LLVM type we return for a given Clang type may not always be the same,
  // most notably when dealing with recursive structs. We mark these potential
  // cases with ShouldUseCache below. Builtin types cannot be recursive.
  // TODO: when clang uses LLVM opaque pointers we won't be able to represent
  // recursive types with LLVM types, making this logic much simpler.
  llvm::Type *CachedType = nullptr;
  bool ShouldUseCache =
      !skip_cache &&
      (Ty->isBuiltinType() ||
       (noRecordsBeingLaidOut() && FunctionsBeingProcessed.empty()));
  if (ShouldUseCache) {
    llvm::DenseMap<const Type *, llvm::Type *>::iterator TCI =
        TypeCache.find(Ty);
    if (TCI != TypeCache.end())
      CachedType = TCI->second;
    if (CachedType) {
#ifndef NDEBUG
      if (!VerifyTypeCache)
        return CachedType;
#else
      return CachedType;
#endif
    }
  }

  // If we don't have it in the cache, convert it now.
  llvm::Type *ResultType = nullptr;
  switch (Ty->getTypeClass()) {
  case Type::Record: // Handled above.
#define TYPE(Class, Base)
#define ABSTRACT_TYPE(Class, Base)
#define NON_CANONICAL_TYPE(Class, Base) case Type::Class:
#define DEPENDENT_TYPE(Class, Base) case Type::Class:
#define NON_CANONICAL_UNLESS_DEPENDENT_TYPE(Class, Base) case Type::Class:
#include "clang/AST/TypeNodes.inc"
    llvm_unreachable("Non-canonical or dependent types aren't possible.");

  case Type::Builtin: {
    switch (cast<BuiltinType>(Ty)->getKind()) {
    case BuiltinType::Void:
    case BuiltinType::ObjCId:
    case BuiltinType::ObjCClass:
    case BuiltinType::ObjCSel:
      // LLVM void type can only be used as the result of a function call.  Just
      // map to the same as char.
      ResultType = llvm::Type::getInt8Ty(getLLVMContext());
      break;

    case BuiltinType::Bool:
      // Note that we always return bool as i1 for use as a scalar type.
      ResultType = llvm::Type::getInt1Ty(getLLVMContext());
      break;

    case BuiltinType::Char_S:
    case BuiltinType::Char_U:
    case BuiltinType::SChar:
    case BuiltinType::UChar:
    case BuiltinType::Short:
    case BuiltinType::UShort:
    case BuiltinType::Int:
    case BuiltinType::UInt:
    case BuiltinType::Long:
    case BuiltinType::ULong:
    case BuiltinType::LongLong:
    case BuiltinType::ULongLong:
    case BuiltinType::WChar_S:
    case BuiltinType::WChar_U:
    case BuiltinType::Char8:
    case BuiltinType::Char16:
    case BuiltinType::Char32:
    case BuiltinType::ShortAccum:
    case BuiltinType::Accum:
    case BuiltinType::LongAccum:
    case BuiltinType::UShortAccum:
    case BuiltinType::UAccum:
    case BuiltinType::ULongAccum:
    case BuiltinType::ShortFract:
    case BuiltinType::Fract:
    case BuiltinType::LongFract:
    case BuiltinType::UShortFract:
    case BuiltinType::UFract:
    case BuiltinType::ULongFract:
    case BuiltinType::SatShortAccum:
    case BuiltinType::SatAccum:
    case BuiltinType::SatLongAccum:
    case BuiltinType::SatUShortAccum:
    case BuiltinType::SatUAccum:
    case BuiltinType::SatULongAccum:
    case BuiltinType::SatShortFract:
    case BuiltinType::SatFract:
    case BuiltinType::SatLongFract:
    case BuiltinType::SatUShortFract:
    case BuiltinType::SatUFract:
    case BuiltinType::SatULongFract:
      ResultType = llvm::IntegerType::get(getLLVMContext(),
                                 static_cast<unsigned>(Context.getTypeSize(T)));
      break;

    case BuiltinType::Float16:
      ResultType =
          getTypeForFormat(getLLVMContext(), Context.getFloatTypeSemantics(T),
                           /* UseNativeHalf = */ true);
      break;

    case BuiltinType::Half:
      // Half FP can either be storage-only (lowered to i16) or native.
      ResultType = getTypeForFormat(
          getLLVMContext(), Context.getFloatTypeSemantics(T),
          Context.getLangOpts().NativeHalfType ||
              !Context.getTargetInfo().useFP16ConversionIntrinsics());
      break;
    case BuiltinType::BFloat16:
    case BuiltinType::Float:
    case BuiltinType::Double:
    case BuiltinType::LongDouble:
    case BuiltinType::Float128:
    case BuiltinType::Ibm128:
      ResultType = getTypeForFormat(getLLVMContext(),
                                    Context.getFloatTypeSemantics(T),
                                    /* UseNativeHalf = */ false);
      break;

    case BuiltinType::NullPtr:
      // Model std::nullptr_t as i8*
      ResultType = llvm::Type::getInt8PtrTy(getLLVMContext());
      break;

    case BuiltinType::UInt128:
    case BuiltinType::Int128:
      ResultType = llvm::IntegerType::get(getLLVMContext(), 128);
      break;

#define IMAGE_TYPE(ImgType, Id, SingletonId, Access, Suffix) \
    case BuiltinType::Id:
#include "clang/Basic/OpenCLImageTypes.def"
#define EXT_OPAQUE_TYPE(ExtType, Id, Ext) \
    case BuiltinType::Id:
#include "clang/Basic/OpenCLExtensionTypes.def"
    case BuiltinType::OCLSampler:
    case BuiltinType::OCLEvent:
    case BuiltinType::OCLClkEvent:
    case BuiltinType::OCLQueue:
    case BuiltinType::OCLReserveID:
    case BuiltinType::OCLPatchControlPoint:
    case BuiltinType::OCLMesh:
    case BuiltinType::OCLMeshGridProperties:
      ResultType = CGM.getOpenCLRuntime().convertOpenCLSpecificType(Ty);
      break;
    case BuiltinType::SveInt8:
    case BuiltinType::SveUint8:
    case BuiltinType::SveInt8x2:
    case BuiltinType::SveUint8x2:
    case BuiltinType::SveInt8x3:
    case BuiltinType::SveUint8x3:
    case BuiltinType::SveInt8x4:
    case BuiltinType::SveUint8x4:
    case BuiltinType::SveInt16:
    case BuiltinType::SveUint16:
    case BuiltinType::SveInt16x2:
    case BuiltinType::SveUint16x2:
    case BuiltinType::SveInt16x3:
    case BuiltinType::SveUint16x3:
    case BuiltinType::SveInt16x4:
    case BuiltinType::SveUint16x4:
    case BuiltinType::SveInt32:
    case BuiltinType::SveUint32:
    case BuiltinType::SveInt32x2:
    case BuiltinType::SveUint32x2:
    case BuiltinType::SveInt32x3:
    case BuiltinType::SveUint32x3:
    case BuiltinType::SveInt32x4:
    case BuiltinType::SveUint32x4:
    case BuiltinType::SveInt64:
    case BuiltinType::SveUint64:
    case BuiltinType::SveInt64x2:
    case BuiltinType::SveUint64x2:
    case BuiltinType::SveInt64x3:
    case BuiltinType::SveUint64x3:
    case BuiltinType::SveInt64x4:
    case BuiltinType::SveUint64x4:
    case BuiltinType::SveBool:
    case BuiltinType::SveFloat16:
    case BuiltinType::SveFloat16x2:
    case BuiltinType::SveFloat16x3:
    case BuiltinType::SveFloat16x4:
    case BuiltinType::SveFloat32:
    case BuiltinType::SveFloat32x2:
    case BuiltinType::SveFloat32x3:
    case BuiltinType::SveFloat32x4:
    case BuiltinType::SveFloat64:
    case BuiltinType::SveFloat64x2:
    case BuiltinType::SveFloat64x3:
    case BuiltinType::SveFloat64x4:
    case BuiltinType::SveBFloat16:
    case BuiltinType::SveBFloat16x2:
    case BuiltinType::SveBFloat16x3:
    case BuiltinType::SveBFloat16x4: {
      ASTContext::BuiltinVectorTypeInfo Info =
          Context.getBuiltinVectorTypeInfo(cast<BuiltinType>(Ty));
      return llvm::ScalableVectorType::get(ConvertType(Info.ElementType),
                                           Info.EC.getKnownMinValue() *
                                               Info.NumVectors);
    }
#define PPC_VECTOR_TYPE(Name, Id, Size) \
    case BuiltinType::Id: \
      ResultType = \
        llvm::FixedVectorType::get(ConvertType(Context.BoolTy), Size); \
      break;
#include "clang/Basic/PPCTypes.def"
#define RVV_TYPE(Name, Id, SingletonId) case BuiltinType::Id:
#include "clang/Basic/RISCVVTypes.def"
    {
      ASTContext::BuiltinVectorTypeInfo Info =
          Context.getBuiltinVectorTypeInfo(cast<BuiltinType>(Ty));
      return llvm::ScalableVectorType::get(ConvertType(Info.ElementType),
                                           Info.EC.getKnownMinValue() *
                                           Info.NumVectors);
    }
   case BuiltinType::Dependent:
#define BUILTIN_TYPE(Id, SingletonId)
#define PLACEHOLDER_TYPE(Id, SingletonId) \
    case BuiltinType::Id:
#include "clang/AST/BuiltinTypes.def"
      llvm_unreachable("Unexpected placeholder builtin type!");
    }
    break;
  }
  case Type::Auto:
  case Type::DeducedTemplateSpecialization:
    llvm_unreachable("Unexpected undeduced type!");
  case Type::Complex: {
    llvm::Type *EltTy = ConvertType(cast<ComplexType>(Ty)->getElementType());
    ResultType = llvm::StructType::get(EltTy, EltTy);
    break;
  }
  case Type::LValueReference:
  case Type::RValueReference: {
    const ReferenceType *RTy = cast<ReferenceType>(Ty);
    QualType ETy = RTy->getPointeeType();
    llvm::Type *PointeeType = ConvertTypeForMem(ETy);
    unsigned AS = Context.getTargetAddressSpace(ETy);
    ResultType = llvm::PointerType::get(PointeeType, AS);
    break;
  }
  case Type::Pointer: {
    const PointerType *PTy = cast<PointerType>(Ty);
    QualType ETy = PTy->getPointeeType();
    llvm::Type *PointeeType = ConvertTypeForMem(ETy);
    if (PointeeType->isVoidTy())
      PointeeType = llvm::Type::getInt8Ty(getLLVMContext());
    unsigned AS = Context.getTargetAddressSpace(ETy);
    ResultType = llvm::PointerType::get(PointeeType, AS);
    break;
  }

  case Type::VariableArray: {
    const VariableArrayType *A = cast<VariableArrayType>(Ty);
    assert(A->getIndexTypeCVRQualifiers() == 0 &&
           "FIXME: We only handle trivial array types so far!");
    // VLAs resolve to the innermost element type; this matches
    // the return of alloca, and there isn't any obviously better choice.
    ResultType = ConvertTypeForMem(A->getElementType());
    break;
  }
  case Type::IncompleteArray: {
    const IncompleteArrayType *A = cast<IncompleteArrayType>(Ty);
    assert(A->getIndexTypeCVRQualifiers() == 0 &&
           "FIXME: We only handle trivial array types so far!");
    // int X[] -> [0 x int], unless the element type is not sized.  If it is
    // unsized (e.g. an incomplete struct) just use [0 x i8].
    ResultType = ConvertTypeForMem(A->getElementType());
    if (!ResultType->isSized()) {
      SkippedLayout = true;
      ResultType = llvm::Type::getInt8Ty(getLLVMContext());
    }
    ResultType = llvm::ArrayType::get(ResultType, 0);
    break;
  }
  case Type::ConstantArray: {
    const ConstantArrayType *A = cast<ConstantArrayType>(Ty);
    llvm::Type *EltTy = ConvertTypeForMem(A->getElementType(), false, false, opts);

    // Lower arrays of undefined struct type to arrays of i8 just to have a
    // concrete type.
    if (!EltTy->isSized()) {
      SkippedLayout = true;
      EltTy = llvm::Type::getInt8Ty(getLLVMContext());
    }

    ResultType = llvm::ArrayType::get(EltTy, A->getSize().getZExtValue());
    break;
  }
  case Type::ExtVector:
  case Type::Vector: {
    const VectorType *VT = cast<VectorType>(Ty);
    ResultType = llvm::FixedVectorType::get(ConvertType(VT->getElementType()),
                                            VT->getNumElements());
    break;
  }
  case Type::ConstantMatrix: {
    const ConstantMatrixType *MT = cast<ConstantMatrixType>(Ty);
    ResultType =
        llvm::FixedVectorType::get(ConvertType(MT->getElementType()),
                                   MT->getNumRows() * MT->getNumColumns());
    break;
  }
  case Type::FunctionNoProto:
  case Type::FunctionProto:
    ResultType = ConvertFunctionTypeInternal(T);
    break;
  case Type::ObjCObject:
    ResultType = ConvertType(cast<ObjCObjectType>(Ty)->getBaseType());
    break;

  case Type::ObjCInterface: {
    // Objective-C interfaces are always opaque (outside of the
    // runtime, which can do whatever it likes); we never refine
    // these.
    llvm::Type *&T = InterfaceTypes[cast<ObjCInterfaceType>(Ty)];
    if (!T)
      T = llvm::StructType::create(getLLVMContext());
    ResultType = T;
    break;
  }

  case Type::ObjCObjectPointer: {
    // Protocol qualifications do not influence the LLVM type, we just return a
    // pointer to the underlying interface type. We don't need to worry about
    // recursive conversion.
    llvm::Type *T =
      ConvertTypeForMem(cast<ObjCObjectPointerType>(Ty)->getPointeeType());
    ResultType = T->getPointerTo();
    break;
  }

  case Type::Enum: {
    const EnumDecl *ED = cast<EnumType>(Ty)->getDecl();
    if (ED->isCompleteDefinition() || ED->isFixed())
      return ConvertType(ED->getIntegerType());
    // Return a placeholder 'i32' type.  This can be changed later when the
    // type is defined (see UpdateCompletedType), but is likely to be the
    // "right" answer.
    ResultType = llvm::Type::getInt32Ty(getLLVMContext());
    break;
  }

  case Type::BlockPointer: {
    const QualType FTy = cast<BlockPointerType>(Ty)->getPointeeType();
    llvm::Type *PointeeType = CGM.getLangOpts().OpenCL
                                  ? CGM.getGenericBlockLiteralType()
                                  : ConvertTypeForMem(FTy);
    // Block pointers lower to function type. For function type,
    // getTargetAddressSpace() returns default address space for
    // function pointer i.e. program address space. Therefore, for block
    // pointers, it is important to pass qualifiers when calling
    // getTargetAddressSpace(), to ensure that we get the address space
    // for data pointers and not function pointers.
    unsigned AS = Context.getTargetAddressSpace(FTy.getQualifiers());
    ResultType = llvm::PointerType::get(PointeeType, AS);
    break;
  }

  case Type::MemberPointer: {
    auto *MPTy = cast<MemberPointerType>(Ty);
    if (!getCXXABI().isMemberPointerConvertible(MPTy)) {
      RecordsWithOpaqueMemberPointers.insert(MPTy->getClass());
      ResultType = llvm::StructType::create(getLLVMContext());
    } else {
      ResultType = getCXXABI().ConvertMemberPointerType(MPTy);
    }
    break;
  }

  case Type::Atomic: {
    QualType valueType = cast<AtomicType>(Ty)->getValueType();
    ResultType = ConvertTypeForMem(valueType);

    // Pad out to the inflated size if necessary.
    uint64_t valueSize = Context.getTypeSize(valueType);
    uint64_t atomicSize = Context.getTypeSize(Ty);
    if (valueSize != atomicSize) {
      assert(valueSize < atomicSize);
      llvm::Type *elts[] = {
        ResultType,
        llvm::ArrayType::get(CGM.Int8Ty, (atomicSize - valueSize) / 8)
      };
      ResultType = llvm::StructType::get(getLLVMContext(),
                                         llvm::makeArrayRef(elts));
    }
    break;
  }
  case Type::Pipe: {
    ResultType = CGM.getOpenCLRuntime().getPipeType(cast<PipeType>(Ty));
    break;
  }
  case Type::BitInt: {
    const auto &EIT = cast<BitIntType>(Ty);
    ResultType = llvm::Type::getIntNTy(getLLVMContext(), EIT->getNumBits());
    break;
  }
  }

  assert(ResultType && "Didn't convert a type?");

#ifndef NDEBUG
  if (CachedType) {
    assert(CachedType == ResultType &&
           "Cached type doesn't match computed type");
  }
#endif

  if (ShouldUseCache)
    TypeCache[Ty] = ResultType;
  return ResultType;
}

bool CodeGenModule::isPaddedAtomicType(QualType type) {
  return isPaddedAtomicType(type->castAs<AtomicType>());
}

bool CodeGenModule::isPaddedAtomicType(const AtomicType *type) {
  return Context.getTypeSize(type) != Context.getTypeSize(type->getValueType());
}

/// image type cache for structs with 1 image field and 2 image fields (used below)
static std::unordered_map<COMPUTE_IMAGE_TYPE, llvm::StructType*> image_type_cache_1if;
static std::unordered_map<COMPUTE_IMAGE_TYPE, llvm::StructType*> image_type_cache_2if;

/// we need to ensure that image-based types are always unique (for Metal)
/// -> if this RecordDecl contains an opaque (image) type, check the existing cache of converted image types and
///    set Entry to a matching StructType if there is one + return { true-if-new, cache-func } if this is an image type
static std::pair<bool, std::function<void(llvm::StructType*)>>
handle_image_rdecl(ASTContext& Context, const RecordDecl *RD, llvm::StructType *&Entry) {
	if (Entry || !Context.getLangOpts().Metal) {
		return { false, {} };
	}
	
	auto def = RD->getDefinition();
	if (!def || !def->isCompleteDefinition()) {
		return { false, {} };
	}
	
	auto pot_img_def = dyn_cast_or_null<ClassTemplateSpecializationDecl>(def);
	if (!pot_img_def) {
		return { false, {} };
	}
	
	bool is_all_image_fields = true;
	uint32_t active_field_count = 0;
	for (auto field : def->fields()) {
		if (field->isZeroSize(RD->getASTContext())) {
			continue;
		}
		++active_field_count;
		if (!field->getType()->isImageType() &&
			!field->getType()->isArrayImageType(true)) {
			is_all_image_fields = false;
			break;
		}
	}
	if (!is_all_image_fields || active_field_count < 1 || active_field_count > 2) {
		// must have either 1 or 2 image fields, and all fields must be image types
		return { false, {} };
	}
	
	const auto& templ_args = pot_img_def->getTemplateArgs();
	if (templ_args.size() == 0) {
		return { false, {} };
	}
	const auto& arg_0 = templ_args.get(0);
	if (arg_0.getKind() != TemplateArgument::Integral) {
		return { false, {} };
	}
	
	// mask/bits that we need to identify an opaque image type
	static constexpr const COMPUTE_IMAGE_TYPE opaque_image_mask {
		COMPUTE_IMAGE_TYPE::__DIM_MASK |
		COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
		COMPUTE_IMAGE_TYPE::FLAG_ARRAY |
		COMPUTE_IMAGE_TYPE::FLAG_BUFFER |
		COMPUTE_IMAGE_TYPE::FLAG_CUBE |
		COMPUTE_IMAGE_TYPE::FLAG_MSAA
	};
	const auto image_type = (COMPUTE_IMAGE_TYPE)arg_0.getAsIntegral().getZExtValue();
	const auto masked_image_type = image_type & opaque_image_mask;
	
	// check if we have a cached entry, return it if so
	std::unordered_map<COMPUTE_IMAGE_TYPE, llvm::StructType*>& cache =
		(active_field_count == 1 ? image_type_cache_1if : image_type_cache_2if);
	auto iter = cache.find(masked_image_type);
	if (iter != cache.end()) {
		Entry = iter->second;
		return { true, {} };
	}
	
	// this is a new image type, provide cache function
	std::function<void(llvm::StructType*)> cache_func = [masked_image_type, active_field_count](llvm::StructType* type) {
		std::unordered_map<COMPUTE_IMAGE_TYPE, llvm::StructType*>& cache =
			(active_field_count == 1 ? image_type_cache_1if : image_type_cache_2if);
		cache.emplace(masked_image_type, type);
	};
	
	return { true, std::move(cache_func) };
}

/// ConvertRecordDeclType - Lay out a tagged decl type like struct or union.
llvm::StructType *CodeGenTypes::ConvertRecordDeclType(const RecordDecl *RD, type_conversion_opts_t opts) {
  // TagDecl's are not necessarily unique, instead use the (clang)
  // type connected to the decl.
  const Type *Key = Context.getTagDeclType(RD).getTypePtr();

  llvm::StructType *&Entry = (!opts.io_type_conversion ? RecordDeclTypes[Key] : GraphicsIORecordDeclTypes[Key]);

  // we need to ensure that image-based types are always unique (for Metal)
  auto [is_new_image_rdecl, cache_func] = handle_image_rdecl(Context, RD, Entry);

  // If we don't have a StructType at all yet, create the forward declaration.
  if (!Entry) {
    Entry = llvm::StructType::create(getLLVMContext());
    addRecordTypeName(RD, Entry, "", opts.io_type_conversion);
    if (opts.io_type_conversion) {
      Entry->setGraphicsIOType();
    }
  }
  llvm::StructType *Ty = Entry;

  // If this is still a forward declaration, or the LLVM type is already
  // complete, there's nothing more to do.
  RD = RD->getDefinition();
  if (!RD || !RD->isCompleteDefinition() || (!Ty->isOpaque() && !is_new_image_rdecl))
    return Ty;

  // If converting this type would cause us to infinitely loop, don't do it!
  if (!isSafeToConvert(RD, *this)) {
    DeferredRecords.push_back(RD);
    return Ty;
  }

  // Okay, this is a definition of a type.  Compile the implementation now.
  bool InsertResult = RecordsBeingLaidOut.insert(Key).second;
  (void)InsertResult;
  assert(InsertResult && "Recursively compiling a struct?");

  // Force conversion of non-virtual base classes recursively.
  if (const CXXRecordDecl *CRD = dyn_cast<CXXRecordDecl>(RD)) {
    for (const auto &I : CRD->bases()) {
      if (I.isVirtual()) continue;
      ConvertRecordDeclType(I.getType()->castAs<RecordType>()->getDecl(), opts);
    }
  }

  // Layout fields.
  std::unique_ptr<CGRecordLayout> Layout = ComputeRecordLayout(RD, Ty, opts);
  CGRecordLayouts[Key] = std::move(Layout);

  // We're done laying out this struct.
  bool EraseResult = RecordsBeingLaidOut.erase(Key); (void)EraseResult;
  assert(EraseResult && "struct not in RecordsBeingLaidOut set?");

  // If this struct blocked a FunctionType conversion, then recompute whatever
  // was derived from that.
  // FIXME: This is hugely overconservative.
  if (SkippedLayout)
    TypeCache.clear();

  // If we're done converting the outer-most record, then convert any deferred
  // structs as well.
  if (RecordsBeingLaidOut.empty())
    while (!DeferredRecords.empty())
      ConvertRecordDeclType(DeferredRecords.pop_back_val(), opts);

  // cache the image type
  if (is_new_image_rdecl && cache_func) {
    cache_func(Ty);
  }

  return Ty;
}

llvm::Type *CodeGenTypes::ConvertArrayImageType(const Type* Ty) {
  // ptr to array of images
  if(Ty->isPointerType() &&
     Ty->getPointeeType()->isArrayType() &&
     Ty->getPointeeType()->getArrayElementTypeNoTypeQual()->isImageType()) {
    return llvm::PointerType::get(ConvertArrayImageType(Ty->getPointeeType().getTypePtr()), 0);
  }
	
  // simple C-style array that contains an image type
  if (Ty->isArrayType()) {
    const auto array_elem_type_no_qual = Ty->getArrayElementTypeNoTypeQual();
    if (array_elem_type_no_qual &&
        (array_elem_type_no_qual->isImageType() ||
         array_elem_type_no_qual->isAggregateImageType())) {
      const ConstantArrayType *CAT = Context.getAsConstantArrayType(QualType(Ty, 0));
      const auto elem_type = CAT->getElementType();
      if (elem_type->isImageType()) {
        return llvm::ArrayType::get(ConvertType(elem_type), CAT->getSize().getZExtValue());
      } else if (elem_type->isAggregateImageType()) {
        // must be an aggregate image with exactly one image
        const auto agg_img_type = elem_type->getAsCXXRecordDecl();
        auto agg_img_fields = get_aggregate_scalar_fields(agg_img_type, agg_img_type, false, false, false, true /* TODO */, true /* TODO */);
        if (agg_img_fields.size() != 1) return nullptr;
        return llvm::ArrayType::get(ConvertType(agg_img_fields[0].type), CAT->getSize().getZExtValue());
      }
    }
    assert(false && "invalid array of images type");
  }

  // must be struct or class, union is not allowed
  if(!Ty->isStructureOrClassType()) return nullptr;

  // must be a cxx rdecl
  const auto decl = Ty->getAsCXXRecordDecl();
  if(!decl) return nullptr;

  // must have definition
  if(!decl->hasDefinition()) return nullptr;

  // must have exactly one field
  const auto field_count = std::distance(decl->field_begin(), decl->field_end());
  if(field_count != 1) return nullptr;

  // field must be an array
  const QualType arr_field_type = decl->field_begin()->getType();
  const ConstantArrayType *CAT = Context.getAsConstantArrayType(arr_field_type);
  if(!CAT) return nullptr;

  // must be an aggregate image with exactly one image
  const auto agg_img_type = CAT->getElementType()->getAsCXXRecordDecl();
  auto agg_img_fields = get_aggregate_scalar_fields(agg_img_type, agg_img_type, false, false, false,
                                                    true /* TODO */, true /* TODO */);
  if(agg_img_fields.size() != 1) return nullptr;

  // got everything we need
  return llvm::ArrayType::get(ConvertType(agg_img_fields[0].type), CAT->getSize().getZExtValue());
}

template <bool is_array_of_buffers>
static llvm::Type* ConvertArrayType(const Type* Ty, CodeGenTypes& CGT, ASTContext& Context) {
  // ptr to array of buffer
  if constexpr (is_array_of_buffers) {
    if (Ty->isPointerType() &&
        Ty->getPointeeType()->isArrayType() &&
        Ty->getPointeeType()->getAsArrayTypeUnsafe()->getElementType()->isPointerType() &&
        Ty->getPointeeType()->getAsArrayTypeUnsafe()->getElementType()->getPointeeType().getAddressSpace() != LangAS::Default) {
      auto converted_type = CGT.ConvertArrayBufferType(Ty->getPointeeType().getTypePtr());
      if (!converted_type) {
        return nullptr;
      }
      return llvm::PointerType::get(converted_type, 0);
    }
  }

  // simple C-style array that contains a buffer
  if (Ty->isArrayType()) {
    const auto array_elem_type = Ty->getAsArrayTypeUnsafe()->getElementType();
    if constexpr (is_array_of_buffers) {
      if (!array_elem_type->isPointerType() || array_elem_type->getPointeeType().getAddressSpace() == LangAS::Default) {
        assert(false && "invalid array of buffers type");
        return nullptr;
      }
    }

    const ConstantArrayType *CAT = Context.getAsConstantArrayType(QualType(Ty, 0));
    const type_conversion_opts_t conv_opts {
      // do not recursively convert this
      .convert_array_image_or_buffer_type = false,
      .single_field_array_image_or_buffer_only = false,
    };
    return llvm::ArrayType::get(CGT.ConvertType(array_elem_type, conv_opts), CAT->getSize().getZExtValue());
  }

  // must be struct or class, union is not allowed
  if (!Ty->isStructureOrClassType()) return nullptr;

  // must be a cxx rdecl
  const auto decl = Ty->getAsCXXRecordDecl();
  if (!decl) return nullptr;

  // must have definition
  if (!decl->hasDefinition()) return nullptr;

  // must have exactly one field
  const auto field_count = std::distance(decl->field_begin(), decl->field_end());
  // NOTE: contrary to images and aggregate images, we always want this to be a single field
  if (field_count != 1) return nullptr;

  // field must be an array (array of buffers / normal array) or nested type (normal array only)
  const QualType arr_field_type = decl->field_begin()->getType();
  const ConstantArrayType *CAT = Context.getAsConstantArrayType(arr_field_type);
  if (!CAT) {
    if constexpr (!is_array_of_buffers) { // nested
      return ConvertArrayType<false>(arr_field_type.getTypePtr(), CGT, Context);
    }
    return nullptr;
  }

  // element type must be a buffer
  const auto elem_type = arr_field_type->getAsArrayTypeUnsafe()->getElementType();
  if constexpr (is_array_of_buffers) {
    if (!elem_type->isPointerType()) {
      return nullptr;
    }
    if (elem_type->getPointeeType().getAddressSpace() == LangAS::Default) {
      return nullptr;
    }
  }

  // got everything we need
  const type_conversion_opts_t conv_opts {
    // do not recursively convert this
    .convert_array_image_or_buffer_type = false,
    .single_field_array_image_or_buffer_only = false,
  };
  return llvm::ArrayType::get(CGT.ConvertType(elem_type, conv_opts), CAT->getSize().getZExtValue());
}

llvm::Type *CodeGenTypes::ConvertArrayBufferType(const Type* Ty) {
  return ::ConvertArrayType<true>(Ty, *this, Context);
}

llvm::Type *CodeGenTypes::ConvertArrayType(const Type* Ty) {
  return ::ConvertArrayType<false>(Ty, *this, Context);
}

const CodeGenTypes::flattened_entry_t* CodeGenTypes::find_flattened_entry(const RecordDecl* decl,
																		  const llvm::Type* type,
																		  const bool is_arg_buffer) const {
	if (!type) {
		return nullptr;
	}
	
	const auto is_unnamed = (!isa<llvm::StructType>(type) ? false : !cast<llvm::StructType>(type)->hasName());
	
	const auto flat_rec_iter = flattened_records.find(decl);
	if (flat_rec_iter == flattened_records.end()) {
		return nullptr;
	}
	const auto& flat_rec = flat_rec_iter->second;
	
	// check direct match
	if (is_arg_buffer) {
		// can never be unnamed
		if (!is_unnamed && flat_rec.arg_buffer.type == type) {
			return &flat_rec.arg_buffer;
		}
	} else if (is_unnamed && flat_rec.unnamed.type == type) {
		return &flat_rec.unnamed;
	} else if (!is_unnamed && flat_rec.named.type == type) {
		return &flat_rec.named;
	}
	
	// check aliases
	for (const auto& alias_decl : flat_rec.aliases) {
		if (auto entry = find_flattened_entry(alias_decl, type, is_arg_buffer); entry) {
			return entry;
		}
	}
	
	// check children (recursively)
	for (const auto& child_decl : flat_rec.children) {
		if (auto child_entry = find_flattened_entry(child_decl, type, is_arg_buffer); child_entry) {
			return child_entry;
		}
	}
	
	return nullptr;
}

/// getCGRecordLayout - Return record layout info for the given record decl.
const CGRecordLayout &
CodeGenTypes::getCGRecordLayout(const RecordDecl *RD, llvm::Type* struct_type, const bool is_arg_buffer) {
  // check if there is a flattened layout for this llvm (struct) type,
  // return it if so, otherwise continue as usual
  if (struct_type) {
    if (const auto flat_entry = find_flattened_entry(RD, struct_type, is_arg_buffer); flat_entry) {
      assert(flat_entry->record_layout);
      return *flat_entry->record_layout;
    }

#ifndef NDEBUG
    if (auto st_type = dyn_cast_or_null<llvm::StructType>(struct_type); st_type && st_type->hasName()) {
      if (is_arg_buffer) {
        assert(should_have_flattened_layout_arg_buf.count(st_type) == 0);
      } else {
        assert(should_have_flattened_layout.count(st_type) == 0);
      }
    }
#endif
  }

  const Type *Key = Context.getTagDeclType(RD).getTypePtr();

  auto I = CGRecordLayouts.find(Key);
  if (I != CGRecordLayouts.end())
    return *I->second;
  // Compute the type information.
  ConvertRecordDeclType(RD);

  // Now try again.
  I = CGRecordLayouts.find(Key);

  assert(I != CGRecordLayouts.end() &&
         "Unable to find record layout information for type");
  return *I->second;
}

bool CodeGenTypes::is_flattened_struct_type(const RecordDecl* decl, const llvm::Type* Ty, const bool is_arg_buffer) const {
	const auto entry = find_flattened_entry(decl, Ty, is_arg_buffer);
#ifndef NDEBUG
	if (!entry) {
		if (auto st_type = dyn_cast_or_null<llvm::StructType>(Ty); st_type && st_type->hasName()) {
			if (is_arg_buffer) {
				assert(should_have_flattened_layout_arg_buf.count(st_type) == 0);
			} else {
				assert(should_have_flattened_layout.count(st_type) == 0);
			}
		}
	}
#endif
	return (entry != nullptr);
}

llvm::Type* CodeGenTypes::getAnyFlattenedType(const CXXRecordDecl* D, const bool prefer_arg_buffer_type) const {
	const auto flat_rec_iter = flattened_records.find(D);
	if (flat_rec_iter == flattened_records.end()) {
		return nullptr;
	}
	const auto& flat_rec = flat_rec_iter->second;
	
	if (prefer_arg_buffer_type) {
		if (flat_rec.arg_buffer.type) {
			return flat_rec.arg_buffer.type;
		}
		return flat_rec.named.type;
	} else {
		if (flat_rec.named.type) {
			return flat_rec.named.type;
		}
		return flat_rec.arg_buffer.type;
	}
}

llvm::Type* CodeGenTypes::getFlattenedRecordType(const CXXRecordDecl* D, const bool is_unnamed) const {
	const auto flat_rec_iter = flattened_records.find(D);
	if (flat_rec_iter == flattened_records.end()) {
		return nullptr;
	}
	return (is_unnamed ? flat_rec_iter->second.unnamed.type : flat_rec_iter->second.named.type);
}

llvm::Type* CodeGenTypes::getFlattenedFloorArgBufferType(const CXXRecordDecl* D) const {
	const auto flat_rec_iter = flattened_records.find(D);
	if (flat_rec_iter == flattened_records.end()) {
		return nullptr;
	}
	return flat_rec_iter->second.arg_buffer.type;
}

bool CodeGenTypes::isPointerZeroInitializable(QualType T) {
  assert((T->isAnyPointerType() || T->isBlockPointerType()) && "Invalid type");
  return isZeroInitializable(T);
}

bool CodeGenTypes::isZeroInitializable(QualType T) {
  if (T->getAs<PointerType>())
    return Context.getTargetNullPointerValue(T) == 0;

  if (const auto *AT = Context.getAsArrayType(T)) {
    if (isa<IncompleteArrayType>(AT))
      return true;
    if (const auto *CAT = dyn_cast<ConstantArrayType>(AT))
      if (Context.getConstantArrayElementCount(CAT) == 0)
        return true;
    T = Context.getBaseElementType(T);
  }

  // Records are non-zero-initializable if they contain any
  // non-zero-initializable subobjects.
  if (const RecordType *RT = T->getAs<RecordType>()) {
    const RecordDecl *RD = RT->getDecl();
    return isZeroInitializable(RD);
  }

  // We have to ask the ABI about member pointers.
  if (const MemberPointerType *MPT = T->getAs<MemberPointerType>())
    return getCXXABI().isZeroInitializable(MPT);

  // Everything else is okay.
  return true;
}

bool CodeGenTypes::isZeroInitializable(const RecordDecl *RD) {
  return getCGRecordLayout(RD).isZeroInitializable();
}

//
std::vector<ASTContext::aggregate_scalar_entry>
CodeGenTypes::get_aggregate_scalar_fields(const CXXRecordDecl* root_decl,
										  const CXXRecordDecl* decl,
										  const bool ignore_root_vec_compat,
										  const bool ignore_vec_compat,
										  const bool ignore_bases,
										  const bool expand_array_image,
										  const bool expand_array_other,
										  const bool merge_parent_field_decl) const {
	return Context.get_aggregate_scalar_fields(root_decl, decl, &TheCXXABI.getMangleContext(), ignore_root_vec_compat,
											   ignore_vec_compat, ignore_bases, expand_array_image, expand_array_other,
											   merge_parent_field_decl);
}
