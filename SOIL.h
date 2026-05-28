/* SOIL.h - Custom header-only SOIL implementation using stb_image.h
   Designed for seamless modern Visual Studio compiling without linking old legacy .lib binaries.
   This file implements the essential SOIL API functions for texture loading. */

#ifndef HEADER_SOIL_STB_WRAPPER
#define HEADER_SOIL_STB_WRAPPER

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>

/* SOIL API Compatibility Constants */
#define SOIL_LOAD_AUTO 0
#define SOIL_LOAD_L 1
#define SOIL_LOAD_LA 2
#define SOIL_LOAD_RGB 3
#define SOIL_LOAD_RGBA 4

#define SOIL_CREATE_NEW_ID 0

#define SOIL_FLAG_MIPMAPS 1
#define SOIL_FLAG_INVERT_Y 2
#define SOIL_FLAG_COMPRESS_TO_DXT 4
#define SOIL_FLAG_DIE_ON_ERROR 8
#define SOIL_FLAG_NTSC_SAFE_RGB 16
#define SOIL_FLAG_CoCg_YCoCg 32
#define SOIL_FLAG_TEXTURE_REPEATS 64

/* Loads an image file directly into an OpenGL texture ID.
   Returns the texture ID on success, or 0 on failure. */
static unsigned int SOIL_load_OGL_texture(
    const char *filename,
    int force_channels,
    unsigned int reuse_texture_ID,
    unsigned int flags
) {
    int width = 0;
    int height = 0;
    int channels = 0;
    
    /* OpenGL texture coordinates start at bottom-left, but image files start at top-left.
       So we invert Y if the flag is specified. */
    if (flags & SOIL_FLAG_INVERT_Y) {
        stbi_set_flip_vertically_on_load(1);
    } else {
        stbi_set_flip_vertically_on_load(0);
    }
    
    /* Map SOIL's force_channels to stb_image channels parameter */
    int req_comp = 0;
    if (force_channels >= 1 && force_channels <= 4) {
        req_comp = force_channels;
    }
    
    unsigned char *data = stbi_load(filename, &width, &height, &channels, req_comp);
    if (!data) {
        /* Print warning inside console for troubleshooting */
        printf("SOIL WARNING: Failed to load '%s' (%s)\n", filename, stbi_failure_reason());
        return 0;
    }
    
    /* If force_channels was specified, override our channel count */
    int active_channels = req_comp > 0 ? req_comp : channels;
    
    GLuint textureID = reuse_texture_ID;
    if (textureID == 0) {
        glGenTextures(1, &textureID);
    }
    
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    /* Set wrapping mode */
    GLint wrap_mode = (flags & SOIL_FLAG_TEXTURE_REPEATS) ? GL_REPEAT : GL_CLAMP;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
    
    /* Set filtering mode based on whether mipmaps are requested */
    if (flags & SOIL_FLAG_MIPMAPS) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    /* Detect standard pixel formats */
    GLenum internal_format = GL_RGB;
    GLenum pixel_format = GL_RGB;
    
    if (active_channels == 1) {
        internal_format = GL_LUMINANCE;
        pixel_format = GL_LUMINANCE;
    } else if (active_channels == 2) {
        internal_format = GL_LUMINANCE_ALPHA;
        pixel_format = GL_LUMINANCE_ALPHA;
    } else if (active_channels == 3) {
        internal_format = GL_RGB;
        pixel_format = GL_RGB;
    } else if (active_channels == 4) {
        internal_format = GL_RGBA;
        pixel_format = GL_RGBA;
    }
    
    /* Upload the texture data */
    if (flags & SOIL_FLAG_MIPMAPS) {
        /* Generate mipmaps using gluBuild2DMipmaps */
        gluBuild2DMipmaps(GL_TEXTURE_2D, internal_format, width, height, pixel_format, GL_UNSIGNED_BYTE, data);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, pixel_format, GL_UNSIGNED_BYTE, data);
    }
    
    stbi_image_free(data);
    return textureID;
}

#endif /* HEADER_SOIL_STB_WRAPPER */
