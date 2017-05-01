

#define CONVOLUTION	"xhgerd"
#define HISTORY		"nxcbhe"
#define SOURCE		"onrwrx"
#define THRESHOLD	"wnrvdf"
#define VOLUME		"ibtxdy"

static const char *fshader = 
  "#version 150\n"
  "uniform sampler1D "CONVOLUTION";\n"
  "uniform sampler1D "HISTORY"[8];\n"
  "uniform sampler1D "SOURCE"[8];\n"
  "uniform float f, "THRESHOLD";\n"
  "uniform int cm, cs, dm, tm;\n"

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
   GLuint fshader;
   int i, max;

   fshader = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(fshader, 1, &fragment_shader, 0);
   glCompileShader(fshader);

#ifndef NDEBUG
   shader_error_check(fshader, "fragment shader", glGetShaderInfoLog,
                      glGetShaderiv, GL_COMPILE_STATUS);
#endif

   handle->shader = glCreateProgram();
   glAttachShader(handle->shader, fshader);
   glDeleteShader(fshader);

   glLinkProgram(handle->shader);

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

_aaxOpenGLShaderDestroy(_aaxRingBufferConvolutionData *shader)
{
   glDeleteProgram(handle->shader);
   glDeleteBuffers(1, &handle->cptr);
   glDeleteBuffers(_AAX_MAX_SPEAKERS, &handle->hptr);
   glDeleteBuffers(_AAX_MAX_SPEAKERS, &handle->sptr);
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

   // wait for the shader to finish
   glMemoryBarrier(GL_ALL_BARRIER_BITS); // GL_SHADER_STORAGE_BARRIER_BIT


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

