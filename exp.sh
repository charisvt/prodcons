#!/bin/bash

executable="./prod-cons"
num_runs=500

# Create an output file for the results
output_file="results.csv"
echo "qt,avg_waiting_time" > "${output_file}"

# Loop over the desired qt values
for qt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
  # Initialize the sum of waiting times
  sum_waiting_time=0

  # Run the program num_runs times and accumulate the waiting times
  for run in $(seq 1 ${num_runs}); do
    waiting_time=$(${executable} ${qt})
    sum_waiting_time=$(echo "${sum_waiting_time} + ${waiting_time}" | bc)
  done

  # Calculate the average waiting time for the current qt value
  avg_waiting_time=$(echo "scale=4; ${sum_waiting_time} / ${num_runs}" | bc)

  # Append the result to the output file
  echo "${qt},${avg_waiting_time}" >> "${output_file}"
done
