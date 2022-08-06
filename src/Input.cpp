#define LOG_TAG "bp_input"

#define WAITING_SERVER_SECONDS 5
#define CMD_REREAD_CONFIG "cmd_config_file_updated\n"

#include <utils/Log.h>

#include "Input.hpp"

namespace inputsubsystem {

#define DEVICE_PATH "/dev/input"
int                 Input::nfds         = 0;
struct pollfd*      Input::ufds         = nullptr;
char**              Input::device_names = nullptr;

EasySocket         Input::socketManager;
const std::string* Input::channel_name = nullptr;

ConfigParser Input::configParser;
const char*  Input::config_file_path = nullptr;

Input::Input(const char *configPath, const std::string &channelName,
                const std::string &ip, std::uint16_t port)
{
    ALOGD("constructor");

    // Try to connect socket server
    channel_name = &channelName;
    bool scanning = true;
    while (scanning) {
        if (SOCKET_OK == socketManager.socketConnect(*channel_name, ip, port)) {
            ALOGD("Socket connect to server successful");
            scanning = false;
        } else {
            ALOGD("Socket connect failed, waiting %dsec and trying again", WAITING_SERVER_SECONDS);
            sleep(WAITING_SERVER_SECONDS);
        }
    }

    recvThread = new std::thread(RecvFunction);
    recvThread->detach();

    if(!InitNotify()) {
        ALOGE("Init Notify failed!");
    }

    config_file_path = configPath;
    if(!configParser.ParseConfig(config_file_path)) {
        ALOGE("Parse Config failed!");
    }
}

Input::~Input()
{
    ALOGD("disstructor");

    socketManager.closeConnection(*channel_name);

    free(ufds);
    ufds = nullptr;

    free(device_names);
    device_names = nullptr;

    delete recvThread;
}

Input& Input::GetInstance(const char *configPath, const std::string &channelName,
                            const std::string &ip, std::uint16_t port)
{
    static Input instance(configPath, channelName, ip, port);
    return instance;
}

void Input::RecvFunction()
{
    int ret = 0;
    char msg[256] = {'\0'};
    while (1) {
        ret = socketManager.socketRecv(*channel_name, msg, 255);
        if (SOCKET_OK != ret) {
            ALOGE("socket recv failed in %d", ret);
        }
        if (!strcmp(msg, CMD_REREAD_CONFIG)) {
            if(!configParser.ParseConfig(config_file_path)) {
                ALOGE("Parse Config failed!");
            }
            ALOGI("Reloaded config file!");
        } else {
#if DEBUG_INPUT
            for (int i=0; i<256; i++)
                ALOGE("recv msg: %d", msg[i]);
#endif
            ALOGE("recv msg: %s", msg);
        }
    }
}

bool Input::InitNotify()
{
    int res;

    nfds = 1;
    ufds = (struct pollfd *)calloc(1, sizeof(ufds[0]));
    ufds[0].fd = inotify_init();
    ufds[0].events = POLLIN;

	res = inotify_add_watch(ufds[0].fd, DEVICE_PATH, IN_DELETE | IN_CREATE);
    if(res < 0) {
        ALOGE("could not add watch for %s, %s\n", DEVICE_PATH, std::strerror(errno));
        return false;
    }
    res = scan_dir(DEVICE_PATH);
    if(res < 0) {
        ALOGE("scan dir failed for %s\n", DEVICE_PATH);
        return false;
    }

    return true;
}

void Input::PollingInputEvent()
{
    int res;
    struct input_event event;

    while(1) {
        //int pollres =
        poll(ufds, nfds, -1);
        if(ufds[0].revents & POLLIN) {
            read_notify(DEVICE_PATH, ufds[0].fd);
        }
        for(int i = 1; i < nfds; i++) {
            if(ufds[i].revents) {
                if(ufds[i].revents & POLLIN) {
                    res = read(ufds[i].fd, &event, sizeof(event));
                    if(res < (int)sizeof(event)) {
                        ALOGE("could not get event\n");
                        return;
                    }
                    //ALOGD("[%8ld.%06ld] ", event.time.tv_sec, event.time.tv_usec);
                    //ALOGD("%s: ", device_names[i]);
                    //print_event(event.type, event.code, event.value);
                    bool is_key = (EV_KEY == event.type);
                    bool is_down = (1 == event.value);
                    if (is_key && is_down) {
                        key_info key;
                        key.name = get_label(key_labels, event.code);
                        key.time = event.time;
                        std::string combination_key_name = configParser.CheckCombinationKeys(key);
                        if(!combination_key_name.empty()) {
                            ALOGD("send combine_key=%s to server", combination_key_name.c_str());
                            combination_key_name += '\r';   //add '\r' for java readLine()
                            res = socketManager.socketSend(*channel_name, combination_key_name);
                            if (SOCKET_OK != res) {
                                ALOGE("socket send failed(%d) for %s", res, combination_key_name.c_str());
                            }
                        }
                    }
                }
            }
        }
    }
}

void Input::print_event(int type, int code, int value)
{
    const char *type_label, *code_label, *value_label;

    type_label = get_label(ev_labels, type);
    code_label = get_label(key_labels, code);
    value_label = get_label(key_value_labels, value);

    if (type_label && code_label && value_label) {
        ALOGI("%-12.12s %-20.20s %-20.20s", type_label, code_label, value_label);
    }
/*
    if (type_label) {
        ALOGI("%-12.12s", type_label);
    } else {
        printf("%04x        ", type);
    }
    if (code_label) {
        ALOGI(" %-20.20s", code_label);
    } else {
        ALOGI(" %04x                ", code);
    }
    if (value_label) {
        ALOGI(" %-20.20s", value_label);
    } else {
        ALOGI(" %08x            ", value);
    }
*/
}

int Input::read_notify(const char *dirname, int nfd)
{
    int res;
    char devname[PATH_MAX];
    char *filename;
    char event_buf[512];
    int event_size;
    int event_pos = 0;
    struct inotify_event *event;

    res = read(nfd, event_buf, sizeof(event_buf));
    if(res < (int)sizeof(*event)) {
        if(errno == EINTR)
            return 0;
        ALOGE("could not get event, %s\n", std::strerror(errno));
        return 1;
    }
    //printf("got %d bytes of event information\n", res);

    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';

    while(res >= (int)sizeof(*event)) {
        event = (struct inotify_event *)(event_buf + event_pos);
        //printf("%d: %08x \"%s\"\n", event->wd, event->mask, event->len ? event->name : "");
        if(event->len) {
            strcpy(filename, event->name);
            if(event->mask & IN_CREATE) {
                open_device(devname);
            }
            else {
                close_device(devname);
            }
        }
        event_size = sizeof(*event) + event->len;
        res -= event_size;
        event_pos += event_size;
    }
    return 0;
}

int Input::open_device(const char *device)
{
    int version;
    int fd;
    int clkid = CLOCK_MONOTONIC;
    struct pollfd *new_ufds;
    char **new_device_names;
    char name[80];
    char location[80];
    char idstr[80];
    struct input_id id;

    fd = open(device, O_RDONLY | O_CLOEXEC);
    if(fd < 0) {
        ALOGE("could not open %s, %s\n", device, std::strerror(errno));
        return -1;
    }

    if(ioctl(fd, EVIOCGVERSION, &version)) {
        ALOGE("could not get driver version for %s, %s\n", device, std::strerror(errno));
        return -1;
    }

    if(ioctl(fd, EVIOCGID, &id)) {
        ALOGE("could not get driver id for %s, %s\n", device, std::strerror(errno));
        return -1;
    }
    name[sizeof(name) - 1] = '\0';
    location[sizeof(location) - 1] = '\0';
    idstr[sizeof(idstr) - 1] = '\0';
    if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
        ALOGE("could not get device name for %s, %s\n", device, std::strerror(errno));
        name[0] = '\0';
    }
    if(ioctl(fd, EVIOCGPHYS(sizeof(location) - 1), &location) < 1) {
        ALOGE("could not get location for %s, %s\n", device, std::strerror(errno));
        location[0] = '\0';
    }
    if(ioctl(fd, EVIOCGUNIQ(sizeof(idstr) - 1), &idstr) < 1) {
        ALOGE("could not get idstring for %s, %s\n", device, std::strerror(errno));
        idstr[0] = '\0';
    }

    if (ioctl(fd, EVIOCSCLOCKID, &clkid) != 0) {
        ALOGE("Can't enable monotonic clock reporting: %s\n", std::strerror(errno));
        // a non-fatal error
    }

    new_ufds = (struct pollfd *)realloc(ufds, sizeof(ufds[0]) * (nfds + 1));
    if(new_ufds == nullptr) {
        ALOGE("out of memory\n");
        return -1;
    }
    ufds = new_ufds;
    new_device_names = (char **)realloc(device_names, sizeof(device_names[0]) * (nfds + 1));
    if(new_device_names == nullptr) {
        ALOGE("out of memory\n");
        return -1;
    }
    device_names = new_device_names;

    ALOGD("add device %d: %s\n", nfds, device);
    ALOGD("bus=%04x, vendor=%04x, product=%04x, version=%04x\n",
               id.bustype, id.vendor, id.product, id.version);
    ALOGD("name=%s\n", name);
    ALOGD("location=%s,id=%s\n", location, idstr);
    ALOGD("version=%d.%d.%d\n",
               version >> 16, (version >> 8) & 0xff, version & 0xff);

    ufds[nfds].fd = fd;
    ufds[nfds].events = POLLIN;
    device_names[nfds] = strdup(device);
    nfds++;

    return 0;
}

int Input::close_device(const char *device)
{
    int i;
    for(i = 1; i < nfds; i++) {
        if(strcmp(device_names[i], device) == 0) {
            int count = nfds - i - 1;
            ALOGD("remove device %d: %s\n", i, device);
            free(device_names[i]);
            memmove(device_names + i, device_names + i + 1, sizeof(device_names[0]) * count);
            memmove(ufds + i, ufds + i + 1, sizeof(ufds[0]) * count);
            nfds--;
            return 0;
        }
    }

    return -1;
}

const char *Input::get_label(const struct label *labels, int value)
{
    while(labels->name && value != labels->value) {
        labels++;
    }
    return labels->name;
}

int Input::scan_dir(const char *dirname)
{
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if(dir == nullptr)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        open_device(devname);
    }
    closedir(dir);
    return 0;
}

}
