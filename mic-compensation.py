'''
Interpolation should be logarithmic which will be a pain
'''

import matplotlib.pyplot as plt
import numpy as np

# Define points to interpolate between
frequencies = [50, 100, 200, 500, 1000, 2000, 5000, 10000, 12000, 17000]
responses = [-8, -1, 0, 0, 0, 4, 9, 9, 14, 0]
assert(len(frequencies) == len(responses))

# Normalize responses so lowest response is 0
responses = [x - min(responses) for x in responses]
print('Normalized responses:', responses)

# Flip responses to produce a compensation curve
compensations = [max(responses) - x for x in responses]

# Add 1 to everything - if the lowest response is 0 we won't be able to take the log later
compensations = [x + 1 for x in compensations]
print('Compensations in dB:', compensations)

# Convert to what the microphone measures: for every +6dB, the volume doubles
compensations = np.power(2.0, np.log(compensations) / np.log(6))

# Interpolate for necessary points
interpolated_frequencies = [31, 33, 34, 36, 38, 40, 42, 44, 46, 48, 51, 53, 56, 59, 62, 65, 68, 72, 75, 79, 83, 87, 92, 96, 101, 106, 112, 117, 123, 129, 136, 142, 150, 157, 165, 173, 182, 191, 201, 211, 221, 232, 244, 256, 269, 283, 297, 312, 327, 344, 361, 379, 398, 418, 439, 461, 484, 508, 533, 560, 588, 617, 648, 681, 715, 750, 788, 827, 869, 912, 958, 1006, 1056, 1109, 1164, 1223, 1284, 1348, 1416, 1486, 1561, 1639, 1721, 1807, 1897, 1992, 2092, 2196, 2306, 2421, 2543, 2670, 2803, 2943, 3091, 3245, 3407, 3578, 3757, 3945, 4142, 4349, 4566, 4795, 5034, 5286, 5551, 5828, 6120, 6426, 6747, 7084, 7438, 7810, 8201, 8611, 9042, 9494, 9968, 10467, 10990, 11540, 12117, 12723, 13359, 14027, 14728, 15465, 16238, 17050, 17902, 18797, 19737]
interpolated_compensations = [np.interp(x, frequencies, compensations) for x in interpolated_frequencies]
print(len(interpolated_compensations))

# Print compensations
print('const double COMPENSATIONS[] = {' + ', '.join([f'{x:05f}' for x in interpolated_compensations]) + '};')

# Plot it
fig, ax = plt.subplots()
ax.plot(interpolated_frequencies, interpolated_compensations)
ax.grid()
plt.xscale("log")
plt.show()

