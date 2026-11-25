#include <stdio.h>
#include <3ds.h>

#include <yatt_shader_utils.h>
#include <yatt_error.h>

#include "example_shader_shbin.h" // u8 example_shader_shbin_end[], u8 example_shader_shbin[], u32 example_shader_shbin_size;

/**
 * This is an example file based on a real-world use case. It should build,
 * but is not absolutely guaranteed to do so.
 */

#define ARRAY_COUNT(arr_) (size_t) (sizeof(arr_) / sizeof(arr_[0]))

// Must match linking order of shlist file
enum {
   DVLE_BOTH_G = 0,
   DVLE_BOTH_V,
   DVLE_VERTEX_ONLY_V,
};

// Externally visible data
int uLoc_uniform_both_g, uLoc_uniform_both_v, uLoc_uniform_v_only;
shaderProgram_s sAffine, sChar, sFinal;

// DVLB must be initialized to NULL.
YATT_ShaderBinary shader_binary = { example_shader_shbin, &example_shader_shbin_size, NULL };

YATT_ShaderProgram shader_programs[] = {
    { &shader_binary, &sAffine, DVLE_BOTH_G, DVLE_BOTH_V,        3 },
    { &shader_binary, &sChar,            -1, DVLE_VERTEX_ONLY_V, 0 },
};

YATT_ShaderUniform uniforms[] = {
    { "uniform_both_g", &uLoc_uniform_both_g },
    { "uniform_both_v", &uLoc_uniform_both_v },
    { "uniform_v_only", &uLoc_uniform_v_only },
};

void yatt_shaders_init(void)
{
    // Prevent double initialization
    static bool initialized = false;
    
    if (initialized) return;
    initialized = true;

    // We want to see error messages
    yatt_shaders_enable_printing(true);
    yatt_shaders_enable_printing(stderr);
    
    // Actually initialize everything
    if (yatt_init_shader_binaries(&shader_binary, 1) != YATT_OK)
        svcBreak(USERBREAK_PANIC);

    if (yatt_init_shader_programs(shader_programs, ARRAY_COUNT(shader_programs)) != YATT_OK)
        svcBreak(USERBREAK_PANIC);
    
    if (yatt_parse_shader_uniforms(uniforms, shader_programs, ARRAY_COUNT(uniforms), ARRAY_COUNT(shader_programs)) != YATT_OK)
        svcBreak(USERBREAK_PANIC);

    // Printing the uniform locations allows us to debug easily
    yatt_print_shader_uniforms(uniforms, ARRAY_COUNT(uniforms));
}
