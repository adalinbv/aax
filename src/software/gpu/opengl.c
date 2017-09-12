/*
  *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>


#define CONVOLUTION	"xhgerd"
#define HISTORY		"nxcbhe"
#define SOURCE		"onrwrx"
#define THRESHOLD	"wnrvdf"
#define VOLUME		"ibtxdy"

static const char *cshader = 
  "#version 310 es\n"
  "layout (local_size_x=1,local_size_y=1,local_size_z=1) in;\n"
  "uniform sampler1D "HISTORY"[8];\n"
  "uniform sampler1D "SOURCE"[8];\n"
  "uniform float f, "THRESHOLD";\n"
  "uniform int cm, cs, dm, tm;\n"
  "layout (local_size_x=1,local_size_y=1,local_size_z=1) in;\n"
  "void main() {\n"
    "for(int c=0; c<cm; c+=s) {\n"
      "float "VOLUME" = "CONVOLUTION"[c]*f;\n"
      "if (abs("VOLUME") > "THRESHOLD") {\n"
        "for(int t=0; t<tm; t++) {\n"
          "for(int d=0; d<dm; d++) {\n"
            HISTORY"[t][c+d] += "SOURCE"[t][d]*"VOLUME";\n"
          "}\n"
        "}\n"
      "}\n"
    "}\n"
  "}\n";

#ifndef NDEBUG
static void shader_error_check(GLuint object, const char *kind, GetLogFunc getLog, GetParamFunc getParam, GLenum param)
{
   GLchar log[BUFSIZE];
   GLsizei length;
   getLog(object, BUFSIZE, &length, log);
   if (length)
      fprintf(stderr, "%s log:\n%s", kind, log);

   GLint status;
   getParam(object, param, &status);
   if (status == GL_FALSE)
      exit(1);
}
#endif

GLuint
_aaxOpenGLShaderCreate(_aaxRingBufferConvolutionData *handle)
{
   GLuint cshader;
   int i, max;

#if __LINUX__
   /* setup EGL from the GBM device */
   handle->fd = open ("/dev/dri/renderD128", O_RDWR);
   if (handle->!fd) return _aaxOpenGLShaderDestroy(handle);

   handle->gbm = gbm_create_device(fd);
   if (handle->!gbm) return _aaxOpenGLShaderDestroy(handle);

   handle->display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, gbm, NULL);
   if (!handle->display) return _aaxOpenGLShaderDestroy(handle);

#elif __WIN32__
   handle->hwnd = createWindow(winWidth, winHeight);
   if (!handle->hwnd) return _aaxOpenGLShaderDestroy(handle);

   handle->hdc = GetDC(handle->hwnd);
   if (!handle->hdc) return _aaxOpenGLShaderDestroy(handle);

   handle->display = eglGetDisplay(hdc);
   if (!handle->display) return _aaxOpenGLShaderDestroy(handle);
#endif

   res = glInitialize(handle->display, NULL, NULL);
   if (!res) return _aaxOpenGLShaderDestroy(handle);

   const char *ext = eglQueryString (handle->display, EGL_EXTENSIONS);
   if (!strstr(ext, "EGL_KHR_create_context") ||
       !strstr(ext, "EGL_KHR_surfaceless_context")) return _aaxOpenGLShaderDestroy(handle);


   static const EGLint config_attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE
   };
   EGLConfig cfg;
   EGLint count;

   res = eglChooseConfig (handle->display, config_attribs, &cfg, 1, &count);
   if (!res) return _aaxOpenGLShaderDestroy(handle);

   res = eglBindAPI (EGL_OPENGL_ES_API);
   if (!res) return _aaxOpenGLShaderDestroy(handle);

    static const EGLint attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 3,
      EGL_NONE
   };
   EGLContext handle->context = eglCreateContext (handle->display, cg,
                                           EGL_NO_CONTEXT, attribs);
   if (!handle->context) return _aaxOpenGLShaderDestroy(handle);

   res = eglMakeCurrent (handle->display, EGL_NO_SURFACE, EGL_NO_SURFACE, handle->context);
   if (!res) return _aaxOpenGLShaderDestroy(handle);

   /* creeate the compute shader */
   cshader = glCreateShader(GL_COMPUTE_SHADER);
   glShaderSource(cshader, 1, &fragment_shader, 0);
   glCompileShader(cshader);

#ifndef NDEBUG
   shader_error_check(cshader, "fragment shader", glGetShaderInfoLog,
                      glGetShaderiv, GL_COMPILE_STATUS);
#endif

   handle->shader = glCreateProgram();
   glAttachShader(handle->shader, cshader);
   glLinkProgram(handle->shader);
   glDeleteShader(cshader);

#ifndef NDEBUG
   shader_error_check(handle->shader, "program", glGetProgramInfoLog,
                      glGetProgramiv, GL_LINK_STATUS);
#endif

   /* step */
   glUniform1i(glGetUniformLocation(handle->shader, "cs"), convolution->step);
   /* cnum */
   glUniform1i(glGetUniformLocation(handle->shader, "cm"), convolution->no_samples);
   /* threshold */
   glUniform1f(glGetUniformLocation(handle->shader, THRESHOLD), convolution->threshold);


   /* convolution sample */
   glGenBuffers(1, &handle->cptr);
   glBindBuffer(GL_ARRAY_BUFFER, handle->cptr);
   glBufferData(GL_ARRAY_BUFFER, handle->no_samples, handle->sample, GL_STATIC_DRAW);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glUniform1i(glGetUniformLocation(shader, CONVOLUTION), handle->cptr);
   
   /* history buffers */
   max = handl->history_samples;
   glGenBuffers(_AAX_MAX_SPEAKERS, &handle->hptr);
   for(i=0; i<_AAX_MAX_SPEAKERS; ++i)
   {
      glBindBuffer(GL_ARRAY_BUFFER, handle->hptr[i]);
      lClearBufferData(
      glBufferData(GL_ARRAY_BUFFER, max, handle->history[i], GL_DYNAMIC_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
   }
   glUniform1iv(glGetUniformLocation(shader, HISTORY), _AAX_MAX_SPEAKERS, handle->hptr);

   /* source tracks */
   max = rb->get_parami(rb, RB_NO_SAMPLES);
   glUniform1i(glGetUniformLocation(shader, "tm"), max);
   glGenBuffers(_AAX_MAX_SPEAKERS, &handle->sptr);
   glUniform1iv(glGetUniformLocation(shader, SOURCE), _AAX_MAX_SPEAKERS, handle->sptr);
   glUniform1i(glGetUniformLocation(shader, "dm"),rb->get_parami(rb, RB_NO_SAMPLES));
}

int
_aaxOpenGLShaderDestroy(_aaxRingBufferConvolutionData *handle)
{
   glDeleteProgram(handle->shader);
   glDeleteBuffers(1, &handle->cptr);
   glDeleteBuffers(_AAX_MAX_SPEAKERS, &handle->hptr);
   glDeleteBuffers(_AAX_MAX_SPEAKERS, &handle->sptr);

   eglDestroyContext(handle->display, handle->context);
   eglTerminate(handle->display);
#if __LINUX__
   gbm_device_destroy(handle->gbm);
   close(handle->fd);
#endif
}

int
_aaxOpenGLShaderRun(_aaxRingBufferConvolutionData *shader)
{
   GLuint shader = handle->shader;
   int i, max;

   /* copy the data to the shader */
   glUniform1f(glGetUniformLocation(shader, "f"),
               convolution->rms * convolution->delay_gain);

   /* copy the source buffer */
   max = rb->get_parami(rb, RB_NO_TRACKS);
   for(i=0; i<max; ++i)
   {
      glBindBuffer(GL_ARRAY_BUFFER, handle->sptr[i]);
      glBufferData(GL_ARRAY_BUFFER, max, rbd->track[i], GL_STREAM_DRAW);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
   }


   // run the shader
   glUseProgram(handle->shader);
   glDispatchCompute(1, 1, 1);


   /* copy back the updates history buffer */
   max = handl->history_samples;
   for(i=0; i<_AAX_MAX_SPEAKERS; ++i)
   {
      glBindBuffer(GL_ARRAY_BUFFER, handle->hptr[i]);
      glGetBufferSubData(GL_ARRAY_BUFFER, 0, max, handle->history[i]);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
   }
   
   /* proceed wiht the frequency filter */
   if (convolution->freq_filter)
   {
      _aaxRingBufferFreqFilterData *flt = convolution->freq_filter;

      if (convolution->fc > 15000.0f) {
         rbd->multiply(dptr, sizeof(MIX_T), dnum, flt->low_gain);
      }
      else {
         _aaxRingBufferFilterFrequency(rbd, dptr, sptr, 0, dnum, 0, t,
                                       convolution->freq_filter, NULL, 0);
      }
   }
   rbd->add(dptr, hptr+hpos, dnum, 1.0f, 0.0f);

   hpos += dnum;
// if ((hpos + cnum) > convolution->history_samples)
   {
      memmove(hptr, hptr+hpos, cnum*sizeof(MIX_T));
      hpos = 0;
   }
   memset(hptr+hpos+cnum, 0, dnum*sizeof(MIX_T));
   convolution->history_start[t] = hpos;
}

