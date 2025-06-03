#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2020-2022, Collabora, Ltd. and the Monado contributors
# SPDX-License-Identifier: CC0-1.0

tmpdir=$(mktemp -d)
trap "rm -rf $tmpdir" EXIT
echo "Creating python3 venv in a temporary directory"
python3 -m venv "${tmpdir}/venv"
. "${tmpdir}/venv"/bin/activate

echo "Installing glad2 from git"
python3 -m pip install git+https://github.com/Dav1dde/glad.git@v2.0.8

# command line (for the glad2 branch!)

# Please keep extensions in sorter order.

echo "GLAD2 generation"

glad --merge \
	--api='gl:compatibility=4.6,gles2=3.2,egl=1.5,glx=1.4,wgl=1.0' \
	--extensions=\
EGL_IMG_context_priority,\
EGL_KHR_create_context,\
EGL_KHR_fence_sync,\
EGL_KHR_gl_colorspace,\
EGL_KHR_image,\
EGL_KHR_image_base,\
EGL_KHR_no_config_context,\
EGL_KHR_platform_android,\
GLX_ARB_create_context,\
GLX_EXT_swap_control,\
GLX_NV_delay_before_swap,\
GL_ARB_buffer_storage,\
GL_ARB_texture_storage_multisample,\
GL_ARB_timer_query,\
GL_EXT_buffer_storage,\
GL_EXT_disjoint_timer_query,\
GL_EXT_framebuffer_multisample,\
GL_EXT_multisampled_render_to_texture,\
GL_EXT_sRGB,\
GL_EXT_texture_border_clamp,\
GL_EXT_texture_compression_latc,\
GL_EXT_texture_compression_s3tc,\
GL_EXT_texture_cube_map_array,\
GL_EXT_texture_filter_anisotropic,\
GL_EXT_texture_sRGB,\
GL_EXT_texture_sRGB_R8,\
GL_EXT_texture_sRGB_RG8,\
GL_EXT_timer_query,\
GL_KHR_texture_compression_astc_hdr,\
GL_NV_depth_buffer_float,\
GL_OES_EGL_image,\
GL_OES_EGL_image_external,\
GL_OES_EGL_image_external_essl3,\
GL_OES_compressed_ETC1_RGB8_texture,\
GL_OES_depth_texture,\
GL_OES_packed_depth_stencil,\
GL_OES_rgb8_rgba8,\
GL_OES_texture_border_clamp,\
GL_OVR_multiview,\
GL_OVR_multiview2,\
GL_OVR_multiview_multisampled_render_to_texture,\
WGL_ARB_create_context,\
WGL_ARB_create_context_profile,\
WGL_ARB_multisample,\
WGL_ARB_pixel_format,\
WGL_EXT_swap_control,\
WGL_NV_delay_before_swap,\
	--out-path . \
	--reproducible \
	c \
	--loader
