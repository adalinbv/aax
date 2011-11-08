ADD_SUBDIRECTORY(software alsasoft alsa oss dmedia)

ADD_LIBRARY(AEONWAVE_INT
    aax_audioframe.c
    aax_buffer.c
    aax_driver.c
    aax_effects.c
    aax_emitter.c
    aax_filters.c
    aax_instrument.c
    aax_matrix.c
    aax_mixer.c
    aax_scenery.c
    aax_sensor.c
    aax_support.c
    aax_waveforms.c
    devices.c
    objects.c
   )
