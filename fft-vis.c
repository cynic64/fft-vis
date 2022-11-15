/*
 * TODO: instead of summing over the entire array every time, store the last sum, subtract the oldest element, add
 * the newest element
 */
#include <stdlib.h>
#include <string.h>
#include <ladspa.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include <fftw3.h>

#define SAMPLE_RATE		48000
// How many sample chunks will be stored. Sample chunk size is 2048 for me
#define BUFFER_SIZE		320
// How many bars in the visualizer there are
#define BAR_COUNT		200

// Ports
#define PORT_COUNT		2
#define FV_INPUT		0
#define FV_OUTPUT		1

// LADSPA_Handle
typedef struct {
	unsigned long sampleRate;

	// We take an average over a longer time period than each sample chunk.
	// This an array of BUFFER_SIZE pointers to arrays matching the FFT size.
	fftwf_complex *buffer[BUFFER_SIZE];
	int buffer_idx;			// Next address in buffer to write to

	// FFT stuff
	fftwf_plan fft_plan;
	float* fft_input;
	fftwf_complex* fft_output;
	int fft_size;			// We'll have to re-create the FFT if the sample chunk size changes,
					// so we better keep track of the current size

	LADSPA_Data *input_buffer;
	LADSPA_Data *output_buffer;	// Dummy output to make jack-rack happy
} FFTVis;

// handle new instance
static LADSPA_Handle instantiateFFTVis(const LADSPA_Descriptor *descriptor, unsigned long sampleRate) {
	FFTVis *fft_vis;

	fft_vis = (FFTVis *)malloc(sizeof(FFTVis));
	if (fft_vis == NULL) return NULL;

	fft_vis->sampleRate = sampleRate;
	fft_vis->fft_plan = NULL;
	fft_vis->fft_input = NULL;
	fft_vis->fft_output = NULL;
	fft_vis->fft_size = -1;

	fft_vis->buffer_idx = 0;
	for (int i = 0; i < BUFFER_SIZE; i++) fft_vis->buffer[i] = NULL;

	return fft_vis;
}

// Assign given parameters accordingly in handle
static void connectPort(LADSPA_Handle handle, unsigned long port, LADSPA_Data *data) {
	FFTVis *fft_vis = (FFTVis *) handle;

	switch (port) {
	case FV_INPUT:
		fft_vis->input_buffer = data;
		break;
	case FV_OUTPUT:
		fft_vis->output_buffer = data;
		break;
	}
}

// initialize the state
static void activateFFTVis(LADSPA_Handle handle) {
	FFTVis *fft_vis = (FFTVis *)handle;
}

// main handler. forward samples or mute according to state
static void runFFTVis(LADSPA_Handle handle, unsigned long sampleCount) {
	FFTVis *fft_vis = (FFTVis *) handle;

	// Maybe re-create fft_plan / output buffer
	if (fft_vis->fft_size != sampleCount) {
		printf("Re-creating...\n");
		if (fft_vis->fft_plan != NULL) fftwf_destroy_plan(fft_vis->fft_plan);
		if (fft_vis->fft_input != NULL) fftwf_free(fft_vis->fft_input);
		if (fft_vis->fft_output != NULL) fftwf_free(fft_vis->fft_output);

		fft_vis->fft_input = (float *) fftwf_alloc_complex(sizeof(float) * sampleCount);
		fft_vis->fft_output = (fftwf_complex*) fftwf_alloc_complex(sizeof(fftwf_complex) * sampleCount);
		fft_vis->fft_plan = fftwf_plan_dft_r2c_1d(sampleCount,
			fft_vis->fft_input, fft_vis->fft_output, FFTW_ESTIMATE);

		for (int i = 0; i < BUFFER_SIZE; i++) {
			if (fft_vis->buffer[i] != NULL) free(fft_vis->buffer[i]);
			fft_vis->buffer[i] = calloc(sampleCount, sizeof(fftwf_complex));
		}

		fft_vis->fft_size = sampleCount;
	}

	printf("sampleCount: %lu\n", sampleCount);

	// Fill input and execute
	memcpy(fft_vis->fft_input, fft_vis->input_buffer, sizeof(float) * sampleCount);
	fftwf_execute(fft_vis->fft_plan);

	// Copy output to long-term buffer
	memcpy(fft_vis->buffer[fft_vis->buffer_idx], fft_vis->fft_output, sizeof(fftwf_complex) * sampleCount);
	printf("%d %d\n", fft_vis->buffer_idx, (fft_vis->buffer_idx + 1) % BUFFER_SIZE);
	fft_vis->buffer_idx = (fft_vis->buffer_idx + 1) % BUFFER_SIZE;
	printf("%d\n", fft_vis->buffer_idx);

	// Print output
	// There are too many bars to print indivually, so instead we average chunks of them. The size of each
	// chunk should grow logarithmically, since frequency is logarithmic in the sense that doubling frequency
	// means a note one octave higher.
	int frequency_count = sampleCount / 2;
	double factor = pow((double) frequency_count, 1.0 / BAR_COUNT);
	float bar_amplitude_sums[BAR_COUNT] = {0};

	// For each FFT output in fft_vis->buffer...
	for (int i = 0; i < BUFFER_SIZE; i++) {
		double start_idx = 1;
		double end_idx = factor;
		int bar_amplitude_idx = 0;

		// For every bar...
		while ((int) end_idx < frequency_count - 1) {
			// Compute an average over this chunk of bins
			double amplitude_sum = 0;
			int count = 0;

			for (int j = start_idx; j < end_idx; j++) {
				// Square complex number
				float real = fft_vis->buffer[i][j][0], imag = fft_vis->buffer[i][j][1];
				float amplitude = sqrt(real * real + imag * imag) / sampleCount;
				amplitude_sum += amplitude;
				count++;
			}

			float average_amplitude = amplitude_sum / count;

			assert(bar_amplitude_idx < BAR_COUNT);
			bar_amplitude_sums[bar_amplitude_idx++] += average_amplitude;

			start_idx *= factor;
			end_idx *= factor;
		}
	}

	// Actually print the bars
	printf("---\n");
	for (int i = 0; i < BAR_COUNT; i++) {
		float average_amplitude = bar_amplitude_sums[i] / BUFFER_SIZE;
		//printf("%10.5f\n", average_amplitude);

		int char_count = average_amplitude / 0.15 * 300;
		for (int j = 0; j < char_count; j++) printf("#");
		printf("\n");
	}
	printf("---\n");
}

// free the handle
static void cleanupFFTVis(LADSPA_Handle handle) {
	FFTVis *fft_vis = (FFTVis *) handle;

	if (fft_vis->fft_plan != NULL) fftwf_destroy_plan(fft_vis->fft_plan);
	if (fft_vis->fft_output != NULL) fftwf_free(fft_vis->fft_output);

	free(handle);
}

static LADSPA_Descriptor *descriptor = NULL;

// On plugin load
static void __attribute__ ((constructor)) init() {
	LADSPA_PortDescriptor * portDescriptors;
	LADSPA_PortRangeHint * portRangeHints;
	char ** portNames;

	descriptor = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));

	if (!descriptor) {
		return;
	}

	descriptor->UniqueID  = 123; // should be unique
	descriptor->Label     = strdup("fft_vis");
	descriptor->Name      = strdup("FFT visualizer");
	descriptor->Maker     = strdup("cynic64");
	descriptor->Copyright = strdup("None");

	descriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;

	descriptor->PortCount = PORT_COUNT;

	portDescriptors = (LADSPA_PortDescriptor *) calloc(PORT_COUNT, sizeof(LADSPA_PortDescriptor));

	portDescriptors[FV_INPUT] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
	portDescriptors[FV_OUTPUT] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;

	descriptor->PortDescriptors = portDescriptors;

	portNames = (char **) calloc(PORT_COUNT, sizeof(char *));
	portNames[FV_INPUT] = strdup("Input");
	portNames[FV_INPUT] = strdup("Output");

	descriptor->PortNames = (const char * const *) portNames;

	portRangeHints = (LADSPA_PortRangeHint *) calloc(PORT_COUNT, sizeof(LADSPA_PortRangeHint));

	portRangeHints[FV_INPUT].HintDescriptor = 0;
	portRangeHints[FV_OUTPUT].HintDescriptor = 0;

	descriptor->PortRangeHints = portRangeHints;

	descriptor->instantiate = instantiateFFTVis;
	descriptor->connect_port = connectPort;
	descriptor->activate = activateFFTVis;
	descriptor->run = runFFTVis;
	descriptor->run_adding = NULL;
	descriptor->set_run_adding_gain = NULL;
	descriptor->deactivate = NULL;
	descriptor->cleanup = cleanupFFTVis;
}

// On plugin unload
static void __attribute__ ((destructor)) fini() {
	if (descriptor == NULL) return;

	free((char *) descriptor->Label);
	free((char *) descriptor->Name);
	free((char *) descriptor->Maker);
	free((char *) descriptor->Copyright);
	free((char *) descriptor->PortDescriptors);
	for (int i=0; i<PORT_COUNT; i++) {
		free((char *) descriptor->PortNames[i]);
	}

	free((char **) descriptor->PortNames);
	free((LADSPA_PortRangeHint *) descriptor->PortRangeHints);
	free(descriptor);
}

// we only have one type of plugin
const LADSPA_Descriptor * ladspa_descriptor(unsigned long index) {
	if (index != 0) {
		return NULL;
	}

	return descriptor;
}
