#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <unordered_map>
#include <sstream>
#include <ctime>
#include <deque>
#include <random>
#include <stdexcept>
#include <string>
#include <cmath>
#include <stdexcept>
#include <iomanip>

// Function to generate a memory access trace file
void generate_trace_file(int num_accesses, const std::string& filename) {
    std::ofstream f(filename);
    if (!f.is_open()) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> op_dist(0, 1);  // 0 for 'R', 1 for 'W'
    
    // Generate a smaller set of base addresses that we'll reuse (temporal locality)
    const int num_base_addresses = 100;
    std::vector<unsigned int> base_addresses;
    std::uniform_int_distribution<unsigned int> base_addr_dist(0, 0xFFFFFFFF);  
    
    for (int i = 0; i < num_base_addresses; ++i) {
        base_addresses.push_back(base_addr_dist(gen));
    }

    //I was doignnonpattern and completely random file generations but then I kept having 100% misses as non of thr address
    //Where the same so i adopted this method from a combination of online resources and use of AI

    // Distribution for selecting base addresses
    std::uniform_int_distribution<int> base_select(0, num_base_addresses - 1);
    
    // Distribution for offset from base address (spatial locality)
    std::uniform_int_distribution<int> offset_dist(0, 256);  // Small offset for spatial locality
    
    // Distribution for access pattern type
    std::uniform_int_distribution<int> pattern_dist(0, 2);

    for (int i = 0; i < num_accesses; ++i) {
        char operation = (std::rand() % 2) == 0 ? 'R' : 'W';
        unsigned int address;

        int pattern_type = pattern_dist(gen);
        
        switch (pattern_type) {
            case 0: {
                // Sequential access pattern
                static unsigned int curr_seq_addr = base_addr_dist(gen);
                address = curr_seq_addr;
                curr_seq_addr += 64;  // Move to next cache line
                break;
            }
            case 1: {
                // Repeated access pattern (temporal locality)
                address = base_addresses[base_select(gen)];
                break;
            }
            case 2: {
                // Spatial locality pattern
                unsigned int base = base_addresses[base_select(gen)];
                unsigned int offset = offset_dist(gen);
                address = base + offset;
                break;
            }
        }

        f << operation << " 0x" << std::hex << address << std::endl;
    }

    f << "#eof";        //End of memory_trace file
    f.close();

    std::cout << "Trace file '" << filename << "' generated with " << num_accesses << " accesses." << std::endl;
}

// Cache Simulator Class
class CacheSimulator {
private:
    size_t cache_size;
    size_t block_size;
    size_t num_blocks;
    size_t prefetch_size;
    size_t num_ways;
    size_t num_sets;
    std::string associativity;
    std::unordered_map<int, std::deque<int>> cache;
    size_t hits = 0;
    size_t misses = 0;

    // Fucniton to Compute index of cache set
    size_t get_set_index(size_t block_address) {
        if(num_sets > 1)
        {
            return block_address % num_sets;
        }
        else
        {
            return 0;
        }
    }

    // Function to access a block in the cache
    void access_block(size_t block_address) {
        size_t set_index = get_set_index(block_address);
        if(cache.find(set_index) == cache.end())
        {
            cache[set_index] = std::deque<int>();
        }

        auto& cache_set = cache[set_index];
        auto it = std::find(cache_set.begin(), cache_set.end(), block_address);
        if (it != cache_set.end()) {
            // Cache hit
            hits +=1;
        } else {
            // Cache miss
            misses+=1;
            cache_set.push_back(block_address);

            //FIFO logic to remove the first in and make room in the back
            if(cache_set.size() > num_ways)
            {
                cache_set.pop_front();
            }
        }
    }

    // function for Prefetch blocks
    //When a block misses ir will predict the next prefetch_size sequntial blocks
    //Will load them into the cache
    void prefetch_blocks(size_t missed_block_address) {
        for (size_t i = 1; i <= prefetch_size; ++i) {
            size_t next_block = missed_block_address + i;       //Calculate the address of the nect block to prefect
            size_t set_index = get_set_index(next_block);       //Determine the set index
            
            //If set doesnt exist initalize an empty dequeue
            if(cache.find(set_index) == cache.end())
            {
                cache[set_index] = std::deque<int>();
            }

            auto& cache_set = cache[set_index];

            //Check if block is in set
            if (std::find(cache_set.begin(), cache_set.end(), next_block) == cache_set.end()) {
                cache_set.push_back(next_block);        //Move it to the bak
                if (cache_set.size() > num_ways) 
                {
                    cache_set.pop_front();
                }
            }
        }
    }

public:
    // Constructor
    CacheSimulator(size_t cache_size, size_t block_size, const std::string& associativity, size_t prefetch_size)
        : cache_size(cache_size), block_size(block_size), associativity(associativity), prefetch_size(prefetch_size) {
        num_blocks = cache_size / block_size;

        //Check the associativuy of the set, Direct, assoc, assoc# with # being a multiple of 2
        if (associativity == "direct") {
            num_ways = 1;
            num_sets = num_blocks;
        } else if (associativity == "assoc") {
            num_ways = num_blocks;
            num_sets = 1;
        } else if (associativity.rfind("assoc:", 0) == 0) {
            num_ways = std::stoi(associativity.substr(6));
            if ((num_blocks % num_ways) != 0) throw std::invalid_argument("Invalid associativity.");
            num_sets = num_blocks / num_ways;
        } else {
            throw std::invalid_argument("Invalid associativity format.");
        }

        cache = std::unordered_map<int, std::deque<int>>(num_sets);
    }

    // Simulate cache operation
    void simulate(const std::string& trace_file) {
        std::ifstream file(trace_file);
        if (!file.is_open()) {
            std::cerr << "Error: Trace file '" << trace_file << "' does not exist." << std::endl;
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line == "#eof") break;

            std::istringstream iss(line);
            char operation;
            std::string address_str;

            if (!(iss >> operation >> address_str)) continue; // Skip malformed lines

            try {
                size_t address = std::stoull(address_str, nullptr, 16);
                size_t block_address = address / block_size;
                access_block(block_address);

                if (prefetch_size > 0 && operation == 'R') prefetch_blocks(block_address);
            } catch (const std::exception&) {
                std::cerr << "Skipping malformed line: " << line << std::endl;
                continue; // Skip malformed address
            }
        }

        size_t total_accesses = hits + misses;
        double hit_rate = 0.0;
        if(total_accesses > 0)
        {
             hit_rate = (static_cast<double>(hits) / total_accesses) * 100;
        }

        std::cout << "Cache Hits: " << hits << std::endl;
        std::cout << "Cache Misses: " << misses << std::endl;
        std::cout << "Hit Rate: " << std::fixed << std::setprecision(2) << hit_rate << "%" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "To run it ./cacheSimulator <cache size> <block size> <associativity> <prefetch size> <trace file>" << std::endl;
        return 1;
    }

    generate_trace_file(10000, "memory_trace.txt");

    try {
        size_t cache_size = std::stoul(argv[1]);
        size_t block_size = std::stoul(argv[2]);
        std::string associativity = argv[3];
        size_t prefetch_size = std::stoul(argv[4]);
        std::string trace_file = argv[5];

        if ((cache_size & (cache_size - 1)) != 0 || (block_size & (block_size - 1)) != 0) {
            throw std::invalid_argument("Cache size and block size must be powers of 2.");
        }

        CacheSimulator simulator(cache_size, block_size, associativity, prefetch_size);
        simulator.simulate(trace_file);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
