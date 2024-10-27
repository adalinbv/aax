import aeonwave

aax = aeonwave.AeonWave(aeonwave.aaxRenderMode.AAX_MODE_WRITE_STEREO);
aax.set(aeonwave.aaxState.AAX_INITIALIZED)
aax.set(AAX_PLAYING)
buffer = aax.get("test-sine.aaxs");

emitter = aeonwave.Emitter();
emitter.add(buffer);
emitter.play();
