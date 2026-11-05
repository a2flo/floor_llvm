//===- MetalImage.cpp - Metal-specific floor image transformations --------===//
//
//  Flo's Open libRary (floor)
//  Copyright (C) 2004 - 2025 Florian Ziesche
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; version 2 of the License only.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
//===----------------------------------------------------------------------===//
//
// This pass implements the Metal-specific floor image transformations, i.e.
// floor.opaque.<read/write function>.* -> air.<read/write function>.*
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/LibFloor.h"
#include "llvm/Transforms/LibFloor/FloorImage.h"
#include "llvm/Transforms/LibFloor/MetalTypes.h"
using namespace llvm;

#define DEBUG_TYPE "MetalImage"

#if 1
#define DBG(x)
#else
#define DBG(x) x
#endif

namespace {
	struct MetalImage : public FloorImageBasePass {
		static char ID; // Pass identification, replacement for typeid
		
		static constexpr const uint32_t Metal_ConstantAS = 2;
		
		MetalImage(const uint32_t image_capabilities_ = 0) :
		FloorImageBasePass(ID, IMAGE_TYPE_ID::OPAQUE, image_capabilities_) {
			initializeMetalImagePass(*PassRegistry::getPassRegistry());
		}
		
		static const char* type_to_geom(const COMPUTE_IMAGE_TYPE& image_type) {
			switch(image_type) {
				case COMPUTE_IMAGE_TYPE::IMAGE_1D:
					return "texture_1d";
				case COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY:
					return "texture_1d_array";
				case COMPUTE_IMAGE_TYPE::IMAGE_1D_BUFFER:
					return "texture_buffer_1d";
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH:
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_STENCIL:
					return "depth_2d";
				case COMPUTE_IMAGE_TYPE::IMAGE_2D:
					return "texture_2d";
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_ARRAY:
					return "depth_2d_array";
				case COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY:
					return "texture_2d_array";
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA:
					return "depth_2d_ms";
				case COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA:
					return "texture_2d_ms";
				case COMPUTE_IMAGE_TYPE::IMAGE_3D:
					return "texture_3d";
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_CUBE:
					return "depth_cube";
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_CUBE_ARRAY:
					return "depth_cube_array";
				case COMPUTE_IMAGE_TYPE::IMAGE_CUBE:
					return "texture_cube";
				case COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY:
					return "texture_cube_array";
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA_ARRAY:
					return "depth2d_ms_array";
				case COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY:
					return "texture2d_ms_array";
				default:
					return nullptr;
			}
		}
		
		static llvm::Value* make_sampler(llvm::ConstantInt* const_sampler_arg,
										 llvm::Value* dyn_sampler_arg,
										 LLVMContext* ctx,
										 Module& M,
										 const Instruction& originating_instruction) {
			// sampler handling since Metal 2.0+ is more complex
			// -> need to transform sampler into a global constant
			// -> need to add this constant to !air.sampler_states metadata
			if (dyn_sampler_arg == nullptr) {
				ctx->emitError(&originating_instruction, "sampler arg should be dynamic");
				return nullptr;
			}
			
            auto* sampler_pointer = dyn_cast<PointerType>(dyn_sampler_arg->getType());
            auto* sampler_struct = sampler_pointer ?
                dyn_cast<StructType>(sampler_pointer->getPointerElementType()) : nullptr;
            if (!sampler_pointer || sampler_pointer->getAddressSpace() != Metal_ConstantAS ||
                !sampler_struct || !sampler_struct->isOpaque() ||
                sampler_struct->getName() != "struct._sampler_t") {
                ctx->emitError(&originating_instruction, "invalid Metal sampler handle type");
                return nullptr;
            }
            // Runtime handles are already opaque AIR sampler values. Preserve the
            // loaded SSA value; never substitute an immutable sampler state.
            if (!isa<Constant>(dyn_sampler_arg)) return dyn_sampler_arg;

			auto ce_sampler = dyn_cast<ConstantExpr>(dyn_sampler_arg);
			if (ce_sampler == nullptr) {
				ctx->emitError(&originating_instruction, "sampler arg must be a constant expression");
				return nullptr;
			}
			
			if (!ce_sampler->isCast() || ce_sampler->getOpcode() != llvm::Instruction::IntToPtr) {
				ctx->emitError(&originating_instruction, "sampler arg must be a constant inttoptr expression");
				return nullptr;
			}
			
			auto sampler_constant_value = dyn_cast<ConstantInt>(ce_sampler->getOperand(0));
			if (sampler_constant_value == nullptr) {
				ctx->emitError(&originating_instruction, "sampler arg must contain a constant value");
				return nullptr;
			}
			
			// Reuse only globals owned by this module. A process-static pointer cache
			// survives LLVMContext destruction and can return freed or foreign values.
			auto sampler_constant_value_u64 = sampler_constant_value->getZExtValue();
			Constant* sampler_initializer = sampler_constant_value;
			if (metal::get_metal_version(M) >= 320) {
				sampler_initializer = ConstantArray::get(ArrayType::get(sampler_constant_value->getType(), 2u), {
					ConstantInt::get(Type::getInt64Ty(*ctx), sampler_constant_value_u64),
					ConstantInt::get(Type::getInt64Ty(*ctx), 0ull)
				});
			}
			GlobalVariable* sampler_state = nullptr;
			if (auto* samplers = M.getNamedMetadata("air.sampler_states")) {
				for (auto* record : samplers->operands()) {
					if (record->getNumOperands() != 2) continue;
					auto* tag = dyn_cast<MDString>(record->getOperand(0).get());
					auto* value = dyn_cast<ConstantAsMetadata>(record->getOperand(1).get());
					if (!tag || tag->getString() != "air.sampler_state" || !value) continue;
					auto* candidate = dyn_cast<GlobalVariable>(value->getValue());
					if (candidate && candidate->getParent() == &M && candidate->isConstant() &&
						candidate->getAddressSpace() == Metal_ConstantAS && candidate->hasInitializer() &&
						candidate->getInitializer() == sampler_initializer) {
						sampler_state = candidate;
						break;
					}
				}
			}
			if (!sampler_state) {
				sampler_state = new GlobalVariable(M, sampler_initializer->getType(), true,
					GlobalVariable::InternalLinkage, sampler_initializer, "__air_sampler_state",
					nullptr, GlobalValue::NotThreadLocal, Metal_ConstantAS);
				sampler_state->setAlignment(MaybeAlign { 8u });
				
				// insert metadata
				SmallVector<llvm::Metadata*, 2> sampler_md_info;
				sampler_md_info.push_back(llvm::MDString::get(*ctx, "air.sampler_state"));
				sampler_md_info.push_back(llvm::ConstantAsMetadata::get(sampler_state));
				
				auto samplers_md = M.getOrInsertNamedMetadata("air.sampler_states");
				samplers_md->addOperand(llvm::MDNode::get(*ctx, sampler_md_info));
			}
			
			// still need to bitcast to %struct._sampler_t*
			auto sampler_type = dyn_cast<PointerType>(dyn_sampler_arg->getType());
			auto cast_sampler_state = ConstantExpr::getBitCast(sampler_state, sampler_type);
			
			return cast_sampler_state;
		}
		
        // Narrow float gather ABI, independently checked against Xcode AIR and
        // DXMT AIRBuilder. Preserve the runtime texture and sampler handles.
        void handle_gather_image(CallBase& CB) override {
            auto fail = [&]() {
                ctx->emitError(&CB, "unsupported or malformed float texture gather");
            };
            if (CB.arg_size() != 7) { fail(); return; }
            auto *kind = dyn_cast<ConstantInt>(CB.getArgOperand(2));
            auto *component = dyn_cast<ConstantInt>(CB.getArgOperand(6));
            if (!kind || !kind->getType()->isIntegerTy(64) ||
                !component || !component->getType()->isIntegerTy(32) ||
                component->getZExtValue() > 3) { fail(); return; }
            const auto bits = kind->getZExtValue();
            const bool array = bits == 0x12f400ULL;
            const bool cube = bits == 0x82f400ULL;
            if (bits != 0x2f400ULL && !array && !cube) { fail(); return; }
            const StringRef geometry = array ? "texture_2d_array" :
                (cube ? "texture_cube" : "texture_2d");
            auto *image = CB.getArgOperand(0);
            auto *image_pointer = dyn_cast<PointerType>(image->getType());
            auto *image_record = image_pointer && !image_pointer->isOpaque() ?
                dyn_cast<StructType>(image_pointer->getPointerElementType()) : nullptr;
            const std::string expected_type = "struct._" + geometry.str() + "_t";
            auto *f4 = FixedVectorType::get(builder->getFloatTy(), 4);
            auto *coord_type = FixedVectorType::get(builder->getFloatTy(), cube ? 3 : 2);
            auto *offset_type = FixedVectorType::get(builder->getInt32Ty(), 2);
            if (!image_pointer || image_pointer->getAddressSpace() != 1 ||
                !image_record || image_record->getName() != expected_type ||
                CB.getType() != f4 || CB.getArgOperand(3)->getType() != coord_type ||
                !CB.getArgOperand(4)->getType()->isIntegerTy(32) ||
                CB.getArgOperand(5)->getType() != offset_type) { fail(); return; }
            if (!array) {
                auto *layer = dyn_cast<ConstantInt>(CB.getArgOperand(4));
                if (!layer || !layer->isZero()) { fail(); return; }
            }
            if (cube) {
                auto *offset = dyn_cast<Constant>(CB.getArgOperand(5));
                if (!offset || !offset->isNullValue()) { fail(); return; }
            }
            auto *sampler = make_sampler(nullptr, CB.getArgOperand(1), ctx, *M, CB);
            if (!sampler) return;
            builder->SetInsertPoint(&CB);
            SmallVector<Value*, 8> args { image, sampler, CB.getArgOperand(3) };
            if (array) args.push_back(CB.getArgOperand(4));
            if (!cube) {
                args.push_back(builder->getTrue());
                args.push_back(CB.getArgOperand(5));
            }
            args.push_back(component);
            args.push_back(builder->getInt32(0)); // sample access
            SmallVector<Type*, 8> types;
            for (auto *arg : args) types.push_back(arg->getType());
            auto *result_type = StructType::get(*ctx, { f4, builder->getInt8Ty() });
            auto *function_type = FunctionType::get(result_type, types, false);
            const std::string symbol = "air.gather_" + geometry.str() + ".v4f32";
            if (auto *existing = M->getFunction(symbol)) {
                if (existing->getFunctionType() != function_type) { fail(); return; }
            }
            auto callee = M->getOrInsertFunction(symbol, function_type);
            auto *call = builder->CreateCall(callee, args);
            call->setDoesNotThrow();
            call->setOnlyReadsMemory();
            auto *value = builder->CreateExtractValue(call, { 0 });
            CB.replaceAllUsesWith(value);
            CB.eraseFromParent();
        }

		void handle_read_image(Instruction& I,
							   const StringRef& func_name,
							   llvm::Value* img_handle_arg,
							   const COMPUTE_IMAGE_TYPE& image_type,
							   llvm::ConstantInt* const_sampler_arg,
							   llvm::Value* dyn_sampler_arg,
							   llvm::Value* coord_arg,
							   llvm::Value* layer_arg,
							   llvm::Value* sample_arg,
							   llvm::Value* offset_arg,
							   const SmallVector<llvm::Value*, 3>& offset_elems,
							   const bool is_offset,
							   llvm::Value* lod_or_bias_arg,
							   const bool is_lod_or_bias, // true: lod, false: bias
							   llvm::Value* dpdx_arg,
							   llvm::Value* dpdy_arg,
							   const bool is_gradient,
							   const COMPARE_FUNCTION& compare_function,
							   llvm::Value* compare_value_arg,
							   const bool is_compare) override {
			const auto metal_version = metal::get_metal_version(*M);
			SmallVector<llvm::Type*, 16> func_arg_types;
			SmallVector<llvm::Value*, 16> func_args;
			
			// -> return data
			std::string dtype;
			llvm::Type* ret_type;
			if(func_name.endswith(".float")) {
				dtype = "v4f32";
				ret_type = llvm::FixedVectorType::get(llvm::Type::getFloatTy(*ctx), 4);
			}
			else if(func_name.endswith(".int")) {
				dtype = "s.v4i32";
				ret_type = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*ctx), 4);
			}
			else if(func_name.endswith(".uint")) {
				dtype = "u.v4i32";
				ret_type = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*ctx), 4);
			}
			else if(func_name.endswith(".half")) {
				dtype = "v4f16";
				ret_type = llvm::FixedVectorType::get(llvm::Type::getHalfTy(*ctx), 4);
			}
			else if(func_name.endswith(".short")) {
				dtype = "s.v4i16";
				ret_type = llvm::FixedVectorType::get(llvm::Type::getInt16Ty(*ctx), 4);
			}
			else if(func_name.endswith(".ushort")) {
				dtype = "u.v4i16";
				ret_type = llvm::FixedVectorType::get(llvm::Type::getInt16Ty(*ctx), 4);
			}
			// unknown -> ignore
			else return;
			
			// -> geom
			const auto geom_cstr = type_to_geom(image_type);
			if (!geom_cstr) {
				ctx->emitError(&I, "unknown or incorrect image type");
				return;
			}
			std::string geom = geom_cstr;
			const auto is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
			const auto is_msaa = has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type);
			const auto is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
			const auto is_depth = has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type);
			
			// -> coord type
			auto coord_vec_type = dyn_cast_or_null<FixedVectorType>(coord_arg->getType());
			if(!coord_vec_type) {
				ctx->emitError(&I, "invalid image coordinate argument (cast to vector failed)");
				return;
			}
			
			const auto coord_type = coord_vec_type->getElementType();
			if(is_msaa && !coord_type->isIntegerTy()) {
				ctx->emitError(&I, "coordinate type must be integer for msaa images");
				return;
			}
			if(is_cube && !coord_type->isFloatTy()) {
				ctx->emitError(&I, "coordinate type must be float for cube images");
				return;
			}
			
			// air.read_* or air.sample_*?
			// for msaa: always read
			// for all else: read if int coords, sample if float coords
			const bool is_sample_call = (!is_msaa && !coord_type->isIntegerTy());
            const bool is_buffer = image_type == COMPUTE_IMAGE_TYPE::IMAGE_1D_BUFFER;
            if (is_buffer && (is_sample_call || coord_vec_type->getNumElements() != 1)) {
                ctx->emitError(&I, "buffer textures require scalar integer read coordinates");
                return;
            }
			
			// img handle and sampler
			func_arg_types.push_back(img_handle_arg->getType());
			func_args.push_back(img_handle_arg);
			
			// only add the sampler arg if this is a sample call
			bool set_sampler_attrs = false;
			uint32_t sampler_param_idx = 0;
			if (is_sample_call) {
				auto sampler = make_sampler(const_sampler_arg, dyn_sampler_arg, ctx, *M, I);
				if (!sampler) return; // make_sampler already emitted a diagnostic.
				
				func_arg_types.push_back(sampler->getType());
				func_args.push_back(sampler);
				
				set_sampler_attrs = true;
				sampler_param_idx = func_args.size() - 1;
			} else if (metal_version >= 400) {
				// even for reads, we still need a sampler now -> get it via air.get_read_sampler() call
				auto get_read_sampler = M->getFunction("air.get_read_sampler");
				if (!get_read_sampler) {
					AttrBuilder attr_builder(*ctx);
					attr_builder.addAttribute(llvm::Attribute::InaccessibleMemOnly);
					attr_builder.addAttribute(llvm::Attribute::MustProgress);
					attr_builder.addAttribute(llvm::Attribute::NoFree);
					attr_builder.addAttribute(llvm::Attribute::NoUnwind);
					attr_builder.addAttribute(llvm::Attribute::ReadOnly);
					attr_builder.addAttribute(llvm::Attribute::WillReturn);
					auto func_attrs = AttributeList::get(*ctx, ~0, attr_builder);
					
					auto sampler_type = llvm::StructType::getTypeByName(*ctx, "struct._sampler_t");
					if (!sampler_type) {
						sampler_type = llvm::StructType::create(*ctx, "struct._sampler_t");
					}
					auto sampler_ret_type = llvm::PointerType::get(sampler_type, 2);
					const auto get_read_sampler_func_type = llvm::FunctionType::get(sampler_ret_type, false);
					
					get_read_sampler = (llvm::Function*)M->getOrInsertFunction("air.get_read_sampler", get_read_sampler_func_type,
																			   func_attrs).getCallee();
				}
				
				auto read_sampler_call = builder->CreateCall(get_read_sampler);
				read_sampler_call->setOnlyAccessesInaccessibleMemory();
				read_sampler_call->addFnAttr(Attribute::NoUnwind);
				read_sampler_call->setOnlyReadsMemory();
				read_sampler_call->addFnAttr(Attribute::WillReturn);
				
				func_arg_types.push_back(read_sampler_call->getType());
				func_args.push_back(read_sampler_call);
			}
			
			if(is_depth) {
				// must always add the depth type 1 (== float)
				func_arg_types.push_back(llvm::Type::getInt32Ty(*ctx));
				func_args.push_back(builder->getInt32(1));
				
				// depth return type is always a float
				ret_type = llvm::Type::getFloatTy(*ctx);
				dtype = "f32";
				
				if(is_compare) {
					if(!is_sample_call) {
						ctx->emitError(&I, "compare must be a sample call");
						return;
					}
				}
			}
			else {
				if(is_compare) {
					ctx->emitError(&I, "compare is only allowed with depth types");
					return;
				}
			}
			
			// handle offset
			llvm::Value* offset_coord_arg = coord_arg;
			if(!is_sample_call && is_offset) {
				offset_coord_arg = builder->CreateAdd(coord_arg, offset_arg);
			}
			
			// -> coords: coords, sample, face, layer
			if(coord_vec_type->getNumElements() == 1) {
				// 1D coord, make it scalar
				func_arg_types.push_back(coord_type);
				func_args.push_back(builder->CreateExtractElement(offset_coord_arg, builder->getInt32(0)));
			}
			else {
				// normal coord arg
				if(!is_cube || coord_type->isFloatTy()) {
					func_arg_types.push_back(offset_coord_arg->getType());
					func_args.push_back(offset_coord_arg);
				}
				// cube with int coords
				else {
					// extract the face integer from the coords
					const auto face_arg = builder->CreateExtractElement(coord_arg, builder->getInt32(2));
					
					// create new int2 arg
					llvm::Value* coord_i2_arg = UndefValue::get(llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*ctx), 2));
					coord_i2_arg = builder->CreateInsertElement(coord_i2_arg, builder->CreateExtractElement(offset_coord_arg, builder->getInt32(0)), builder->getInt32(0));
					coord_i2_arg = builder->CreateInsertElement(coord_i2_arg, builder->CreateExtractElement(offset_coord_arg, builder->getInt32(1)), builder->getInt32(1));
					
					func_arg_types.push_back(coord_i2_arg->getType());
					func_args.push_back(coord_i2_arg);
					
					func_arg_types.push_back(face_arg->getType());
					func_args.push_back(face_arg);
				}
			}
			
			if(is_msaa) {
				func_arg_types.push_back(sample_arg->getType());
				func_args.push_back(sample_arg);
			}
			
			if(is_array) {
				func_arg_types.push_back(layer_arg->getType());
				func_args.push_back(layer_arg);
			}
			
			if(is_compare) {
				func_arg_types.push_back(compare_value_arg->getType());
				func_args.push_back(compare_value_arg);
			}
			
			// -> additional args: lod, bias, gradient, offset
			if(!is_sample_call) {
				// 2D/3D non-cube/non-msaa images: unknown zero init argument
				if (metal_version >= 400 && !is_cube && !is_msaa && coord_vec_type->getNumElements() > 1) {
					func_arg_types.push_back(coord_vec_type);
					func_args.push_back(llvm::Constant::getNullValue(coord_vec_type));
				}
				
				// -> read
				if(!is_msaa && !is_buffer) { // neither MSAA nor buffer textures have a mip argument
					// -> lod
					if(is_lod_or_bias) {
						// for read, only int lods are allowed
						if(!lod_or_bias_arg->getType()->isIntegerTy()) {
							// convert to int
							const auto int_lod = builder->CreateFPToSI(lod_or_bias_arg, llvm::Type::getInt32Ty(*ctx));
							func_arg_types.push_back(int_lod->getType());
							func_args.push_back(int_lod);
						}
						else {
							func_arg_types.push_back(lod_or_bias_arg->getType());
							func_args.push_back(lod_or_bias_arg);
						}
					}
					else {
						// if no lod is specified (bias is not allowed here), add a 0 lod
						func_arg_types.push_back(llvm::Type::getInt32Ty(*ctx));
						func_args.push_back(builder->getInt32(0));
					}
				}
				
                // AIR read access: buffer textures in this supported subset
                // use read (1). Preserve existing sample-access textures (0).
                func_arg_types.push_back(llvm::Type::getInt32Ty(*ctx));
                func_args.push_back(builder->getInt32(is_buffer ? 1 : 0));
			}
			else {
				// -> sample
				
				// -> gradient
				if(is_gradient) {
					func_arg_types.push_back(dpdx_arg->getType());
					func_args.push_back(dpdx_arg);
					func_arg_types.push_back(dpdy_arg->getType());
					func_args.push_back(dpdy_arg);
					
					// additional "min LOD clamp" f32 argument -> set to 0.0 for now
					func_arg_types.push_back(llvm::Type::getFloatTy(*ctx));
					func_args.push_back(ConstantFP::get(llvm::Type::getFloatTy(*ctx), 0.0f));
				}
				
				// -> offset
				if(!is_cube) { // cube allows no offset
					func_arg_types.push_back(llvm::Type::getInt1Ty(*ctx));
                    // Xcode AIR and pinned DXMT use true for this control
                    // operand on supported 2D/3D/2D-array samples, including gradients and a zero
                    // offset. Its precise semantic name is not established.
                    // Do not extrapolate to depth, cube or other geometries.
                    const bool reference_sample_control =
                        image_type == COMPUTE_IMAGE_TYPE::IMAGE_2D ||
                        image_type == COMPUTE_IMAGE_TYPE::IMAGE_3D ||
                        image_type == COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY ||
                        image_type == COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_ARRAY;
                    func_args.push_back(builder->getInt1(reference_sample_control || is_offset));
					
					func_arg_types.push_back(offset_arg->getType());
					func_args.push_back(offset_arg);
				}
				
				// -> lod / bias
				if (!is_gradient) {
					// lod or bias?
					func_arg_types.push_back(llvm::Type::getInt1Ty(*ctx));
					func_args.push_back(builder->getInt1(is_lod_or_bias));
					
					// for sample, only float lods are allowed
					if(!lod_or_bias_arg->getType()->isFloatTy()) {
						// convert to float
						const auto float_lod = builder->CreateSIToFP(lod_or_bias_arg, llvm::Type::getFloatTy(*ctx));
						func_arg_types.push_back(float_lod->getType());
						func_args.push_back(float_lod);
					}
					else {
						func_arg_types.push_back(lod_or_bias_arg->getType());
						func_args.push_back(lod_or_bias_arg);
					}
					
					// additional (unknown) f32 0.0 argument
					func_arg_types.push_back(llvm::Type::getFloatTy(*ctx));
					func_args.push_back(ConstantFP::get(llvm::Type::getFloatTy(*ctx), 0.0f));
				}
				
				// an additional (unknown) i32 0 argument
				func_arg_types.push_back(llvm::Type::getInt32Ty(*ctx));
				func_args.push_back(builder->getInt32(0));
			}
			
			// return type is now always { <sample/read-type>, i8 }
			ret_type = llvm::StructType::get(*ctx, { ret_type, llvm::Type::getInt8Ty(*ctx) }, false /* !packed */);
			assert(((llvm::StructType*)ret_type)->isLiteral() && "must be literal");
			
			// -> build read func name
			std::string read_func_name = "air.";
			
			read_func_name += (is_sample_call ? "sample_" : "read_");
			if(is_compare) read_func_name += "compare_";
			read_func_name += geom;
			if(is_gradient) read_func_name += "_grad";
			
			read_func_name += '.' + dtype;
			
			AttrBuilder attr_builder(*ctx);
			attr_builder.addAttribute(llvm::Attribute::Convergent);
			attr_builder.addAttribute(llvm::Attribute::ArgMemOnly);
			attr_builder.addAttribute(llvm::Attribute::NoUnwind);
			attr_builder.addAttribute(llvm::Attribute::ReadOnly);
			attr_builder.addAttribute(llvm::Attribute::WillReturn);
			auto func_attrs = AttributeList::get(*ctx, ~0, attr_builder);
			
			// create the air call
			const auto func_type = llvm::FunctionType::get(ret_type, func_arg_types, false);
			llvm::CallInst* read_call = builder->CreateCall(M->getOrInsertFunction(read_func_name, func_type, func_attrs), func_args);
			read_call->setConvergent();
			read_call->setOnlyAccessesArgMemory();
			read_call->setDoesNotThrow();
			read_call->setOnlyReadsMemory(); // all reads are readonly (can be optimized away if unused)
			read_call->addFnAttr(Attribute::WillReturn);
			read_call->setDebugLoc(I.getDebugLoc()); // keep debug loc
			
			// set sampler* cast attributes if necessary
			if (set_sampler_attrs) {
				read_call->addParamAttr(sampler_param_idx, Attribute::NoCapture);
				read_call->addParamAttr(sampler_param_idx, Attribute::ReadOnly);
			}
			
			// if this is a depth read/sample, the return type is a float -> create a float4
			llvm::Value* read_call_result = read_call;
			// we always ignore the i8 return value -> extract are wanted result
			read_call_result = builder->CreateExtractValue(read_call_result, { 0 });
			if(is_depth) {
				auto undef_vec = UndefValue::get(llvm::FixedVectorType::get(llvm::Type::getFloatTy(*ctx), 4));
				read_call_result = builder->CreateInsertElement(undef_vec, read_call_result, builder->getInt32(0));
				// rest is undef/zero (and will be stripped away again anyways)
			}
			
			//
			I.replaceAllUsesWith(read_call_result);
			I.eraseFromParent();
		}
		
		void handle_write_image(Instruction& I,
								const StringRef& func_name,
								llvm::Value* img_handle_arg,
								const COMPUTE_IMAGE_TYPE& full_image_type,
								const COMPUTE_IMAGE_TYPE& image_type,
								const COMPUTE_IMAGE_TYPE& format_type,
								const COMPUTE_IMAGE_TYPE& data_type,
								llvm::Value* /* rt_image_type */,
								const bool& is_normalized,
								const uint32_t& image_channel_count,
								llvm::Value* coord_arg,
								llvm::Value* layer_arg,
								llvm::Value* lod_arg,
								const bool is_lod,
								llvm::Value* data_arg) override {
			if (image_type == COMPUTE_IMAGE_TYPE::IMAGE_1D_BUFFER) {
                ctx->emitError(&I, "buffer texture writes are not supported");
                return;
            }
            SmallVector<llvm::Type*, 16> func_arg_types;
			SmallVector<llvm::Value*, 16> func_args;
			
			//// more arg checking
			auto coord_vec_type = dyn_cast_or_null<FixedVectorType>(coord_arg->getType());
			if(!coord_vec_type) {
				ctx->emitError(&I, "invalid image coordinate argument (cast to vector failed)");
				return;
			}
			
			const auto coord_type = coord_vec_type->getElementType();
			if(!coord_type->isIntegerTy()) {
				ctx->emitError(&I, "coordinate type must be integer");
				return;
			}
			
			std::string dtype;
			if (func_name.endswith(".float.depth")) {
				dtype = "f32";
			} else if (func_name.endswith(".float")) {
				dtype = "v4f32";
			} else if (func_name.endswith(".int")) {
				dtype = "s.v4i32";
			} else if (func_name.endswith(".uint")) {
				dtype = "u.v4i32";
			} else if (func_name.endswith(".half")) {
				dtype = "v4f16";
			} else if (func_name.endswith(".short")) {
				dtype = "s.v4i16";
			} else if (func_name.endswith(".ushort")) {
				dtype = "u.v4i16";
			}
			// unknown -> ignore
			else return;
			
			//// func replacement
			func_arg_types.push_back(img_handle_arg->getType());
			func_args.push_back(img_handle_arg);
			
			// -> geom
			const auto geom_cstr = type_to_geom(image_type);
			if (!geom_cstr) {
				ctx->emitError(&I, "unknown or incorrect image type");
				return;
			}
			std::string geom = geom_cstr;
			const auto is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
			const auto is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
			const auto is_depth = has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type);
			
			// filter types that are not allowed, b/c they can't be directly written to
			switch (image_type) {
				case COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA:
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA:
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_CUBE:
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_CUBE_ARRAY:
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA_ARRAY:
				case COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY:
					ctx->emitError(&I, "invalid image type - type is not writable");
					return;
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH:
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_STENCIL:
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_ARRAY:
					// TODO/NOTE: not officially exposed by Metal headers, but exists in metalfe -> figure out what the min version is
					break;
				default:
					break;
			}
			
			// -> coords: coords, face, layer
			if(coord_vec_type->getNumElements() == 1) {
				// 1D coord, make it scalar
				func_arg_types.push_back(coord_type);
				func_args.push_back(builder->CreateExtractElement(coord_arg, builder->getInt32(0)));
			}
			else {
				// normal coord arg
				if(!is_cube) {
					func_arg_types.push_back(coord_vec_type);
					func_args.push_back(coord_arg);
				}
				// cube with int coords
				else {
					// extract the face integer from the coords
					const auto face_arg = builder->CreateExtractElement(coord_arg, builder->getInt32(2));
					
					// create new int2 arg
					llvm::Value* coord_i2_arg = UndefValue::get(llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*ctx), 2));
					coord_i2_arg = builder->CreateInsertElement(coord_i2_arg, builder->CreateExtractElement(coord_arg, builder->getInt32(0)), builder->getInt32(0));
					coord_i2_arg = builder->CreateInsertElement(coord_i2_arg, builder->CreateExtractElement(coord_arg, builder->getInt32(1)), builder->getInt32(1));
					
					func_arg_types.push_back(coord_i2_arg->getType());
					func_args.push_back(coord_i2_arg);
					
					func_arg_types.push_back(face_arg->getType());
					func_args.push_back(face_arg);
				}
			}
			
			if(is_array) {
				func_arg_types.push_back(layer_arg->getType());
				func_args.push_back(layer_arg);
			}
			
			// -> data (a 4-component vector or a single float if depth)
			func_arg_types.push_back(data_arg->getType());
			func_args.push_back(data_arg);
			
			// -> lod
			func_arg_types.push_back(llvm::Type::getInt32Ty(*ctx));
			if (!is_lod) {
				func_args.push_back(builder->getInt32(0));
			} else {
				func_args.push_back(lod_arg);
			}
			
			// additional rounding mode argument (always set to 0 for now, or 2 if depth)
			func_arg_types.push_back(llvm::Type::getInt32Ty(*ctx));
			const bool write_only_access =
                (full_image_type & COMPUTE_IMAGE_TYPE::__ACCESS_MASK) == COMPUTE_IMAGE_TYPE::WRITE &&
                (image_type == COMPUTE_IMAGE_TYPE::IMAGE_2D || image_type == COMPUTE_IMAGE_TYPE::IMAGE_3D);
            func_args.push_back(builder->getInt32(write_only_access ? 2 : (!is_depth ? 0 : 2)));
			
			// -> build write func name
			const std::string write_func_name = "air.write_" + geom + '.' + dtype;
			
			// create the air call
			const auto func_type = llvm::FunctionType::get(builder->getVoidTy(), func_arg_types, false);
			llvm::CallInst* write_call = builder->CreateCall(M->getOrInsertFunction(write_func_name, func_type, nounwind_attr), func_args);
			write_call->setDebugLoc(I.getDebugLoc()); // keep debug loc
			
			//
			I.replaceAllUsesWith(write_call);
			I.eraseFromParent();
		}
		
		void handle_get_image_dim(Instruction& I,
								  const StringRef& func_name,
								  llvm::Value* img_handle_arg,
								  const COMPUTE_IMAGE_TYPE& /* full_image_type */,
								  const COMPUTE_IMAGE_TYPE& image_type,
								  llvm::Value* lod_arg) override {
			// gather info
			const auto dim_count = image_dim_count(image_type);
			const auto is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);

			const auto geom_cstr = type_to_geom(image_type);
			if (!geom_cstr) {
				ctx->emitError(&I, "unknown or incorrect image type");
				return;
			}
			const std::string geom = geom_cstr;

			// query/get function base
			const auto query_image = [&](const std::string& query_name) {
				SmallVector<llvm::Type*, 2> func_arg_types;
				SmallVector<llvm::Value*, 2> func_args;
				func_arg_types.push_back(img_handle_arg->getType());
				func_args.push_back(img_handle_arg);
				func_arg_types.push_back(lod_arg->getType());
				func_args.push_back(lod_arg);
				
				// -> build get func name
				const std::string get_func_name = "air.get_" + query_name + '_' + geom;
				
				AttrBuilder attr_builder(*ctx);
				attr_builder.addAttribute(llvm::Attribute::Convergent);
				attr_builder.addAttribute(llvm::Attribute::ArgMemOnly);
				attr_builder.addAttribute(llvm::Attribute::NoUnwind);
				attr_builder.addAttribute(llvm::Attribute::ReadOnly);
				auto func_attrs = AttributeList::get(*ctx, ~0, attr_builder);
				
				// create the air call
				const auto ret_type = llvm::Type::getInt32Ty(*ctx);
				const auto func_type = llvm::FunctionType::get(ret_type, func_arg_types, false);
				llvm::CallInst* get_call = builder->CreateCall(M->getOrInsertFunction(get_func_name, func_type, func_attrs), func_args);
				get_call->setConvergent();
				get_call->setOnlyAccessesArgMemory();
				get_call->setDoesNotThrow();
				get_call->setOnlyReadsMemory(); // all get_* calls are readonly (can be optimized away if unused)
				get_call->setDebugLoc(I.getDebugLoc()); // keep debug loc
				return get_call;
			};
			
			// we have to a return a full image dim query for all dims of the image (type dependent)
			// order is: width [, height] [, depth], [, layer_count]
			// non-existing dims are set to 0
			const auto ret_type = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*ctx), 4);
			llvm::Value* ret_vec = UndefValue::get(ret_type);
			uint32_t ret_vec_idx = 0;
			// all images have a width
			ret_vec = builder->CreateInsertElement(ret_vec, query_image("width"), builder->getInt32(ret_vec_idx++));
			if (dim_count >= 2) {
				ret_vec = builder->CreateInsertElement(ret_vec, query_image("height"), builder->getInt32(ret_vec_idx++));
			}
			if (dim_count >= 3) {
				ret_vec = builder->CreateInsertElement(ret_vec, query_image("depth"), builder->getInt32(ret_vec_idx++));
			}
			if (is_array) {
				ret_vec = builder->CreateInsertElement(ret_vec, query_image("array_size"), builder->getInt32(ret_vec_idx++));
			}
			// NOTE: while cube maps technically have 6 layers, this number is not stored in the image dim
			
			// fill remaining components with 0
			for (uint32_t vec_idx = ret_vec_idx; vec_idx < 4; ++vec_idx) {
				ret_vec = builder->CreateInsertElement(ret_vec, builder->getInt32(0), builder->getInt32(ret_vec_idx++));
			}
			
			//
			I.replaceAllUsesWith(ret_vec);
			I.eraseFromParent();
		}
		
		void handle_query_image_lod(Instruction& I,
									const StringRef& func_name,
									llvm::Value* img_handle_arg,
									const COMPUTE_IMAGE_TYPE& image_type,
									llvm::ConstantInt* const_sampler_arg,
									llvm::Value* dyn_sampler_arg,
									llvm::Value* coord_arg) override {
			SmallVector<llvm::Type*, 4> func_arg_types;
			SmallVector<llvm::Value*, 4> func_args;
			
			// -> return data
			std::string dtype;
			
			// -> geom
			const auto geom_cstr = type_to_geom(image_type);
			if (!geom_cstr) {
				ctx->emitError(&I, "unknown or incorrect image type");
				return;
			}
			std::string geom = geom_cstr;
			
			// filter types that are not allowed
			switch (image_type) {
				case COMPUTE_IMAGE_TYPE::IMAGE_1D_BUFFER:
				case COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA:
				case COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY:
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA:
				case COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA_ARRAY:
					ctx->emitError(&I, "invalid image type - LOD can not be queried for this image type");
					return;
				case COMPUTE_IMAGE_TYPE::IMAGE_1D:
				case COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY: {
					// not supported, but we still want to allow this for compat with Vulkan -> always return 0.0
					auto result = ConstantFP::get(llvm::Type::getFloatTy(*ctx), 0.0f);
					I.replaceAllUsesWith(result);
					I.eraseFromParent();
					return;
				}
				default:
					break;
			}
			
			// -> coord type
			auto coord_vec_type = dyn_cast_or_null<FixedVectorType>(coord_arg->getType());
			if (!coord_vec_type) {
				ctx->emitError(&I, "invalid image coordinate argument (cast to vector failed)");
				return;
			}
			const auto coord_type = coord_vec_type->getElementType();
			if (!coord_type->isFloatTy()) {
				ctx->emitError(&I, "coordinate type must be float");
				return;
			}
			
			func_arg_types.push_back(img_handle_arg->getType());
			func_args.push_back(img_handle_arg);
			
			auto sampler = make_sampler(const_sampler_arg, dyn_sampler_arg, ctx, *M, I);
			if (!sampler) return; // make_sampler already emitted a diagnostic.
			func_arg_types.push_back(sampler->getType());
			func_args.push_back(sampler);
			
			func_arg_types.push_back(coord_arg->getType());
			func_args.push_back(coord_arg);
			
			// access: always "sample" (0)
			func_arg_types.push_back(llvm::Type::getInt32Ty(*ctx));
			func_args.push_back(builder->getInt32(0));
			
			// -> build query func name
			// NOTE: we use the "clamped" rather than the "unclamped" variant here, because we want the actual LOD that would be used
			std::string query_lod_func_name = "air.calculate_clamped_lod_" + geom;
			
			AttrBuilder attr_builder(*ctx);
			attr_builder.addAttribute(llvm::Attribute::Convergent);
			attr_builder.addAttribute(llvm::Attribute::ArgMemOnly);
			attr_builder.addAttribute(llvm::Attribute::NoUnwind);
			attr_builder.addAttribute(llvm::Attribute::ReadOnly);
			auto func_attrs = AttributeList::get(*ctx, ~0, attr_builder);
			
			// create the air call
			const auto func_type = llvm::FunctionType::get(llvm::Type::getFloatTy(*ctx), func_arg_types, false);
			llvm::CallInst* query_lod_call = builder->CreateCall(M->getOrInsertFunction(query_lod_func_name, func_type, func_attrs), func_args);
			query_lod_call->setConvergent();
			query_lod_call->setOnlyAccessesArgMemory();
			query_lod_call->setDoesNotThrow();
			query_lod_call->setOnlyReadsMemory(); // all reads are readonly (can be optimized away if unused)
			query_lod_call->setDebugLoc(I.getDebugLoc()); // keep debug loc
			
			// set sampler* cast attributes
			query_lod_call->addParamAttr(1, Attribute::NoCapture);
			query_lod_call->addParamAttr(1, Attribute::ReadOnly);
			
			//
			I.replaceAllUsesWith(query_lod_call);
			I.eraseFromParent();
		}
		
	};
}

char MetalImage::ID = 0;
INITIALIZE_PASS_BEGIN(MetalImage, "MetalImage", "MetalImage Pass", false, false)
INITIALIZE_PASS_END(MetalImage, "MetalImage", "MetalImage Pass", false, false)

FunctionPass *llvm::createMetalImagePass(const uint32_t image_capabilities) {
	return new MetalImage(image_capabilities);
}
