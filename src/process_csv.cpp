#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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
    // Check if input filename is provided
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_csv_file> [threshold]" << std::endl;
        std::cerr << "Example: " << argv[0] << " predictions.csv 0.5" << std::endl;
        return 1;
    }

    std::string input_filename = argv[1];
    // Generate output filename by adding "processed_" prefix
    std::string output_filename = "processed_" + input_filename;
    
    // Get threshold from command line if provided, otherwise use default 0.5
    double threshold = 0.5;
    if (argc >= 3) {
        try {
            threshold = std::stod(argv[2]);
        } catch (const std::exception& e) {
            std::cerr << "Error: Invalid threshold value. Using default (0.5)" << std::endl;
        }
    }

    try {
        process_predictions_csv(input_filename, output_filename, threshold);
        std::cout << "File processed successfully!" << std::endl;
        std::cout << "Output written to: " << output_filename << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
