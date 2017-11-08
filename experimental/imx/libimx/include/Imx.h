/* Copyright 2015 Google Inc.  All Rights Reserved. */

/* This file is the top-level file to be included by applications that use
 * the paintbox API.  It is a valid C header file.
 * This file should NOT contain definitions of any internal data structures,
 * such as the contents pointed to by "handles".
 */

#ifndef PAINTBOX_IMAGE_PROCESSOR_H_
#define PAINTBOX_IMAGE_PROCESSOR_H_

// imx.h includes stdint.h as it uses types such as "uint64_t"
// imx.h is also included in the halide module. However, halide builds its
// runtime files in freestanding mode (i.e. it assumes that a
// hosted STDC environment may not present), and uses runtime_internal.h
// to provide the "*_t" types.
// This causes conflicts when imx.h is included (via transitive inclusion) in
// files that are built in freestanding mode. We solve this by asking imx.h to
// include stdint.h only when not compiling halide runtime.
#ifndef COMPILING_HALIDE_RUNTIME
#include <stdint.h>
#else
#include "runtime_internal.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Error return values. The order is from generic to specific failure types. */
typedef enum {
  IMX_SUCCESS = 0,
  IMX_FAILURE, /* Generic failure */
  IMX_INVALID, /* Invalid request (e.g. internally inconsistent) */
  IMX_NODEV,   /* Device allocation failed */
  IMX_NOMEM,   /* Memory allocation failed */
  IMX_TIMEOUT, /* Request timed out (e.g. while waiting for interrupt) */
  IMX_NOT_FOUND, /* Resource not found */
  IMX_TYPE_MISMATCH, /* Type doesn't match */
  IMX_OVERFLOW,  /* Data transfer/stream overflow; typically with MIPI Input */
  IMX_CANCEL,  /* Job ended early due to a cancel request */
} ImxError;

/* Data types.  Corresponding validation routine is "numeric_type_valid()". */
/* Comment on the "packed" data types:
   A packed data type such as PACKED_UINT_2_10_10_10 indicates that the actual
   element type varies across the last dimension.  When a packed data type is
   used, the extent of the last dimension (as provided in an associated
   ImxParameterType) must match, e.g. 4 for PACKED_UINT_2_10_10_10.
   By representing packed types in this manner, the number of dimensions
   stays the same for both the packed and unpacked representations.

  Comment on IMX_INT10, IMX_INT12 data types:
  We currently do not allow specification of these types - use the unsigned
  versions instead. DMA does not support sign-extension, so the signed-bit is
  lost when elements of these types are brought to the processor. Software (or
  compiler) must explicitly be written to perform the appropriate
  sign-extension to processor supported type (such as IMX_INT16).
*/
typedef enum {
  /* "processor" types, valid anywhere */
  IMX_INT8 = 0,
  IMX_UINT8,
  IMX_INT16,
  IMX_UINT16,
  IMX_INT32,
  IMX_UINT32,
  /* "processor" types, valid only on simulator */
  IMX_FLOAT16,
  IMX_FLOAT32,
  IMX_kMaxProcessorNumericType, /* Internal use only */

  /* "memory" types, valid only when specifically permitted */
  IMX_UINT10 = IMX_kMaxProcessorNumericType, // leave first in this second category!
  IMX_UINT12,
  /* TODO(ahalambi): Add IMX_INT10, IMX_INT12 when compiler support for these
   * types is added.
   */
  IMX_PACKED_UINT_6_5_6,
  IMX_PACKED_UINT_5_5_5_1,
  IMX_PACKED_UINT_1_5_5_5,
  IMX_PACKED_UINT_2_10_10_10,
  IMX_PACKED_UINT_10_10_10_2,
  IMX_kMaxNumericType /* Internal use only */
} ImxNumericType;

/* Layout in memory. */
typedef enum {
  /* TODO(billmark/ahalambi): Not clear that it's useful to distinguish
   * these three layouts (linear/planar/linear_planar) as enums, given the
   * ability to represent a hybrid.  Probably should merge them into one
   * and rely on components_per_plane and strides to tell us what we need.
   */
  IMX_LAYOUT_LINEAR = 0, /* e.g. RGBRGBRGB... */
  IMX_LAYOUT_PLANAR,     /* e.g. RRRR ..., GGG ...., BBB .... */
  IMX_LAYOUT_LINEAR_PLANAR, /* e.g. RGBRGBRGB..., ABCDABCD... */

  IMX_LAYOUT_RASTER_RAW10, /* Android RAW10 layout, allowed only for
                            * element_type == IMX_UINT10. */
  IMX_LAYOUT_LINEAR_TILED_4X4,  /* Placeholder -- Details TBD */
  IMX_LAYOUT_PLANAR_TILED_4X4,  /* Placeholder -- Details TBD */
  IMX_kMaxLayout  /* Internal use only */
} ImxLayout;

/* Typedefs for opaque handles */

struct ImxDevice;
typedef struct ImxDevice ImxDevice;
typedef ImxDevice* ImxDeviceHandle;

struct ImxMemoryAllocator;
typedef struct ImxMemoryAllocator ImxMemoryAllocator;
typedef ImxMemoryAllocator* ImxMemoryAllocatorHandle;

struct ImxPrecompiledGraphs;
typedef struct ImxPrecompiledGraphs ImxPrecompiledGraphs;
typedef ImxPrecompiledGraphs* ImxPrecompiledGraphsHandle;

struct ImxCompiledGraph;
typedef struct ImxCompiledGraph ImxCompiledGraph;
typedef ImxCompiledGraph* ImxCompiledGraphHandle;

struct ImxGraph;
typedef struct ImxGraph ImxGraph;
typedef ImxGraph* ImxGraphHandle;

struct ImxNode;
typedef struct ImxNode ImxNode;
typedef ImxNode* ImxNodeHandle;

struct ImxJob;
typedef struct ImxJob ImxJob;
typedef ImxJob* ImxJobHandle;

struct ImxDeviceBuffer;
typedef struct ImxDeviceBuffer ImxDeviceBuffer;
typedef ImxDeviceBuffer* ImxDeviceBufferHandle;

/* Routines, and "Info" structs that go with them */

/* A description of the resources requested for a device.
 * Use this struct to specify the number of:
 *   stencil processors,
 *   line buffer pools,
 *   dma channels,
 *   interrupts
 * to be allocated to the device.
 * To specify the exact ids of the resources to be reserved, use the
 * ImxSpecificCoreResourcesDescription instead.
 */
typedef struct ImxNumberOfCoreResourcesDescription {
  int num_stps;
  int num_lbps;
  int num_dma_channels; // does not include those reserved for mipi streams
} ImxNumberOfCoreResourcesDescription;

/* A description of the specific resources requested for a device.
 * Use this struct to specify any specific ids for:
 *   stencil processors,
 *   line buffer pools,
 *   dma channels,
 *   interrupts
 * to be allocated to the device.
 * Note: It is not allowed to mix and match specific resource request with
 * number of resource request.
 *
 * Resource Ids:
 *   Currently, STP Ids start from 1 (i.e. there is no STP0).
 *   LBP Ids start from 0. LBP0 is closest to DMA Channels; LBPi is paired
 *   with STPi (e.g. LBP2 is closest to STP2).
 *   DMA Channel Ids start from 0.
 *   Interrupts are implicitly allocated when an interrupt generating resource
 *   (such as DMA Channel or STP) is allocated.
 */
typedef struct ImxSpecificCoreResourcesDescription {
  int *specific_stps;
  int specific_stp_count;
  int *specific_lbps;
  int specific_lbp_count;
  int *specific_dma_channels;  // only those not reserved for mipi streams
  int specific_dma_channel_count;
} ImxSpecificCoreResourcesDescription;

/* A description of the mipi streams requested for a device. A mipi stream
 * transfers (image) data for one <virtual-channel-id, mipi-data-type> pair.
 * num_mipi_in_streams contains the number of streams required for each
 * mipi_in interface.
 * mipi_in_interface_count specifies the size of num_mipi_in_streams array
 * num_mipi_out_streams contains the number of streams required for each
 * mipi_out interface.
 * mipi_out_interface_count specifies the size of num_mipi_out_streams array
 */
typedef struct ImxNumberOfMipiResourcesDescription {
  int *num_mipi_in_streams;
  int mipi_in_interface_count;
  int *num_mipi_out_streams;
  int mipi_out_interface_count;
} ImxNumberOfMipiResourcesDescription;

/* A description of specific mipi streams requested for a device. A mipi stream
 * transfers (image) data for one <virtual-channel-id, mipi-data-type> pair.
 * mipi_in streams are numbered consecutively starting from 0 to
 * num_mipi_in_interfaces * num_streams_per_mipi_in_interface - 1
 * mipi_out streams are numbered consecutively starting from 0 to
 * num_mipi_out_interfaces * num_streams_per_mipi_out_interface - 1
 *
 * specific_mipi_in_streams contains the required mipi in stream ids.
 * specific_mipi_in_stream_count is size of specific_mipi_in_streams array.
 * specific_mipi_out_streams contains the required mipi out stream ids.
 * specific_mipi_out_stream_count is size of specific_mipi_out_streams array.
 *
 */
typedef struct ImxSpecificMipiResourcesDescription {
  int *specific_mipi_in_streams;
  int specific_mipi_in_stream_count;
  int *specific_mipi_out_streams;
  int specific_mipi_out_stream_count;
} ImxSpecificMipiResourcesDescription;

typedef enum {
  IMX_NO_RESOURCES_DESCRIPTION = 0,
  IMX_ALL_RESOURCES_DESCRIPTION,
  IMX_NUMBER_OF_RESOURCES_DESCRIPTION,
  IMX_SPECIFIC_RESOURCES_DESCRIPTION,
  IMX_AVAILABLE_RESOURCES_DESCRIPTION,
} ImxResourceDescriptionMode;

/* Describes how the resources assigned to a device will be used for the
 * duration of device.
 * The default usage mode is cooperative - runtime is allowed to reassign
 * allocated resources to another device when this device is idle.
 * The other supported mode is exclusive use - allocated resources will not
 * be reassigned for the duration of this device.
 *
 * In future, there may be other modes, such as:
 *   PREEMPTION_ALLOWED - device resources can be taken away and assigned to a
 *                        (higher priority) application.
 *   AUTO_MANAGED - runtime and compiler can manage the resources, possibly
 *                  choosing one of the above modes based on a global strategy
 */
typedef enum {
  IMX_RESOURCE_USAGE_COOPERATIVE = 0,  // can relinquish resources when
                                       // device is "idle"
  IMX_RESOURCE_USAGE_EXCLUSIVE         // resources are assigned for duration
                                       // of device (until device is deleted).
} ImxResourceUsageMode;

/* A description of the overall "virtual" device characteristics.
 *   1) Resource requirements (number of resources or specific resource ids)
 *   2) Resource usage: E.g. Resources are in exclusive use for the duration of
 *                      this device, or can be cooperatively context-switched.
 * A device description consists of a description of it's core resources (such
 * as STP, LBP, DMA channels) and it's io resources (for now, only
 * the mipi interfaces require description).
 *
 * Note: Currently, it is not allowed to mix-and-match number of resources
 * and specific ids when requesting resources.
 * Allowed combinations are:
 *       core resources                         io resources
 * IMX_ALL_RESOURCES_DESCRIPTION          IMX_NO_RESOURCES_DESCRIPTION
 *
 * IMX_NO_RESOURCES_DESCRIPTION           IMX_ALL_RESOURCES_DESCRIPTION
 * IMX_NO_RESOURCES_DESCRIPTION           IMX_NUMBER_OF_RESOURCES_DESCRIPTION
 * IMX_NO_RESOURCES_DESCRIPTION           IMX_SPECIFIC_RESOURCES_DESCRIPTION
 *
 * IMX_NUMBER_OF_RESOURCES_DESCRIPTION    IMX_NO_RESOURCES_DESCRIPTION
 * IMX_NUMBER_OF_RESOURCES_DESCRIPTION    IMX_NUMBER_OF_RESOURCES_DESCRIPTION
 *
 * IMX_SPECIFIC_RESOURCES_DESCRIPTION     IMX_NO_RESOURCES_DESCRIPTION
 * IMX_SPECIFIC_RESOURCES_DESCRIPTION     IMX_SPECIFIC_RESOURCES_DESCRIPTION
 *
 * IMX_AVAILABLE_RESOURCES_DESCRIPTION    IMX_NO_RESOURCES_DESCRIPTION
 * IMX_AVAILABLE_RESOURCES_DESCRIPTION    IMX_ALL_RESOURCES_DESCRIPTION
 * IMX_AVAILABLE_RESOURCES_DESCRIPTION    IMX_NUMBER_OF_RESOURCES_DESCRIPTION
 *
 * device_path selects the physical device that the virtual device is created
 * on. A device_path of NULL selects the default device.
 */
typedef struct ImxDeviceDescription {
  ImxResourceDescriptionMode core_resource_description_mode;
  union {
    ImxNumberOfCoreResourcesDescription num_resources;
    ImxSpecificCoreResourcesDescription specific_resources;
  } core_resource_description;
  ImxResourceDescriptionMode io_resource_description_mode;
  union {
    ImxNumberOfMipiResourcesDescription num_mipi_resources;
    ImxSpecificMipiResourcesDescription specific_mipi_resources;
  } io_resource_description;
  ImxResourceUsageMode resource_usage_mode;
  const char *device_path;
  int num_simulator_options;
  char **simulator_options;
} ImxDeviceDescription;

/* ImxGetDevice and ImxGetDefaultDevice are used to acquire IPU resources (such
 * as STPs, LBPs, etc.) for use in the application. It is ok to make multiple
 * calls to these functions to get multiple devices (either one after the other,
 * or simultaneously active). In general, we expect that the resources are
 * requested in a co-operative manner. Note that the nature of resources being
 * requested may impose an order on the ImxGet*Device calls. MIPI resources are
 * more constrained than other resources (because each MIPI stream is tied to
 * a particular dma channel). Currently, it is the application (or applications)
 * responsibility to ensure that MIPI allocations are done before other
 * allocations. In future, we'll provide the ImxSpecific*Resources description
 * to make it easier to request specific dma channels and avoid any conflicts.
 */

/* Create device with all available core (no mipi) resources
 * (cooperative use).
 */
ImxError ImxGetDefaultDevice(ImxDeviceHandle *device_handle_ptr);
ImxError ImxGetDefaultDeviceWithOptions(ImxDeviceHandle *device_handle_ptr,
                                        int num_simulator_options,
                                        char **simulator_options);

/* Creates a device with resources as specified in device_descr
 * device_descr device description, includes resource required for this device.
 * device_descr is not modified in this function; however, the resource
 * allocation specified by device_descr could be overridden by options in
 * simulator_options or command-line flags.
 * device_handle_ptr the returned handle pointer for the device created.
 */
ImxError ImxGetDevice(ImxDeviceDescription *device_descr  /* in */,
                      ImxDeviceHandle *device_handle_ptr  /* out */);
ImxError ImxGetDeviceWithOptions(
    ImxDeviceDescription *device_descr  /* in */,
    int num_simulator_options,
    char **simulator_options,  /* modified */
    ImxDeviceHandle *device_handle_ptr  /* out */);

/* Gets a description of the device's allocated resources.
 * For each resource group, the following resource description modes are
 * supported:
 *
 * IMX_NO_RESOURCES_DESCRIPTION:
 *   - No description is returned for the resource group.
 *
 * IMX_SPECIFIC_RESOURCES_DESCRIPTION:
 *   - specific_<resource> arrays must be allocated by the caller.
 *   - For each specific_<resource> array, set specific_<resource>_count equal
 *     to the size of the array.
 *   - Upon return, the specific_<resource> arrays will be filled to the lesser
 *     of the size of the array and the number of allocated <resource>s.
 *   - Upon return, specific_<resource>_count is set to the number of allocated
 *     <resource>s.
 *
 * The following device description fields are left unmodified:
 * resource_usage_mode, device_path, and [num_]simulator_options.
 */
ImxError ImxGetDeviceDescription(
    ImxDeviceHandle device_handle  /* in */,
    ImxDeviceDescription *device_descr  /* modified */);

/* Sets the command line options (i.e. gFlags FLAG_... variables) for use
 * by IPU compiler, runtime (and simulator, if present).
 * options array contains a list of individual options, specified as
 * --<key>=<value>
 */
ImxError ImxSetCommandLineOptions(
    char **options,  /* modified */
    int num_options);

typedef void (*ImxDumpCallback)(const char *);

/* Redirects "dump" instruction output dump_callback
 *
 * Default behavior is to write to STDOUT
 *
 * This interface is intended for automated testing the dump functionality
 */
ImxError ImxDeviceSetDumpCallback(ImxDeviceHandle device,
                                  ImxDumpCallback dump_callback);

typedef void (*ImxFatalErrorCallback)();

/* Register a fatal error callback.
 *
 * A fatal error in the IPU will be reported through this callback.  This
 * allows for a client to be notified of a fatal error in all circumstances.
 * Note that fatal errors may also be reported through return codes in addition
 * the fatal error callback.
 *
 * Fatal errors that can be reported through this callback include, but are not
 * limted to, BIF errors, DMA VA errors, MMU errors.
 */
ImxError ImxDeviceSetFatalErrorCallback(ImxDeviceHandle device,
                                        ImxFatalErrorCallback fatal_callback);

/* Deletes the device, releasing all its resources. Note: Dereferencing
 * device_handle after deletion of device may result in unspecified behavior.
 */
ImxError ImxDeleteDevice(ImxDeviceHandle device_handle);

/* If |trace_dir| is a non-empty string, writes a compile renames file is to
 * |trace_dir| every time a job is executed.
 */
ImxError ImxDeviceSetTraceDir(ImxDeviceHandle device_handle,
                              const char *trace_dir);

typedef enum {
  IMX_MEMORY_READ = 0,       /* Transfer node (DMA) reads from memory */
  IMX_MEMORY_WRITE,          /* Transfer node writes to memory */
  IMX_MIPI_READ,             /* Transfer node reads from MIPI */
  IMX_MIPI_WRITE,            /* Transfer node writes to MIPI */
  IMX_MIPI_READ_MEMORY_WRITE /* Transfer node reads from MIPI and
                                directly writes to memory */
} ImxParameterUse;

typedef enum {
  IMX_ACTUAL_SIZE = 0,  /* Specified size is actual image/tensor size */
  IMX_MAX_SIZE,         /* Specified size is maximum allowed size */
  IMX_UNKNOWN_SIZE,     /* Nothing is known about size at current time */
  IMX_kMaxSizeKind      /* Internal use only */
} ImxSizeKind;

/* Shape (dimensions, extent, and border sizes) of an image/tensor; the
 * extents and border sizes may be only partially specified.
 * The boundary sizes affect the (x,y) coordinates seen in the vISA code:
 *   The vISA (x=0, y=0) coordinate corresponds to the first pixel that is
 *   not in the boundary region.
 * For an input image, boundary pixels are used in lieu of boundary-clamping
 *   behavior.  However, if the vISA kernel accesses pixels outside of the
 *   provided boundary, the pixel value will be defined by the boundary-clamping
 *   behavior.  Note that mirroring, etc. are done using the full image
 *   (including boundary pixels).  Depending on the needs of the consuming
 *   kernel, boundary pixels might not be read at all.
 * For an output image, the boundary pixels will always be computed; this
 *   capability is typically used for computing an intermediate image.  The
 *   only impact of the boundary definition is on the (x,y) vISA coordinates.
 * Dimensions are: width(0), height(1), planes(2), components_per_plane(3)
 */
typedef enum {
  IMX_DIM_FIRST,

  IMX_DIM_X = IMX_DIM_FIRST,
  IMX_DIM_Y,
  IMX_DIM_PLANE,
  IMX_DIM_COMPONENT_PER_PLANE,

  IMX_DIM_MAX
} ImxDimension;

typedef struct ImxShape {
  unsigned int dimensions; /* zero indicates scalar */
  struct {
    ImxSizeKind kind;
    /* The following are only valid if kind is ACTUAL_SIZE or MAX_SIZE */
    uint64_t extent; // size of this dimension, including border pixels
    int64_t min; // minimum coordinate of the first pixel.
  } dim[IMX_DIM_MAX];          /* [0]=x, [1]=y, etc.
                        Same order applies in Halide and vISA */
} ImxShape;

/* Data type of a kernel parameter (incl. stream/tensor) as seen by kernel.
 * Omits information about layout in memory
 */
typedef struct ImxParameterType {
  ImxShape shape;
  ImxNumericType element_type;  /* "STP/LBP" types only, e.g. IMX_INT16 */
} ImxParameterType;

/* Storage information for a parameter/stream/tensor stored in memory,
 * augmenting information provided by ParameterType.
 * If the element_type specified here is different from that specified in an
 * associated ParameterType a conversion is implied, e.g. from INT12 to INT16.
*/
typedef struct ImxStorage {
  ImxNumericType element_type; /* can be "processor" or "memory" type */
  ImxLayout layout;
} ImxStorage;

/* A "buffer" maintains the data location for dma transfer (source buffer for a
 * transfer into IPU line-buffer, or destination buffer for a transfer from IPU
 * line-buffer). This enum indicates the type of buffer; currently, we allow
 * two types: DEVICE (i.e. DRAM memory) or MIPI.
 * DEVICE buffers have an address where data is read/written.
 */
typedef enum {
  IMX_DEVICE_BUFFER = 0,
  IMX_MIPI_BUFFER,
  IMX_kMaxBufferType  /* Internal use only */
} ImxBufferType;

// TODO(dfinchel) make sure this is checked wherever we iterate over planes
enum { kImxMaxPlanes = 512 };
typedef struct ImxLateBufferConfig {
  ImxBufferType buffer_type;
  ImxDeviceBufferHandle buffer;
  /* Information for each plane
   *   o If layout is IMX_LAYOUT_LINEAR, only plane[0] should be set.
   *   o If layout is IMX_LAYOUT_PLANAR*, the number of planes is
   *     determined by the extent of the last dimension.
   * TODO(ahalambi): Refactor this structure to more closely match purple text
   * in API documentation
   */
  struct {
    uint64_t offset; /* offset within buffer for start of the plane, in bytes */
    /* stride, in bytes, for the dimensions stored in this plane.
     * 0 = use default (tightly packed).
     * currently only stride[1] is allowed to be non-zero
     */
    uint64_t stride[IMX_DIM_MAX];
  } plane[kImxMaxPlanes]; // TODO(dfinchel) do we use planes other than 0?
} ImxLateBufferConfig;

/* Creates graph with a program to be executed on IPU.
 * If the graph does not contain any kernels, set visa_string to NULL
 * (or nullptr, if using this file as C++ header)
 * The specified graph_name will be used as a string id for the graph, used for
 * hashing and while dumping logging information. Specifying a unique name is
 * desired but not required. If graph_name is NULL, a default unique name will
 * be created by Runtime.
 */
/* TODO(billmark): Switch to desired definition of ImxCreateGraph
 */
ImxError ImxCreateGraph(
    const char *graph_name,
    const char *visa_string,
    ImxNodeHandle *nodes, /* Input - Array of nodes */
    /* Input - Parameter name for each xfer node */
    const char **node_names,
    int node_count,  /* Size of previous two arrays */
    ImxGraphHandle *graph_handle_ptr /* Output */);

/* DEPRECATED; use ImxCreateGraphFromVisaString instead */
ImxError ImxCreateGraphHack(
    const char *visa_string,
    ImxNodeHandle *nodes, /* Input - Array of nodes */
    /* Input - Parameter name for each xfer node */
    const char **node_names,
    int node_count,  /* Size of previous two arrays */
    ImxGraphHandle *graph_handle_ptr /* Output */);

/* Option flags when compiling a graph.
 * This enum is used only for passing simulator options to the runtime
 * or for directly controlling the runtime.
 * Translator options are embedded in vISA program string as directives.
 */
typedef enum {
  // Simulator only options
  IMX_OPTION_SIMULATOR_DUMP_PATH = 0,
  IMX_OPTION_SIMULATOR_DUMP_IMAGE = 1,
  IMX_OPTION_SIMULATOR_ENABLE_JIT = 2,
  IMX_OPTION_SIMULATOR_ENABLE_BINARY_PISA = 3,
  IMX_OPTION_SIMULATOR_HW_CONFIG_FILE = 4, // not used for graph compile
  IMX_OPTION_HISA = 5,
  IMX_OPTION_ENABLE_STRIPING = 6,
  IMX_kMaxCompileGraphOption  /* Internal use only */
} ImxCompileGraphOption;

typedef enum {
  IMX_OPTION_VALUE_TYPE_INT64 = 0,
  IMX_OPTION_VALUE_TYPE_POINTER = 1,
  IMX_OPTION_VALUE_TYPE_NONE /* Internal use only */
} ImxOptionValueType;

typedef struct ImxOptionValue {
  ImxOptionValueType type;
  union {
    int64_t i;
    void *p;
  };
} ImxOptionValue;

/* Holds the value for one compilation option */
typedef struct ImxCompileGraphOptionSetting {
  ImxCompileGraphOption option;
  ImxOptionValue value;
} ImxCompileGraphOptionSetting;

/* Describes the parameter to set, and its value */
typedef struct ImxParameterSetting {
  ImxNodeHandle node;     /* kernel node containing the parameter */
  const char *parameter_name;
  ImxParameterType type;
  /* Pointer to the data, which is tightly packed.
   * For multi-dimensional arrays, access as:
   * value[dimension_2][dimension_1][dimension_0], where the dimension
   * indices match those used in the "shape".
   */
  void *value;
} ImxParameterSetting;

typedef struct ImxCompileGraphInfo {
  ImxDeviceHandle device;    /* in */
  ImxParameterSetting* params; /* in (array) */
  int num_params;            /* in -- size of array above */
  ImxCompileGraphOptionSetting *options; /* in (array) */
  int num_options;                /* in -- size of array above */
} ImxCompileGraphInfo;

ImxError ImxCompileGraph(
    ImxGraphHandle graph,
    const ImxCompileGraphInfo *info,
    ImxCompiledGraphHandle *compiled_handle);

ImxError ImxDeleteCompiledGraph(ImxCompiledGraphHandle compiled_graph_handle);

/* Saves a copy of compiled_graph (and related structures) to file(s) in the
 * directory: save_dir_path. Function will overwrite if there exists any
 * identical name within the save_dir_path. Function reports an error if the
 * specified save directory cannot be created. This API function is typically
 * used in conjunction with ImxLoadMatchingPrecompiledGraph()
 *
 * Currently, multiple files are saved: the precompiled graph configuration
 * (save_dir_path/file_name) and binary pISA files (*.bpisa in save_dir_path)
 * for the stencil-processor program kernels.
 */
/* TODO(ahalambi): Change this API (or provide a new one) that returns a
 * binary blob containing all saved state. Application can decide how to use it.
 */
ImxError ImxSaveCompiledGraph(
    const char *save_dir_path,
    const char *file_name,
    ImxCompiledGraphHandle compiled_graph  /* const */);

/* Saves the given compiled_graph into a unique sub-directory within
 * save_dir_base_path, with file name "file_name".
 * Note: This API function is typically used in conjunction with
 * ImxLoadMatchingPrecompiledGraph
 */
ImxError ImxSaveAsUniqueCompiledGraph(
    const char *save_dir_base_path,
    const char *file_name,
    ImxCompiledGraphHandle compiled_graph  /* const */);

/* Loads a precompiled graph configuration (and related files) to create an
 * ImxCompiledGraph object. The precompiled graph configuration file should be
 * in the load_dir_path directory and should be named <file_name>.
 * The supplied transfer_nodes, transfer_node_names, and info input arguments
 * must be the same as those supplied to the saved precompiled graph (via a
 * call to ImxSaveCompiledGraph API).
 */
ImxError ImxLoadPrecompiledGraph(
    const char *load_dir_path,
    const char *file_name,
    ImxNodeHandle *transfer_nodes,  /* Input - Array of nodes */
    /* Input - Parameter name for each xfer node */
    const char **transfer_node_names,
    int transfer_node_count,  /* Size of previous two arrays */
    const ImxCompileGraphInfo *info,
    ImxCompiledGraphHandle *compiled_graph_handle_ptr  /* Output */);

/* Finds a precompiled graph configuration that matches the given input (
 * transfer_nodes, parameter settings, etc) by searching within
 * load_dir_base_path (and sub-directories one level down) and file named
 * "file_name".
 * Returns IMX_SUCCESS if a matching precompiled graph configuration was found;
 * IMX_FAILURE (or another error code) otherwise.
 * Note: This API function is typically used in conjunction with
 * ImxSaveCompiledGraph() or ImxSaveAsUniqueCompiledGraph()
 */
ImxError ImxLoadMatchingPrecompiledGraph(
    const char *load_dir_base_path,
    const char *file_name,
    ImxNodeHandle *nodes,  /* Input - Array of nodes */
    /* Input - Parameter name for each xfer node */
    const char **node_names,
    int node_count,  /* Size of previous two arrays */
    const ImxCompileGraphInfo *info,
    ImxCompiledGraphHandle *compiled_graph_handle_ptr  /* Output */);

typedef enum {
  IMX_PCG_FAILURE,  // generic failure
  IMX_PCG_NOT_FOUND,
  IMX_PCG_UNKNOWN_DIMENSIONS,  // internal use (initialization purposes only).
  IMX_PCG_IS_INCOMPATIBLE,
  // "unequal" implies that some transfer nodes were larger and some were
  // smaller dimensions.
  IMX_PCG_HAS_UNEQUAL_DIMENSIONS,
  IMX_PCG_HAS_SAME_DIMENSIONS,
  IMX_PCG_HAS_SMALLER_DIMENSIONS,
  IMX_PCG_HAS_LARGER_DIMENSIONS
} ImxFindPcgResult;

/* Same as ImxLoadMatchingPrecompiledGraph except that it doesn't return a
 * handle to the compiled graph and doesn't log errors on failure.
 * Intended for checking whether a matching PCG has already been saved.
 * Sets the result of PCG (precompiled graph) comparison in find_pcg_result.
 */
ImxError ImxFindMatchingPrecompiledGraph(
    const char *load_dir_base_path,
    const char *file_name,
    ImxNodeHandle *nodes,  /* Input - Array of nodes */
    /* Input - Parameter name for each xfer node */
    const char **node_names,
    int node_count,  /* Size of previous two arrays */
    const ImxCompileGraphInfo *info,
    /* Output - Result of matching against pcg */
    ImxFindPcgResult *find_pcg_result);

/* Preloads all precompiled graph configs that match the given file name and
 * returns them in lexicographical order of their base paths.
 * Expects the following directory hierarchy:
 *
 *        <pcg root>
 *        /        \
 *   <base1>  ...  <baseN>
 *      |             |
 * [<pcg_xxx>]   [<pcg_xxx>]
 *      |             |
 * <file name>   <file name>
 *
 * Returns IMX_SUCCESS if no errors occured.
 */
ImxError ImxPreloadPrecompiledGraphs(
    const char *load_dir_root_path,  /* in */
    const char *file_name,  /* in */
    ImxPrecompiledGraphsHandle *precompiled_graphs_handle_ptr  /* out */);

/* Finds the precompiled graph by base name and creates a compiled graph from it
 * and the passed parameters.
 */
ImxError ImxCreateCompiledGraph(
    ImxPrecompiledGraphsHandle precompiled_graphs_handle,  /* in */
    const char *base_name,  /* in */
    ImxNodeHandle *nodes,  /* in - array of nodes */
    /* in - parameter name for each xfer node */
    const char **node_names,
    int node_count,  /* size of previous two arrays */
    const ImxCompileGraphInfo *info,
    ImxCompiledGraphHandle *compiled_graph_handle_ptr  /* out */);

void ImxDeletePrecompiledGraphs(
    ImxPrecompiledGraphsHandle precompiled_graphs_handle  /* in */);

ImxError ImxDeleteGraph(ImxGraphHandle graph_handle);

typedef enum {
  IMX_CONVERT_NONE = 0,
  IMX_CONVERT_LOWBITS,  /* Raw cast of LSBs.
                         * On downsize, discard extra high bits.
                         * On upsize, set extra high bits to zero
                         */
  /* others will be added later */
} ImxConversion;

/* Specifies what to return for an out-of-bounds access to an input image */
typedef enum {
  IMX_BORDER_ZERO = 0,     /* zero */
  IMX_BORDER_CONSTANT,     /* constant value, as specified in 'border_value' */
  IMX_BORDER_REPEAT_EDGE,  /* Return value of nearest in-bounds edge sample */
  IMX_BORDER_REPEAT_WIDE_EDGE, /* Out of bounds periodically repeats
                                * 'edge_width' edge pixels in wrap/tile style
                                */
  IMX_kMaxBorderMode
} ImxBorderMode;

typedef struct ImxBorder {
  ImxBorderMode mode;
  union {
    int8_t   int8;
    uint8_t  uint8;
    int16_t  int16;
    uint16_t uint16;
    int32_t  int32;
    uint32_t uint32;
    float    float32; /* only supported on simulator */
    uint16_t float16; /* only supported on simulator.  contains raw bits */
  } border_value; /* Only used with IMX_BORDER_CONSTANT */
  int32_t edge_width;  /* Only used with IMX_BORDER_REPEAT_WIDE_EDGE */
} ImxBorder;

typedef struct ImxMipiStreamIdentifier {
  int interface_id;  /* which mipi input or output hardware interface */
  int virtual_channel_id;  /* As defined in MIPI CSI-2 spec;
                            * valid values: 0..3
                            */
  int data_type;  /* As defined in Mipi CSI-2 spec; valid values range between
                   * 0x0 - 0x3f. Not all values in range are applicable (e.g.
                   * some values are reserved for future use)
                   */
} ImxMipiStreamIdentifier;

/*
 * ImxTransferNodes, by default, imply dma channels and linebuffers will be
 * configured, as decided by the mapper. These values override those decisions
 * by either stopping the dma or lbp from being configured, or overriding the
 * mapper's decision on number of read pointers.
 *
 * The use of this is for inter-configuration/execution communication through
 * linebuffers. This is different from the normal flow of all inputs and outputs
 * of a launch being accounted for and streamed through the dma, by instead
 * leaving data in the lbp's during one launch, and consuming that data in the
 * next launch. This situation requires the first launch to skip_configure_dma
 * of the first launch's outputs along with setting the number of read pointers
 * that exist in the next launch, then skipping configuration of both the
 * linebuffer and dma in the second launch and instead consuming the data that
 * is already at that address.
 */
typedef struct ImxTransferNodeOverrides {
  int skip_configure_linebuffer;         /* boolean: 0 = configure as normal.
                                          * Otherwise do not configure the lbp
                                          * for this linebuffer.  */
  int skip_configure_dma;                /* boolean: 0 = configure as normal.
                                          * Otherwise do not configure DMA
                                          * channels to connect to this
                                          * linebuffer.  */
  int override_linebuffer_num_consumers; /* boolean: 0 = use the mapper's
                                          * decison for number of consumers.
                                          * Otherwise use
                                          * linebuffer_num_consumers.  */
  int linebuffer_num_consumers; /* Used in configuring the number of read
                                 * pointers for the linebuffer.  Only used if
                                 * skip_configure_linebuffer is 0 and
                                 * override_linebuffer_num_consumers is
                                 * non-zero. This should not be greater than the
                                 * max supported number of read pointers.  */
} ImxTransferNodeOverrides;

typedef struct ImxCreateTransferNodeInfo {
  ImxParameterUse use;
  ImxParameterType parameter_type;
  ImxStorage storage;
  ImxConversion conversion;
  ImxBorder border;  /* Only used for MEMORY_READ */
  ImxMipiStreamIdentifier mipi_stream_id;  /* Only used for MIPI transfer */
  /* If stripe_width > 0 is specified, the IPU runtime will vertically split
   * the image into multiple stripes and execute one stripe at a time. This is
   * useful for reducing line buffer size requirement. */
  int stripe_width;  /* 0 means no striping */
  // TODO(tpopp): remove ImxTransferNodeOverrides from being stored inside
  // ImxCreateTransferNodeInfo. Instead keep it in a separate mapping.
  ImxTransferNodeOverrides transfer_node_overrides;
} ImxCreateTransferNodeInfo;

typedef enum ImxGatherChannelType {
  IMX_GATHER_COORDINATE = 0,
  IMX_GATHER_DATA = 1
} ImxGatherChannelType;

typedef struct ImxGatherInfo {
  /* coordinate channel or image data channel */
  ImxGatherChannelType type;

  /* The 2 fields below are the sheet width and height of the dma channel.
   * As mentioned in the dma design doc, for coordinate channel, the sheet is
   * always 4x4 like this:
   *   X0  X1  X2  X3
   *   Y0  Y1  Y2  Y3
   *   P0  P1  P2  P3
   *   M0  M1  M2  M3
   * Where Xn/Yn are the coordinates, Pn is the plane, and Mn is the active mask
   * (only LSB is valid).
   * For image data channel, the sheet width and height specifies the size of
   * the block the dma channel will be gathering, i.e., the size of the block
   * that will be consumed by the 16x16 simd lanes, e.g.,
   * If the input inst is like [x*2+st0][y*2+st1], then sheet size for gather
   * data channel is 32x32.
   */
  int sheet_width;
  int sheet_height;

  /* Gather mode is ALWAYS configured in pair, in order to know which
   * two channels form a pair (and there might be multiple gather pairs),
   * fifo_id is used to show two dma channels are in the same fifo/pair.
   * In other words, transfer nodes in the same fifo will have same fifo_id.
   * Undefined error may appear if the pair is not formed properly in the input.
   */
  int fifo_id;
} ImxGatherInfo;

ImxCreateTransferNodeInfo ImxDefaultCreateTransferNodeInfo();

ImxError ImxCreateTransferNode(
    const ImxCreateTransferNodeInfo *info,
    ImxNodeHandle *node_handle_ptr);

ImxCreateTransferNodeInfo ImxDefaultCreateTransferNodeInfo();

typedef struct ImxCreatePaddingNodeInfo {
  ImxShape padding_region;  /* Must be two-dimensional. */
  ImxBorder border;
} ImxCreatePaddingNodeInfo;

ImxError ImxCreateInternalNode(
    const ImxCreatePaddingNodeInfo *info,
    ImxNodeHandle *node_handle_ptr);

ImxError ImxCreateTransferNodeWithGatherInfo(
    const ImxCreateTransferNodeInfo *info,
    const ImxGatherInfo *gather_info,
    ImxNodeHandle *node_handle_ptr);

ImxError ImxDeleteNode(
    ImxNodeHandle node_handle);

ImxError ImxCreateJob(
    ImxCompiledGraphHandle compiled_graph, /* in */
    ImxJobHandle *job_handle_ptr /* out */);

ImxError ImxDeleteJob(ImxJobHandle job_handle);

ImxError ImxSetLateParameters(
    ImxJobHandle job,
    ImxParameterSetting *params,
    int num_params);

/* TODO(ahalambi): timestamp could be uint64
 * Requires other timestamps (paintbox.h) to also be unsigned.
 */
typedef struct ImxTransferEventStatus {
  /* Event timestamp, in nanoseconds, using device local boot time.
   * This may need to be reconciled with global/main time (from AP).
   */
  int64_t timestamp_ns;
  /* Error code, in case of error.
   * TODO(ahalambi): Standardize and enumerate the error codes. */
  int error;
  /* Payload information from the event. Not valid for all nodes. For nodes
   * representing MIPI input streams this is the frame number.
   */
  union {
    uint16_t frame_number;
  } event_data;
} ImxTransferEventStatus;

/* Collects status information (such as timestamp, error code, etc) during
 * an actual transfer for the given node. The actual transfer (called a
 * transfer event) may be the transfer of an image frame via MIPI Input, a
 * DeviceBuffer transferred via DMA, etc. For the specified transfer node,
 * latest transfer status will be recorded in the given status object.
 * Set status = nullptr to stop monitoring.
 *
 * For MIPI Input transfers, Start-Of-Frame (SOF) timestamp is recorded.
 * For MIPI Output and DMA transfers, End-Of-Frame (EOF) timestamp is recorded.
 *
 * The TransferEventStatus object will be updated by ImxExecuteJob.
 * Note that the status values are undefined if this object is accessed
 * *during* the time ImxExecuteJob is active.
 */
ImxError ImxSetTransferEventMonitor(
    ImxJobHandle job,  /* in */
    ImxNodeHandle node,  /* in */
    ImxTransferEventStatus *status  /* will be modified later */);

ImxError ImxClearTransferEventMonitor(
    ImxJobHandle job,  /* in */
    ImxNodeHandle node  /* in */);

typedef enum ImxMemoryAllocatorType {
  IMX_MEMORY_ALLOCATOR_NONE,
  IMX_MEMORY_ALLOCATOR_MALLOC,  /* Implies no ION support */
  IMX_MEMORY_ALLOCATOR_ION,     /* Implies both malloc and ION support */
  IMX_MEMORY_ALLOCATOR_DEFAULT  /* Default varies based on target system */
} ImxMemoryAllocatorType;

/* A general note about the memory-allocator and device-buffer APIs below:
 * These APIs are not thread-aware/thread-safe. The user of these APIs must
 * use appropriate synchronization/locking mechanisms based on the usage model.
 * If two or more threads may simultaneously call these APIs on objects that
 * are shared across them, please ensure mutually-exclusive, atomic use of
 * these APIs with appropriate locking mechanisms.
 */

/* MemoryAllocator manages the allocation, mapping and sharing of data buffers
 * used by IPU (e.g. buffers to store input/output image frames).
 * Currrent supported memory allocators are
 *   1. user-space dynamic memory allocation (available on all systems)
 *      Corresponds to IMX_MEMORY_ALLOCATOR_MALLOC
 *   2. ION memory allocation (available on systems with IPU driver)
 *      Corresponds to IMX_MEMORY_ALLOCATOR_ION
 *
 * Not thread-safe; see general note on thread-safety above.
 */
ImxError ImxGetMemoryAllocator(
    ImxMemoryAllocatorType allocator_type,
    ImxMemoryAllocatorHandle *memory_allocator_handle_ptr);

/* Gets an ION Memory Allocator initialized with a file-descriptor (ion_fd) that
 * was already created with a call to ion_open().
 *
 * Not thread-safe; see general note on thread-safety above.
 */
ImxError ImxGetIonMemoryAllocator(
    int ion_fd,
    ImxMemoryAllocatorHandle *memory_allocator_handle_ptr);

/* Not thread-safe; see general note on thread-safety above. */
ImxError ImxDeleteMemoryAllocator(
    ImxMemoryAllocatorHandle memory_allocator_handle_ptr);

/* Creates a device buffer using dynamic memory allocation (i.e. malloc).
 * We assume that malloc is available on all systems. Using malloc'd buffers
 * will result in data copy when the buffer is transferred between CPU and IPU.
 * TODO(ahalambi): Deprecate this API in favor of always using a
 * memory-allocator when creating buffers. "Simple" is not a very descriptive
 * term. Ideally, the API name should describe some characteristics of the
 * buffer (rather than the allocation strategy to create the buffer).
 * "Simple" buffers do not have advanced features such as support
 * for alignment, partial-mapping, sharing, etc.
 *
 * flags: The intent is to be able to pass any system supported flags for
 * allocation purposes. Currently, this must be set to 0.
 *
 * Not thread-safe; see general note on thread-safety above.
 */
ImxError ImxCreateDeviceBufferSimple(
    uint64_t size_bytes,
    int flags,
    ImxDeviceBufferHandle *buffer_handle_ptr);

// Sets the default alignment (in bytes) based on LPDDR4 burst length
#define kImxDefaultDeviceBufferAlignment 64

// Specifies to use default heap created for the underlying system.
// Currently, the default heap for ION is CARVEOUT heap.
#define kImxDefaultDeviceBufferHeap 0

/* Creates a device buffer using preferred allocation strategy of the specified
 * memory allocator. Currently, the only zero-copy memory allocator supported
 * is ION.
 *
 * align_bytes: Specifies the minimum required alignment. This value must be a
 * power of 2. Use kImxDefaultDeviceBufferAlignment for default alignment.
 *
 * heap_type: Specific to the Memory Allocator used. heap_type = 0 is reserved
 * for the "default" heap. To specify default heap, please use the constant
 * kImxDefaultDeviceBufferHeap defined above.
 * For ION heaps, kImxDefaultDeviceBufferHeap and heap_id_masks in <uapi/ion.h>
 * are the valid values for heap_type.
 *
 * flags: Memory-allocator (and target system) specific options that will be
 * passed to the allocator when allocating memory space to the buffer.
 * For ION flags, please refer to allocation flags in <uapi/ion.h>
 *   For buffers shared between CPU and IPU, specify ION_FLAG_CACHED
 *   A flag value 0 implies that this buffer does not need cache
 *   flush/invalidation (e.g. for intermediate, IPU-use-only buffers).
 *
 * Not thread-safe; see general note on thread-safety above.
 */
ImxError ImxCreateDeviceBufferManaged(
    ImxMemoryAllocatorHandle memory_allocator,
    uint64_t size_bytes,
    uint32_t align_bytes,
    int heap_type,
    int flags,
    ImxDeviceBufferHandle *buffer_handle_ptr);

/* TODO(ahalambi): Add an API to create an ION DeviceBuffer given a
 * ion handle (or equivalent such as file-descriptor). See b/33843974
 */

/* Flushes and invalidates a device buffer's cache entries.
 * The buffer must be managed and locked, otherwise IMX_INVALID is returned.
 */
ImxError ImxFlushAndInvalidateDeviceBufferCacheEntries(
    ImxDeviceBufferHandle buffer_handle);

/* Not thread-safe; see general note on thread-safety above. */
ImxError ImxDeleteDeviceBuffer(
    ImxDeviceBufferHandle buffer_handle);

/* Schedules buffer_handle for deletion. Buffers are deleted in FIFO order
 * after earlier scheduled jobs are completed.
 * It is an error if buffer_handle or device_handle is invalid.
 */
ImxError ImxDeleteDeviceBufferAsync(
    ImxDeviceBufferHandle buffer_handle,
    ImxDeviceHandle device_handle);

/* Provides the host (i.e. CPU) mapped address for buffer_handle device buffer.
 * vaddr is initialized with the host address.
 * If the buffer has not been mapped to a host address (i.e. it is a device
 * only buffer), then vaddr is initialized to NULL.
 */
ImxError ImxGetDeviceBufferHostAddress(
    ImxDeviceBufferHandle buffer_handle,
    void **vaddr);

/* Lock: Makes the buffer ready for use by CPU user-space process.
 * *(vaddr) will hold the mapped virtual address of the buffer.
 * May result in a mmap operation to map a kernel buffer to user-space.
 * Will reuse a previous mapping if it is still valid.
 * After locking, buffer is ready for exclusive use by CPU.
 *
 * Not thread-safe; see general note on thread-safety above.
 */
ImxError ImxLockDeviceBuffer(
    ImxDeviceBufferHandle buffer_handle,
    void **vaddr);

/* Unlock: The buffer is unavailable for use by CPU user-space process.
 * After unlocking, the buffer can be used by IPU (to perform DMA transfers).
 * This is the default state of a newly created device buffer.
 * Note: Unlocking does *not* also unmap a previously mmap'd virtual address.
 *
 * Not thread-safe; see general note on thread-safety above.
 */
ImxError ImxUnlockDeviceBuffer(
    ImxDeviceBufferHandle buffer_handle);

/* Share: Makes the buffer ready to be shared with other user-space processes
 * and/or kernel. Note: This API is typically for sharing across processes;
 * among threads within the same process, share the buffer_handle instead.
 * (*fd) will hold the file-descriptor that can be passed to the other processes
 * to facilitate importing of the shared buffer.
 * Returns IMX_FAILURE if it is not possible to share this buffer (for example,
 * if it is a malloc'd buffer).
 *
 * Note: The shared fd must be passed to other processes using sendmsg, pipe,
 * or similar mechanism for shared memory message passing. This allows the
 * kernel to recognize that the message is a file-descriptor and translate it
 * to a new number usable in the other process.
 *
 * Not thread-safe; see general note on thread-safety above.
 */
ImxError ImxShareDeviceBuffer(
    ImxDeviceBufferHandle buffer_handle,
    int *fd);

/* Import: Creates a buffer handle for an imported buffer (which was shared by
 * another process). This API will create a new device buffer
 * with access to the shared content (without copying to another location).
 * fd is the file-descriptor that was shared by another process. This fd is
 * usually the file-descriptor provided by a call to ImxShareDeviceBuffer (in
 * another process).
 *
 * Note: The shared fd must have been passed from other processes using sendmsg,
 * pipe, or similar mechanism for shared memory message passing.
 *
 * Not thread-safe; see general note on thread-safety above.
 */
ImxError ImxImportDeviceBuffer(
    int fd,
    uint64_t size_bytes,
    ImxDeviceBufferHandle *buffer_handle);

/* Reverses mapping from a mapped virtual address to buffer handle.
 * If vaddr is a valid address in range of any previously created (and mmapped)
 * DeviceBuffer, place the buffer handle in buffer_handle_ptr. Also, compute
 * the offset from start of buffer address and place it in offset
 *
 * Not thread-safe; see general note on thread-safety above.
 */
ImxError ImxGetDeviceBufferFromAddress(
    const void *vaddr,  /* in */
    ImxDeviceBufferHandle *buffer_handle_ptr,  /* modified */
    uint64_t *offset  /* modified */);

typedef struct ImxFinalizeBufferInfo {
  ImxNodeHandle node;
  ImxLateBufferConfig config;
} ImxFinalizeBufferInfo;

ImxError ImxFinalizeBuffers(
    ImxJobHandle job, /* modified */
    const ImxFinalizeBufferInfo *info, /* in */
    int num_info);

/* TODO(ahalambi): Consolidate the two API functions (below) for executing a
 * job into a single function that takes a timeout argument. Also provide a
 * "default" value for the timeout.
 */

/* Blocking call to execute the input job. This function loads IPU device
 * configuration, starts the execution and waits for completion of the job.
 * All late-bound configuration information (such as DRAM buffers for DMA
 * transfers) must already be provided before invoking this function.
 */
ImxError ImxExecuteJob(
    ImxJobHandle job /* modified */);

/* Non-blocking call to execute the input job in a background thread.
 * The background thread loads IPU device configuration, starts the
 * execution and waits for completion of the job.
 * All late-bound configuration information (such as DRAM buffers for DMA
 * transfers) must already be provided before invoking this function.
 */
ImxError ImxExecuteJobAsync(
    ImxJobHandle job /* modified */);

/* Non-blocking call to execute the input job in a background thread.
 * The background thread loads IPU device configuration, starts the
 * execution and waits for completion of the job.
 * All late-bound configuration information (such as DRAM buffers for DMA
 * transfers) must already be provided before invoking this function.
 *
 * timeout_ns specifies the maximum elapsed time (in nano-seconds) to wait for
 * this job to complete. The countdown begins once this job actually starts
 * executing on the IPU (and not when it is enqueued for execution).
 */
ImxError ImxExecuteJobAsyncWithTimeout(
    ImxJobHandle job, /* modified */
    int64_t timeout_ns);

/* Waits for the previous asynchronous execution of the job to complete.
 *
 * Note: ImxExecuteJobWait must be paired with an ImxExecuteJobAsync.
 */
ImxError ImxExecuteJobWait(
    ImxJobHandle job /* modified */);

/* timeout_ns: elapsed time (in nanoseconds) to wait for the MIPI flush to
 * complete.  If MIPI flush time exceeds timeout then IMX_TIMEOUT is returned.
 */
ImxError ImxFlushMipiInputJobWait(
    ImxJobHandle job, /* modified */
    int64_t timeout_ns);

/* Waits for the completion of one item in the asynchronous job queue,
 * typically ImxJob enqueued using ImxExecuteJobAsync api or ImxDeviceBuffer
 * deletion enqueued using ImxDeleteDeviceBufferAsync.
 * It is an error if input device_handle is invalid, or if no
 * job queue item has been enqueued.
 * Returns the error code of completed item, or IMX_TIMEOUT if no item was
 * completed within the (default) thread wait timeout.
 */
ImxError ImxWaitForJobQueueOneItemCompletion(
    ImxDeviceHandle device_handle /* input */);

/* Waits for completion of all items enqueued in the asynchronous job queue.
 * If there are no items, returns immediately with IMX_SUCCESS.
 * If any pending job item times out, returns immediately with IMX_TIMEOUT.
 * If any completed/pending job item fails, returns error code of the failure.
 * Returns IMX_SUCCESS otherwise.
 * If this function returns failure, no further items will be executed.
 * Please call ImxResetJobQueue and re-enqueue any further items.
 */
ImxError ImxWaitForJobQueueEmpty(
    ImxDeviceHandle device_handle /* input */);

/* Waits for completion of current item in job queue and resets the queue.
 * All pending and completed items will be discarded.
 * Normally, this function should only be called if ImxWaitForJobQueueEmpty or
 * ImxWaitForJobQueueOneItemCompletion was unsuccessful.
 */
ImxError ImxResetJobQueue(
    ImxDeviceHandle device_handle /* input */);

/* timeout_ns: elapsed time (in nanoseconds) to wait for the job to complete.
 * If job execution time exceeds timeout, the job is terminated and
 * IMX_TIMEOUT is returned.
 */
ImxError ImxExecuteJobWithTimeout(
    ImxJobHandle job,  /* modified */
    int64_t timeout_ns);

ImxError ImxJobSetStpPcHistogramEnable(ImxJobHandle job, /* modified */
                                       uint64_t stp_pc_histogram_mask);

/* The Job APIs below can be used to implement a Queueing model for job
 * configuration and execution. The basic structure is:
 * One time, at start of job queue:
 *   ImxLoadDeviceConfig : Load device with the required configuration.
 * During actual execution:
 *   ImxEnqueueJob : Non-blocking call to queue up an iteration of the job.
 *   ImxWaitForCompletion: Blocking call to wait for an iteration of the job
 *   to complete (successfully or with error).
 *   Note: Calls to EnqueueJob and WaitForCompletion can be in any order, as
 *   long as the number of WaitForCompletion calls do not exceed the calls to
 *   EnqueueJob.
 * One time, at end of job queue:
 *   ImxUnloadDeviceConfig: Release device for a different job (or shutdown).
 *
 * The current job queue model requires the application/user to keep track of
 * output buffers. E.g., copying output buffers to host address if required
 * (not required if using ION buffers in host-device unified memory space).
 *
 * TODO(ahalambi): Currently, the queue model is only supported for jobs that
 * do not require STP configuration. E.g. MPI->DRAM or DRAM->MPO transfer.
 * Extend the implementation to support jobs with STP configuration.
 * TODO(ahalambi): The APIs below require that the same job handle be used for
 * the duration of queue. It is preferable to have job represent one execution
 * (i.e. producing one output frame), and have another data-structure
 * (DeviceConfig) represent the persistent device state (e.g. resource bindings)
 * required to execute the job.
 */

/* Loads all the required IPU configuration to execute this job. This may
 * involve loading STPs with programs, configuring DMA channels, etc.
 * Note: Some configuration is late-bound and is only done at actual job
 * execution. E.g. DRAM buffers for DMA transfers.
 *
 * It is an error to call this API function when there is already a loaded
 * device configuration (for any job associated with the device).
 *
 * NOTE: Jobs with STP programs are currently not supported!
 * Please use ImxExecuteJob for such jobs.
 */
ImxError ImxLoadDeviceConfig(
    ImxJobHandle job  /* modified */);

/* Unloads previously loaded IPU configuration.
 *
 * It is an error to call this API function with a job that is not the
 * currently loaded job.
 *
 * NOTE: Jobs with STP programs are currently not supported!
 * Please use ImxExecuteJob for such jobs.
 */
ImxError ImxUnloadDeviceConfig(
    ImxJobHandle job  /* modified */);

/* Enqueues job for (eventual) execution. All late-bound parameters must be
 * finalized before calling this API. This function does not block.
 *
 * NOTE: Jobs with STP programs are currently not supported!
 * Please use ImxExecuteJob for such jobs.
 */
ImxError ImxEnqueueJob(
    ImxJobHandle job  /* modified */);

/* Waits for completion of the currently executing enqueued job. Blocking call!
 *
 * NOTE: Jobs with STP programs are currently not supported!
 * Please use ImxExecuteJob for such jobs.
 */
ImxError ImxWaitForCompletion(
    ImxJobHandle job  /* modified */);

/* Waits for completion of the currently executing enqueued job. Blocking call!
 *
 * NOTE: Jobs with STP programs are currently not supported!
 * Please use ImxExecuteJob for such jobs.
 */
ImxError ImxWaitForCompletionWithTimeout(
    ImxJobHandle job,  /* modified */
    int64_t timeout_ns);

/* Untile buffer: This is a utility function that converts a buffer layout with
 * 4x4 tiling into linear layout. buffer_address can be either host address or
 * device address (ION buffer). For device address, caller is responsible for
 * locking/unlokcing the buffer.
 * 'buffer_address' is used for both source and destination; it will read buffer
 * contents from 'buffer_address' and store the converted layout into
 * 'buffer_address'. That means it does conversion effecitively 'in place'.
 */
ImxError ImxUntileBufferLayout(const ImxParameterType* buffer_type,
                               void *buffer_address);

/* Allows profiler to show information about the execution phases
 * All Profiler descriptions will be prefixed with the phase_name
 */
ImxError ImxProfilerSetPhaseName(const char* phase_name);

/* Enables profiling to the file path profiler_file. If the IPU is under
 * continuous use, then ImxDisableProfiling should be called to close
 * and flush open files safely.
 */
ImxError ImxEnableProfiling(const char *profiler_file);

/* Disables profiling after flushing and closing any open profiler files.
 *
 * This call is only necessary if the IPU is being used continuously. It is
 * safe to call ImxDisableProfiling multiple times if necessary.
 */
ImxError ImxDisableProfiling();

/* Compares stripe configurations stored in two compiled graph. It will print
 * out different configurations.
 * TODO(parkhc): Return the result of comparison to the caller.
 */
ImxError ImxCompareStripeConfigurations(ImxCompiledGraphHandle one,
                                        ImxCompiledGraphHandle two);

/* Updates compiled graph for the new image dimension (width/height).
 * The new image dimensions should be already stored in the compiled graph.
 * This will re-generate new stripe configurations for the new dimensions.
 */
ImxError ImxUpdateImageDimensionsIfNecessary(
    ImxCompiledGraphHandle compiled_graph);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PAINTBOX_IMAGE_PROCESSOR_H_ */

