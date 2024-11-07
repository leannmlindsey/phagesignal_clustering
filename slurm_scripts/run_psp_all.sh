#!/bin/bash

# Check if directory is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <directory>"
    exit 1
fi

directory=$1

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
        for algo in mwa rle dbscan median ccl; do
            case $algo in
                mwa)
                    ./prophage_signal_processor "$file" "${temp_dir}/${filename%.csv}_${algo}.csv" mwa 70 0.2 4 > "${temp_dir}/${filename%.csv}_${algo}_output.txt"
                    ;;
                rle)
                    ./prophage_signal_processor "$file" "${temp_dir}/${filename%.csv}_${algo}.csv" rle 8 4 > "${temp_dir}/${filename%.csv}_${algo}_output.txt"
                    ;;
                dbscan)
                    ./prophage_signal_processor "$file" "${temp_dir}/${filename%.csv}_${algo}.csv" dbscan 50 20 4 > "${temp_dir}/${filename%.csv}_${algo}_output.txt"
                    ;;
                median)
                    ./prophage_signal_processor "$file" "${temp_dir}/${filename%.csv}_${algo}.csv" median 50 4 > "${temp_dir}/${filename%.csv}_${algo}_output.txt"
                    ;;
                ccl)
                    ./prophage_signal_processor "$file" "${temp_dir}/${filename%.csv}_${algo}.csv" ccl 40 8 4 > "${temp_dir}/${filename%.csv}_${algo}_output.txt"
                    ;;
            esac
            echo "made it through processing"
            # Extract metrics and add to results table
            metrics=$(extract_metrics "${temp_dir}/${filename%.csv}_${algo}_output.txt" $algo)
            echo "$filename,$algo,$metrics" >> results_table.csv
	    echo "made it through extraction"

        done
    fi
done

# Calculate and add averages to the results table
echo "" >> results_table.csv
echo "Algorithm" > index.csv
echo "Moving Window Average" >> index.csv
echo "Run Length Encoding" >> index.csv
echo "DBScan" >> index.csv
echo "Median Filter" >> index.csv
echo "Connected Component Labeling" >> index.csv

echo "Accuracy,Precision,Recall,F1,MCC"> averages.csv
for algo in mwa rle dbscan median ccl; do
	cat results_table.csv | grep $algo | sed 's/-nan/0/g' | awk -F, 'NR > 1 && $3 != "" {OFS = ","} { acc_sum += $3; prec_sum += $4; recall_sum += $5; f1_sum += $6; mcc_sum += $7; count++ } END { print acc_sum/count,prec_sum/count,recall_sum/count,f1_sum/count,mcc_sum/count }' >> averages.csv  
done
paste -d , index.csv averages.csv > final_averages.csv
rm index.csv 
rm averages.csv
echo "Processing complete. Full results are in results_table.csv. Algorithm averages are in final_averages.csv"


# Clean up temporary directory
rm -rf "$temp_dir"
