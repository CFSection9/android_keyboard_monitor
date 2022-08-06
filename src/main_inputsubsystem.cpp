#include "Input.hpp"

#define CHANNEL_NAME  "bp_input_subsystem"
#define IP_ADDRESS    "127.0.0.1"
#define PORT          5050
#define CONFIG_PATH "/data/input_config.json"

using namespace inputsubsystem;

int main(int argc __unused, char** argv)
{
    // Ignore SIGPIPE signal
    signal(SIGPIPE, SIG_IGN);

    // Get instance
    Input& input = Input::GetInstance(CONFIG_PATH, CHANNEL_NAME, IP_ADDRESS, PORT);

    // Check input
    input.PollingInputEvent();

    return 0;
}