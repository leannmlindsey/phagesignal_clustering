#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;
// Function to process CSV file with prediction data
void process_predictions_csv(const std::string& input_filename, 
                           const std::string& output_filename,
                           double threshold = 0.5) {
    std::ifstream inFile(input_filename);
    std::ofstream outFile(output_filename);
    
    if (!inFile.is_open()) {
        throw std::runtime_error("Could not open input file: " + input_filename);
    }
    
    if (!outFile.is_open()) {
        throw std::runtime_error("Could not open output file: " + output_filename);
    }
    
    // Write header for output CSV
    outFile << "Seq_ID,prob_0,prob_1,predicted_label" << std::endl;
    
    std::string line;
    // Skip header line
    std::getline(inFile, line);
    
    // Process each line
    while (std::getline(inFile, line)) {
        std::istringstream ss(line);
        std::string seqID, predictions;
        
        // Parse the line
        if (std::getline(ss, seqID, ',') && std::getline(ss, predictions)) {
            // Remove brackets from predictions
            predictions = predictions.substr(1, predictions.length() - 2);
            
            // Parse the two probability values
            std::istringstream pred_ss(predictions);
            std::string prob0_str, prob1_str;
            
            // Get first probability
            std::getline(pred_ss, prob0_str, ',');
            // Get second probability and remove leading space
            std::getline(pred_ss, prob1_str);
            while (prob1_str[0] == ' ') {
                prob1_str = prob1_str.substr(1);
            }
            
            // Convert strings to doubles
            double prob0 = std::stod(prob0_str);
            double prob1 = std::stod(prob1_str);
            
            // Determine predicted label
            int predicted_label = (prob1 >= threshold) ? 1 : 0;
            
            // Write to output file
            outFile << seqID << ","
                   << prob0 << ","
                   << prob1 << ","
                   << predicted_label << std::endl;
        }
    }
    
    inFile.close();
    outFile.close();
}

int main(int argc, char* argv[]) {
    // Check if input path is provided
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_path> [threshold]" << std::endl;
        std::cerr << "input_path can be a single file or directory" << std::endl;
        std::cerr << "Example: " << argv[0] << " ../data/raw_inference_data 0.5" << std::endl;
        return 1;
    }

    std::string input_path = argv[1];
    
    // Get threshold from command line if provided
    double threshold = 0.5;
    if (argc >= 3) {
        try {
            threshold = std::stod(argv[2]);
        } catch (const std::exception& e) {
            std::cerr << "Error: Invalid threshold value. Using default (0.5)" << std::endl;
        }
    }

    try {
        if (fs::is_directory(input_path)) {
        // Process all CSV files in directory
        for (const auto& entry : fs::directory_iterator(input_path)) {
            if (entry.path().extension() == ".csv") {
                std::string input_file = entry.path().string();
                std::string base_filename = entry.path().filename().string();
                std::string output_file = "../data/processed_data/" + base_filename;
                
                try {
                    std::cout << "Processing: " << input_file << std::endl;
                    process_predictions_csv(input_file, output_file, threshold);
                } catch (const std::exception& e) {
                    std::cerr << "Error processing file " << input_file << ": " << e.what() << std::endl;
                    std::cerr << "Continuing with next file..." << std::endl;
                    continue;  // Skip to next file instead of stopping
                }
            }
        }
        std::cout << "Processing completed. Check error messages above for any skipped files." << std::endl;   
        } else {
            // Process single file
            size_t last_slash = input_path.find_last_of("/\\");
            std::string base_filename = (last_slash != std::string::npos) ? 
                input_path.substr(last_slash + 1) : input_path;
            std::string output_file = "../data/processed_data/" + base_filename;
            
            process_predictions_csv(input_path, output_file, threshold);
            std::cout << "File processed successfully!" << std::endl;
            std::cout << "Output written to: " << output_file << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
