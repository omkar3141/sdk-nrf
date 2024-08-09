import re
import matplotlib.pyplot as plt


#TODO: Paste your log here:
log = """
Button 1: Received response: 0 Time delta: 19 ms
Button 1: Received response: 1 Time delta: 19 ms
Button 1: Received response: 0 Time delta: 28 ms
Button 1: Received response: 1 Time delta: 30 ms
Button 1: Received response: 0 Time delta: 18 ms
Button 1: Received response: 1 Time delta: 22 ms
Button 1: Received response: 0 Time delta: 25 ms
Button 1: Received response: 1 Time delta: 18 ms
Button 1: Received response: 0 Time delta: 24 ms
Button 1: Received response: 1 Time delta: 20 ms
Button 1: Received response: 0 Time delta: 16 ms
Button 1: Received response: 1 Time delta: 28 ms
Button 1: Received response: 0 Time delta: 47 ms
Button 1: Received response: 1 Time delta: 23 ms
"""


# Main function
if __name__ == '__main__':
        # Extract time delta values using regular expressions
        time_deltas = [int(match) for match in re.findall(r'Time delta: (\d+) ms', log)]

        # Plot histogram
        plt.hist(time_deltas, bins=range(min(time_deltas), max(time_deltas) + 2, 1), edgecolor='black')
        plt.title('Histogram of Time Delta Readings')
        plt.xlabel('Time Delta (ms)')
        plt.ylabel('Frequency')
        plt.show()
