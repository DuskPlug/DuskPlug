#include "../brightness.h"

#include "../platform_util.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

struct BacklightDevice {
    std::string brightnessPath;
    int maxBrightness = 0;
    int capturedBrightness = 0;
};

bool CommandExistsOnPath(const char* command) {
    const char* pathEnv = getenv("PATH");
    if (!pathEnv) {
        return false;
    }

    std::string path = pathEnv;
    size_t start = 0;
    while (start < path.size()) {
        const size_t end = path.find(':', start);
        const std::string dir = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!dir.empty()) {
            const std::string candidate = dir + "/" + command;
            struct stat st {};
            if (stat(candidate.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
                return true;
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

bool RunCommandCaptureStdout(const std::string& command, std::string& output) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return false;
    }

    char buffer[256];
    output.clear();
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    const int status = pclose(pipe);
    return status == 0;
}

int ReadIntFromFile(const std::string& path) {
    FILE* file = fopen(path.c_str(), "r");
    if (!file) {
        return -1;
    }
    int value = -1;
    fscanf(file, "%d", &value);
    fclose(file);
    return value;
}

bool WriteIntToFile(const std::string& path, int value) {
    FILE* file = fopen(path.c_str(), "w");
    if (!file) {
        return false;
    }
    fprintf(file, "%d", value);
    fclose(file);
    return true;
}

void DiscoverBacklights(std::vector<BacklightDevice>& out) {
    DIR* dir = opendir("/sys/class/backlight");
    if (!dir) {
        return;
    }

    dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        const std::string base = std::string("/sys/class/backlight/") + entry->d_name;
        const int maxBrightness = ReadIntFromFile(base + "/max_brightness");
        if (maxBrightness <= 0) {
            continue;
        }

        BacklightDevice device{};
        device.brightnessPath = base + "/brightness";
        device.maxBrightness = maxBrightness;
        out.push_back(device);
    }
    closedir(dir);
}

void DiscoverDdcDisplays(std::vector<int>& out) {
    if (!CommandExistsOnPath("ddcutil")) {
        return;
    }

    std::string output;
    if (!RunCommandCaptureStdout("ddcutil detect --brief 2>/dev/null", output)) {
        return;
    }

    size_t pos = 0;
    while ((pos = output.find("Display ", pos)) != std::string::npos) {
        pos += 8;
        const int displayNumber = std::atoi(output.c_str() + pos);
        if (displayNumber > 0) {
            out.push_back(displayNumber);
        }
        ++pos;
    }
}

int ReadDdcBrightness(int displayNumber) {
    const std::string command =
        "ddcutil getvcp 10 --brief --display=" + std::to_string(displayNumber) + " 2>/dev/null";
    std::string output;
    if (!RunCommandCaptureStdout(command, output)) {
        return -1;
    }

    const size_t equals = output.find('=');
    if (equals == std::string::npos) {
        return -1;
    }
    return std::atoi(output.c_str() + equals + 1);
}

bool SetDdcBrightness(int displayNumber, int percent) {
    const std::string command = "ddcutil setvcp 10 " + std::to_string(percent)
        + " --display=" + std::to_string(displayNumber) + " --noverify --brief >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

class LinuxBrightnessController : public IBrightnessController {
public:
    LinuxBrightnessController() {
        DiscoverBacklights(backlights_);
        DiscoverDdcDisplays(ddcDisplays_);
    }

    bool AnyControllable() const override {
        return !backlights_.empty() || !ddcDisplays_.empty();
    }

    void Capture() override {
        if (captured_) {
            return;
        }

        for (auto& device : backlights_) {
            const int current = ReadIntFromFile(device.brightnessPath);
            device.capturedBrightness = current >= 0 ? current : 0;
        }

        capturedDdcValues_.clear();
        capturedDdcValues_.reserve(ddcDisplays_.size());
        for (const int displayNumber : ddcDisplays_) {
            const int current = ReadDdcBrightness(displayNumber);
            capturedDdcValues_.push_back(current >= 0 ? current : -1);
        }

        captured_ = true;
        lastPercent_ = -1;
    }

    void Restore() override {
        if (!captured_) {
            return;
        }

        for (const auto& device : backlights_) {
            if (device.capturedBrightness >= 0) {
                WriteIntToFile(device.brightnessPath, device.capturedBrightness);
            }
        }

        for (size_t i = 0; i < ddcDisplays_.size() && i < capturedDdcValues_.size(); ++i) {
            if (capturedDdcValues_[i] >= 0) {
                SetDdcBrightness(ddcDisplays_[i], capturedDdcValues_[i]);
            }
        }

        captured_ = false;
        lastPercent_ = -1;
        capturedDdcValues_.clear();
    }

    bool SetPercent(int percent) override {
        if (percent < 0) {
            percent = 0;
        } else if (percent > 100) {
            percent = 100;
        }

        if (lastPercent_ == percent) {
            return AnyControllable();
        }

        bool anySet = false;
        for (const auto& device : backlights_) {
            const int target = (device.maxBrightness * percent + 50) / 100;
            if (WriteIntToFile(device.brightnessPath, target)) {
                anySet = true;
            }
        }

        for (const int displayNumber : ddcDisplays_) {
            if (SetDdcBrightness(displayNumber, percent)) {
                anySet = true;
            }
        }

        if (anySet) {
            lastPercent_ = percent;
        }
        return anySet;
    }

private:
    std::vector<BacklightDevice> backlights_;
    std::vector<int> ddcDisplays_;
    std::vector<int> capturedDdcValues_;
    bool captured_ = false;
    int lastPercent_ = -1;
};

}  // namespace

IBrightnessController* CreateLinuxBrightnessController() {
    return new LinuxBrightnessController();
}
