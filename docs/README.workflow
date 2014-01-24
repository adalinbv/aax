ALSA/WASAPI/Software Thread

 _aaxSoftwareMixerTreadUpdate
 | _aaxSensorCapture							(READ)
 |
 | _aaxSoftwareMixerSignalFrames					(WRITE)
 | _aaxAudioFrameProcess
 | _aaxEmittersProcess
 | + [0 .. no_emitters]
 | + | be->prepare3d						(stage == 2)
 | + | be->mix3d 
 | + | + rb->mix
 | + | + | [0 .. no_tracks]
 | + | + | + rb->codec
 | + | + | + rb->resample
 | + | + | + rb->effects
 | + | + | rb->mix1n
 | + |   
 | + | be->mix2d						(stage == 1)
 | + | + rb->mix
 | + | + | [0 .. no_tracks]
 | + | + | + rb->codec
 | + | + | + rb->resample
 | + | + | + rb->effects
 | + | + | rb->mixmn
 | + 
 | + [0 .. no_frames]
 | + | _aaxAudioFrameRender
 | +
 | + _aaxSensorsProcess
 | _aaxSoftwareMixerPlay
 | + (threaded frames?)
 | + | _aaxSoftwareMixerMixFrames
 | + | be->effects
 | + | be->postprocess
 | + be->play
