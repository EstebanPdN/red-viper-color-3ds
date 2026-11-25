#include "yatt_shader_utils.h"
#include "yatt_error.h"

#define ARRAY_COUNT(arr_) (size_t) (sizeof(arr_) / sizeof(arr_[0]))

#define SET_RETCODE(code_) do { if (!retcode) retcode = (code_); } while (0) // Sets a retcode, but will not overwrite an existing one
#define PRINT_ERROR(...) do { if (print_messages) fprintf(print_output == NULL ? stderr : print_output, __VA_ARGS__); } while (0) // if print_messages is enabled, prints an error message
#define ERROR(code_, ...) do { SET_RETCODE((code_)); PRINT_ERROR(__VA_ARGS__); } while (0) // Conveneince macro for setting retcode and printing an error

static bool print_messages = false;
static FILE* print_output = NULL; // Defaults to stderr

int yatt_validate_shader_binaries(YATT_ShaderBinary* binaries, size_t num_binaries)
{
    int retcode = YATT_OK;

    if (binaries == NULL) {
        ERROR(YATT_SHBIN_ARRAY_IS_NULL, "Shader binary array cannot be nullptr\n");
        goto ret;
    }

    for (size_t i = 0; i < num_binaries; i++) {
        YATT_ShaderBinary* binary = &binaries[i];

        if (binary->data == NULL)
            ERROR(YATT_SHBIN_DATA_IS_NULL, "Shader binary %d data pointer cannot be null\n", i);

        if (binary->size == NULL)
            ERROR(YATT_SHBIN_SIZE_IS_NULL, "Shader binary %d size pointer cannot be null\n", i);

        if (binary->dvlb != NULL)
            ERROR(YATT_SHBIN_DVLB_IS_NOT_NULL, "Shader binary %d DVLB must be nullpointer before init\n", i);
    }

ret:
    return retcode;
}

int yatt_validate_shader_programs(YATT_ShaderProgram* shaders, size_t num_shaders)
{
    int retcode = YATT_OK;

    if (shaders == NULL) {
        ERROR(YATT_SHPROG_ARRAY_IS_NULL, "Shader array cannot be nullptr\n");
        goto ret;
    }

    // Check for errors
    for (size_t i = 0; i < num_shaders; i++) {
        YATT_ShaderProgram* shader = &shaders[i];

        if (shader->pica_program == NULL) {
            ERROR(YATT_SHPROG_PICA_PROG_IS_NULL, "Shader %d PICA200 shader program cannot be a nullptr.\n", i);
        } else {
            if (shader->pica_program->vertexShader != NULL || shader->pica_program->geometryShader != NULL) {
                ERROR(YATT_SHPROG_PICA_PROG_ALREADY_INITIALIZED, "Shader %d PICA200 shader program cannot already be initialized.\n", i);
            }
        }

        if (shader->gsh_dvle_index == shader->vsh_dvle_index) {
            ERROR(YATT_SHPROG_GSH_VSH_DVLE_ARE_SAME, "Shader %d cannot have same DVLE index for VSH and GSH\n", i);
        }

        if (shader->vsh_dvle_index < 0) {
            ERROR(YATT_SHPROG_VSH_DVLE_IS_NEGATIVE, "Shader %d VSH DVLE index cannot be negative: %lu\n", i, shader->vsh_dvle_index);
        }

        if (shader->binary == NULL) {
            ERROR(YATT_SHPROG_BINARY_IS_NULL, "Shader %d cannot have null binary.\n", i);
            continue;
        }
        
        if (shader->binary->dvlb == NULL) {
            ERROR(YATT_SHPROG_BINARY_DVLB_IS_NULL, "Shader %d cannot have null binary->dvlb. Did you forget to call yatt_init_shader_binaries?\n", i);
            continue;
        }

        if (shader->vsh_dvle_index > shader->binary->dvlb->numDVLE) {
            ERROR(YATT_SHPROG_VSH_DVLE_OUT_OF_RANGE, "Shader %d VSH DVLE out-of-range for its DVLB: %lu\n", i, shader->vsh_dvle_index);
        }
        
        if (shader->gsh_dvle_index > shader->binary->dvlb->numDVLE) {
            ERROR(YATT_SHPROG_GSH_DVLE_OUT_OF_RANGE, "Shader %d GSH DVLE out-of-range for its DVLB: %lu\n", i, shader->gsh_dvle_index);
        }
    }

ret:
    return retcode;
}

int yatt_validate_shader_uniforms(YATT_ShaderUniform* uniforms, size_t num_uniforms)
{
    int retcode = YATT_OK;

    if (uniforms == NULL) {
        ERROR(YATT_SHUNIF_ARRAY_IS_NULL, "Shader uniform array cannot be nullptr\n");
        goto ret;
    }

    for (size_t i = 0; i < num_uniforms; i++) {
        YATT_ShaderUniform* uniform = &uniforms[i];
        
        if (uniform->loc_pointer == NULL)
            ERROR(YATT_SHUNIF_LOCATION_PTR_IS_NULL, "Shader uniform %d location pointer cannot be null\n", i);
        
        if (uniform->name == NULL)
            ERROR(YATT_SHUNIF_NAME_IS_NULL, "Shader uniform %d name cannot be null\n", i);
    }

ret:
    return retcode;
}

int yatt_free_shader_binaries(YATT_ShaderBinary* binaries, size_t num_binaries)
{
    if (binaries == NULL)
        return YATT_INPUT_MALFORMED;

    for (size_t i = 0; i < num_binaries; i++) {
        YATT_ShaderBinary* binary = &binaries[i];
        DVLB_Free(binary->dvlb); // Handles nullptr check
        binary->dvlb = NULL;
    }

    return YATT_OK;
}

int yatt_free_shader_programs(YATT_ShaderProgram* shaders, size_t num_shaders)
{
    if (shaders == NULL)
        return YATT_INPUT_MALFORMED;

    for (size_t i = 0; i < num_shaders; i++) {
        YATT_ShaderProgram* shader = &shaders[i];
        shaderProgramFree(shader->pica_program);
    }

    return YATT_OK;
}

int yatt_init_shader_binaries(YATT_ShaderBinary* binaries, size_t num_binaries)
{
    if(yatt_validate_shader_binaries(binaries, num_binaries) != YATT_OK)
        return YATT_INPUT_MALFORMED;

    for (size_t i = 0; i < num_binaries; i++) {
        YATT_ShaderBinary* binary = &binaries[i];
        
        binary->dvlb = DVLB_ParseFile((u32*) binary->data, *binary->size);
        
        if (binary->dvlb == NULL) {
            yatt_free_shader_binaries(binaries, num_binaries);
            return YATT_EXTERNAL_FAILURE;
        }
    }

    return YATT_OK;
}

int yatt_init_shader_programs(YATT_ShaderProgram* shaders, size_t num_shaders)
{
    if(yatt_validate_shader_programs(shaders, num_shaders) != YATT_OK)
        return YATT_INPUT_MALFORMED;

    // No errors: we can go ahead and initialize.
    // If we fail to init any part of a shader program, free everything.
    for (int i = 0; i < num_shaders; i++) {
        YATT_ShaderProgram* shader = &shaders[i];
        DVLB_s* dvlb = shader->binary->dvlb;

        // If we failed to init, free all and return
        if (!R_SUCCEEDED(shaderProgramInit(shader->pica_program))) {
            yatt_free_shader_programs(shaders, num_shaders);
            return YATT_EXTERNAL_FAILURE;
        }

        if (!R_SUCCEEDED(shaderProgramSetVsh(shader->pica_program, &dvlb->DVLE[shader->vsh_dvle_index]))) {
            yatt_free_shader_programs(shaders, num_shaders);
            return YATT_EXTERNAL_FAILURE;
        }

        // Geometry shader is optional
        if (shader->gsh_dvle_index >= 0) {
            if (!R_SUCCEEDED(shaderProgramSetGsh(shader->pica_program, &dvlb->DVLE[shader->gsh_dvle_index], shader->gsh_stride))) {
                yatt_free_shader_programs(shaders, num_shaders);
                return YATT_EXTERNAL_FAILURE;
            }
        }
    }

    return YATT_OK;
}

// Searches for a uniform within the given list of shader programs.
static s8 search_for_uniform(YATT_ShaderProgram* shaders, size_t num_shaders, char* name)
{
    // Search each DVLE until we find the uniform we're looking for.
    for (size_t sh = 0; sh < num_shaders; sh++) {
        YATT_ShaderProgram* shader = &shaders[sh];

        DVLB_s* dvlb = shader->binary->dvlb;

        s32 dvle_indices[2] = {shader->vsh_dvle_index, shader->gsh_dvle_index};

        for (size_t j = 0; j < ARRAY_COUNT(dvle_indices); j++) {
            DVLE_s* dvle = &dvlb->DVLE[dvle_indices[j]];

            s8 loc = DVLE_GetUniformRegister(dvle, name);

            if (loc >= 0)
                return loc;
        }
    }

    return -1;
}

int yatt_parse_shader_uniforms(YATT_ShaderUniform* uniforms, YATT_ShaderProgram* shaders, size_t num_uniforms, size_t num_shaders)
{
    if(yatt_validate_shader_uniforms(uniforms, num_uniforms) != YATT_OK) {
        PRINT_ERROR("Could not parse uniforms: Shader uniforms malformed\n");
        return YATT_INPUT_MALFORMED;
    }
        
    if (shaders == NULL) {
        PRINT_ERROR("Could not parse uniforms: Shader array cannot be nullptr\n");
        return YATT_INPUT_MALFORMED;
    }
    
    bool error = false;

    // Validate DVLEs
    for (size_t sh = 0; sh < num_shaders; sh++) {
        YATT_ShaderProgram* shader = &shaders[sh];

        DVLB_s* dvlb = shader->binary->dvlb;
        if (dvlb == NULL) {
            PRINT_ERROR("Could not parse uniforms: Shader %d DVLB is null\n", sh);
            error = true;
            continue;
        }

        if (dvlb->DVLE == NULL) {
            PRINT_ERROR("Could not parse uniforms: Shader %d DVLB->DVLE is null\n", sh);
            error = true;
            continue;
        }
    }

    if (error) return YATT_INPUT_MALFORMED;

    // Failsafe: write -1 to all uniforms. If any aren't found, they will remain -1.
    for (size_t i = 0; i < num_uniforms; i++) {
        int* loc_pointer = uniforms[i].loc_pointer;
        *loc_pointer = -1; // Failsafe: any not found will be -1
    }

    // Parse uniform locations into the struct
    for (size_t i = 0; i < num_uniforms; i++) {
        int* loc_pointer = uniforms[i].loc_pointer;

        s8 found_uloc = search_for_uniform(shaders, num_shaders, uniforms[i].name);

        if (found_uloc >= 0)
            *loc_pointer = found_uloc;
        else {
            error = true;
            PRINT_ERROR("Could not find uniform location of %s\n", uniforms[i].name);
        }
    }

    return error ? YATT_EXTERNAL_FAILURE : YATT_OK;
}

void yatt_print_shader_uniforms(YATT_ShaderUniform* uniforms, size_t num_uniforms)
{
    if (!print_messages)
        return;
        
    for (int i = 0; i < num_uniforms; i++)
        PRINT_ERROR("%s\t%d\n", uniforms[i].name, *uniforms[i].loc_pointer);
}

void yatt_shaders_enable_printing(bool enable)
{
    print_messages = enable;
}

void yatt_shaders_set_print_output(FILE* output)
{
    print_output = output;
}
