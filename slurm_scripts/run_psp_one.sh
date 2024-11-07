#!/bin/bash

# Check if directory is provided
if [ $# -ne 4 ]; then
    echo "Usage: $0 <directory> <algorithm> <param1> <param2>"
    exit 1
fi

directory=$1
algo=$2
param1=$3
param2=$4

# Check if directory exists
if [ ! -d "$directory" ]; then
    echo "Error: Directory $directory does not exist."
    exit 1
fi

# Create a temporary directory for results
temp_dir=$(mktemp -d)

# Create the results table header
echo "Filename,Algorithm,Accuracy,Precision,Recall,F1_Score,MCC" > results_table.csv

# Function to extract metrics from the output
extract_metrics() {
    local file=$1
    local algorithm=$2
    local metrics=$(tail -n 10 "$file" | awk '/Accuracy:/ {acc=$2} /Precision:/ {prec=$2} /Recall:/ {rec=$2} /F1 Score:/ {f1=$3} /MCC:/ {mcc=$2} END {print acc "," prec "," rec "," f1 "," mcc}')
    echo "$metrics"
}
echo "made it to declare variables"

# Initialize variables for average calculation
echo "ade it to the processing loop"
# Process each CSV file in the directory
for file in "$directory"/*.csv; do
    if [ -f "$file" ]; then
        filename=$(basename "$file")
        echo "Processing $filename..."

        # Run each algorithm on the file
        #for algo in mwa rle dbscan median ccl; do
            case $algo in
                mwa)
                    ./prophage_signal_processor "$file" "${temp_dir}/${filename%.csv}_${algo}.csv" mwa $param1 $param2 4 > "${temp_dir}/${filename%.csv}_${algo}_output.txt"
                    ;;
                rle)
                    ./prophage_signal_processor "$file" "${temp_dir}/${filename%.csv}_${algo}.csv" rle $param1 4 > "${temp_dir}/${filename%.csv}_${algo}_output.txt"
                    ;;
                dbscan)
                    ./prophage_signal_processor "$file" "${temp_dir}/${filename%.csv}_${algo}.csv" dbscan $param1 $param2 4 > "${temp_dir}/${filename%.csv}_${algo}_output.txt"
                    ;;
                median)
                    ./prophage_signal_processor "$file" "${temp_dir}/${filename%.csv}_${algo}.csv" median $param1 4 > "${temp_dir}/${filename%.csv}_${algo}_output.txt"
                    ;;
                ccl)
                    ./prophage_signal_processor "$file" "${temp_dir}/${filename%.csv}_${algo}.csv" ccl $param1 $param2 4 > "${temp_dir}/${filename%.csv}_${algo}_output.txt"
                    ;;
            esac
            echo "made it through processing"
            # Extract metrics and add to results table
            metrics=$(extract_metrics "${temp_dir}/${filename%.csv}_${algo}_output.txt" $algo)
            echo "$filename,$algo,$metrics" >> results_table.csv
	    echo "made it through extraction"

        #done
    fi
done

# Calculate and add averages to the results table
echo "" >> results_table.csv
cat results_table.csv | sed 's/-nan/0/g' > results_fixed.csv
echo "Processing complete. Results are in results_table.csv"
# Clean up temporary directory
rm -rf "$temp_dir"
