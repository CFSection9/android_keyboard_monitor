#define LOG_TAG "bp_input"

#include <utils/Log.h>
#include <set>

#include "ConfigParser.hpp"

#define TIMEOUT_MS 300L

namespace inputsubsystem {

ConfigParser::ConfigParser()
{
    keys_vectors.clear();
}

ConfigParser::~ConfigParser()
{
}

bool ConfigParser::ParseConfig(const char * fileName)
{
    std::ifstream config_doc(fileName, std::ifstream::binary);

    // Paser config
    bool parsingSuccessful = reader.parse(config_doc, root);
    if (!parsingSuccessful) {
        ALOGE("%s: Could not parse %s document.",
            __FUNCTION__, fileName);
        return false;
    }

    return true;
}

std::string ConfigParser::CheckCombinationKeys(key_info key)
{
    ALOGD("search string: %s", key.name.c_str());

    // Check the press time with restord combination keys
    if (!keys_vectors.empty()) {
        long diff_ms = ((key.time.tv_sec - keys_vectors.back().time.tv_sec) * 1000 
                      + (key.time.tv_usec - keys_vectors.back().time.tv_usec)/1000.0);
        ALOGD("diff_time=%ld, clear restored keys", diff_ms);
        if (TIMEOUT_MS < diff_ms) {
            keys_vectors.clear();
        }
    }

    // Check if it already restored in vector
    for (const auto &it : keys_vectors) {
        if (it.name == key.name) {
            return std::string();
        }
    }

    if (keys_vectors.empty()) {
        for (int i = 0; i < root["combination_keys"].size(); i++) {
#ifdef DEBUG_INPUT
            ALOGD("name=%s",root["combination_keys"][i]["name"].asString().c_str());
#endif
            for (int j = 0; j < root["combination_keys"][i]["keys"].size(); j++) {
#ifdef DEBUG_INPUT
                ALOGD("keys[%d] = %s", j, root["combination_keys"][i]["keys"][j].asString().c_str());
#endif
                if (root["combination_keys"][i]["keys"][j].asString() == key.name) {
                    keys_vectors.push_back(key);
#ifdef DEBUG_INPUT
                    for (const auto &it : keys_vectors) {
                        ALOGD("keys_vectors keys=%s", it.name.c_str());
                    }
#endif
                    return CheckFullMatched();
                }
            }
        }
    } else {
        std::set<int> search_index;
        // Find the Config file's specail lines which has been restored in vector
        for (int i = 0; i < root["combination_keys"].size(); i++) {
            for ( int j = 0; j < root["combination_keys"][i]["keys"].size(); j++) {
                for (const auto &it : keys_vectors) {
                    if (it.name == root["combination_keys"][i]["keys"][j].asString()) {
                        search_index.emplace(i);
                    }
                }
            }
        }
#ifdef DEBUG_INPUT
        for (const auto &it : search_index) {
            ALOGD("search_index=%d", it);
        }
#endif
        // Only check the Config file's specail lines if it existed in config file.
        for (int i = 0; i < root["combination_keys"].size(); i++) {
            if (search_index.count(i)) {
                for ( int j = 0; j < root["combination_keys"][i]["keys"].size(); j++) {
                    if (key.name == root["combination_keys"][i]["keys"][j].asString()) {
                        keys_vectors.push_back(key);
#ifdef DEBUG_INPUT
                        for (const auto &it : keys_vectors) {
                            ALOGD("keys_vectors keys=%s", it.name.c_str());
                        }
#endif
                        return CheckFullMatched();
                    }
                }
            }
        }
        search_index.clear();
    }
#ifdef DEBUG_INPUT
    ALOGD("%s clean vector",__FUNCTION__);
#endif
    keys_vectors.clear();
    return std::string();
}

std::string ConfigParser::CheckFullMatched(void)
{
    std::string set_string;
    std::string config_string;

    // Calc current restored keys value
    for (const auto &it : keys_vectors) {
            set_string += it.name;
    }
    std::sort(set_string.begin(),set_string.end());
#ifdef DEBUG_INPUT
    ALOGD("set_string=%s", set_string.c_str());
#endif

    // Calc config file's keys value and compare with current resotred keys value
    for (int i = 0; i < root["combination_keys"].size(); i++) {
        for ( int j = 0; j < root["combination_keys"][i]["keys"].size(); j++) {
            config_string += root["combination_keys"][i]["keys"][j].asString();
        }
        std::sort(config_string.begin(),config_string.end());
#ifdef DEBUG_INPUT
        ALOGD("config_string=%s", config_string.c_str());
#endif

        if (set_string == config_string) {
            keys_vectors.clear();
            return root["combination_keys"][i]["name"].asString();
        } else {
            config_string.clear();
        }
    }

    return std::string();
}

} // namespace inputsubsystem