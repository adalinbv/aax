
#include <cstdio>
#include <cstring>
#include <aax/aeonwave>
#include <aax/matrix>

namespace aax = aeonwave;

#define SPOSX	-1000.0
#define SPOSY	 2200.0
#define SPOSZ	15200.0

#define SVOLX	   12.0f
#define SVOLY	   -5.0f
#define SVOLZ	    0.5f

#define EPOSX	 -900.0
#define EPOSY	 2250.0
#define EPOSZ	15175.0

#define EVELX	   12.0f
#define EVELY      -5.0f
#define EVELZ       0.5f

// Note:
// Absolue means absolute values for position and velocity and direction
// Relative means relative values for position and velocity and direction
//                (relative to the sensors values)
#define MODE	AAX_ABSOLUTE

const char *sine = "<?xml version='1.0'?>	\
 <aeonwave>					\
  <sound frequency='440' duration='0.1'>	\
   <waveform src='sine'/>			\
  </sound>					\
 </aeonwave>";

int main()
{
   aax::Vector at, up, velocity;
   aax::Matrix64 matrix;
   aax::Vector64 pos;
   aaxVec3f at3f, up3f;
   aaxVec3d pos3d;

   // sensor state
   aax::AeonWave aax(AAX_MODE_WRITE_STEREO);
   aax.set(AAX_INITIALIZED);
   aax.set(AAX_PLAYING);
   aax.set(AAX_SUSPENDED);

   // emitter state
   aax::Buffer buf(aax, 1600, 1, AAX_AAXS16S);
   buf.set(AAX_FREQUENCY, 16000);
   buf.fill(sine);

   aax::Emitter emitter(MODE);
   emitter.set(AAX_INITIALIZED);
   emitter.add(buf);
   emitter.set(AAX_PLAYING);
   aax.add(emitter);

   // sensor position
   pos.set(SPOSX, SPOSY, SPOSZ);
   at.set(0.2f, 0.6f, -0.6f);
   up.set(0.0f, 1.0f, 0.0f);
   matrix.set(pos, at, up);
   matrix.inverse();
   aax.sensor_matrix(matrix);

   velocity.set(SVOLX, SVOLY, SVOLZ);
   aax.sensor_velocity(velocity);
   
   // emitter position
   pos.set(EPOSX, EPOSY, EPOSZ);
   at.set(0.0f, 0.0f, 1.0f);
   matrix.set(pos, at, up);
   emitter.matrix(matrix);

   velocity.set(EVELX, EVELY, EVELZ);
   emitter.velocity(velocity);

   // update
   aax.set(AAX_UPDATE);

   // test
   emitter.get(matrix);
   matrix.get(pos3d, at3f, up3f);
   printf("pos: %f %f %f\n", pos3d[0], pos3d[1], pos3d[2]);


   return 0;
}
