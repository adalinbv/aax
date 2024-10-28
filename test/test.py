import aeonwave
import time

mixer = aeonwave.SimpleMixer()

source = mixer.source("sine", "test-sine.aaxs")

source.play(2.0)
time.sleep(1)
source.stop()
