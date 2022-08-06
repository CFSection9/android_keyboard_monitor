#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <time.h>
#include <string>
#include <fstream>
#include <vector>
#include <json/json.h>

//#define DEBUG_INPUT

namespace inputsubsystem {

  struct key_info {
    std::string name;
    struct timeval time;
  };

  class ConfigParser
  {
  public:
        /**
         * @brief Private Constructor function for singleton mode
         *
         * @param[in] void
         * @return void
         */
        ConfigParser();

        /**
         * @brief Private Distructor function for singleton mode
         *
         * @param[in] void
         * @return void
         */
        ~ConfigParser();

        /**
         * @brief Parse HarmanAudioRouter XML configuration file
         *
         * @param[in] xmlFileName XML file name
         * @return zero as success, others as failed
         */
        bool ParseConfig(const char * xmlFileName);

        /**
         * @brief Get Combination Keys
         *
         * @param[in] rootNode Pointer to XML  root node
         * @return zero as success, others as failed
         */
        std::string CheckCombinationKeys(key_info key);

  private:
        /**
         * @brief Check if inputed key is full matched with config file define
         *
         * @param[in] void
         * @return zero as success, others as failed
         */
        std::string CheckFullMatched(void);

        Json::Reader reader;
        Json::Value root;
        std::vector<key_info> keys_vectors;
  };
} // namespace inputsubsystem
#endif