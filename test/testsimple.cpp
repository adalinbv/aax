
#include <chrono>
#include <thread>

#include <aax/simple_aeonwave>

int main()
{
    aeonwave::SimpleMixer mixer;

    auto& source = mixer.source("sine", SRC_PATH"/test-sine.aaxs");

    source.play(2.0f);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    source.stop();
};
