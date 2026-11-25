#ifndef YATT_SHADER_UTILS_H
#define YATT_SHADER_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <3ds.h>

#include "yatt_error.h"

/**
 * Utilities for initializing PICA200 shader programs with a data-driven design.
 * 
 * If a function does not have special return codes, see yatt_error.h for definitions.
 * If a function returns YATT_INPUT_MALFORMED, the data is guaranteed to be unchanged.
 */

// yatt_validate_shader_binaries error codes
enum {
   YATT_SHBIN_OK = YATT_OK,      // Success
   YATT_SHBIN_ARRAY_IS_NULL,     // The array passed to the function is null
   YATT_SHBIN_DATA_IS_NULL,      // The shader binary's data pointer is null
   YATT_SHBIN_SIZE_IS_NULL,      // The shader binary's size pointer is null
   YATT_SHBIN_DVLB_IS_NOT_NULL,  // The shader binary's DVLB pointer is not null
};

// yatt_validate_shader_programs error codes
enum {
   YATT_SHPROG_OK = YATT_OK,                  // Success
   YATT_SHPROG_ARRAY_IS_NULL,                 // The array passed to the function is null
   YATT_SHPROG_PICA_PROG_IS_NULL,             // The shaderProgram_s pointer is null
   YATT_SHPROG_PICA_PROG_ALREADY_INITIALIZED, // The shaderProgram_s has already been initialized
   YATT_SHPROG_GSH_VSH_DVLE_ARE_SAME,         // The geometry shader and vertex shader have the same DVLE indices
   YATT_SHPROG_VSH_DVLE_IS_NEGATIVE,          // The vertex shader DVLE index is negative
   YATT_SHPROG_BINARY_IS_NULL,                // The YATT_ShaderBinary pointer is null
   YATT_SHPROG_BINARY_DVLB_IS_NULL,           // The YATT_ShaderBinary pointer's DVLB is null
   YATT_SHPROG_VSH_DVLE_OUT_OF_RANGE,         // The vshader's DVLE index is greater than the shader binary's DVLE count
   YATT_SHPROG_GSH_DVLE_OUT_OF_RANGE,         // The geoshader's DVLE index is greater than the shader binary's DVLE count
};

// yatt_validate_shader_uniforms error codes
enum {
   YATT_SHUNIF_OK = YATT_OK,         // Success
   YATT_SHUNIF_ARRAY_IS_NULL,        // The array passed to the function is null
   YATT_SHUNIF_LOCATION_PTR_IS_NULL, // The uniform's loc_pointer is NULL
   YATT_SHUNIF_NAME_IS_NULL,         // The uniform's name is NULL
};

typedef struct
{
   const u8* data;  // Pointer to the raw shader data
   const u32* size; // Pointer to the raw data's size.
   DVLB_s* dvlb;    // Pointer to the parsed DVLB. Initialize to NULL.
} YATT_ShaderBinary;

typedef struct
{
   YATT_ShaderBinary* binary;     // Pointer to the binary to read from
   shaderProgram_s* pica_program; // Pointer to the shader program to write to
   s32 gsh_dvle_index;            // Which DVLE should be assigned to the geometry shader, or -1 to disable
   s32 vsh_dvle_index;            // Which DVLE should be assigned to the vertex shader
   u8 gsh_stride;                 // The stride of the geometry shader
} YATT_ShaderProgram;

typedef struct
{
    char* name;       // Name of the uniform to search for
    int* loc_pointer; // Pointer to a uniform location to overwrite
} YATT_ShaderUniform;

/*
 * Validates a list of shader binaries. See above for possible error codes.
 * This function will not return early after finding an error; it will
 * continue to check for others. However, the return code will be that
 * of the first error found.
 */
int yatt_validate_shader_binaries(YATT_ShaderBinary* binaries, size_t num_binaries);

/*
 * Validates a list of shader programs. See above for possible error codes.
 * This function will not return early after finding an error; it will
 * continue to check for others. However, the return code will be that
 * of the first error found.
 */
int yatt_validate_shader_programs(YATT_ShaderProgram* shaders, size_t num_shaders);

/*
 * Validates a list of shader uniforms. See above for possible error codes.
 * This function will not return early after finding an error; it will
 * continue to check for others. However, the return code will be that
 * of the first error found.
 */
int yatt_validate_shader_uniforms(YATT_ShaderUniform* uniforms, size_t num_uniforms);

/*
 * Frees a list of shader binaries by calling DVLB_Free on each.
 * Sets all DVLB pointers to NULL.
 * Returns YATT_INPUT_MALFORMED if the array is null.
 */
int yatt_free_shader_binaries(YATT_ShaderBinary* binaries, size_t num_binaries);

/*
 * Frees a list of shader programs by calling shaderProgramFree on each.
 * Returns YATT_INPUT_MALFORMED if the array is null.
 */
int yatt_free_shader_programs(YATT_ShaderProgram* shaders, size_t num_shaders);

/*
 * Initializes shader binaries, parsing their DVLBs from the raw data.
 * If any program fails, all binaries will be freed.
 * 
 * Calls the corresponding validation function to validate inputs.
 */
int yatt_init_shader_binaries(YATT_ShaderBinary* binaries, size_t num_binaries);

/*
 * Initializes shader programs.
 * If any program fails, all programs will be freed.
 * 
 * Calls the corresponding validation function to validate inputs.
 */
int yatt_init_shader_programs(YATT_ShaderProgram* shaders, size_t num_shaders);

/*
 * Parses shader uniform locations from the given programs.
 * 
 * Due to libctru's shader program format, this function must search through
 * the given shaders, making it potentially somewhat slow.
 * 
 * If any uniform cannot be found, it will be given a value of -1. The
 * function will continue for the other uniforms, but it will return
 * YATT_EXTERNAL_FAILURE.
 * 
 * Calls the corresponding validation function to validate inputs.
 */
int yatt_parse_shader_uniforms(YATT_ShaderUniform* uniforms, YATT_ShaderProgram* shaders, size_t num_uniforms, size_t num_shaders);

// Prints shader uniform names and their locations.
void yatt_print_shader_uniforms(YATT_ShaderUniform* uniforms, size_t num_uniforms);

// Enables or disables message printing. The default is false.
void yatt_shaders_enable_printing(bool enable);

// Sets the FILE where message printing will be output.
// The default is NULL, which redirects to stderr.
void yatt_shaders_set_print_output(FILE* output);

#endif
