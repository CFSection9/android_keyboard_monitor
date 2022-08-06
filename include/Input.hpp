#ifndef INPUT_HPP
#define INPUT_HPP

#include <stdlib.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <linux/input.h>
#include <unistd.h>
#include <err.h>
#include <cerrno>
#include <cstring>
#include <thread>

#include "EasySocket.hpp"
using namespace masesk;

#include "ConfigParser.hpp"

namespace inputsubsystem {

  struct label {
      const char *name;
      int value;
  };
  #define LABEL(constant) { #constant, constant }
  #define LABEL_END { NULL, -1 }
  static struct label key_value_labels[] = {
          { "UP", 0 },
          { "DOWN", 1 },
          { "REPEAT", 2 },
          LABEL_END,
  };
  #include "input.h-labels.h"
  #undef LABEL
  #undef LABEL_END

  class Input
  {
  public:
      /**
       * @brief get Input Singleton Instance
       *
       * @param[in] void
       * @return Input Instance
       */
      static Input& GetInstance(const char *configPath, const std::string &channelName,
                              const std::string &ip, std::uint16_t port);

      /**
       * @brief Polling input event
       *
       * @param[in] void
       * @return void
       */
      static void PollingInputEvent();

  private:
      /**
       * @brief Private Constructor function for singleton mode
       *
       * @param[in] void
       * @return void
       */
      Input(const char *configPath, const std::string &channelName,
          const std::string &ip, std::uint16_t port);

      /**
       * @brief Private Distructor function for singleton mode
       *
       * @param[in] void
       * @return void
       */
      ~Input();

      /**
       * @brief init notify
       *
       * @param[in] void
       * @return void
       */
      static bool InitNotify();

      /**
       * @brief scan input devices by dirname
       *
       * @param[in] the dir name
       * @return zero as success, others as failed
       */
      static int scan_dir(const char *dirname);

      /**
       * @brief get input event lable from system
       *
       * @param[in] labels
       * @param[in] value
       * @return the label been get
       */
      static const char* get_label(const struct label *labels, int value);

      /**
       * @brief open input event deivce
       *
       * @param[in] device name
       * @return zero as success, others as failed
       */
      static int open_device(const char *device);

      /**
       * @brief close input event deivce
       *
       * @param[in] device name
       * @return zero as success, others as failed
       */
      static int close_device(const char *device);

      /**
       * @brief read_notify from kernel space
       *
       * @param[in] the dir name
       * @param[in] the file descriptor
       * @return zero as success, others as failed
       */
      static int read_notify(const char *dirname, int nfd);

      /**
       * @brief print the event we get from input event
       *
       * @param[in] event type
       * @param[in] event code
       * @param[in] event value
       * @return void
       */
      static void print_event(int type, int code, int value);

      /**
       * @brief recv the socket message form server
       *
       * @param[in] void
       * @return void
       */
      static void RecvFunction(void);

      //input
      static int nfds;                //index of polling file
      static struct pollfd *ufds;     //polling file descriptor
      static char **device_names;

      //socket
      static EasySocket socketManager;
      static const std::string* channel_name;
      std::thread *recvThread;

      //config parser
      static ConfigParser configParser;
      static const char* config_file_path;
  };
}
#endif