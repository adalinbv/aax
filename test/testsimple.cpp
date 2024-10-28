
#include <chrono>
#include <thread>

#include <aax/simple_aeonwave>

int main()
{
    aeonwave::SimpleMixer mixer;

    auto& source = mixer.source("sine", SRC_PATH"/test-sine.aaxs");
    source.set_volume(0.7f);
    source.set_pitch(2.0f);
    source.play();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for(int i=0; i<100; ++i) {
        source.set_pitch(2.0f - 0.01f*i);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    source.stop();
};
