// prophage_signal_processor.cpp

#include "prophage_signal_processor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <cmath>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file> <algorithm> [parameters] [num_threads]\n";
        std::cerr << "Use -h or --help for more information.\n";
        return 1;
    }

    if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
        printHelp();
        return 0;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    std::string algorithm = argv[3];
    int numThreads = std::thread::hardware_concurrency();

    auto [input, trueLabels] = readCSV(inputFile);
    std::vector<int> output;

    enum AlgorithmType { MWA, RLE, DBSCAN, MEDIAN, CCL, UNKNOWN };
    AlgorithmType algoType = UNKNOWN;

    if (algorithm == "mwa") algoType = MWA;
    else if (algorithm == "rle") algoType = RLE;
    else if (algorithm == "dbscan") algoType = DBSCAN;
    else if (algorithm == "median") algoType = MEDIAN;
    else if (algorithm == "ccl") algoType = CCL;

    switch (algoType) {
        case MWA:
            if (argc < 6) {
                std::cerr << "Moving Window Average requires window size and threshold.\n";
                return 1;
            }
            {
                int windowSize = std::stoi(argv[4]);
                double threshold = std::stod(argv[5]);
                if (argc >= 7) numThreads = std::stoi(argv[6]);
                output = movingWindowAverage(input, windowSize, threshold, numThreads);
            }
            break;

        case RLE:
            if (argc < 5) {
                std::cerr << "Run Length Encoding requires minimum length.\n";
                return 1;
            }
            {
                int minLength = std::stoi(argv[4]);
                if (argc >= 6) numThreads = std::stoi(argv[5]);
                output = runLengthEncoding(input, minLength, numThreads);
            }
            break;

        case DBSCAN:
            if (argc < 6) {
                std::cerr << "DBSCAN requires eps and minPts.\n";
                return 1;
            }
            {
                int eps = std::stoi(argv[4]);
                int minPts = std::stoi(argv[5]);
                if (argc >= 7) numThreads = std::stoi(argv[6]);
                output = dbscan(input, eps, minPts, numThreads);
            }
            break;

        case MEDIAN:
            if (argc < 5) {
                std::cerr << "Median Filter requires window size.\n";
                return 1;
            }
            {
                int windowSize = std::stoi(argv[4]);
                if (argc >= 6) numThreads = std::stoi(argv[5]);
                output = medianFilter(input, windowSize, numThreads);
            }
            break;

        case CCL:
            if (argc < 6) {
                std::cerr << "Connected Component Labeling requires minimum size and gap tolerance.\n";
            return 1;
            }
            {
            int minSize = std::stoi(argv[4]);
            int gapTolerance = std::stoi(argv[5]);
            if (argc >= 7) numThreads = std::stoi(argv[6]);
                output = connectedComponentLabeling(input, minSize, gapTolerance, numThreads);
            }
            break;

        case UNKNOWN:
        default:
            std::cerr << "Unknown algorithm: " << algorithm << "\n";
            return 1;
    }

    writeCSV(outputFile, output);
    std::cout << "Processing complete. Output written to " << outputFile << "\n";

    calculateMetrics(trueLabels, input, output);

    return 0;
}

std::vector<int> movingWindowAverage(const std::vector<int>& input, int windowSize, double threshold, int numThreads) {
    std::vector<int> output(input.size(), 0);
    std::vector<std::thread> threads;
    
    auto worker = [&](int start, int end) {
        int sum = 0;
        for (int i = start; i < end; ++i) {
            sum += input[i];
            if (i >= windowSize) sum -= input[i - windowSize];
            
            if (i >= windowSize - 1) {
                double avg = static_cast<double>(sum) / windowSize;
                output[i - windowSize / 2] = (avg >= threshold) ? 1 : 0;
            }
        }
    };

    int chunkSize = input.size() / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize;
        int end = (i == numThreads - 1) ? input.size() : (i + 1) * chunkSize;
        threads.emplace_back(worker, start, end);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return output;
}

std::vector<int> runLengthEncoding(const std::vector<int>& input, int minLength, int numThreads) {
    std::vector<int> output(input.size(), 0);
    std::vector<std::pair<int, int>> runs;
    std::mutex runsMutex;

    auto worker = [&](int start, int end) {
        int count = 0;
        int runStart = -1;
        
        for (int i = start; i < end; ++i) {
            if (input[i] == 1) {
                if (count == 0) runStart = i;
                count++;
            } else {
                if (count >= minLength) {
                    std::lock_guard<std::mutex> lock(runsMutex);
                    runs.emplace_back(runStart, i);
                }
                count = 0;
            }
        }
        
        if (count >= minLength) {
            std::lock_guard<std::mutex> lock(runsMutex);
            runs.emplace_back(runStart, end);
        }
    };

    std::vector<std::thread> threads;
    int chunkSize = input.size() / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize;
        int end = (i == numThreads - 1) ? input.size() : (i + 1) * chunkSize;
        threads.emplace_back(worker, start, end);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (const auto& run : runs) {
        std::fill(output.begin() + run.first, output.begin() + run.second, 1);
    }

    return output;
}

std::vector<int> dbscan(const std::vector<int>& input, int eps, int minPts, int numThreads) {
    std::vector<int> output(input.size(), 0);
    std::vector<std::atomic<bool>> visited(input.size());
    std::vector<std::mutex> pointMutexes(input.size());

    auto expandCluster = [&](int point) {
        std::queue<int> seeds;
        seeds.push(point);

        while (!seeds.empty()) {
            int currentPoint = seeds.front();
            seeds.pop();

            if (!visited[currentPoint].load()) {
                visited[currentPoint].store(true);
                if (input[currentPoint] == 1) {
                    std::lock_guard<std::mutex> lock(pointMutexes[currentPoint]);
                    output[currentPoint] = 1;

                    for (int i = std::max(0, currentPoint - eps); i < std::min(static_cast<int>(input.size()), currentPoint + eps + 1); ++i) {
                        if (!visited[i].load()) seeds.push(i);
                    }
                }
            }
        }
    };

    auto worker = [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            if (!visited[i].load() && input[i] == 1) {
                int count = 0;
                for (int j = std::max(0, i - eps); j < std::min(static_cast<int>(input.size()), i + eps + 1); ++j) {
                    if (input[j] == 1) count++;
                }

                if (count >= minPts) {
                    expandCluster(i);
                }
            }
        }
    };

    std::vector<std::thread> threads;
    int chunkSize = input.size() / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize;
        int end = (i == numThreads - 1) ? input.size() : (i + 1) * chunkSize;
        threads.emplace_back(worker, start, end);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return output;
}

std::vector<int> medianFilter(const std::vector<int>& input, int windowSize, int numThreads) {
    std::vector<int> output(input.size());

    auto worker = [&](int start, int end) {
        std::vector<int> window(windowSize);
        for (int i = start; i < end; ++i) {
            for (int j = 0; j < windowSize; ++j) {
                int idx = i - windowSize / 2 + j;
                window[j] = (idx >= 0 && idx < input.size()) ? input[idx] : 0;
            }

            std::sort(window.begin(), window.end());
            output[i] = window[windowSize / 2];
        }
    };

    std::vector<std::thread> threads;
    int chunkSize = input.size() / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize;
        int end = (i == numThreads - 1) ? input.size() : (i + 1) * chunkSize;
        threads.emplace_back(worker, start, end);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return output;
}

/*
std::vector<int> connectedComponentLabeling(const std::vector<int>& input, int minSize, int numThreads) {
    std::vector<int> output(input.size(), 0);
    std::vector<int> labels(input.size(), 0);
    std::atomic<int> currentLabel(1);

    auto worker = [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            if (input[i] == 1 && labels[i] == 0) {
                int componentStart = i;
                int label = currentLabel.fetch_add(1);
                while (i < end && input[i] == 1) {
                    labels[i] = label;
                    ++i;
                }
                if (i - componentStart >= minSize) {
                    std::fill(output.begin() + componentStart, output.begin() + i, 1);
                }
            }
        }
    };

    std::vector<std::thread> threads;
    int chunkSize = input.size() / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize;
        int end = (i == numThreads - 1) ? input.size() : (i + 1) * chunkSize;
        threads.emplace_back(worker, start, end);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return output;
}
*/
std::vector<int> connectedComponentLabeling(const std::vector<int>& input, int minSize, int gapTolerance, int numThreads) {
    std::vector<int> output(input.size(), 0);
    std::vector<int> labels(input.size(), 0);
    std::atomic<int> currentLabel(1);

    auto worker = [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            if (input[i] == 1 && labels[i] == 0) {
                int componentStart = i;
                int label = currentLabel.fetch_add(1);
                
                // Forward pass: label and expand
                while (i < end) {
                    labels[i] = label;
                    ++i;
                    // Check for gap
                    int gapSize = 0;
                    while (i < end && input[i] == 0 && gapSize < gapTolerance) {
                        labels[i] = label;
                        ++i;
                        ++gapSize;
                    }
                    if (i < end && input[i] == 1) {
                        continue; // Continue expanding
                    } else {
                        break; // End of component
                    }
                }
                
                // Backward pass: expand in reverse direction
                for (int j = componentStart - 1; j >= start && componentStart - j <= gapTolerance; --j) {
                    if (input[j] == 1) {
                        componentStart = j;
                        break;
                    }
                    labels[j] = label;
                }
                
                // Check component size and set output
                int componentSize = i - componentStart;
                if (componentSize >= minSize) {
                    std::fill(output.begin() + componentStart, output.begin() + i, 1);
                }
            }
        }
    };

    std::vector<std::thread> threads;
    int chunkSize = input.size() / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize;
        int end = (i == numThreads - 1) ? input.size() : (i + 1) * chunkSize;
        threads.emplace_back(worker, start, end);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return output;
}

std::pair<std::vector<int>, std::vector<int>> readCSV(const std::string& filename) {
    std::vector<int> labels;
    std::vector<int> trueLabels;
    std::ifstream file(filename);
    std::string line;

    // Read header
    std::getline(file, line);
    
    // Read data
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        int i = 0;
        while (std::getline(iss, token, ',')) {
            if (i == 1) { // Assuming "labels" is the second column (index 1)
                labels.push_back(std::stoi(token));
            } else if (i == 3) { // Assuming "true_labels" is the third column (index 3)
                trueLabels.push_back(std::stoi(token));
            }
            i++;
        }
    }

    return {labels, trueLabels};
}

void writeCSV(const std::string& filename, const std::vector<int>& data) {
    std::ofstream file(filename);
    file << "label\n";
    for (int value : data) {
        file << value << "\n";
    }
}

void calculateMetrics(const std::vector<int>& trueLabels, const std::vector<int>& predictedLabels, const std::vector<int>& clusteredLabels) {
    auto calculateMetricsForLabels = [](const std::vector<int>& true_labels, const std::vector<int>& pred_labels) {
        int tp = 0, fp = 0, tn = 0, fn = 0;
        for (size_t i = 0; i < true_labels.size(); ++i) {
            if (true_labels[i] == 1 && pred_labels[i] == 1) tp++;
            else if (true_labels[i] == 0 && pred_labels[i] == 1) fp++;
            else if (true_labels[i] == 0 && pred_labels[i] == 0) tn++;
            else if (true_labels[i] == 1 && pred_labels[i] == 0) fn++;
        }

        double accuracy = static_cast<double>(tp + tn) / (tp + tn + fp + fn);
        double precision = static_cast<double>(tp) / (tp + fp);
        double recall = static_cast<double>(tp) / (tp + fn);
        double f1 = 2 * (precision * recall) / (precision + recall);
        
        double mcc_numerator = static_cast<double>(tp * tn - fp * fn);
        double mcc_denominator = std::sqrt(static_cast<double>(tp + fp) * (tp + fn) * (tn + fp) * (tn + fn));
        double mcc = mcc_numerator / mcc_denominator;

        std::cout << "Accuracy: " << accuracy << std::endl;
        std::cout << "Precision: " << precision << std::endl;
        std::cout << "Recall: " << recall << std::endl;
        std::cout << "F1 Score: " << f1 << std::endl;
        std::cout << "MCC: " << mcc << std::endl;
    };

    std::cout << "Metrics before clustering:" << std::endl;
    calculateMetricsForLabels(trueLabels, predictedLabels);

    std::cout << "\nMetrics after clustering:" << std::endl;
    calculateMetricsForLabels(trueLabels, clusteredLabels);
}

void printHelp() {
     std::cout << "Usage: prophage_signal_processor <input_file> <output_file> <algorithm> [parameters] [num_threads]\n\n"
              << "Algorithms:\n"
              << "  mwa <window_size> <threshold>   : Moving Window Average\n"
              << "  rle <min_length>                : Run Length Encoding\n"
              << "  dbscan <eps> <min_pts>          : DBSCAN\n"
              << "  median <window_size>            : Median Filter\n"
              << "  ccl <min_size>                  : Connected Component Labeling\n\n"
              << "Example:\n"
              << "  prophage_signal_processor input.csv output.csv mwa 5 0.6 4\n";
}
