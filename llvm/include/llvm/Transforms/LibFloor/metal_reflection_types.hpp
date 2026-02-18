
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "metal_reflection_types.hpp"

namespace metal::reflection {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif

struct table_vtable_t {
	uint16_t vtable_size;
	uint16_t inline_data_size;
};
static_assert(sizeof(table_vtable_t) == 4);

struct node_vtable_t : table_vtable_t {
	uint16_t node_type_offset;
	uint16_t node_indirection_offset;
};
static_assert(sizeof(node_vtable_t) == 8);

struct table_root_t {
	int32_t vtable_offset;
};
static_assert(sizeof(table_root_t) == 4);

struct node_root_t : table_root_t {
	NODE_TYPE type;
	int32_t indirection_offset;
};
static_assert(sizeof(node_root_t) == 12);

//! NodeType
enum class NODE_TYPE : uint32_t {
	NONE = 0u, //!< NONE
	FRAGMENT_FUNCTION = 1u, //!< FragmentFunction
	KERNEL_FUNCTION = 2u, //!< KernelFunction
	VERTEX_FUNCTION = 3u, //!< VertexFunction
	VISIBLE_FUNCTION = 4u, //!< VisibleFunction
	INTERSECTION_FUNCTION = 5u, //!< IntersectionFunction
	MESH_FUNCTION = 6u, //!< MeshFunction
	OBJECT_FUNCTION = 7u, //!< ObjectFunction
	CI_FUNCTION = 8u, //!< CIFunction

	VEC_TYPE_HINT_FN_ATTR = 4096u, //!< VecTypeHintFnAttr
	WORKGROUP_SIZE_FN_ATTR = 4097u, //!< WorkgroupSizeFnAttr
	WORKGROUP_SIZE_HINT_FN_ATTR = 4098u, //!< WorkgroupSizeHintFnAttr
	WORKGROUP_MAX_SIZE_FN_ATTR = 4099u, //!< WorkgroupMaxSizeFnAttr
	PATCH_FN_ATTR = 4100u, //!< PatchFnAttr
	MAX_MESH_WORKGROUPS_FN_ATTR = 4101u, //!< MaxMeshWorkgroupsFnAttr
	USER_ANNOTATION_FN_ATTR = 4102u, //!< UserAnnotationFnAttr

	CLIP_DISTANCE_RET = 131072u, //!< ClipDistanceRet
	POINT_SIZE_RET = 131073u, //!< PointSizeRet
	POSITION_RET = 131074u, //!< PositionRet
	RENDER_TARGET_ARRAY_INDEX_RET = 131075u, //!< RenderTargetArrayIndexRet
	VERTEX_OUTPUT_RET = 131076u, //!< VertexOutputRet
	VIEWPORT_ARRAY_INDEX_RET = 131077u, //!< ViewportArrayIndexRet

	RENDER_TARGET_RET = 135168u, //!< RenderTargetRet
	DEPTH_RET = 135169u, //!< DepthRet
	STENCIL_RET = 135170u, //!< StencilRet
	SAMPLE_MASK_RET = 135171u, //!< SampleMaskRet
	IMAGEBLOCK_DATA_RET = 135172u, //!< ImageblockDataRet

	ACCEPT_INTERSECTION_RET = 139264u, //!< AcceptIntersectionRet
	CONTINUE_SEARCH_RET = 139265u, //!< ContinueSearchRet
	DISTANCE_RET = 139266u, //!< DistanceRet

	MESH_PRIMITIVE_DATA_RET = 143360u, //!< MeshPrimitiveDataRet
	MESH_VERTEX_DATA_RET = 143361u, //!< MeshVertexDataRet
	PRIMITIVE_CULLED_RET = 143362u, //!< PrimitiveCulledRet
	PRIMITIVE_ID_RET = 143363u, //!< PrimitiveIDRet

	CIPOINTER_RET = 147456u, //!< CIPointerRet
	CISTRUCT_RET = 147457u, //!< CIStructRet
	CITEXTURE_RET = 147458u, //!< CITextureRet
	CIBUILTIN_RET = 147459u, //!< CIBuiltinRet
	CIMATRIX_RET = 147460u, //!< CIMatrixRet
	CISAMPLER_RET = 147461u, //!< CISamplerRet
	CIIMAGEBLOCK_RET = 147462u, //!< CIImageblockRet

	BUFFER_ARG = 262144u, //!< BufferArg
	SAMPLER_ARG = 262145u, //!< SamplerArg
	TEXTURE_ARG = 262146u, //!< TextureArg
	CONSTANT_ARG = 262147u, //!< ConstantArg
	INDIRECT_BUFFER_ARG = 262148u, //!< IndirectBufferArg
	INDIRECT_CONSTANT_ARG = 262149u, //!< IndirectConstantArg
	COMMAND_BUFFER_ARG = 262150u, //!< CommandBufferArg
	COMPUTE_PIPELINE_STATE_ARG = 262151u, //!< ComputePipelineStateArg
	RENDER_PIPELINE_STATE_ARG = 262152u, //!< RenderPipelineStateArg
	VISIBLE_FUNCTION_TABLE_ARG = 262153u, //!< VisibleFunctionTableArg
	INTERSECTION_FUNCTION_TABLE_ARG = 262154u, //!< IntersectionFunctionTableArg
	INSTANCE_ACCELERATION_STRUCTURE_ARG = 262155u, //!< InstanceAccelerationStructureArg
	PRIMITIVE_ACCELERATION_STRUCTURE_ARG = 262156u, //!< PrimitiveAccelerationStructureArg
	BUFFER_STRIDE_ARG = 262157u, //!< BufferStrideArg
	DEPTH_STENCIL_STATE_ARG = 262158u, //!< DepthStencilStateArg
	FUNCTION_HANDLE_ARG = 262159u, //!< FunctionHandleArg
	TENSOR_ARG = 262160u, //!< TensorArg

	THREAD_POSITION_IN_GRID_ARG = 266240u, //!< ThreadPositionInGridArg
	THREADS_PER_GRID_ARG = 266241u, //!< ThreadsPerGridArg
	THREADGROUP_POSITION_IN_GRID_ARG = 266242u, //!< ThreadgroupPositionInGridArg
	THREADGROUPS_PER_GRID_ARG = 266243u, //!< ThreadgroupsPerGridArg
	THREAD_POSITION_IN_THREADGROUP_ARG = 266244u, //!< ThreadPositionInThreadgroupArg
	THREADS_PER_THREADGROUP_ARG = 266245u, //!< ThreadsPerThreadgroupArg
	DISPATCH_THREADS_PER_THREADGROUP_ARG = 266246u, //!< DispatchThreadsPerThreadgroupArg
	THREAD_INDEX_IN_THREADGROUP_ARG = 266247u, //!< ThreadIndexInThreadgroupArg
	THREAD_EXECUTION_WIDTH_ARG = 266248u, //!< ThreadExecutionWidthArg
	STAGE_IN_ARG = 266249u, //!< StageInArg
	STAGE_IN_GRID_ORIGIN_ARG = 266250u, //!< StageInGridOriginArg
	STAGE_IN_GRID_SIZE_ARG = 266251u, //!< StageInGridSizeArg
	THREAD_INDEX_IN_SIMDGROUP_ARG = 266252u, //!< ThreadIndexInSimdgroupArg
	THREADS_PER_SIMDGROUP_ARG = 266253u, //!< ThreadsPerSimdgroupArg
	SIMDGROUP_INDEX_IN_THREADGROUP_ARG = 266254u, //!< SimdgroupIndexInThreadgroupArg
	SIMDGROUPS_PER_THREADGROUP_ARG = 266255u, //!< SimdgroupsPerThreadgroupArg
	DISPATCH_SIMDGROUPS_PER_THREADGROUP_ARG = 266256u, //!< DispatchSimdgroupsPerThreadgroupArg
	THREAD_INDEX_IN_QUADGROUP_ARG = 266257u, //!< ThreadIndexInQuadgroupArg
	QUADGROUP_INDEX_IN_THREADGROUP_ARG = 266258u, //!< QuadgroupIndexInThreadgroupArg
	QUADGROUPS_PER_THREADGROUP_ARG = 266259u, //!< QuadgroupsPerThreadgroupArg
	DISPATCH_QUADGROUPS_PER_THREADGROUP_ARG = 266260u, //!< DispatchQuadgroupsPerThreadgroupArg

	BASE_INSTANCE_ARG = 270336u, //!< BaseInstanceArg
	BASE_VERTEX_ARG = 270337u, //!< BaseVertexArg
	INSTANCE_ID_ARG = 270338u, //!< InstanceIDArg
	VERTEX_ID_ARG = 270339u, //!< VertexIDArg
	VERTEX_INPUT_ARG = 270340u, //!< VertexInputArg
	CONTROL_POINT_INDEX_BUFFER_ARG = 270341u, //!< ControlPointIndexBufferArg
	PATCH_ID_ARG = 270342u, //!< PatchIDArg
	POSITION_IN_PATCH_ARG = 270343u, //!< PositionInPatchArg
	PATCH_INPUT_ARG = 270344u, //!< PatchInputArg
	CONTROL_POINT_INPUT_ARG = 270345u, //!< ControlPointInputArg
	CONTROL_POINT_FIELD = 270346u, //!< ControlPointField

	AMPLIFICATION_COUNT_ARG = 270350u, //!< AmplificationCountArg
	AMPLIFICATION_ID_ARG = 270351u, //!< AmplificationIDArg

	FRAGMENT_INPUT_ARG = 274432u, //!< FragmentInputArg
	FRONT_FACING_ARG = 274433u, //!< FrontFacingArg
	POSITION_ARG = 274434u, //!< PositionArg
	POINT_COORD_ARG = 274435u, //!< PointCoordArg
	RENDER_TARGET_ARG = 274436u, //!< RenderTargetArg
	RENDER_TARGET_ARRAY_INDEX_ARG = 274437u, //!< RenderTargetArrayIndexArg
	SAMPLE_ID_ARG = 274438u, //!< SampleIDArg
	SAMPLE_MASK_ARG = 274439u, //!< SampleMaskArg
	VIEWPORT_ARRAY_INDEX_ARG = 274440u, //!< ViewportArrayIndexArg

	BARYCENTRIC_COORD_ARG = 274445u, //!< BarycentricCoordArg
	PRIMITIVE_ID_ARG = 274446u, //!< PrimitiveIDArg

	PIXEL_POSITION_IN_TILE_ARG = 278528u, //!< PixelPositionInTileArg
	PIXELS_PER_TILE_ARG = 278529u, //!< PixelsPerTileArg
	TILE_INDEX_ARG = 278530u, //!< TileIndexArg
	IMAGEBLOCK_ARG = 278531u, //!< ImageblockArg
	IMAGEBLOCK_DATA_ARG = 278532u, //!< ImageblockDataArg

	PAYLOAD_ARG = 282624u, //!< PayloadArg
	ORIGIN_ARG = 282625u, //!< OriginArg
	DIRECTION_ARG = 282626u, //!< DirectionArg
	MIN_DISTANCE_ARG = 282627u, //!< MinDistanceArg
	MAX_DISTANCE_ARG = 282628u, //!< MaxDistanceArg
	DISTANCE_ARG = 282629u, //!< DistanceArg
	WORLD_SPACE_ORIGIN_ARG = 282630u, //!< WorldSpaceOriginArg
	WORLD_SPACE_DIRECTION_ARG = 282631u, //!< WorldSpaceDirectionArg
	GEOMETRY_ID_ARG = 282632u, //!< GeometryIDArg
	USER_INSTANCE_ID_ARG = 282633u, //!< UserInstanceIDArg
	GEOMETRY_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG = 282634u, //!< GeometryIntersectionFunctionTableOffsetArg
	INSTANCE_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG = 282635u, //!< InstanceIntersectionFunctionTableOffsetArg
	OPAQUE_PRIMITIVE_ARG = 282636u, //!< OpaquePrimitiveArg
	OBJECT_TO_WORLD_TRANSFORM_ARG = 282637u, //!< ObjectToWorldTransformArg
	WORLD_TO_OBJECT_TRANSFORM_ARG = 282638u, //!< WorldToObjectTransformArg
	TIME_ARG = 282639u, //!< TimeArg
	KEY_FRAME_COUNT_ARG = 282640u, //!< KeyFrameCountArg
	MOTION_START_TIME_ARG = 282641u, //!< MotionStartTimeArg
	MOTION_END_TIME_ARG = 282642u, //!< MotionEndTimeArg
	PRIMITIVE_DATA_ARG = 282643u, //!< PrimitiveDataArg
	INSTANCE_ID_COUNT_ARG = 282644u, //!< InstanceIDCountArg
	USER_INSTANCE_ID_COUNT_ARG = 282645u, //!< UserInstanceIDCountArg
	CURVE_PARAMETER_ARG = 282646u, //!< CurveParameterArg
	FUNCTION_ID_ARG = 282647u, //!< FunctionIDArg
	USER_DATA_BUFFER_ARG = 282648u, //!< UserDataBufferArg

	MESH_ARG = 286720u, //!< MeshArg

	MESH_GRID_PROPERTIES_ARG = 290816u, //!< MeshGridPropertiesArg

	CIARRAY_ARG = 294912u, //!< CIArrayArg
	CIPADDING_ARG = 294913u, //!< CIPaddingArg
	CIPOINTER_ARG = 294914u, //!< CIPointerArg
	CISTRUCT_ARG = 294915u, //!< CIStructArg
	CITEXTURE_ARG = 294916u, //!< CITextureArg
	CIBUILTIN_ARG = 294917u, //!< CIBuiltinArg
	CIMATRIX_ARG = 294918u, //!< CIMatrixArg
	CISAMPLER_ARG = 294919u, //!< CISamplerArg
	CIIMAGEBLOCK_ARG = 294920u, //!< CIImageblockArg

	FUNCTION_CONSTANT = 524288u, //!< FunctionConstant
	STRUCT_TYPE_INFO = 524289u, //!< StructTypeInfo
	STITCHING_ARGUMENT = 524290u, //!< StitchingArgument
	MESH_TYPE_INFO = 524291u, //!< MeshTypeInfo
	GLOBAL_BINDING = 524292u, //!< GlobalBinding
	INLINE_TYPE_INFO = 524293u, //!< InlineTypeInfo
	VISIBLE_FUNCTION_REFERENCE = 524294u, //!< VisibleFunctionReference

	OPAQUE_TYPE = 528384u, //!< OpaqueType
	VOID_TYPE = 528385u, //!< VoidType
	BOOL_TYPE = 528386u, //!< BoolType
	CHAR_TYPE = 528387u, //!< CharType
	UCHAR_TYPE = 528388u, //!< UCharType
	SHORT_TYPE = 528389u, //!< ShortType
	USHORT_TYPE = 528390u, //!< UShortType
	INT_TYPE = 528391u, //!< IntType
	UINT_TYPE = 528392u, //!< UIntType
	LONG_TYPE = 528393u, //!< LongType
	ULONG_TYPE = 528394u, //!< ULongType
	LLONG_TYPE = 528395u, //!< LLongType
	ULLONG_TYPE = 528396u, //!< ULLongType
	HALF_TYPE = 528397u, //!< HalfType
	FLOAT_TYPE = 528398u, //!< FloatType
	DOUBLE_TYPE = 528399u, //!< DoubleType
	BFLOAT_TYPE = 528400u, //!< BFloatType
	VECTOR_TYPE = 528401u, //!< VectorType
	PACKED_VECTOR_TYPE = 528402u, //!< PackedVectorType
	MATRIX_TYPE = 528403u, //!< MatrixType
	FUNCTION_TYPE = 528404u, //!< FunctionType
	POINTER_TYPE = 528405u, //!< PointerType
	LVALUE_REFERENCE_TYPE = 528406u, //!< LValueReferenceType
	RVALUE_REFERENCE_TYPE = 528407u, //!< RValueReferenceType
	ARRAY_TYPE = 528408u, //!< ArrayType
	ENUM_TYPE = 528409u, //!< EnumType
	RECORD_BASE = 528410u, //!< RecordBase
	RECORD_FIELD = 528411u, //!< RecordField
	STRUCT_TYPE = 528412u, //!< StructType
	UNION_TYPE = 528413u, //!< UnionType

	ARRAY_OF_TYPE = 532480u, //!< ArrayOfType
	ARRAY_REF_OF_TYPE = 532481u, //!< ArrayRefOfType
	TEXTURE1D_TYPE = 532482u, //!< Texture1dType
	TEXTURE1D_ARRAY_TYPE = 532483u, //!< Texture1dArrayType
	TEXTURE2D_TYPE = 532484u, //!< Texture2dType
	TEXTURE2D_ARRAY_TYPE = 532485u, //!< Texture2dArrayType
	TEXTURE3D_TYPE = 532486u, //!< Texture3dType
	TEXTURE_CUBE_TYPE = 532487u, //!< TextureCubeType
	TEXTURE_CUBE_ARRAY_TYPE = 532488u, //!< TextureCubeArrayType
	TEXTURE2D_MS_TYPE = 532489u, //!< Texture2dMsType
	TEXTURE2D_MS_ARRAY_TYPE = 532490u, //!< Texture2dMsArrayType
	TEXTURE_BUFFER1D_TYPE = 532491u, //!< TextureBuffer1dType
	DEPTH2D_TYPE = 532492u, //!< Depth2dType
	DEPTH2D_ARRAY_TYPE = 532493u, //!< Depth2dArrayType
	DEPTH_CUBE_TYPE = 532494u, //!< DepthCubeType
	DEPTH_CUBE_ARRAY_TYPE = 532495u, //!< DepthCubeArrayType
	DEPTH2D_MS_TYPE = 532496u, //!< Depth2dMsType
	DEPTH2D_MS_ARRAY_TYPE = 532497u, //!< Depth2dMsArrayType
	SAMPLER_TYPE = 532498u, //!< SamplerType
	PATCH_CONTROL_POINT_TYPE = 532499u, //!< PatchControlPointType
	IMAGEBLOCK_TYPE = 532500u, //!< ImageblockType
	R8UNORM_TYPE = 532501u, //!< R8UNormType
	R8SNORM_TYPE = 532502u, //!< R8SNormType
	R16UNORM_TYPE = 532503u, //!< R16UNormType
	R16SNORM_TYPE = 532504u, //!< R16SNormType
	RG8UNORM_TYPE = 532505u, //!< RG8UNormType
	RG8SNORM_TYPE = 532506u, //!< RG8SNormType
	RG16UNORM_TYPE = 532507u, //!< RG16UNormType
	RG16SNORM_TYPE = 532508u, //!< RG16SNormType
	RGBA8UNORM_TYPE = 532509u, //!< RGBA8UNormType
	RGBA8SNORM_TYPE = 532510u, //!< RGBA8SNormType
	RGBA16UNORM_TYPE = 532511u, //!< RGBA16UNormType
	RGBA16SNORM_TYPE = 532512u, //!< RGBA16SNormType
	SRGBA8UNORM_TYPE = 532513u, //!< SRGBA8UNormType
	RGB10A2_TYPE = 532514u, //!< RGB10A2Type
	RG11B10F_TYPE = 532515u, //!< RG11B10FType
	RGB9E5_TYPE = 532516u, //!< RGB9E5Type
	COMMAND_BUFFER_TYPE = 532517u, //!< CommandBufferType
	COMPUTE_PIPELINE_STATE_TYPE = 532518u, //!< ComputePipelineStateType
	RENDER_PIPELINE_STATE_TYPE = 532519u, //!< RenderPipelineStateType
	INTERPOLANT_TYPE = 532520u, //!< InterpolantType
	VISIBLE_FUNCTION_TABLE_TYPE = 532521u, //!< VisibleFunctionTableType
	INTERSECTION_FUNCTION_TABLE_TYPE = 532522u, //!< IntersectionFunctionTableType
	ACCELERATION_STRUCTURE_TYPE = 532523u, //!< AccelerationStructureType
	MESH_TYPE = 532524u, //!< MeshType
	MESH_GRID_PROPERTIES_TYPE = 532525u, //!< MeshGridPropertiesType

	VERTEX_VALUE_TYPE = 532527u, //!< VertexValueType
	DEPTH_STENCIL_STATE_TYPE = 532528u, //!< DepthStencilStateType
	FUNCTION_HANDLE_TYPE = 532529u, //!< FunctionHandleType
	INTERSECTION_FUNCTION_HANDLE_TYPE = 532530u, //!< IntersectionFunctionHandleType
	EXTENTS_TYPE = 532531u, //!< ExtentsType
	TENSOR_TYPE = 532532u, //!< TensorType

	ADDRESS_SPACE_TYPE_QUAL = 536576u, //!< AddressSpaceTypeQual

	CLIP_DISTANCE_ATTR = 540672u, //!< ClipDistanceAttr
	FUNCTION_CONSTANT_PREDICATE_ATTR = 540673u, //!< FunctionConstantPredicateAttr
	LOCATION_INDEX_ATTR = 540674u, //!< LocationIndexAttr
	POINT_SIZE_ATTR = 540675u, //!< PointSizeAttr
	POSITION_ATTR = 540676u, //!< PositionAttr
	PRIMITIVE_CULLED_ATTR = 540677u, //!< PrimitiveCulledAttr
	PRIMITIVE_ID_ATTR = 540678u, //!< PrimitiveIDAttr
	RENDER_TARGET_ATTR = 540679u, //!< RenderTargetAttr
	RENDER_TARGET_ARRAY_INDEX_ATTR = 540680u, //!< RenderTargetArrayIndexAttr
	VIEWPORT_ARRAY_INDEX_ATTR = 540681u, //!< ViewportArrayIndexAttr
	USER_ATTR = 540682u, //!< UserAttr
	INVARIANT_ATTR = 540683u, //!< InvariantAttr
	SHARED_ATTR = 540684u, //!< SharedAttr

	MESH_EMULATION_VALUE_GROUP = 544768u, //!< MeshEmulationValueGroup
	MESH_EMULATION_BLOCK = 544769u, //!< MeshEmulationBlock
	MESH_EMULATION_MESH_LAYOUT = 544770u, //!< MeshEmulationMeshLayout
	MESH_EMULATION_MESH_KERNEL = 544771u, //!< MeshEmulationMeshKernel
	MESH_EMULATION_MESH_VERTEX = 544772u, //!< MeshEmulationMeshVertex
	MESH_EMULATION_OBJECT_KERNEL = 544773u, //!< MeshEmulationObjectKernel
	MESH_EMULATION_FRAGMENT_ANALYSIS_RESULT = 544774u, //!< MeshEmulationFragmentAnalysisResult
};

static inline const char* node_type_to_string(const NODE_TYPE value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case NODE_TYPE::NONE:
			return "none";
		case NODE_TYPE::FRAGMENT_FUNCTION:
			return "fragment-function";
		case NODE_TYPE::KERNEL_FUNCTION:
			return "kernel-function";
		case NODE_TYPE::VERTEX_FUNCTION:
			return "vertex-function";
		case NODE_TYPE::VISIBLE_FUNCTION:
			return "visible-function";
		case NODE_TYPE::INTERSECTION_FUNCTION:
			return "intersection-function";
		case NODE_TYPE::MESH_FUNCTION:
			return "mesh-function";
		case NODE_TYPE::OBJECT_FUNCTION:
			return "object-function";
		case NODE_TYPE::CI_FUNCTION:
			return "ci-function";
		case NODE_TYPE::VEC_TYPE_HINT_FN_ATTR:
			return "vec-type-hint-fn-attr";
		case NODE_TYPE::WORKGROUP_SIZE_FN_ATTR:
			return "workgroup-size-fn-attr";
		case NODE_TYPE::WORKGROUP_SIZE_HINT_FN_ATTR:
			return "workgroup-size-hint-fn-attr";
		case NODE_TYPE::WORKGROUP_MAX_SIZE_FN_ATTR:
			return "workgroup-max-size-fn-attr";
		case NODE_TYPE::PATCH_FN_ATTR:
			return "patch-fn-attr";
		case NODE_TYPE::MAX_MESH_WORKGROUPS_FN_ATTR:
			return "max-mesh-workgroups-fn-attr";
		case NODE_TYPE::USER_ANNOTATION_FN_ATTR:
			return "user-annotation-fn-attr";
		case NODE_TYPE::CLIP_DISTANCE_RET:
			return "clip-distance-ret";
		case NODE_TYPE::POINT_SIZE_RET:
			return "point-size-ret";
		case NODE_TYPE::POSITION_RET:
			return "position-ret";
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_RET:
			return "render-target-array-index-ret";
		case NODE_TYPE::VERTEX_OUTPUT_RET:
			return "vertex-output-ret";
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_RET:
			return "viewport-array-index-ret";
		case NODE_TYPE::RENDER_TARGET_RET:
			return "render-target-ret";
		case NODE_TYPE::DEPTH_RET:
			return "depth-ret";
		case NODE_TYPE::STENCIL_RET:
			return "stencil-ret";
		case NODE_TYPE::SAMPLE_MASK_RET:
			return "sample-mask-ret";
		case NODE_TYPE::IMAGEBLOCK_DATA_RET:
			return "imageblock-data-ret";
		case NODE_TYPE::ACCEPT_INTERSECTION_RET:
			return "accept-intersection-ret";
		case NODE_TYPE::CONTINUE_SEARCH_RET:
			return "continue-search-ret";
		case NODE_TYPE::DISTANCE_RET:
			return "distance-ret";
		case NODE_TYPE::MESH_PRIMITIVE_DATA_RET:
			return "mesh-primitive-data-ret";
		case NODE_TYPE::MESH_VERTEX_DATA_RET:
			return "mesh-vertex-data-ret";
		case NODE_TYPE::PRIMITIVE_CULLED_RET:
			return "primitive-culled-ret";
		case NODE_TYPE::PRIMITIVE_ID_RET:
			return "primitive-id-ret";
		case NODE_TYPE::CIPOINTER_RET:
			return "cipointer-ret";
		case NODE_TYPE::CISTRUCT_RET:
			return "cistruct-ret";
		case NODE_TYPE::CITEXTURE_RET:
			return "citexture-ret";
		case NODE_TYPE::CIBUILTIN_RET:
			return "cibuiltin-ret";
		case NODE_TYPE::CIMATRIX_RET:
			return "cimatrix-ret";
		case NODE_TYPE::CISAMPLER_RET:
			return "cisampler-ret";
		case NODE_TYPE::CIIMAGEBLOCK_RET:
			return "ciimageblock-ret";
		case NODE_TYPE::BUFFER_ARG:
			return "buffer-arg";
		case NODE_TYPE::SAMPLER_ARG:
			return "sampler-arg";
		case NODE_TYPE::TEXTURE_ARG:
			return "texture-arg";
		case NODE_TYPE::CONSTANT_ARG:
			return "constant-arg";
		case NODE_TYPE::INDIRECT_BUFFER_ARG:
			return "indirect-buffer-arg";
		case NODE_TYPE::INDIRECT_CONSTANT_ARG:
			return "indirect-constant-arg";
		case NODE_TYPE::COMMAND_BUFFER_ARG:
			return "command-buffer-arg";
		case NODE_TYPE::COMPUTE_PIPELINE_STATE_ARG:
			return "compute-pipeline-state-arg";
		case NODE_TYPE::RENDER_PIPELINE_STATE_ARG:
			return "render-pipeline-state-arg";
		case NODE_TYPE::VISIBLE_FUNCTION_TABLE_ARG:
			return "visible-function-table-arg";
		case NODE_TYPE::INTERSECTION_FUNCTION_TABLE_ARG:
			return "intersection-function-table-arg";
		case NODE_TYPE::INSTANCE_ACCELERATION_STRUCTURE_ARG:
			return "instance-acceleration-structure-arg";
		case NODE_TYPE::PRIMITIVE_ACCELERATION_STRUCTURE_ARG:
			return "primitive-acceleration-structure-arg";
		case NODE_TYPE::BUFFER_STRIDE_ARG:
			return "buffer-stride-arg";
		case NODE_TYPE::DEPTH_STENCIL_STATE_ARG:
			return "depth-stencil-state-arg";
		case NODE_TYPE::FUNCTION_HANDLE_ARG:
			return "function-handle-arg";
		case NODE_TYPE::TENSOR_ARG:
			return "tensor-arg";
		case NODE_TYPE::THREAD_POSITION_IN_GRID_ARG:
			return "thread-position-in-grid-arg";
		case NODE_TYPE::THREADS_PER_GRID_ARG:
			return "threads-per-grid-arg";
		case NODE_TYPE::THREADGROUP_POSITION_IN_GRID_ARG:
			return "threadgroup-position-in-grid-arg";
		case NODE_TYPE::THREADGROUPS_PER_GRID_ARG:
			return "threadgroups-per-grid-arg";
		case NODE_TYPE::THREAD_POSITION_IN_THREADGROUP_ARG:
			return "thread-position-in-threadgroup-arg";
		case NODE_TYPE::THREADS_PER_THREADGROUP_ARG:
			return "threads-per-threadgroup-arg";
		case NODE_TYPE::DISPATCH_THREADS_PER_THREADGROUP_ARG:
			return "dispatch-threads-per-threadgroup-arg";
		case NODE_TYPE::THREAD_INDEX_IN_THREADGROUP_ARG:
			return "thread-index-in-threadgroup-arg";
		case NODE_TYPE::THREAD_EXECUTION_WIDTH_ARG:
			return "thread-execution-width-arg";
		case NODE_TYPE::STAGE_IN_ARG:
			return "stage-in-arg";
		case NODE_TYPE::STAGE_IN_GRID_ORIGIN_ARG:
			return "stage-in-grid-origin-arg";
		case NODE_TYPE::STAGE_IN_GRID_SIZE_ARG:
			return "stage-in-grid-size-arg";
		case NODE_TYPE::THREAD_INDEX_IN_SIMDGROUP_ARG:
			return "thread-index-in-simdgroup-arg";
		case NODE_TYPE::THREADS_PER_SIMDGROUP_ARG:
			return "threads-per-simdgroup-arg";
		case NODE_TYPE::SIMDGROUP_INDEX_IN_THREADGROUP_ARG:
			return "simdgroup-index-in-threadgroup-arg";
		case NODE_TYPE::SIMDGROUPS_PER_THREADGROUP_ARG:
			return "simdgroups-per-threadgroup-arg";
		case NODE_TYPE::DISPATCH_SIMDGROUPS_PER_THREADGROUP_ARG:
			return "dispatch-simdgroups-per-threadgroup-arg";
		case NODE_TYPE::THREAD_INDEX_IN_QUADGROUP_ARG:
			return "thread-index-in-quadgroup-arg";
		case NODE_TYPE::QUADGROUP_INDEX_IN_THREADGROUP_ARG:
			return "quadgroup-index-in-threadgroup-arg";
		case NODE_TYPE::QUADGROUPS_PER_THREADGROUP_ARG:
			return "quadgroups-per-threadgroup-arg";
		case NODE_TYPE::DISPATCH_QUADGROUPS_PER_THREADGROUP_ARG:
			return "dispatch-quadgroups-per-threadgroup-arg";
		case NODE_TYPE::BASE_INSTANCE_ARG:
			return "base-instance-arg";
		case NODE_TYPE::BASE_VERTEX_ARG:
			return "base-vertex-arg";
		case NODE_TYPE::INSTANCE_ID_ARG:
			return "instance-id-arg";
		case NODE_TYPE::VERTEX_ID_ARG:
			return "vertex-id-arg";
		case NODE_TYPE::VERTEX_INPUT_ARG:
			return "vertex-input-arg";
		case NODE_TYPE::CONTROL_POINT_INDEX_BUFFER_ARG:
			return "control-point-index-buffer-arg";
		case NODE_TYPE::PATCH_ID_ARG:
			return "patch-id-arg";
		case NODE_TYPE::POSITION_IN_PATCH_ARG:
			return "position-in-patch-arg";
		case NODE_TYPE::PATCH_INPUT_ARG:
			return "patch-input-arg";
		case NODE_TYPE::CONTROL_POINT_INPUT_ARG:
			return "control-point-input-arg";
		case NODE_TYPE::CONTROL_POINT_FIELD:
			return "control-point-field";
		case NODE_TYPE::AMPLIFICATION_COUNT_ARG:
			return "amplification-count-arg";
		case NODE_TYPE::AMPLIFICATION_ID_ARG:
			return "amplification-id-arg";
		case NODE_TYPE::FRAGMENT_INPUT_ARG:
			return "fragment-input-arg";
		case NODE_TYPE::FRONT_FACING_ARG:
			return "front-facing-arg";
		case NODE_TYPE::POSITION_ARG:
			return "position-arg";
		case NODE_TYPE::POINT_COORD_ARG:
			return "point-coord-arg";
		case NODE_TYPE::RENDER_TARGET_ARG:
			return "render-target-arg";
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ARG:
			return "render-target-array-index-arg";
		case NODE_TYPE::SAMPLE_ID_ARG:
			return "sample-id-arg";
		case NODE_TYPE::SAMPLE_MASK_ARG:
			return "sample-mask-arg";
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_ARG:
			return "viewport-array-index-arg";
		case NODE_TYPE::BARYCENTRIC_COORD_ARG:
			return "barycentric-coord-arg";
		case NODE_TYPE::PRIMITIVE_ID_ARG:
			return "primitive-id-arg";
		case NODE_TYPE::PIXEL_POSITION_IN_TILE_ARG:
			return "pixel-position-in-tile-arg";
		case NODE_TYPE::PIXELS_PER_TILE_ARG:
			return "pixels-per-tile-arg";
		case NODE_TYPE::TILE_INDEX_ARG:
			return "tile-index-arg";
		case NODE_TYPE::IMAGEBLOCK_ARG:
			return "imageblock-arg";
		case NODE_TYPE::IMAGEBLOCK_DATA_ARG:
			return "imageblock-data-arg";
		case NODE_TYPE::PAYLOAD_ARG:
			return "payload-arg";
		case NODE_TYPE::ORIGIN_ARG:
			return "origin-arg";
		case NODE_TYPE::DIRECTION_ARG:
			return "direction-arg";
		case NODE_TYPE::MIN_DISTANCE_ARG:
			return "min-distance-arg";
		case NODE_TYPE::MAX_DISTANCE_ARG:
			return "max-distance-arg";
		case NODE_TYPE::DISTANCE_ARG:
			return "distance-arg";
		case NODE_TYPE::WORLD_SPACE_ORIGIN_ARG:
			return "world-space-origin-arg";
		case NODE_TYPE::WORLD_SPACE_DIRECTION_ARG:
			return "world-space-direction-arg";
		case NODE_TYPE::GEOMETRY_ID_ARG:
			return "geometry-id-arg";
		case NODE_TYPE::USER_INSTANCE_ID_ARG:
			return "user-instance-id-arg";
		case NODE_TYPE::GEOMETRY_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG:
			return "geometry-intersection-function-table-offset-arg";
		case NODE_TYPE::INSTANCE_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG:
			return "instance-intersection-function-table-offset-arg";
		case NODE_TYPE::OPAQUE_PRIMITIVE_ARG:
			return "opaque-primitive-arg";
		case NODE_TYPE::OBJECT_TO_WORLD_TRANSFORM_ARG:
			return "object-to-world-transform-arg";
		case NODE_TYPE::WORLD_TO_OBJECT_TRANSFORM_ARG:
			return "world-to-object-transform-arg";
		case NODE_TYPE::TIME_ARG:
			return "time-arg";
		case NODE_TYPE::KEY_FRAME_COUNT_ARG:
			return "key-frame-count-arg";
		case NODE_TYPE::MOTION_START_TIME_ARG:
			return "motion-start-time-arg";
		case NODE_TYPE::MOTION_END_TIME_ARG:
			return "motion-end-time-arg";
		case NODE_TYPE::PRIMITIVE_DATA_ARG:
			return "primitive-data-arg";
		case NODE_TYPE::INSTANCE_ID_COUNT_ARG:
			return "instance-id-count-arg";
		case NODE_TYPE::USER_INSTANCE_ID_COUNT_ARG:
			return "user-instance-id-count-arg";
		case NODE_TYPE::CURVE_PARAMETER_ARG:
			return "curve-parameter-arg";
		case NODE_TYPE::FUNCTION_ID_ARG:
			return "function-id-arg";
		case NODE_TYPE::USER_DATA_BUFFER_ARG:
			return "user-data-buffer-arg";
		case NODE_TYPE::MESH_ARG:
			return "mesh-arg";
		case NODE_TYPE::MESH_GRID_PROPERTIES_ARG:
			return "mesh-grid-properties-arg";
		case NODE_TYPE::CIARRAY_ARG:
			return "ciarray-arg";
		case NODE_TYPE::CIPADDING_ARG:
			return "cipadding-arg";
		case NODE_TYPE::CIPOINTER_ARG:
			return "cipointer-arg";
		case NODE_TYPE::CISTRUCT_ARG:
			return "cistruct-arg";
		case NODE_TYPE::CITEXTURE_ARG:
			return "citexture-arg";
		case NODE_TYPE::CIBUILTIN_ARG:
			return "cibuiltin-arg";
		case NODE_TYPE::CIMATRIX_ARG:
			return "cimatrix-arg";
		case NODE_TYPE::CISAMPLER_ARG:
			return "cisampler-arg";
		case NODE_TYPE::CIIMAGEBLOCK_ARG:
			return "ciimageblock-arg";
		case NODE_TYPE::FUNCTION_CONSTANT:
			return "function-constant";
		case NODE_TYPE::STRUCT_TYPE_INFO:
			return "struct-type-info";
		case NODE_TYPE::STITCHING_ARGUMENT:
			return "stitching-argument";
		case NODE_TYPE::MESH_TYPE_INFO:
			return "mesh-type-info";
		case NODE_TYPE::GLOBAL_BINDING:
			return "global-binding";
		case NODE_TYPE::INLINE_TYPE_INFO:
			return "inline-type-info";
		case NODE_TYPE::VISIBLE_FUNCTION_REFERENCE:
			return "visible-function-reference";
		case NODE_TYPE::OPAQUE_TYPE:
			return "opaque-type";
		case NODE_TYPE::VOID_TYPE:
			return "void-type";
		case NODE_TYPE::BOOL_TYPE:
			return "bool-type";
		case NODE_TYPE::CHAR_TYPE:
			return "char-type";
		case NODE_TYPE::UCHAR_TYPE:
			return "uchar-type";
		case NODE_TYPE::SHORT_TYPE:
			return "short-type";
		case NODE_TYPE::USHORT_TYPE:
			return "ushort-type";
		case NODE_TYPE::INT_TYPE:
			return "int-type";
		case NODE_TYPE::UINT_TYPE:
			return "uint-type";
		case NODE_TYPE::LONG_TYPE:
			return "long-type";
		case NODE_TYPE::ULONG_TYPE:
			return "ulong-type";
		case NODE_TYPE::LLONG_TYPE:
			return "llong-type";
		case NODE_TYPE::ULLONG_TYPE:
			return "ullong-type";
		case NODE_TYPE::HALF_TYPE:
			return "half-type";
		case NODE_TYPE::FLOAT_TYPE:
			return "float-type";
		case NODE_TYPE::DOUBLE_TYPE:
			return "double-type";
		case NODE_TYPE::BFLOAT_TYPE:
			return "bfloat-type";
		case NODE_TYPE::VECTOR_TYPE:
			return "vector-type";
		case NODE_TYPE::PACKED_VECTOR_TYPE:
			return "packed-vector-type";
		case NODE_TYPE::MATRIX_TYPE:
			return "matrix-type";
		case NODE_TYPE::FUNCTION_TYPE:
			return "function-type";
		case NODE_TYPE::POINTER_TYPE:
			return "pointer-type";
		case NODE_TYPE::LVALUE_REFERENCE_TYPE:
			return "lvalue-reference-type";
		case NODE_TYPE::RVALUE_REFERENCE_TYPE:
			return "rvalue-reference-type";
		case NODE_TYPE::ARRAY_TYPE:
			return "array-type";
		case NODE_TYPE::ENUM_TYPE:
			return "enum-type";
		case NODE_TYPE::RECORD_BASE:
			return "record-base";
		case NODE_TYPE::RECORD_FIELD:
			return "record-field";
		case NODE_TYPE::STRUCT_TYPE:
			return "struct-type";
		case NODE_TYPE::UNION_TYPE:
			return "union-type";
		case NODE_TYPE::ARRAY_OF_TYPE:
			return "array-of-type";
		case NODE_TYPE::ARRAY_REF_OF_TYPE:
			return "array-ref-of-type";
		case NODE_TYPE::TEXTURE1D_TYPE:
			return "texture1d-type";
		case NODE_TYPE::TEXTURE1D_ARRAY_TYPE:
			return "texture1d-array-type";
		case NODE_TYPE::TEXTURE2D_TYPE:
			return "texture2d-type";
		case NODE_TYPE::TEXTURE2D_ARRAY_TYPE:
			return "texture2d-array-type";
		case NODE_TYPE::TEXTURE3D_TYPE:
			return "texture3d-type";
		case NODE_TYPE::TEXTURE_CUBE_TYPE:
			return "texture-cube-type";
		case NODE_TYPE::TEXTURE_CUBE_ARRAY_TYPE:
			return "texture-cube-array-type";
		case NODE_TYPE::TEXTURE2D_MS_TYPE:
			return "texture2d-ms-type";
		case NODE_TYPE::TEXTURE2D_MS_ARRAY_TYPE:
			return "texture2d-ms-array-type";
		case NODE_TYPE::TEXTURE_BUFFER1D_TYPE:
			return "texture-buffer1d-type";
		case NODE_TYPE::DEPTH2D_TYPE:
			return "depth2d-type";
		case NODE_TYPE::DEPTH2D_ARRAY_TYPE:
			return "depth2d-array-type";
		case NODE_TYPE::DEPTH_CUBE_TYPE:
			return "depth-cube-type";
		case NODE_TYPE::DEPTH_CUBE_ARRAY_TYPE:
			return "depth-cube-array-type";
		case NODE_TYPE::DEPTH2D_MS_TYPE:
			return "depth2d-ms-type";
		case NODE_TYPE::DEPTH2D_MS_ARRAY_TYPE:
			return "depth2d-ms-array-type";
		case NODE_TYPE::SAMPLER_TYPE:
			return "sampler-type";
		case NODE_TYPE::PATCH_CONTROL_POINT_TYPE:
			return "patch-control-point-type";
		case NODE_TYPE::IMAGEBLOCK_TYPE:
			return "imageblock-type";
		case NODE_TYPE::R8UNORM_TYPE:
			return "r8unorm-type";
		case NODE_TYPE::R8SNORM_TYPE:
			return "r8snorm-type";
		case NODE_TYPE::R16UNORM_TYPE:
			return "r16unorm-type";
		case NODE_TYPE::R16SNORM_TYPE:
			return "r16snorm-type";
		case NODE_TYPE::RG8UNORM_TYPE:
			return "rg8unorm-type";
		case NODE_TYPE::RG8SNORM_TYPE:
			return "rg8snorm-type";
		case NODE_TYPE::RG16UNORM_TYPE:
			return "rg16unorm-type";
		case NODE_TYPE::RG16SNORM_TYPE:
			return "rg16snorm-type";
		case NODE_TYPE::RGBA8UNORM_TYPE:
			return "rgba8unorm-type";
		case NODE_TYPE::RGBA8SNORM_TYPE:
			return "rgba8snorm-type";
		case NODE_TYPE::RGBA16UNORM_TYPE:
			return "rgba16unorm-type";
		case NODE_TYPE::RGBA16SNORM_TYPE:
			return "rgba16snorm-type";
		case NODE_TYPE::SRGBA8UNORM_TYPE:
			return "srgba8unorm-type";
		case NODE_TYPE::RGB10A2_TYPE:
			return "rgb10a2-type";
		case NODE_TYPE::RG11B10F_TYPE:
			return "rg11b10f-type";
		case NODE_TYPE::RGB9E5_TYPE:
			return "rgb9e5-type";
		case NODE_TYPE::COMMAND_BUFFER_TYPE:
			return "command-buffer-type";
		case NODE_TYPE::COMPUTE_PIPELINE_STATE_TYPE:
			return "compute-pipeline-state-type";
		case NODE_TYPE::RENDER_PIPELINE_STATE_TYPE:
			return "render-pipeline-state-type";
		case NODE_TYPE::INTERPOLANT_TYPE:
			return "interpolant-type";
		case NODE_TYPE::VISIBLE_FUNCTION_TABLE_TYPE:
			return "visible-function-table-type";
		case NODE_TYPE::INTERSECTION_FUNCTION_TABLE_TYPE:
			return "intersection-function-table-type";
		case NODE_TYPE::ACCELERATION_STRUCTURE_TYPE:
			return "acceleration-structure-type";
		case NODE_TYPE::MESH_TYPE:
			return "mesh-type";
		case NODE_TYPE::MESH_GRID_PROPERTIES_TYPE:
			return "mesh-grid-properties-type";
		case NODE_TYPE::VERTEX_VALUE_TYPE:
			return "vertex-value-type";
		case NODE_TYPE::DEPTH_STENCIL_STATE_TYPE:
			return "depth-stencil-state-type";
		case NODE_TYPE::FUNCTION_HANDLE_TYPE:
			return "function-handle-type";
		case NODE_TYPE::INTERSECTION_FUNCTION_HANDLE_TYPE:
			return "intersection-function-handle-type";
		case NODE_TYPE::EXTENTS_TYPE:
			return "extents-type";
		case NODE_TYPE::TENSOR_TYPE:
			return "tensor-type";
		case NODE_TYPE::ADDRESS_SPACE_TYPE_QUAL:
			return "address-space-type-qual";
		case NODE_TYPE::CLIP_DISTANCE_ATTR:
			return "clip-distance-attr";
		case NODE_TYPE::FUNCTION_CONSTANT_PREDICATE_ATTR:
			return "function-constant-predicate-attr";
		case NODE_TYPE::LOCATION_INDEX_ATTR:
			return "location-index-attr";
		case NODE_TYPE::POINT_SIZE_ATTR:
			return "point-size-attr";
		case NODE_TYPE::POSITION_ATTR:
			return "position-attr";
		case NODE_TYPE::PRIMITIVE_CULLED_ATTR:
			return "primitive-culled-attr";
		case NODE_TYPE::PRIMITIVE_ID_ATTR:
			return "primitive-id-attr";
		case NODE_TYPE::RENDER_TARGET_ATTR:
			return "render-target-attr";
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ATTR:
			return "render-target-array-index-attr";
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_ATTR:
			return "viewport-array-index-attr";
		case NODE_TYPE::USER_ATTR:
			return "user-attr";
		case NODE_TYPE::INVARIANT_ATTR:
			return "invariant-attr";
		case NODE_TYPE::SHARED_ATTR:
			return "shared-attr";
		case NODE_TYPE::MESH_EMULATION_VALUE_GROUP:
			return "mesh-emulation-value-group";
		case NODE_TYPE::MESH_EMULATION_BLOCK:
			return "mesh-emulation-block";
		case NODE_TYPE::MESH_EMULATION_MESH_LAYOUT:
			return "mesh-emulation-mesh-layout";
		case NODE_TYPE::MESH_EMULATION_MESH_KERNEL:
			return "mesh-emulation-mesh-kernel";
		case NODE_TYPE::MESH_EMULATION_MESH_VERTEX:
			return "mesh-emulation-mesh-vertex";
		case NODE_TYPE::MESH_EMULATION_OBJECT_KERNEL:
			return "mesh-emulation-object-kernel";
		case NODE_TYPE::MESH_EMULATION_FRAGMENT_ANALYSIS_RESULT:
			return "mesh-emulation-fragment-analysis-result";
	}
}

//! Generator
enum class GENERATOR : uint8_t {
	UNSPECIFIED = 0u, //!< Unspecified
	NATIVE_TRANSLATOR = 1u, //!< NativeTranslator
	METAL_FRAMEWORK = 2u, //!< MetalFramework
};

static inline const char* generator_to_string(const GENERATOR value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case GENERATOR::UNSPECIFIED:
			return "unspecified";
		case GENERATOR::NATIVE_TRANSLATOR:
			return "native-translator";
		case GENERATOR::METAL_FRAMEWORK:
			return "metal-framework";
	}
}

//! AccessQualifier
enum class ACCESS_QUALIFIER : uint8_t {
	INVALID = 0u, //!< Invalid
	READ = 1u, //!< Read
	WRITE = 2u, //!< Write
	READ_WRITE = 3u, //!< ReadWrite
	SAMPLE = 4u, //!< Sample
};

static inline const char* access_qualifier_to_string(const ACCESS_QUALIFIER value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case ACCESS_QUALIFIER::INVALID:
			return "invalid";
		case ACCESS_QUALIFIER::READ:
			return "read";
		case ACCESS_QUALIFIER::WRITE:
			return "write";
		case ACCESS_QUALIFIER::READ_WRITE:
			return "read-write";
		case ACCESS_QUALIFIER::SAMPLE:
			return "sample";
	}
}

//! AddressSpace
enum class ADDRESS_SPACE : uint8_t {
	PRIVATE = 0u, //!< Private
	GLOBAL = 1u, //!< Global
	CONSTANT = 2u, //!< Constant
	LOCAL = 3u, //!< Local
	THREADGROUP_IMAGEBLOCK = 4u, //!< ThreadgroupImageblock
	RAY_DATA = 5u, //!< RayData
	OBJECT_DATA = 6u, //!< ObjectData
	MESH_DATA = 7u, //!< MeshData

	INTERSECTION_RESULT = 9u, //!< IntersectionResult

	INVALID = 255u, //!< Invalid
};

static inline const char* address_space_to_string(const ADDRESS_SPACE value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case ADDRESS_SPACE::PRIVATE:
			return "private";
		case ADDRESS_SPACE::GLOBAL:
			return "global";
		case ADDRESS_SPACE::CONSTANT:
			return "constant";
		case ADDRESS_SPACE::LOCAL:
			return "local";
		case ADDRESS_SPACE::THREADGROUP_IMAGEBLOCK:
			return "threadgroup-imageblock";
		case ADDRESS_SPACE::RAY_DATA:
			return "ray-data";
		case ADDRESS_SPACE::OBJECT_DATA:
			return "object-data";
		case ADDRESS_SPACE::MESH_DATA:
			return "mesh-data";
		case ADDRESS_SPACE::INTERSECTION_RESULT:
			return "intersection-result";
		case ADDRESS_SPACE::INVALID:
			return "invalid";
	}
}

//! DepthQualifier
enum class DEPTH_QUALIFIER : uint8_t {
	INVALID = 0u, //!< Invalid
	ANY = 1u, //!< Any
	GREATER = 2u, //!< Greater
	LESS = 3u, //!< Less
};

static inline const char* depth_qualifier_to_string(const DEPTH_QUALIFIER value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case DEPTH_QUALIFIER::INVALID:
			return "invalid";
		case DEPTH_QUALIFIER::ANY:
			return "any";
		case DEPTH_QUALIFIER::GREATER:
			return "greater";
		case DEPTH_QUALIFIER::LESS:
			return "less";
	}
}

//! ImageblockLayout
enum class IMAGEBLOCK_LAYOUT : uint8_t {
	INVALID = 0u, //!< Invalid
	EXPLICIT = 1u, //!< Explicit
	IMPLICIT = 2u, //!< Implicit
};

static inline const char* imageblock_layout_to_string(const IMAGEBLOCK_LAYOUT value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case IMAGEBLOCK_LAYOUT::INVALID:
			return "invalid";
		case IMAGEBLOCK_LAYOUT::EXPLICIT:
			return "explicit";
		case IMAGEBLOCK_LAYOUT::IMPLICIT:
			return "implicit";
	}
}

//! InterpolationQualifier
enum class INTERPOLATION_QUALIFIER : uint8_t {
	NONE = 0u, //!< None
	PERSPECTIVE = 1u, //!< Perspective
	FLAT = 2u, //!< Flat
	NO_PERSPECTIVE = 3u, //!< NoPerspective
	VERTEX_VALUE = 4u, //!< VertexValue
};

static inline const char* interpolation_qualifier_to_string(const INTERPOLATION_QUALIFIER value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case INTERPOLATION_QUALIFIER::NONE:
			return "none";
		case INTERPOLATION_QUALIFIER::PERSPECTIVE:
			return "perspective";
		case INTERPOLATION_QUALIFIER::FLAT:
			return "flat";
		case INTERPOLATION_QUALIFIER::NO_PERSPECTIVE:
			return "no-perspective";
		case INTERPOLATION_QUALIFIER::VERTEX_VALUE:
			return "vertex-value";
	}
}

//! PatchKind
enum class PATCH_KIND : uint8_t {
	TRIANGLE = 0u, //!< Triangle
	QUAD = 1u, //!< Quad
};

static inline const char* patch_kind_to_string(const PATCH_KIND value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case PATCH_KIND::TRIANGLE:
			return "triangle";
		case PATCH_KIND::QUAD:
			return "quad";
	}
}

//! PrimitiveKind
enum class PRIMITIVE_KIND : uint8_t {
	TRIANGLE = 0u, //!< Triangle
	BOUNDING_BOX = 1u, //!< BoundingBox
	CURVE = 2u, //!< Curve
};

static inline const char* primitive_kind_to_string(const PRIMITIVE_KIND value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case PRIMITIVE_KIND::TRIANGLE:
			return "triangle";
		case PRIMITIVE_KIND::BOUNDING_BOX:
			return "bounding-box";
		case PRIMITIVE_KIND::CURVE:
			return "curve";
	}
}

//! RoundingMode
enum class ROUNDING_MODE : uint8_t {
	NATIVE = 0u, //!< Native
	RTNE = 1u, //!< RTNE
	RTZ = 2u, //!< RTZ
};

static inline const char* rounding_mode_to_string(const ROUNDING_MODE value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case ROUNDING_MODE::NATIVE:
			return "native";
		case ROUNDING_MODE::RTNE:
			return "rtne";
		case ROUNDING_MODE::RTZ:
			return "rtz";
	}
}

//! SamplingQualifier
enum class SAMPLING_QUALIFIER : uint8_t {
	NONE = 0u, //!< None
	CENTER = 1u, //!< Center
	CENTROID = 2u, //!< Centroid
	SAMPLE = 3u, //!< Sample
	INTERPOLATION_FUNCTION = 4u, //!< InterpolationFunction
};

static inline const char* sampling_qualifier_to_string(const SAMPLING_QUALIFIER value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case SAMPLING_QUALIFIER::NONE:
			return "none";
		case SAMPLING_QUALIFIER::CENTER:
			return "center";
		case SAMPLING_QUALIFIER::CENTROID:
			return "centroid";
		case SAMPLING_QUALIFIER::SAMPLE:
			return "sample";
		case SAMPLING_QUALIFIER::INTERPOLATION_FUNCTION:
			return "interpolation-function";
	}
}

//! TensorKind
enum class TENSOR_KIND : uint8_t {
	HANDLE = 0u, //!< Handle
	INLINE = 1u, //!< Inline
};

static inline const char* tensor_kind_to_string(const TENSOR_KIND value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case TENSOR_KIND::HANDLE:
			return "handle";
		case TENSOR_KIND::INLINE:
			return "inline";
	}
}

//! Topology
enum class TOPOLOGY : uint8_t {
	INVALID = 0u, //!< Invalid
	POINT = 1u, //!< Point
	LINE = 2u, //!< Line
	TRIANGLE = 3u, //!< Triangle
};

static inline const char* topology_to_string(const TOPOLOGY value) {
	switch (value) {
		default:
			return "<UNKNOWN>";
		case TOPOLOGY::INVALID:
			return "invalid";
		case TOPOLOGY::POINT:
			return "point";
		case TOPOLOGY::LINE:
			return "line";
		case TOPOLOGY::TRIANGLE:
			return "triangle";
	}
}

//! Version
struct version_t {
	uint32_t major; //!< [@0]
	uint32_t minor; //!< [@4]
	uint32_t sub_minor; //!< [@8]
};

//! BitfieldInfo
struct bitfield_info_t {
	uint32_t bit_offset; //!< [@0]
	uint32_t bit_size; //!< [@4]
	uint32_t storage_size; //!< [@8]
};

//! BoolValue
struct bool_value_t {
	bool value; //!< [@0]
};

//! NodeId
struct node_id_t {
	uint32_t id; //!< [@0]
};

//! UIntValue
struct uint_value_t {
	uint32_t value; //!< [@0]
};

//! AccelerationStructureType
struct node_acceleration_structure_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::ACCELERATION_STRUCTURE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	std::optional<bool> instancing; //!< [@10]
	std::optional<bool> primitive_motion; //!< [@12]
	std::optional<bool> instance_motion; //!< [@14]
};

//! AcceptIntersectionRet
struct node_accept_intersection_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::ACCEPT_INTERSECTION_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
};

//! AddressSpaceTypeQual
struct node_address_space_type_qual_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::ADDRESS_SPACE_TYPE_QUAL };
	std::optional<ADDRESS_SPACE> address_space; //!< [@4]
};

//! AmplificationCountArg
struct node_amplification_count_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::AMPLIFICATION_COUNT_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! AmplificationIDArg
struct node_amplification_id_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::AMPLIFICATION_ID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ArrayOfType
struct node_array_of_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::ARRAY_OF_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t element_type; //!< [@10] REQUIRED
	std::optional<uint32_t> num_elements; //!< [@12]
};

//! ArrayRefOfType
struct node_array_ref_of_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::ARRAY_REF_OF_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t element_type; //!< [@10] REQUIRED
};

//! ArrayType
struct node_array_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::ARRAY_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t element_type; //!< [@10] REQUIRED
	std::optional<uint32_t> num_elements; //!< [@12]
};

//! BFloatType
struct node_bfloat_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::BFLOAT_TYPE };
	uint32_t size { 2u }; //!< [@4]
	uint32_t alignment { 2u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! BarycentricCoordArg
struct node_barycentric_coord_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::BARYCENTRIC_COORD_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<SAMPLING_QUALIFIER> sampling_qualifier; //!< [@6]
	std::optional<INTERPOLATION_QUALIFIER> interpolation_qualifier; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! BaseInstanceArg
struct node_base_instance_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::BASE_INSTANCE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! BaseVertexArg
struct node_base_vertex_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::BASE_VERTEX_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! BoolType
struct node_bool_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::BOOL_TYPE };
	uint32_t size { 1u }; //!< [@4]
	uint32_t alignment { 1u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! BufferArg
struct node_buffer_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::BUFFER_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> buffer_size; //!< [@6]
	std::optional<uint_value_t> location_index; //!< [@8]
	std::optional<uint_value_t> location_count; //!< [@10]
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
	ADDRESS_SPACE address_space { ADDRESS_SPACE::GLOBAL }; //!< [@14]
	std::optional<node_id_t> struct_type_info; //!< [@16]
	std::optional<uint_value_t> raster_order_group; //!< [@18]
	std::optional<uint_value_t> type_size; //!< [@20]
	std::optional<uint_value_t> type_align; //!< [@22]
	std::optional<std::string> type_name; //!< [@24]
	std::optional<std::string> name; //!< [@26]
	std::optional<bool> unused; //!< [@28]
	std::optional<node_id_t> inline_type_info; //!< [@30]
};

//! BufferStrideArg
struct node_buffer_stride_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::BUFFER_STRIDE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::optional<std::string> type_name; //!< [@10]
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! CIArrayArg
struct node_ciarray_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CIARRAY_ARG };
	std::optional<node_id_t> inline_type_info; //!< [@4]
	std::optional<node_id_t> struct_type_info; //!< [@6]
	uint_value_t type_size; //!< [@8] REQUIRED
	uint_value_t type_align; //!< [@10] REQUIRED
	std::string type_name; //!< [@12] REQUIRED
	std::optional<std::string> name; //!< [@14]
};

//! CIBuiltinArg
struct node_cibuiltin_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CIBUILTIN_ARG };
	std::string type_name; //!< [@4] REQUIRED
	std::optional<std::string> name; //!< [@6]
};

//! CIBuiltinRet
struct node_cibuiltin_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CIBUILTIN_RET };
	std::string type_name; //!< [@4] REQUIRED
	std::optional<std::string> name; //!< [@6]
};

//! CIFunction
struct node_ci_function_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CI_FUNCTION };
	std::string name; //!< [@4] REQUIRED
	std::optional<std::vector<node_id_t>> return_types; //!< [@6]
	std::optional<std::vector<node_id_t>> arguments; //!< [@8]
	std::optional<node_id_t> user_annotation; //!< [@10]
};

//! CIImageblockArg
struct node_ciimageblock_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CIIMAGEBLOCK_ARG };
	std::string type_name; //!< [@4] REQUIRED
	std::optional<std::string> name; //!< [@6]
};

//! CIImageblockRet
struct node_ciimageblock_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CIIMAGEBLOCK_RET };
	std::string type_name; //!< [@4] REQUIRED
	std::optional<std::string> name; //!< [@6]
};

//! CIMatrixArg
struct node_cimatrix_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CIMATRIX_ARG };
	std::string type_name; //!< [@4] REQUIRED
	std::optional<std::string> name; //!< [@6]
};

//! CIMatrixRet
struct node_cimatrix_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CIMATRIX_RET };
	std::string type_name; //!< [@4] REQUIRED
	std::optional<std::string> name; //!< [@6]
};

//! CIPaddingArg
struct node_cipadding_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CIPADDING_ARG };
};

//! CIPointerArg
struct node_cipointer_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CIPOINTER_ARG };
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@4]
	ADDRESS_SPACE address_space { ADDRESS_SPACE::GLOBAL }; //!< [@6]
	std::optional<node_id_t> inline_type_info; //!< [@8]
	std::optional<node_id_t> struct_type_info; //!< [@10]
	uint_value_t type_size; //!< [@12] REQUIRED
	uint_value_t type_align; //!< [@14] REQUIRED
	std::string type_name; //!< [@16] REQUIRED
	std::optional<std::string> name; //!< [@18]
};

//! CIPointerRet
struct node_cipointer_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CIPOINTER_RET };
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@4]
	ADDRESS_SPACE address_space { ADDRESS_SPACE::GLOBAL }; //!< [@6]
	std::optional<node_id_t> inline_type_info; //!< [@8]
	std::optional<node_id_t> struct_type_info; //!< [@10]
	uint_value_t type_size; //!< [@12] REQUIRED
	uint_value_t type_align; //!< [@14] REQUIRED
	std::string type_name; //!< [@16] REQUIRED
};

//! CISamplerArg
struct node_cisampler_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CISAMPLER_ARG };
	std::string type_name; //!< [@4] REQUIRED
	std::optional<std::string> name; //!< [@6]
};

//! CISamplerRet
struct node_cisampler_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CISAMPLER_RET };
	std::string type_name; //!< [@4] REQUIRED
	std::optional<std::string> name; //!< [@6]
};

//! CIStructArg
struct node_cistruct_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CISTRUCT_ARG };
	std::optional<node_id_t> struct_type_info; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
};

//! CIStructRet
struct node_cistruct_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CISTRUCT_RET };
	node_id_t struct_type_info; //!< [@4] REQUIRED
	std::string type_name; //!< [@6] REQUIRED
};

//! CITextureArg
struct node_citexture_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CITEXTURE_ARG };
	uint_value_t location_count; //!< [@4] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@6]
	std::string type_name; //!< [@8] REQUIRED
	std::optional<std::string> name; //!< [@10]
};

//! CITextureRet
struct node_citexture_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CITEXTURE_RET };
	uint_value_t location_count; //!< [@4] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@6]
	std::string type_name; //!< [@8] REQUIRED
};

//! CharType
struct node_char_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CHAR_TYPE };
	uint32_t size { 1u }; //!< [@4]
	uint32_t alignment { 1u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! ClipDistanceAttr
struct node_clip_distance_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CLIP_DISTANCE_ATTR };
};

//! ClipDistanceRet
struct node_clip_distance_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CLIP_DISTANCE_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> array_size; //!< [@6]
	std::optional<bool> shared; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
};

//! CommandBufferArg
struct node_command_buffer_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::COMMAND_BUFFER_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! CommandBufferType
struct node_command_buffer_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::COMMAND_BUFFER_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! ComputePipelineStateArg
struct node_compute_pipeline_state_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::COMPUTE_PIPELINE_STATE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! ComputePipelineStateType
struct node_compute_pipeline_state_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::COMPUTE_PIPELINE_STATE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! ConstantArg
struct node_constant_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CONSTANT_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::optional<node_id_t> struct_type_info; //!< [@10]
	uint_value_t type_size; //!< [@12] REQUIRED
	uint_value_t type_align; //!< [@14] REQUIRED
	std::optional<std::string> type_name; //!< [@16]
	std::optional<std::string> name; //!< [@18]
	std::optional<bool> unused; //!< [@20]
};

//! ContinueSearchRet
struct node_continue_search_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CONTINUE_SEARCH_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
};

//! ControlPointField
struct node_control_point_field_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CONTROL_POINT_FIELD };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! ControlPointIndexBufferArg
struct node_control_point_index_buffer_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CONTROL_POINT_INDEX_BUFFER_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ControlPointInputArg
struct node_control_point_input_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CONTROL_POINT_INPUT_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<std::vector<node_id_t>> fields; //!< [@6]
	std::optional<bool> unused; //!< [@8]
};

//! CurveParameterArg
struct node_curve_parameter_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::CURVE_PARAMETER_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! Depth2dArrayType
struct node_depth2d_array_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DEPTH2D_ARRAY_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! Depth2dMsArrayType
struct node_depth2d_ms_array_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DEPTH2D_MS_ARRAY_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! Depth2dMsType
struct node_depth2d_ms_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DEPTH2D_MS_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! Depth2dType
struct node_depth2d_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DEPTH2D_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! DepthCubeArrayType
struct node_depth_cube_array_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DEPTH_CUBE_ARRAY_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! DepthCubeType
struct node_depth_cube_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DEPTH_CUBE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! DepthRet
struct node_depth_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DEPTH_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<DEPTH_QUALIFIER> depth_qualifier; //!< [@6]
	std::string type_name; //!< [@8] REQUIRED
	std::optional<std::string> name; //!< [@10]
};

//! DepthStencilStateArg
struct node_depth_stencil_state_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DEPTH_STENCIL_STATE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! DepthStencilStateType
struct node_depth_stencil_state_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DEPTH_STENCIL_STATE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! DirectionArg
struct node_direction_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DIRECTION_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! DispatchQuadgroupsPerThreadgroupArg
struct node_dispatch_quadgroups_per_threadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DISPATCH_QUADGROUPS_PER_THREADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! DispatchSimdgroupsPerThreadgroupArg
struct node_dispatch_simdgroups_per_threadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DISPATCH_SIMDGROUPS_PER_THREADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! DispatchThreadsPerThreadgroupArg
struct node_dispatch_threads_per_threadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DISPATCH_THREADS_PER_THREADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! DistanceArg
struct node_distance_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DISTANCE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! DistanceRet
struct node_distance_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DISTANCE_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
};

//! DoubleType
struct node_double_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::DOUBLE_TYPE };
	uint32_t size { 8u }; //!< [@4]
	uint32_t alignment { 8u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! EnumType
struct node_enum_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::ENUM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	std::optional<std::string> name; //!< [@10]
	std::optional<node_id_t> underlying_type; //!< [@12]
};

//! ExtentsType
struct node_extents_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::EXTENTS_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t index_type; //!< [@10] REQUIRED
	std::vector<uint64_t> extents; //!< [@12] REQUIRED
};

//! FloatType
struct node_float_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::FLOAT_TYPE };
	uint32_t size { 4u }; //!< [@4]
	uint32_t alignment { 4u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! FragmentFunction
struct node_fragment_function_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::FRAGMENT_FUNCTION };
	std::string name; //!< [@4] REQUIRED
	std::optional<std::vector<node_id_t>> return_type; //!< [@6]
	std::optional<std::vector<node_id_t>> arguments; //!< [@8]
	std::optional<bool> early_fragment_tests; //!< [@10]
	std::optional<node_id_t> user_annotation; //!< [@12]
};

//! FragmentInputArg
struct node_fragment_input_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::FRAGMENT_INPUT_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string attribute_name; //!< [@6] REQUIRED
	std::optional<uint_value_t> location; //!< [@8]
	std::optional<SAMPLING_QUALIFIER> sampling_qualifier; //!< [@10]
	std::optional<INTERPOLATION_QUALIFIER> interpolation_qualifier; //!< [@12]
	std::string type_name; //!< [@14] REQUIRED
	std::optional<std::string> name; //!< [@16]
	std::optional<bool> unused; //!< [@18]
};

//! FrontFacingArg
struct node_front_facing_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::FRONT_FACING_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! FunctionConstant
struct node_function_constant_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::FUNCTION_CONSTANT };
	std::string type_name; //!< [@4] REQUIRED
	std::string name; //!< [@6] REQUIRED
	std::optional<uint32_t> index; //!< [@8]
	std::optional<bool> required; //!< [@10]
};

//! FunctionConstantPredicateAttr
struct node_function_constant_predicate_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::FUNCTION_CONSTANT_PREDICATE_ATTR };
	std::optional<bool_value_t> predicate; //!< [@4]
};

//! FunctionHandleArg
struct node_function_handle_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::FUNCTION_HANDLE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! FunctionHandleType
struct node_function_handle_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::FUNCTION_HANDLE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! FunctionIDArg
struct node_function_id_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::FUNCTION_ID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! FunctionType
struct node_function_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::FUNCTION_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t return_type; //!< [@10] REQUIRED
	std::optional<std::vector<node_id_t>> param_types; //!< [@12]
};

//! GeometryIDArg
struct node_geometry_id_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::GEOMETRY_ID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! GeometryIntersectionFunctionTableOffsetArg
struct node_geometry_intersection_function_table_offset_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::GEOMETRY_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! GlobalBinding
struct node_global_binding_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::GLOBAL_BINDING };
	std::string name; //!< [@4] REQUIRED
	node_id_t argument; //!< [@6] REQUIRED
};

//! HalfType
struct node_half_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::HALF_TYPE };
	uint32_t size { 2u }; //!< [@4]
	uint32_t alignment { 2u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! ImageblockArg
struct node_imageblock_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::IMAGEBLOCK_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> data_size; //!< [@6]
	std::optional<node_id_t> struct_type_info; //!< [@8]
	std::optional<bool> alias_all_render_targets; //!< [@10]
	std::optional<uint_value_t> alias_render_target_index; //!< [@12]
	uint_value_t type_align; //!< [@14] REQUIRED
	std::optional<std::string> type_name; //!< [@16]
	std::optional<std::string> name; //!< [@18]
	std::optional<bool> unused; //!< [@20]
};

//! ImageblockDataArg
struct node_imageblock_data_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::IMAGEBLOCK_DATA_ARG };
	std::optional<bool> function_constant; //!< [@4]
	uint_value_t data_size; //!< [@6] REQUIRED
	std::optional<node_id_t> struct_type_info; //!< [@8]
	std::optional<node_id_t> master; //!< [@10]
	std::optional<bool> alias_all_render_targets; //!< [@12]
	std::optional<uint_value_t> alias_render_target_index; //!< [@14]
	uint_value_t type_align; //!< [@16] REQUIRED
	std::optional<std::string> type_name; //!< [@18]
	std::optional<std::string> name; //!< [@20]
	std::optional<bool> unused; //!< [@22]
};

//! ImageblockDataRet
struct node_imageblock_data_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::IMAGEBLOCK_DATA_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> data_size; //!< [@6]
	std::optional<node_id_t> struct_type_info; //!< [@8]
	std::optional<node_id_t> master; //!< [@10]
	std::optional<bool> alias_all_render_targets; //!< [@12]
	std::optional<uint_value_t> alias_render_target_index; //!< [@14]
	uint_value_t type_align; //!< [@16] REQUIRED
	std::optional<std::string> type_name; //!< [@18]
	std::optional<std::string> name; //!< [@20]
};

//! ImageblockType
struct node_imageblock_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::IMAGEBLOCK_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	std::optional<IMAGEBLOCK_LAYOUT> layout; //!< [@10]
	node_id_t data_type; //!< [@12] REQUIRED
};

//! IndirectBufferArg
struct node_indirect_buffer_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INDIRECT_BUFFER_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> buffer_size; //!< [@6]
	std::optional<uint_value_t> location_index; //!< [@8]
	std::optional<uint_value_t> location_count; //!< [@10]
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
	ADDRESS_SPACE address_space { ADDRESS_SPACE::GLOBAL }; //!< [@14]
	std::optional<node_id_t> struct_type_info; //!< [@16]
	std::optional<uint_value_t> raster_order_group; //!< [@18]
	std::optional<uint_value_t> type_size; //!< [@20]
	std::optional<uint_value_t> type_align; //!< [@22]
	std::optional<std::string> type_name; //!< [@24]
	std::optional<std::string> name; //!< [@26]
	std::optional<bool> unused; //!< [@28]
	std::optional<node_id_t> inline_type_info; //!< [@30]
};

//! IndirectConstantArg
struct node_indirect_constant_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INDIRECT_CONSTANT_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::optional<std::string> type_name; //!< [@10]
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! InlineTypeInfo
struct node_inline_type_info_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INLINE_TYPE_INFO };
	ADDRESS_SPACE address_space { ADDRESS_SPACE::INVALID }; //!< [@4]
	std::optional<node_id_t> inline_type_info; //!< [@6]
	std::optional<node_id_t> struct_type_info; //!< [@8]
	std::optional<uint32_t> size; //!< [@10]
	std::optional<uint32_t> alignment; //!< [@12]
	std::optional<uint32_t> array_entries; //!< [@14]
	std::optional<std::string> type_name; //!< [@16]
	std::optional<node_id_t> indirect_argument; //!< [@18]
	std::optional<uint_value_t> indirect_location; //!< [@20]
};

//! InstanceAccelerationStructureArg
struct node_instance_acceleration_structure_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INSTANCE_ACCELERATION_STRUCTURE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@10]
	std::optional<uint_value_t> raster_order_group; //!< [@12]
	std::string type_name; //!< [@14] REQUIRED
	std::optional<std::string> name; //!< [@16]
	std::optional<bool> unused; //!< [@18]
};

//! InstanceIDArg
struct node_instance_id_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INSTANCE_ID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! InstanceIDCountArg
struct node_instance_id_count_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INSTANCE_ID_COUNT_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! InstanceIntersectionFunctionTableOffsetArg
struct node_instance_intersection_function_table_offset_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INSTANCE_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! IntType
struct node_int_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INT_TYPE };
	uint32_t size { 4u }; //!< [@4]
	uint32_t alignment { 4u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! InterpolantType
struct node_interpolant_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INTERPOLANT_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	std::optional<bool> perspective; //!< [@10]
	node_id_t value_type; //!< [@12] REQUIRED
};

//! IntersectionFunction
struct node_intersection_function_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INTERSECTION_FUNCTION };
	std::string name; //!< [@4] REQUIRED
	std::optional<std::vector<node_id_t>> return_types; //!< [@6]
	std::optional<std::vector<node_id_t>> arguments; //!< [@8]
	std::optional<PRIMITIVE_KIND> primitive_kind; //!< [@10]
	std::optional<bool> instancing; //!< [@12]
	std::optional<bool> triangle_data; //!< [@14]
	std::optional<bool> world_space_data; //!< [@16]
	std::optional<bool> primitive_motion; //!< [@18]
	std::optional<bool> instance_motion; //!< [@20]
	std::optional<bool> extended_limits; //!< [@22]
	std::optional<bool> curve_data; //!< [@24]
	std::optional<uint32_t> multi_level_instancing; //!< [@26]
	std::optional<bool> intersection_function_buffer; //!< [@28]
	std::optional<bool> user_data; //!< [@30]
	std::optional<node_id_t> user_annotation; //!< [@32]
};

//! IntersectionFunctionHandleType
struct node_intersection_function_handle_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INTERSECTION_FUNCTION_HANDLE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	std::optional<bool> intersection_function_buffer; //!< [@10]
	std::optional<bool> instancing; //!< [@12]
	std::optional<uint32_t> multi_level_instancing; //!< [@14]
	std::optional<bool> triangle_data; //!< [@16]
	std::optional<bool> curve_data; //!< [@18]
	std::optional<bool> world_space_data; //!< [@20]
	std::optional<bool> user_data; //!< [@22]
	std::optional<bool> primitive_motion; //!< [@24]
	std::optional<bool> instance_motion; //!< [@26]
	std::optional<bool> extended_limits; //!< [@28]
};

//! IntersectionFunctionTableArg
struct node_intersection_function_table_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INTERSECTION_FUNCTION_TABLE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@10]
	std::optional<uint_value_t> raster_order_group; //!< [@12]
	std::string type_name; //!< [@14] REQUIRED
	std::optional<std::string> name; //!< [@16]
	std::optional<bool> unused; //!< [@18]
};

//! IntersectionFunctionTableType
struct node_intersection_function_table_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INTERSECTION_FUNCTION_TABLE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	std::optional<bool> instancing; //!< [@10]
	std::optional<bool> triangle_data; //!< [@12]
	std::optional<bool> world_space_data; //!< [@14]
	std::optional<bool> primitive_motion; //!< [@16]
	std::optional<bool> instance_motion; //!< [@18]
	std::optional<bool> extended_limits; //!< [@20]
	std::optional<bool> curve_data; //!< [@22]
	std::optional<uint32_t> multi_level_instancing; //!< [@24]
};

//! InvariantAttr
struct node_invariant_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::INVARIANT_ATTR };
};

//! KernelFunction
struct node_kernel_function_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::KERNEL_FUNCTION };
	std::string name; //!< [@4] REQUIRED
	std::optional<std::vector<node_id_t>> return_types; //!< [@6]
	std::optional<std::vector<node_id_t>> arguments; //!< [@8]
	std::optional<node_id_t> vec_type_hint; //!< [@10]
	std::optional<node_id_t> workgroup_size; //!< [@12]
	std::optional<node_id_t> workgroup_size_hint; //!< [@14]
	std::optional<node_id_t> workgroup_max_size; //!< [@16]
	std::optional<node_id_t> user_annotation; //!< [@18]
};

//! KeyFrameCountArg
struct node_key_frame_count_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::KEY_FRAME_COUNT_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! LLongType
struct node_llong_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::LLONG_TYPE };
	uint32_t size { 16u }; //!< [@4]
	uint32_t alignment { 16u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! LValueReferenceType
struct node_lvalue_reference_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::LVALUE_REFERENCE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t pointee_type; //!< [@10] REQUIRED
};

//! LocalAllocation
struct local_allocation_t {
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
};

//! LocationIndexAttr
struct node_location_index_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::LOCATION_INDEX_ATTR };
	std::optional<uint_value_t> index; //!< [@4]
	std::optional<uint_value_t> count; //!< [@6]
};

//! LongType
struct node_long_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::LONG_TYPE };
	uint32_t size { 8u }; //!< [@4]
	uint32_t alignment { 8u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! MatrixType
struct node_matrix_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MATRIX_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t element_type; //!< [@10] REQUIRED
	std::optional<uint32_t> num_columns; //!< [@12]
	std::optional<uint32_t> num_rows; //!< [@14]
};

//! MaxDistanceArg
struct node_max_distance_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MAX_DISTANCE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! MaxMeshWorkgroupsFnAttr
struct node_max_mesh_workgroups_fn_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MAX_MESH_WORKGROUPS_FN_ATTR };
	std::optional<uint_value_t> workgroups; //!< [@4]
};

//! MeshArg
struct node_mesh_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<node_id_t> mesh_type_info; //!< [@6]
	std::optional<std::string> type_name; //!< [@8]
	std::optional<std::string> name; //!< [@10]
	std::optional<bool> unused; //!< [@12]
};

//! Block
struct node_mesh_emulation_block_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_EMULATION_BLOCK };
	std::optional<std::vector<node_id_t>> value_groups; //!< [@4]
};

//! FragmentAnalysisResult
struct node_mesh_emulation_fragment_analysis_result_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_EMULATION_FRAGMENT_ANALYSIS_RESULT };
	std::string function; //!< [@4] REQUIRED
	std::optional<std::vector<std::string>> used_inputs; //!< [@6]
};

//! MeshKernel
struct node_mesh_emulation_mesh_kernel_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_EMULATION_MESH_KERNEL };
	std::string function; //!< [@4] REQUIRED
	std::optional<uint32_t> emulation_buffer_index; //!< [@6]
	std::optional<node_id_t> layout; //!< [@8]
};

//! MeshLayout
struct node_mesh_emulation_mesh_layout_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_EMULATION_MESH_LAYOUT };
	std::optional<uint32_t> max_vertices; //!< [@4]
	std::optional<uint32_t> max_primitives; //!< [@6]
	std::optional<uint32_t> max_indices; //!< [@8]
	std::optional<uint32_t> max_indices_padding; //!< [@10]
	node_id_t vertices_primitives_block; //!< [@12] REQUIRED
	std::optional<node_id_t> primitive_culled_block; //!< [@14]
};

//! MeshVertex
struct node_mesh_emulation_mesh_vertex_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_EMULATION_MESH_VERTEX };
	std::string function; //!< [@4] REQUIRED
	std::optional<uint32_t> emulation_buffer_index; //!< [@6]
	std::optional<node_id_t> layout; //!< [@8]
};

//! ObjectKernel
struct node_mesh_emulation_object_kernel_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_EMULATION_OBJECT_KERNEL };
	std::string function; //!< [@4] REQUIRED
	std::optional<uint32_t> emulation_buffer_index; //!< [@6]
	std::optional<node_id_t> max_mesh_workgroups; //!< [@8]
};

//! ValueGroup
struct node_mesh_emulation_value_group_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_EMULATION_VALUE_GROUP };
	std::optional<uint32_t> value_alignment; //!< [@4]
	std::optional<uint32_t> value_size; //!< [@6]
	std::optional<uint32_t> max_value_count; //!< [@8]
	std::optional<node_id_t> member_type; //!< [@10]
	std::optional<uint32_t> member_index; //!< [@12]
};

//! MeshFunction
struct node_mesh_function_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_FUNCTION };
	std::string name; //!< [@4] REQUIRED
	std::optional<std::vector<node_id_t>> return_types; //!< [@6]
	std::optional<std::vector<node_id_t>> arguments; //!< [@8]
	std::optional<node_id_t> workgroup_max_size; //!< [@10]
	std::optional<node_id_t> user_annotation; //!< [@12]
	std::optional<node_id_t> workgroup_size; //!< [@14]
};

//! MeshGridPropertiesArg
struct node_mesh_grid_properties_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_GRID_PROPERTIES_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! MeshGridPropertiesType
struct node_mesh_grid_properties_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_GRID_PROPERTIES_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! MeshPrimitiveDataRet
struct node_mesh_primitive_data_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_PRIMITIVE_DATA_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint32_t> id; //!< [@6]
	std::string attribute_name; //!< [@8] REQUIRED
	std::optional<bool> shared; //!< [@10]
	std::string type_name; //!< [@12] REQUIRED
	std::optional<std::string> name; //!< [@14]
};

//! MeshType
struct node_mesh_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t vertex_type; //!< [@10] REQUIRED
	node_id_t primitive_type; //!< [@12] REQUIRED
	std::optional<uint32_t> max_vertices; //!< [@14]
	std::optional<uint32_t> max_primitives; //!< [@16]
	std::optional<TOPOLOGY> topology; //!< [@18]
};

//! MeshTypeInfo
struct node_mesh_type_info_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_TYPE_INFO };
	std::optional<std::vector<node_id_t>> vertex_types; //!< [@4]
	std::optional<std::vector<node_id_t>> primitive_types; //!< [@6]
	std::optional<uint32_t> max_vertices; //!< [@8]
	std::optional<uint32_t> max_primitives; //!< [@10]
	TOPOLOGY topology { TOPOLOGY::POINT }; //!< [@12]
};

//! MeshVertexDataRet
struct node_mesh_vertex_data_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MESH_VERTEX_DATA_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint32_t> id; //!< [@6]
	std::string attribute_name; //!< [@8] REQUIRED
	std::optional<bool> shared; //!< [@10]
	std::string type_name; //!< [@12] REQUIRED
	std::optional<std::string> name; //!< [@14]
};

//! MinDistanceArg
struct node_min_distance_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MIN_DISTANCE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! MotionEndTimeArg
struct node_motion_end_time_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MOTION_END_TIME_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! MotionStartTimeArg
struct node_motion_start_time_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::MOTION_START_TIME_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ObjectFunction
struct node_object_function_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::OBJECT_FUNCTION };
	std::string name; //!< [@4] REQUIRED
	std::optional<std::vector<node_id_t>> return_types; //!< [@6]
	std::optional<std::vector<node_id_t>> arguments; //!< [@8]
	std::optional<node_id_t> workgroup_max_size; //!< [@10]
	std::optional<node_id_t> max_mesh_workgroups; //!< [@12]
	std::optional<node_id_t> user_annotation; //!< [@14]
	std::optional<node_id_t> workgroup_size; //!< [@16]
};

//! ObjectToWorldTransformArg
struct node_object_to_world_transform_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::OBJECT_TO_WORLD_TRANSFORM_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! OpaquePrimitiveArg
struct node_opaque_primitive_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::OPAQUE_PRIMITIVE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! OpaqueType
struct node_opaque_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::OPAQUE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	std::string name; //!< [@10] REQUIRED
};

//! OriginArg
struct node_origin_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::ORIGIN_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! PackedVectorType
struct node_packed_vector_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PACKED_VECTOR_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t element_type; //!< [@10] REQUIRED
	std::optional<uint32_t> num_elements; //!< [@12]
};

//! PatchControlPointType
struct node_patch_control_point_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PATCH_CONTROL_POINT_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t control_point_type; //!< [@10] REQUIRED
};

//! PatchFnAttr
struct node_patch_fn_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PATCH_FN_ATTR };
	std::optional<PATCH_KIND> kind; //!< [@4]
	std::optional<uint_value_t> control_points; //!< [@6]
};

//! PatchIDArg
struct node_patch_id_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PATCH_ID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! PatchInputArg
struct node_patch_input_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PATCH_INPUT_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! PayloadArg
struct node_payload_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PAYLOAD_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<node_id_t> struct_type_info; //!< [@6]
	uint_value_t type_size; //!< [@8] REQUIRED
	uint_value_t type_align; //!< [@10] REQUIRED
	std::optional<std::string> type_name; //!< [@12]
	std::optional<std::string> name; //!< [@14]
	std::optional<bool> unused; //!< [@16]
	std::optional<node_id_t> inline_type_info; //!< [@18]
};

//! PixelPositionInTileArg
struct node_pixel_position_in_tile_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PIXEL_POSITION_IN_TILE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! PixelsPerTileArg
struct node_pixels_per_tile_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PIXELS_PER_TILE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! PointCoordArg
struct node_point_coord_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::POINT_COORD_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! PointSizeAttr
struct node_point_size_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::POINT_SIZE_ATTR };
};

//! PointSizeRet
struct node_point_size_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::POINT_SIZE_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<bool> shared; //!< [@6]
	std::string type_name; //!< [@8] REQUIRED
	std::optional<std::string> name; //!< [@10]
};

//! PointerType
struct node_pointer_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::POINTER_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t pointee_type; //!< [@10] REQUIRED
};

//! PositionArg
struct node_position_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::POSITION_ARG };
	std::optional<bool> function_constant; //!< [@4]
	SAMPLING_QUALIFIER sampling_qualifier { SAMPLING_QUALIFIER::CENTER }; //!< [@6]
	INTERPOLATION_QUALIFIER interpolation_qualifier { INTERPOLATION_QUALIFIER::NO_PERSPECTIVE }; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! PositionAttr
struct node_position_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::POSITION_ATTR };
};

//! PositionInPatchArg
struct node_position_in_patch_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::POSITION_IN_PATCH_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! PositionRet
struct node_position_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::POSITION_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<bool> invariant; //!< [@6]
	std::optional<bool> shared; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
};

//! PrimitiveAccelerationStructureArg
struct node_primitive_acceleration_structure_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PRIMITIVE_ACCELERATION_STRUCTURE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@10]
	std::optional<uint_value_t> raster_order_group; //!< [@12]
	std::string type_name; //!< [@14] REQUIRED
	std::optional<std::string> name; //!< [@16]
	std::optional<bool> unused; //!< [@18]
};

//! PrimitiveCulledAttr
struct node_primitive_culled_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PRIMITIVE_CULLED_ATTR };
};

//! PrimitiveCulledRet
struct node_primitive_culled_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PRIMITIVE_CULLED_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<bool> shared; //!< [@6]
	std::string type_name; //!< [@8] REQUIRED
	std::optional<std::string> name; //!< [@10]
};

//! PrimitiveDataArg
struct node_primitive_data_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PRIMITIVE_DATA_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! PrimitiveIDArg
struct node_primitive_id_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PRIMITIVE_ID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! PrimitiveIDAttr
struct node_primitive_id_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PRIMITIVE_ID_ATTR };
};

//! PrimitiveIDRet
struct node_primitive_id_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::PRIMITIVE_ID_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<bool> shared; //!< [@6]
	std::string type_name; //!< [@8] REQUIRED
	std::optional<std::string> name; //!< [@10]
};

//! QuadgroupIndexInThreadgroupArg
struct node_quadgroup_index_in_threadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::QUADGROUP_INDEX_IN_THREADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! QuadgroupsPerThreadgroupArg
struct node_quadgroups_per_threadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::QUADGROUPS_PER_THREADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! R16SNormType
struct node_r16snorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::R16SNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! R16UNormType
struct node_r16unorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::R16UNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! R8SNormType
struct node_r8snorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::R8SNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! R8UNormType
struct node_r8unorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::R8UNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RG11B10FType
struct node_rg11b10f_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RG11B10F_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RG16SNormType
struct node_rg16snorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RG16SNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RG16UNormType
struct node_rg16unorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RG16UNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RG8SNormType
struct node_rg8snorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RG8SNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RG8UNormType
struct node_rg8unorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RG8UNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RGB10A2Type
struct node_rgb10a2_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RGB10A2_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RGB9E5Type
struct node_rgb9e5_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RGB9E5_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RGBA16SNormType
struct node_rgba16snorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RGBA16SNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RGBA16UNormType
struct node_rgba16unorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RGBA16UNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RGBA8SNormType
struct node_rgba8snorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RGBA8SNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RGBA8UNormType
struct node_rgba8unorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RGBA8UNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! RValueReferenceType
struct node_rvalue_reference_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RVALUE_REFERENCE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t pointee_type; //!< [@10] REQUIRED
};

//! RecordBase
struct node_record_base_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RECORD_BASE };
	std::optional<uint32_t> offset; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	node_id_t type; //!< [@8] REQUIRED
};

//! RecordField
struct node_record_field_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RECORD_FIELD };
	std::optional<uint32_t> offset; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	node_id_t type; //!< [@8] REQUIRED
	std::optional<std::string> name; //!< [@10]
	std::optional<std::vector<node_id_t>> attributes; //!< [@12]
	std::optional<bitfield_info_t> bitfield; //!< [@14]
};

//! Reflection
struct reflection_t {
	std::optional<version_t> version; //!< [@4]
	std::optional<std::vector<std::unique_ptr<node_base_t>>> nodes; //!< [@6]
	std::optional<std::vector<node_id_t>> fragment_functions; //!< [@8]
	std::optional<std::vector<node_id_t>> intersection_functions; //!< [@10]
	std::optional<std::vector<node_id_t>> kernel_functions; //!< [@12]
	std::optional<std::vector<node_id_t>> vertex_functions; //!< [@14]
	std::optional<std::vector<node_id_t>> visible_functions; //!< [@16]
	std::optional<std::vector<node_id_t>> mesh_functions; //!< [@18]
	std::optional<std::vector<node_id_t>> object_functions; //!< [@20]
	std::optional<std::vector<node_id_t>> function_constants; //!< [@22]
	std::optional<std::vector<local_allocation_t>> static_local_allocations; //!< [@24]
	std::optional<std::vector<node_id_t>> emulations; //!< [@26]
	std::optional<std::vector<node_id_t>> global_bindings; //!< [@28]
	std::optional<std::vector<node_id_t>> visible_function_references; //!< [@30]
	std::optional<std::vector<node_id_t>> ci_functions; //!< [@32]
};

//! RenderPipelineStateArg
struct node_render_pipeline_state_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RENDER_PIPELINE_STATE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! RenderPipelineStateType
struct node_render_pipeline_state_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RENDER_PIPELINE_STATE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! RenderTargetArg
struct node_render_target_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RENDER_TARGET_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> render_target_index; //!< [@6]
	std::optional<uint_value_t> raster_order_group; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! RenderTargetArrayIndexArg
struct node_render_target_array_index_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! RenderTargetArrayIndexAttr
struct node_render_target_array_index_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ATTR };
};

//! RenderTargetArrayIndexRet
struct node_render_target_array_index_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<bool> shared; //!< [@6]
	std::string type_name; //!< [@8] REQUIRED
	std::optional<std::string> name; //!< [@10]
};

//! RenderTargetAttr
struct node_render_target_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RENDER_TARGET_ATTR };
	std::optional<uint_value_t> index; //!< [@4]
};

//! RenderTargetRet
struct node_render_target_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::RENDER_TARGET_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> render_target_index; //!< [@6]
	std::optional<uint_value_t> blend_source_index; //!< [@8]
	std::optional<uint_value_t> raster_order_group; //!< [@10]
	std::optional<ROUNDING_MODE> rounding_mode; //!< [@12]
	std::string type_name; //!< [@14] REQUIRED
	std::optional<std::string> name; //!< [@16]
};

//! SRGBA8UNormType
struct node_srgba8unorm_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::SRGBA8UNORM_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t alu_type; //!< [@10] REQUIRED
};

//! SampleIDArg
struct node_sample_id_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::SAMPLE_ID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! SampleMaskArg
struct node_sample_mask_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::SAMPLE_MASK_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<bool> post_depth_coverage; //!< [@6]
	std::string type_name; //!< [@8] REQUIRED
	std::optional<std::string> name; //!< [@10]
	std::optional<bool> unused; //!< [@12]
};

//! SampleMaskRet
struct node_sample_mask_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::SAMPLE_MASK_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
};

//! SamplerArg
struct node_sampler_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::SAMPLER_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! SamplerType
struct node_sampler_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::SAMPLER_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! SharedAttr
struct node_shared_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::SHARED_ATTR };
};

//! ShortType
struct node_short_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::SHORT_TYPE };
	uint32_t size { 2u }; //!< [@4]
	uint32_t alignment { 2u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! SimdgroupIndexInThreadgroupArg
struct node_simdgroup_index_in_threadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::SIMDGROUP_INDEX_IN_THREADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! SimdgroupsPerThreadgroupArg
struct node_simdgroups_per_threadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::SIMDGROUPS_PER_THREADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! StageInArg
struct node_stage_in_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::STAGE_IN_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! StageInGridOriginArg
struct node_stage_in_grid_origin_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::STAGE_IN_GRID_ORIGIN_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! StageInGridSizeArg
struct node_stage_in_grid_size_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::STAGE_IN_GRID_SIZE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! StencilRet
struct node_stencil_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::STENCIL_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
};

//! StitchingArgument
struct node_stitching_argument_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::STITCHING_ARGUMENT };
	node_id_t type; //!< [@4] REQUIRED
	std::optional<std::string> name; //!< [@6]
};

//! StitchingInfo
struct stitching_info_t {
	std::optional<node_id_t> return_type; //!< [@4]
	std::optional<std::vector<node_id_t>> arguments; //!< [@6]
};

//! StructType
struct node_struct_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::STRUCT_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	std::optional<std::string> name; //!< [@10]
	std::optional<std::vector<node_id_t>> members; //!< [@12]
};

//! StructTypeInfoField
struct struct_type_info_field_t {
	std::optional<node_id_t> struct_type_info; //!< [@4]
	std::optional<uint32_t> offset; //!< [@6]
	std::optional<uint32_t> size; //!< [@8]
	std::optional<uint32_t> array_entries; //!< [@10]
	std::optional<std::string> type_name; //!< [@12]
	std::optional<std::string> field_name; //!< [@14]
	std::optional<std::string> attribute_name; //!< [@16]
	std::optional<node_id_t> indirect_argument; //!< [@18]
	std::optional<uint_value_t> indirect_location; //!< [@20]
	std::optional<uint_value_t> raster_order_group; //!< [@22]
	std::optional<uint_value_t> render_target_index; //!< [@24]
	std::optional<node_id_t> inline_type_info; //!< [@26]
};

//! StructTypeInfo
struct node_struct_type_info_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::STRUCT_TYPE_INFO };
	std::optional<std::vector<struct_type_info_field_t>> fields; //!< [@4]
};

//! TensorArg
struct node_tensor_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TENSOR_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@10]
	std::optional<uint_value_t> raster_order_group; //!< [@12]
	std::string type_name; //!< [@14] REQUIRED
	std::optional<std::string> name; //!< [@16]
	std::optional<bool> unused; //!< [@18]
};

//! TensorType
struct node_tensor_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TENSOR_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t element_type; //!< [@10] REQUIRED
	node_id_t extents_type; //!< [@12] REQUIRED
	std::optional<TENSOR_KIND> kind; //!< [@14]
};

//! Texture1dArrayType
struct node_texture1d_array_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE1D_ARRAY_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! Texture1dType
struct node_texture1d_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE1D_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! Texture2dArrayType
struct node_texture2d_array_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE2D_ARRAY_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! Texture2dMsArrayType
struct node_texture2d_ms_array_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE2D_MS_ARRAY_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! Texture2dMsType
struct node_texture2d_ms_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE2D_MS_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! Texture2dType
struct node_texture2d_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE2D_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! Texture3dType
struct node_texture3d_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE3D_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! TextureArg
struct node_texture_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@10]
	std::optional<uint_value_t> raster_order_group; //!< [@12]
	std::string type_name; //!< [@14] REQUIRED
	std::optional<std::string> name; //!< [@16]
	std::optional<bool> unused; //!< [@18]
};

//! TextureBuffer1dType
struct node_texture_buffer1d_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE_BUFFER1D_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! TextureCubeArrayType
struct node_texture_cube_array_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE_CUBE_ARRAY_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! TextureCubeType
struct node_texture_cube_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TEXTURE_CUBE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t channel_type; //!< [@10] REQUIRED
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@12]
};

//! ThreadExecutionWidthArg
struct node_thread_execution_width_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREAD_EXECUTION_WIDTH_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ThreadIndexInQuadgroupArg
struct node_thread_index_in_quadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREAD_INDEX_IN_QUADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ThreadIndexInSimdgroupArg
struct node_thread_index_in_simdgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREAD_INDEX_IN_SIMDGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ThreadIndexInThreadgroupArg
struct node_thread_index_in_threadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREAD_INDEX_IN_THREADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ThreadPositionInGridArg
struct node_thread_position_in_grid_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREAD_POSITION_IN_GRID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ThreadPositionInThreadgroupArg
struct node_thread_position_in_threadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREAD_POSITION_IN_THREADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ThreadgroupPositionInGridArg
struct node_threadgroup_position_in_grid_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREADGROUP_POSITION_IN_GRID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ThreadgroupsPerGridArg
struct node_threadgroups_per_grid_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREADGROUPS_PER_GRID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ThreadsPerGridArg
struct node_threads_per_grid_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREADS_PER_GRID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ThreadsPerSimdgroupArg
struct node_threads_per_simdgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREADS_PER_SIMDGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ThreadsPerThreadgroupArg
struct node_threads_per_threadgroup_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::THREADS_PER_THREADGROUP_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! TileIndexArg
struct node_tile_index_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TILE_INDEX_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! TimeArg
struct node_time_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::TIME_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! UCharType
struct node_uchar_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::UCHAR_TYPE };
	uint32_t size { 1u }; //!< [@4]
	uint32_t alignment { 1u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! UIntType
struct node_uint_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::UINT_TYPE };
	uint32_t size { 4u }; //!< [@4]
	uint32_t alignment { 4u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! ULLongType
struct node_ullong_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::ULLONG_TYPE };
	uint32_t size { 16u }; //!< [@4]
	uint32_t alignment { 16u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! ULongType
struct node_ulong_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::ULONG_TYPE };
	uint32_t size { 8u }; //!< [@4]
	uint32_t alignment { 8u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! UShortType
struct node_ushort_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::USHORT_TYPE };
	uint32_t size { 2u }; //!< [@4]
	uint32_t alignment { 2u }; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! UnionType
struct node_union_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::UNION_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	std::optional<std::string> name; //!< [@10]
	std::optional<std::vector<node_id_t>> members; //!< [@12]
};

//! UserAnnotationFnAttr
struct node_user_annotation_fn_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::USER_ANNOTATION_FN_ATTR };
	std::optional<std::string> annotation; //!< [@4]
};

//! UserAttr
struct node_user_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::USER_ATTR };
	std::string name; //!< [@4] REQUIRED
};

//! UserDataBufferArg
struct node_user_data_buffer_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::USER_DATA_BUFFER_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@6]
	std::optional<node_id_t> inline_type_info; //!< [@8]
	std::optional<node_id_t> struct_type_info; //!< [@10]
	std::optional<uint_value_t> type_size; //!< [@12]
	std::optional<uint_value_t> type_align; //!< [@14]
	std::optional<std::string> type_name; //!< [@16]
	std::optional<std::string> name; //!< [@18]
	std::optional<bool> unused; //!< [@20]
};

//! UserInstanceIDArg
struct node_user_instance_id_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::USER_INSTANCE_ID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! UserInstanceIDCountArg
struct node_user_instance_id_count_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::USER_INSTANCE_ID_COUNT_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! VecTypeHintFnAttr
struct node_vec_type_hint_fn_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VEC_TYPE_HINT_FN_ATTR };
	std::optional<std::string> type_name; //!< [@4]
};

//! VectorType
struct node_vector_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VECTOR_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t element_type; //!< [@10] REQUIRED
	std::optional<uint32_t> num_elements; //!< [@12]
};

//! VertexFunction
struct node_vertex_function_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VERTEX_FUNCTION };
	std::string name; //!< [@4] REQUIRED
	std::optional<std::vector<node_id_t>> return_types; //!< [@6]
	std::optional<std::vector<node_id_t>> arguments; //!< [@8]
	std::optional<node_id_t> patch; //!< [@10]
	std::optional<node_id_t> user_annotation; //!< [@12]
};

//! VertexIDArg
struct node_vertex_id_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VERTEX_ID_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! VertexInputArg
struct node_vertex_input_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VERTEX_INPUT_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::string type_name; //!< [@10] REQUIRED
	std::optional<std::string> name; //!< [@12]
	std::optional<bool> unused; //!< [@14]
};

//! VertexOutputRet
struct node_vertex_output_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VERTEX_OUTPUT_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::string attribute_name; //!< [@6] REQUIRED
	std::optional<uint_value_t> location; //!< [@8]
	std::optional<bool> shared; //!< [@10]
	std::string type_name; //!< [@12] REQUIRED
	std::optional<std::string> name; //!< [@14]
};

//! VertexValueType
struct node_vertex_value_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VERTEX_VALUE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t underlying_type; //!< [@10] REQUIRED
};

//! ViewportArrayIndexArg
struct node_viewport_array_index_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VIEWPORT_ARRAY_INDEX_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! ViewportArrayIndexAttr
struct node_viewport_array_index_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VIEWPORT_ARRAY_INDEX_ATTR };
};

//! ViewportArrayIndexRet
struct node_viewport_array_index_ret_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VIEWPORT_ARRAY_INDEX_RET };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<bool> shared; //!< [@6]
	std::string type_name; //!< [@8] REQUIRED
	std::optional<std::string> name; //!< [@10]
};

//! VisibleFunction
struct node_visible_function_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VISIBLE_FUNCTION };
	std::string name; //!< [@4] REQUIRED
	std::optional<stitching_info_t> stitching_info; //!< [@6]
	std::optional<node_id_t> user_annotation; //!< [@8]
};

//! VisibleFunctionReference
struct node_visible_function_reference_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VISIBLE_FUNCTION_REFERENCE };
	std::string function_name; //!< [@4] REQUIRED
};

//! VisibleFunctionTableArg
struct node_visible_function_table_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VISIBLE_FUNCTION_TABLE_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::optional<uint_value_t> location_index; //!< [@6]
	std::optional<uint_value_t> location_count; //!< [@8]
	std::optional<ACCESS_QUALIFIER> access_qualifier; //!< [@10]
	std::optional<uint_value_t> raster_order_group; //!< [@12]
	std::string type_name; //!< [@14] REQUIRED
	std::optional<std::string> name; //!< [@16]
	std::optional<bool> unused; //!< [@18]
};

//! VisibleFunctionTableType
struct node_visible_function_table_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VISIBLE_FUNCTION_TABLE_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
	node_id_t function_type; //!< [@10] REQUIRED
};

//! VoidType
struct node_void_type_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::VOID_TYPE };
	std::optional<uint32_t> size; //!< [@4]
	std::optional<uint32_t> alignment; //!< [@6]
	std::optional<std::vector<node_id_t>> qualifiers; //!< [@8]
};

//! WorkgroupMaxSizeFnAttr
struct node_workgroup_max_size_fn_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::WORKGROUP_MAX_SIZE_FN_ATTR };
	std::optional<uint_value_t> size; //!< [@4]
};

//! WorkgroupSizeFnAttr
struct node_workgroup_size_fn_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::WORKGROUP_SIZE_FN_ATTR };
	std::optional<uint_value_t> width; //!< [@4]
	std::optional<uint_value_t> height; //!< [@6]
	std::optional<uint_value_t> depth; //!< [@8]
};

//! WorkgroupSizeHintFnAttr
struct node_workgroup_size_hint_fn_attr_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::WORKGROUP_SIZE_HINT_FN_ATTR };
	std::optional<uint_value_t> width; //!< [@4]
	std::optional<uint_value_t> height; //!< [@6]
	std::optional<uint_value_t> depth; //!< [@8]
};

//! WorldSpaceDirectionArg
struct node_world_space_direction_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::WORLD_SPACE_DIRECTION_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! WorldSpaceOriginArg
struct node_world_space_origin_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::WORLD_SPACE_ORIGIN_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

//! WorldToObjectTransformArg
struct node_world_to_object_transform_arg_t : node_base_t {
	const NODE_TYPE node_type { NODE_TYPE::WORLD_TO_OBJECT_TRANSFORM_ARG };
	std::optional<bool> function_constant; //!< [@4]
	std::string type_name; //!< [@6] REQUIRED
	std::optional<std::string> name; //!< [@8]
	std::optional<bool> unused; //!< [@10]
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

} // namespace metal::reflection
