#include "RakNetGuidGenerator.h"
#include <iostream>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
#include <string>

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--dump") {
        for (int index = 0; index < 1000; ++index) std::cout << RakNet::GuidDetail::Next() << '\n';
        return 0;
    }
    std::mutex mutex;
    std::set<std::uint64_t> ids;
    bool valid = true;
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 8; ++worker) workers.emplace_back([&] {
        for (int index = 0; index < 2000; ++index) {
            const auto id = RakNet::GuidDetail::Next();
            std::lock_guard<std::mutex> lock(mutex);
            valid = (id != 0 && id != UINT64_MAX && ids.insert(id).second) && valid;
        }
    });
    for (auto& worker : workers) worker.join();
    if (!valid || ids.size() != 16000) { std::cerr << "peer_guid_test=failed\n"; return 1; }
    std::cout << "peer_guid_test=passed threads=8 ids=" << ids.size() << '\n';
    return 0;
}
